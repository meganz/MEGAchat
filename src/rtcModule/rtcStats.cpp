#include "rtcStats.h"
#include "strophe.jingle.session.h"
#include <timers.h>
#include <string.h> //for memset
#include <karereCommon.h> //for timestampMs()
#include <rtcModule.h>

namespace rtcModule
{
using namespace artc;
using namespace std::placeholders;
namespace stats
{
BasicStats::BasicStats(const Call& call, const std::string& aTermRsn)
:mIsCaller(call.isCaller()), mTermRsn(aTermRsn), mCallId(call.id()){}

Recorder::Recorder(JingleSession& sess, const Options &options)
    :mSession(sess), mOptions(options), mCurrSample(new Sample), mStats(new RtcStats)
{
    memset(mCurrSample.get(), 0, sizeof(Sample));
    AddRef();
    if (mOptions.scanPeriod < 0)
        mOptions.scanPeriod = 1000;
    if (mOptions.maxSamplePeriod < 0)
        mOptions.maxSamplePeriod = 5000;
    resetBwCalculators();
}

void Recorder::addSample()
{
    Sample* sample = mCurrSample.release();
    assert(sample);
    mStats->mSamples.push_back(sample);
    mCurrSample.reset(new Sample(*sample));
    resetBwCalculators();
}
void Recorder::resetBwCalculators()
{
    mVideoRxBwCalc.reset(&(mCurrSample->vstats.r));
    mVideoTxBwCalc.reset(&(mCurrSample->vstats.s));
    mAudioRxBwCalc.reset(&(mCurrSample->astats.r));
    mAudioTxBwCalc.reset(&(mCurrSample->astats.s));
    mConnRxBwCalc.reset(&(mCurrSample->cstats.r));
    mConnTxBwCalc.reset(&(mCurrSample->cstats.s));
}

void Recorder::BwCalculator::calculate(long periodMs, long newTotalBytes)
{
    long deltaBytes = newTotalBytes - mTotalBytes;
    auto bps = mBwInfo->bps = ((float)(deltaBytes)*1000/128) / periodMs; //from bytes/s to kbits/s
    mTotalBytes = newTotalBytes;
    mBwInfo->bs += deltaBytes;
    mBwInfo->abps = (mBwInfo->abps*4+bps)/5;
}

void Recorder::OnComplete(const std::vector<webrtc::StatsReport>& data)
{
    std::shared_ptr<artc::MappedStatsData> mapped(new artc::MappedStatsData(data));
    mega::marshallCall([this, mapped]()
    {
        try
        {
            onStats(mapped);
        }
        catch(std::exception& e)
        {
            KR_LOG_ERROR("Stats: %s", e.what());
        }
    });
}

void dumpStats(const artc::MappedStatsData& data)
{
    printf("====================== Stats report received\n");
    for (auto& item: data)
    {
        printf("%s:\n", item.type.c_str());
        for (auto& val: item.values)
            printf("\t'%s' = '%s'\n", val.first.c_str(), val.second.c_str());
    }
}

#define AVG(name, var) var = round((float)var + item.longVal(name)) / 2

void Recorder::onStats(const std::shared_ptr<artc::MappedStatsData>& data)
{
//    dumpStats(*data);
    long ts = karere::timestampMs() - mStats->mStartTs;
    long period = ts - mCurrSample->ts;
    mCurrSample->ts = ts;
    for (auto& item: *data)
    {
        if (item.type == "ssrc")
        {
            long width;
            if (item.longVal("googFrameWidthReceived", width)) //video rx
            {
                auto& sample = mCurrSample->vstats.r;
                mVideoRxBwCalc.calculate(period, item.longVal("bytesReceived"));
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
                sample.lcpu = (item.strVal("googCpuLimitedResolution") == "true");
                sample.lbw = (item.strVal("googBandwidthLimitedResolution") == "true");
                mVideoTxBwCalc.calculate(period, item.longVal("bytesSent"));
            }
            else if (item.hasVal("audioInputLevel")) //audio rx
            {
                mAudioRxBwCalc.calculate(period, item.longVal("bytesSent"));
                AVG("googRtt", mCurrSample->astats.rtt);
            }
            else if (item.hasVal("audioOutputLevel")) //audio tx
            {
                mAudioTxBwCalc.calculate(period, item.longVal("bytesReceived"));
                AVG("googJitterReceived", mCurrSample->astats.jtr);
                mCurrSample->astats.pl = item.longVal("packetsLost");
            }
        }
        else if ((item.type == "googCandidatePair") && (item.strVal("googActiveConnection") == "true"))
        {
            if (!mHasConnInfo) //happens if peer is Firefox
            {
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
            }
            auto& cstat = mCurrSample->cstats;
            AVG("googRtt", cstat.rtt);
            mConnRxBwCalc.calculate(period, item.longVal("bytesReceived"));
            mConnTxBwCalc.calculate(period, item.longVal("bytesSent"));
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
    bool shouldAddSample = false;

    if (mStats->mSamples.empty())
    {
        shouldAddSample = true;
    }
    else
    {
        auto& last = *mStats->mSamples.back();
        auto d_dly = (mCurrSample->vstats.r.dly - last.vstats.r.dly);
        if (d_dly < 0)
            d_dly = -d_dly;
        auto d_vrtt = (mCurrSample->vstats.s.rtt - last.vstats.s.rtt);
        if (d_vrtt < 0)
            d_vrtt = -d_vrtt;
        auto d_auRtt = mCurrSample->astats.rtt - last.astats.rtt;
        if (d_auRtt < 0)
            d_auRtt = -d_auRtt;
        auto d_auJtr = mCurrSample->astats.jtr - last.astats.jtr;
        if (d_auJtr < 0)
            d_auJtr = -d_auJtr;
        auto d_ts = mCurrSample->ts-last.ts;
        auto d_apl = mCurrSample->astats.pl - last.astats.pl;
        shouldAddSample =
            (mCurrSample->vstats.r.width != last.vstats.r.width)
         || (mCurrSample->vstats.s.width != last.vstats.s.width)
         || ((d_ts >= mOptions.maxSamplePeriod)
         || (d_dly > 100) || (d_auRtt > 100)
         || (d_apl > 0) || (d_vrtt > 150) || (d_auJtr > 40));

//        printf("dw = %d, dh = %d\n", mCurrSample->vstats.r.width != last.vstats.r.width, mCurrSample->vstats.r.height != last.vstats.r.height);
//        printf("mOptions.maxSamplePeriod = %d\n", mOptions.maxSamplePeriod);
//        printf("d_ts = %llu, d_dly = %ld, d_auRtt = %ld, d_vrtt = %ld, d_auJtr = %ld, d_apl = %ld", d_ts, d_dly, d_auRtt, d_vrtt, d_auJtr, d_apl);
    }
    if (shouldAddSample)
    {
        //KR_LOG_DEBUG("Stats: add sample");
        addSample();
        if (onSample)
        {
            if (mStats->mSamples.size() == 1) //first sample that we just added
                onSample(&(mStats->mConnInfo), 0);
            onSample(mStats->mSamples.back(), 1);
        }
    }
}

void Recorder::start()
{
    assert(mSession.mPeerConn);
    mStats->mIsCaller = mSession.isCaller();
    mStats->mCallId = mSession.mCall.id();
    mStats->mSper = mOptions.scanPeriod;
    mStats->mStartTs = karere::timestampMs();
    mTimer = mega::setInterval([this]()
    {
        mSession.mPeerConn->GetStats(static_cast<webrtc::StatsObserver*>(this), nullptr, mStatsLevel);
    }, mOptions.scanPeriod);
}

void Recorder::terminate(const char* termRsn)
{
    mega::cancelInterval(mTimer);
    mTimer = 0;
    mStats->mDur = karere::timestampMs() - mStats->mStartTs;
    mStats->mTermRsn = termRsn ? termRsn : "(unknown)";
    std::string json;
    mStats->toJson(json);
    printf("============== %s\n", json.c_str());
}
Recorder::~Recorder()
{
    if (mTimer)
        mega::cancelInterval(mTimer);
}

const char* decToString(float v)
{
    static char buf[128];
    snprintf(buf, 127, "%.1f", v);
    return buf;
}

#define JSON_ADD_STR(name, val) json.append("\"" #name "\":\"").append(val)+="\",";
#define JSON_ADD_INT(name, val) json.append("\"" #name "\":").append(std::to_string((long)val))+=',';
#define JSON_ADD_DECNUM(name, val) json.append("\"" #name "\":").append(decToStr(val))+=',';

#define JSON_SUBOBJ(name) json+="\"" name "\":{";
#define JSON_END_SUBOBJ() json[json.size()-1]='}'; json+=','

#define JSON_ADD_SAMPLES_WITH_CONV(path, name, conv)   \
    json.append("\"" #name "\":[");    \
    if (mSamples.empty())              \
        json+=']';                     \
    else                               \
    {                                  \
        for (auto sample: mSamples)    \
            json.append(conv(sample->path name))+=","; \
        json[json.size()-1]=']';       \
        json+=',';                     \
    }
#define JSON_ADD_SAMPLES(path, name) JSON_ADD_SAMPLES_WITH_CONV(path, name, std::to_string)
#define JSON_ADD_DEC_SAMPLES(path, name) JSON_ADD_SAMPLES_WITH_CONV(path, name, decToString)

#define JSON_ADD_BWINFO(path)           \
    JSON_ADD_SAMPLES(path., bs);        \
    JSON_ADD_SAMPLES(path., bps);       \
    JSON_ADD_SAMPLES(path., abps)

void RtcStats::toJson(std::string& json) const
{
    json.reserve(10240);
    json ="{";
    JSON_ADD_STR(cid, mCallId);
    JSON_ADD_INT(isCaller, mIsCaller);
    JSON_ADD_INT(ts, mStartTs);
    JSON_ADD_INT(sper, mSper);
    JSON_ADD_INT(dur, round((float)mDur/1000));
    JSON_ADD_STR(termRsn, mTermRsn);
    JSON_ADD_STR(bws, "n"); //TODO: Add platform info

    int isRelay = !mConnInfo.mRlySvr.empty();
    JSON_ADD_INT(rly, isRelay);
    if (isRelay)
        JSON_ADD_STR(rlySvr, mConnInfo.mRlySvr);
    JSON_ADD_STR(ctype, mConnInfo.mCtype);
    JSON_ADD_STR(proto, mConnInfo.mProto);
    JSON_ADD_STR(vcodec, mConnInfo.mVcodec);
    JSON_SUBOBJ("samples");
        JSON_ADD_SAMPLES(, ts);
        JSON_SUBOBJ("c");
            JSON_ADD_SAMPLES(cstats., rtt);
                JSON_SUBOBJ("s");
                    JSON_ADD_BWINFO(cstats.s);
                JSON_END_SUBOBJ();
                JSON_SUBOBJ("r");
                    JSON_ADD_BWINFO(cstats.r);
                JSON_END_SUBOBJ();
        JSON_END_SUBOBJ();
        JSON_SUBOBJ("v");
            JSON_SUBOBJ("s");
                JSON_ADD_BWINFO(vstats.s);
                JSON_ADD_SAMPLES(vstats.s., gbps);
                JSON_ADD_SAMPLES(vstats.s., gabps);
                JSON_ADD_SAMPLES(vstats.s., rtt);
                JSON_ADD_SAMPLES(vstats.s., fps);
                JSON_ADD_SAMPLES(vstats.s., cfps);
                JSON_ADD_SAMPLES(vstats.s., cjtr);
                JSON_ADD_SAMPLES(vstats.s., width);
                JSON_ADD_SAMPLES(vstats.s., height);
                JSON_ADD_DEC_SAMPLES(vstats.s., el);
                JSON_ADD_SAMPLES(vstats.s., lcpu);
                JSON_ADD_SAMPLES(vstats.s., lbw);
                JSON_ADD_SAMPLES(vstats.s., bwav);
            JSON_END_SUBOBJ();
            JSON_SUBOBJ("r");
                JSON_ADD_BWINFO(vstats.r);
                JSON_ADD_SAMPLES(vstats.r., fps);
                JSON_ADD_SAMPLES(vstats.r., jtr);
                JSON_ADD_SAMPLES(vstats.r., dly);
                JSON_ADD_SAMPLES(vstats.r., pl);
                JSON_ADD_SAMPLES(vstats.r., width);
                JSON_ADD_SAMPLES(vstats.r., height);
            JSON_END_SUBOBJ(); //r
        JSON_END_SUBOBJ(); //v
        JSON_SUBOBJ("a");
            JSON_ADD_SAMPLES(astats., rtt);
            JSON_ADD_SAMPLES(astats., jtr);
            JSON_ADD_SAMPLES(astats., pl);
            JSON_SUBOBJ("s");
                JSON_ADD_BWINFO(astats.s);
            JSON_END_SUBOBJ();
            JSON_SUBOBJ("r");
                JSON_ADD_BWINFO(astats.r);
            JSON_END_SUBOBJ();
        JSON_END_SUBOBJ(); //a
    JSON_END_SUBOBJ(); //samples
    json[json.size()-1]='}'; //all
}

void BasicStats::toJson(std::string& json) const
{
    json.reserve(512);
    json ="{";
    JSON_ADD_STR(cid, mCallId);
    JSON_ADD_INT(isCaller, mIsCaller);
    JSON_ADD_STR(termRsn, mTermRsn);
    JSON_ADD_STR(bws, "n"); //TODO: Add platform info
    json[json.size()-1] = '}';
}
}
}
