#include "rtcStats.h"
#include "webrtcPrivate.h"
#include <timers.hpp>
#include <string.h> //for memset
#include <karereCommon.h> //for timestampMs()
#include <chatClient.h>
#define RPTYPE(name) webrtc::StatsReport::kStatsReportType##name
#define VALNAME(name) webrtc::StatsReport::kStatsValueName##name

namespace rtcModule
{
using namespace artc;
using namespace std::placeholders;
using namespace karere;

namespace stats
{

StatSessInfo::StatSessInfo(karere::Id aSid, uint8_t termCode, const std::string& aErrInfo)
:sid(aSid), errInfo(aErrInfo)
{
    if (termCode & TermCode::kPeer)
        mTermReason = std::string("peer-")+termCodeToStr(static_cast<TermCode>(termCode & ~TermCode::kPeer));
    else
        mTermReason = termCodeToStr(static_cast<TermCode>(termCode));
}

EmptyStats::EmptyStats(const Session& sess, const std::string& aTermRsn)
:mIsCaller(sess.isCaller()), mTermRsn(aTermRsn), mCallId(sess.call().id()){}

Recorder::Recorder(Session& sess, int scanPeriod, int maxSamplePeriod)
    :mScanPeriod(scanPeriod), mMaxSamplePeriod(maxSamplePeriod),
    mCurrSample(new Sample), mSession(sess), mStats(new RtcStats)
{
    memset(mCurrSample.get(), 0, sizeof(Sample));
    AddRef();
    if (mScanPeriod < 0)
        mScanPeriod = 1000;
    if (mMaxSamplePeriod < 0)
        mMaxSamplePeriod = 5000;
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

void Recorder::OnComplete(const webrtc::StatsReports& data)
{
    onStats(data);
}

#define AVG(name, var) var = round((float)var + item->FindValue(VALNAME(name))->int64_val()) / 2

void Recorder::onStats(const webrtc::StatsReports &data)
{
    long ts = karere::timestampMs() - mStats->mStartTs;
    long period = ts - mCurrSample->ts;
    mCurrSample->ts = ts;
    for (const webrtc::StatsReport* item: data)
    {
        if (item->id()->type() == RPTYPE(Ssrc))
        {
            long width;
            if (item->FindValue(VALNAME(FrameWidthReceived))) //video rx
            {
                width = item->FindValue(VALNAME(FrameWidthReceived))->int64_val();
                auto& sample = mCurrSample->vstats.r;
                mVideoRxBwCalc.calculate(period, item->FindValue(VALNAME(BytesReceived))->int64_val());
                AVG(FrameRateReceived, sample.fps);
                AVG(CurrentDelayMs, sample.dly);
                AVG(JitterBufferMs, sample.jtr);
                sample.pl = item->FindValue(VALNAME(PacketsLost))->int64_val();
//              vstat.fpsSent = res.stat('googFrameRateOutput'); -- this should be for screen output
                sample.width = width;
                sample.height = item->FindValue(VALNAME(FrameHeightReceived))->int64_val();
            }
            else if (item->FindValue(VALNAME(FrameWidthSent))) //video tx
            {
                width = item->FindValue(VALNAME(FrameWidthSent))->int64_val();
                auto& sample = mCurrSample->vstats.s;
                AVG(Rtt, sample.rtt);
                AVG(FrameRateSent, sample.fps);
                AVG(FrameRateInput, sample.cfps);
//              AVG(CaptureJitterMs, sample.cjtr); //no capture jitter stats anymore?
                sample.width = width;
                sample.height = item->FindValue(VALNAME(FrameHeightSent))->int64_val();
                if (mStats->mConnInfo.mVcodec.empty())
                    mStats->mConnInfo.mVcodec = item->FindValue(VALNAME(CodecName))->string_val();
//              s.et = stat('googAvgEncodeMs');
                AVG(EncodeUsagePercent, sample.el); //(s.et*s.fps)/10; // (encTime*fps/1000ms)*100%
                sample.lcpu = (*item->FindValue(VALNAME(CpuLimitedResolution)) == "true");
                sample.lbw = (*item->FindValue(VALNAME(BandwidthLimitedResolution)) == "true");
                mVideoTxBwCalc.calculate(period, item->FindValue(VALNAME(BytesSent))->int64_val());
            }
            else if (item->FindValue(VALNAME(AudioInputLevel))) //audio rx
            {
                mAudioRxBwCalc.calculate(period, item->FindValue(VALNAME(BytesSent))->int64_val());
                if (item->FindValue(VALNAME(Rtt)))
                {
                    AVG(Rtt, mCurrSample->astats.rtt);
                }
            }
            else if (item->FindValue(VALNAME(AudioOutputLevel))) //audio tx
            {
                mAudioTxBwCalc.calculate(period, item->FindValue(VALNAME(BytesReceived))->int64_val());
                AVG(JitterReceived, mCurrSample->astats.jtr);
                mCurrSample->astats.pl = item->FindValue(VALNAME(PacketsLost))->int64_val();
            }
        }
        else if ((item->id()->type() == RPTYPE(CandidatePair)) && *item->FindValue(VALNAME(ActiveConnection)) == std::string("true"))
        {
            if (!mHasConnInfo) //happens if peer is Firefox
            {
                mHasConnInfo = true;
                bool isRelay = (item->FindValue(VALNAME(LocalCandidateType))->string_val() == "relay");
                if (isRelay)
                {
                    auto& rlySvr = mStats->mConnInfo.mRlySvr;
                    rlySvr = item->FindValue(VALNAME(LocalAddress))->string_val();
                    if (rlySvr.empty())
                        rlySvr = "<error getting relay server>";
                }
                mStats->mConnInfo.mCtype = item->FindValue(VALNAME(RemoteCandidateType))->string_val();
                mStats->mConnInfo.mProto = item->FindValue(VALNAME(TransportType))->string_val();
            }
            auto& cstat = mCurrSample->cstats;
            AVG(Rtt, cstat.rtt);
            mConnRxBwCalc.calculate(period, item->FindValue(VALNAME(BytesReceived))->int64_val());
            mConnTxBwCalc.calculate(period, item->FindValue(VALNAME(BytesSent))->int64_val());

        }
        else if (item->id()->type() == RPTYPE(Bwe))
        {
            mCurrSample->vstats.r.bwav = round((float)item->FindValue(VALNAME(AvailableReceiveBandwidth))->int64_val()/1024);
            auto& sample = mCurrSample->vstats.s;
            sample.bwav = round((float)item->FindValue(VALNAME(AvailableSendBandwidth))->int64_val()/1024);
            sample.gbps = round((float)item->FindValue(VALNAME(TransmitBitrate))->int64_val()/1024); //chrome returns it in bits/s, should be near our calculated bps
            sample.gabps = (sample.gabps*4+sample.gbps)/5;
            sample.targetEncBitrate = round((float)item->FindValue(VALNAME(TargetEncBitrate))->int64_val()/1024);
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
         || ((d_ts >= mMaxSamplePeriod)
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
    }
    if (onSample)
    {
        if ((mStats->mSamples.size() == 1) && shouldAddSample) //first sample that we just added
            onSample(&(mStats->mConnInfo), 0);
        onSample(mCurrSample.get(), 1);
    }
}

void Recorder::start()
{
    assert(mSession.mRtcConn);
    mStats->mIsCaller = mSession.isCaller();
    mStats->mCallId = mSession.call().id();
    mStats->mOwnAnonId = mSession.call().manager().ownAnonId();
    mStats->mPeerAnonId = mSession.peerAnonId();
    mStats->mSper = mScanPeriod;
    mStats->mStartTs = karere::timestampMs();
    mTimer = setInterval([this]()
    {
        mSession.rtcConn()->GetStats(static_cast<webrtc::StatsObserver*>(this), nullptr, mStatsLevel);
    }, mScanPeriod * 1000, mSession.mManager.mClient.appCtx);
}

std::string Recorder::terminate(const StatSessInfo& info)
{
    cancelInterval(mTimer, mSession.mManager.mClient.appCtx);
    mTimer = 0;
    mStats->mDur = karere::timestampMs() - mStats->mStartTs;
    mStats->mTermRsn = info.mTermReason;
    std::string json;
    mStats->toJson(json);
    return json;
}

Recorder::~Recorder()
{
    if (mTimer)
        cancelInterval(mTimer, mSession.mManager.mClient.appCtx);
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
    }\
    json+=',';

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
    JSON_ADD_STR(cid, mCallId.toString());
    JSON_ADD_STR(caid, mIsCaller?mOwnAnonId.toString():mPeerAnonId.toString());
    JSON_ADD_STR(aaid, mIsCaller?mPeerAnonId.toString():mOwnAnonId.toString());
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

void EmptyStats::toJson(std::string& json) const
{
    json.reserve(512);
    json ="{";
    JSON_ADD_STR(cid, mCallId.toString());
    JSON_ADD_INT(isCaller, mIsCaller);
    JSON_ADD_STR(termRsn, mTermRsn);
    JSON_ADD_STR(bws, "n"); //TODO: Add platform info
    json[json.size()-1] = '}';
}
}
}
