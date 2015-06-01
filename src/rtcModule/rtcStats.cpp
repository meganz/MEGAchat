#include "rtcStats.h"
#include "strophe.jingle.session.h"
#include <timers.h>

namespace rtcModule
{
using namespace artc;
using namespace std::placeholders;
namespace stats
{

Recorder::Recorder(JingleSession& sess, const StatOptions& options)
    :mSession(sess), mOptions(options), mCurrSample(new Sample), mStats(new RtcStats)
{}

inline void calcBwAverage(Val period, Val currentTotBytes, Val& totBytes, Val& bps, Val& avgBps)
{
    bps = ((float)(currentTotBytes - totBytes)/128) / period; //from bytes/s to kbits/s
    totBytes = currentTotBytes;
    avgBps = (avgBps*4+bps)/5;
}

void Recorder::onComplete(const std::vector<webrtc::StatsReport> &data)
{
    std::shared_ptr<artc::MappedStatsData> mapped(new artc::MappedStatsData(data));
    mega::marshallCall([this, mapped]()
    {
        onStats(mapped);
    });
}
#define AVG(name, var) var = (var + item.longVal(name)) / 2

void Recorder::onStats(const std::shared_ptr<artc::MappedStatsData>& data)
{
    Val period = karere::timestampMs() - mCurrSample->ts;
    bool isFirstSample = mStats->mSamples.empty();
    for (auto& item: *data)
    {
        if (item.type == "ssrc")
        {
            Val width;
            if (item.longVal("googFrameWidthReceived", width)) //video rx
            {
                auto& sample = mCurrSample->vstats.r;
                calcBwAverage(period, item.longVal("bytesReceived"), sample.bt, sample.bps, sample.abps);
                AVG("googFrameRateReceived", sample.fps);
                AVG("googCurrentDelayMs", sample.dly);
                AVG("googJitterBufferMs", sample.jtr);
                sample.pl = item.longVal("packetsLost");
//              vstat.fpsSent = res.stat('googFrameRateOutput'); -- this should be for screen output
                sample.width = width;
                sample.height = item.longVal("googFrameHeightReceived");
            }
            else if (item.longVal("googFrameWidthSent", width)) //video tx
            {
                auto& sample = mCurrSample->vstats.s;
                AVG("googRtt", sample.rtt);
                AVG("googFrameRateSent", sample.fps);
                AVG("googFrameRateInput", sample.cfps);
                AVG("googCaptureJitterMs", sample.cjtr);
                sample.width = width;
                sample.height = item.longVal("googFrameHeightSent");
                if (mStats->mConnInfo.mVcodec.empty())
                    mStats->mConnInfo.mVcodec = item.strVal("googCodecName");
//              s.et = stat('googAvgEncodeMs');
                AVG("googEncodeUsagePercent", sample.el); //(s.et*s.fps)/10; // (encTime*fps/1000ms)*100%
                sample.lcpu = item.longVal("googCpuLimitedResolution");
                sample.lbw = item.longVal("googBandwidthLimitedResolution");
                calcBwAverage(period, item.longVal("bytesSent"), sample.bt, sample.bps, sample.abps);
            }
            else if (item.hasVal("audioInputLevel")) //audio rx
            {
                AVG("googRtt", mCurrSample->astats.rtt);
            }
            else if (item.hasVal("audioOutputLevel")) //audio tx
            {
                AVG("googJitterReceived", mCurrSample->astats.jtr);
                mCurrSample->astats.pl = item.longVal("packetsLost");
            }
        }
        else if (item.hasVal("googCandidatePair") && (item.strVal("googActiveConnection") == "true"))
        {
            if (mHasConnInfo) //happens if peer is Firefox
                continue;

            mHasConnInfo = true;
            bool isRelay = (item.strVal("googLocalCandidateType") == "relay");
            if (isRelay)
            {
                auto& rlySvr = mStats->mConnInfo.mRlySvr;
                rlySvr = item.strVal("googLocalAddress");
                if (rlySvr.empty())
                    rlySvr = "<error getting relay server>";
            }
            mStats->mConnInfo.mCtype = item.strVal("googRemoteCandidateType");
            mStats->mConnInfo.mProto = item.strVal("googTransportType");

            auto& cstat = mCurrSample->cstats;
            AVG("googRtt", cstat.rtt);
            calcBwAverage(period, item.longVal("bytesReceived"), cstat.r.bt, cstat.r.bps, cstat.r.abps);
            calcBwAverage(period, item.longVal("bytesSent"), cstat.r.bt, cstat.r.bps, cstat.r.abps);
        }
        else if (item.type == "VideoBwe")
        {
            mCurrSample->vstats.r.bwav = round((float)item.longVal("googAvailableReceiveBandwidth")/1024);
            auto& sample = mCurrSample->vstats.s;
            sample.bwav = round((float)item.longVal("googAvailableSendBandwidth")/1024);
            sample.gbps = round((float)item.longVal("googTransmitBitrate")/1024); //chrome returns it in bits/s, should be near our calculated bps
            sample.gabps = (sample.gabps*4+sample.gbps)/5;
        }
    } //end item loop
    auto& last = *mStats->mSamples.back();
    bool shouldAddSample = false;

    if (mStats->mSamples.empty())
    {
        shouldAddSample = true;
    }
    else
    {
        Val d_dly = (mCurrSample->vstats.r.dly - last.vstats.r.dly);
        if (d_dly < 0)
            d_dly = -d_dly;
        Val d_vrtt = (mCurrSample->vstats.s.rtt - last.vstats.s.rtt);
        if (d_vrtt < 0)
            d_vrtt = -d_vrtt;
        Val d_auRtt = mCurrSample->astats.rtt - last.astats.rtt;
        if (d_auRtt < 0)
            d_auRtt = -d_auRtt;
        Val d_auJtr = mCurrSample->astats.jtr - last.astats.jtr;
        if (d_auJtr < 0)
            d_auJtr = -d_auJtr;
        Val d_ts = mCurrSample->ts-last.ts;
        Val d_apl = mCurrSample->astats.pl - last.astats.pl;
        shouldAddSample =
            (mCurrSample->vstats.r.width != last.vstats.r.width)
         || (mCurrSample->vstats.s.width != last.vstats.r.width)
         || ((d_ts >= mOptions.maxSamplePeriod)
         || (d_dly > 100) || (d_auRtt > 100)
         || (d_apl > 0) || (d_vrtt > 150) || (d_auJtr > 40));
    }
    if (shouldAddSample)
    {
        mStats->mSamples.push_back(mCurrSample.release());
        mCurrSample.reset(new Sample);
        if (onSample)
        {
            if (isFirstSample) //first sample that we just added
                onSample(&(mStats->mConnInfo), 0);
            onSample(mStats->mSamples.back(), 1);
        }
    }
}
/*


    printf("====================== Stats report received\n");
    for (auto& item: data)
    {
        printf("%s:\n", item.type.c_str());
        for (auto& val: item.values)
            printf("\t'%s' = '%s'\n", val.name, val.value.c_str());
    }
}
*/
void Recorder::start()
{
    assert(mSession.mPeerConn);
    mStats->mCallId = mSession.getCallId();
    mTimer = mega::setInterval([this]()
    {
        mSession.mPeerConn->GetStats(this, nullptr, mStatsLevel);
    }, 1000);
}

void Recorder::terminate(const char* termRsn)
{
    mega::cancelInterval(mTimer);
    mTimer = 0;
    mStats->mTermRsn = termRsn ? termRsn : "(unknown)";
}
Recorder::~Recorder()
{
    if (mTimer)
        mega::cancelInterval(mTimer);
}
}
}
