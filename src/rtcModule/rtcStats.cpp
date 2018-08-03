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

StatSessInfo::StatSessInfo(karere::Id aSid, uint8_t termCode, const std::string& aErrInfo, const std::string& aDeviceInfo)
:sid(aSid), errInfo(aErrInfo), deviceInfo(aDeviceInfo)
{
    if (termCode & TermCode::kPeer)
        mTermReason = std::string("peer-")+termCodeToStr(static_cast<TermCode>(termCode & ~TermCode::kPeer));
    else
        mTermReason = termCodeToStr(static_cast<TermCode>(termCode));
}

Recorder::Recorder(Session& sess, int scanPeriod, int maxSamplePeriod)
    :mScanPeriod(scanPeriod * 1000), mMaxSamplePeriod(maxSamplePeriod * 1000),
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

int64_t Recorder::getLongValue(webrtc::StatsReport::StatsValueName name, const webrtc::StatsReport *item)
{
    int64_t numericalValue = 0;
    const webrtc::StatsReport::Value *value = item->FindValue(name);
    if (value)
    {
        if (value->type() == webrtc::StatsReport::Value::kInt)
        {
            numericalValue = value->int_val();
        }
        else if (value->type() == webrtc::StatsReport::Value::kInt64)
        {
            numericalValue = value->int64_val();
        }
        else
        {
            KR_LOG_DEBUG("Incorrect type: Value with id %s is not an int, but has type %d", value->ToString().c_str(), value->type());
            assert(false);
        }
    }

    return numericalValue;
}

std::string Recorder::getStringValue(webrtc::StatsReport::StatsValueName name, const webrtc::StatsReport *item)
{
    std::string stringValue;
    const webrtc::StatsReport::Value *value = item->FindValue(name);
    if (value)
    {
        stringValue = value->ToString();
    }

    return stringValue;
}

void Recorder::BwCalculator::calculate(uint64_t periodMs, uint64_t newTotalBytes)
{
    uint64_t deltaBytes = newTotalBytes - mTotalBytes;
    auto bps = mBwInfo->bps = ((float)(deltaBytes)/128.0) / (periodMs / 1000.0); //from bytes/s to kbits/s
    mTotalBytes = newTotalBytes;
    mBwInfo->bs += deltaBytes;
    mBwInfo->abps = (mBwInfo->abps * 4+bps) / 5;
}

void Recorder::OnComplete(const webrtc::StatsReports& data)
{
    onStats(data);
}

#define AVG(name, var) var = round((float)var + getLongValue(VALNAME(name), item)) / 2

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
                width = getLongValue(VALNAME(FrameWidthReceived), item);
                auto& sample = mCurrSample->vstats.r;
                mVideoRxBwCalc.calculate(period, getLongValue(VALNAME(BytesReceived), item));
                AVG(FrameRateReceived, sample.fps);
                AVG(CurrentDelayMs, sample.dly);
                AVG(JitterBufferMs, sample.jtr);
                sample.pl = getLongValue(VALNAME(PacketsLost), item);
//              vstat.fpsSent = res.stat('googFrameRateOutput'); -- this should be for screen output
                sample.width = width;
                sample.height = getLongValue(VALNAME(FrameHeightReceived), item);
            }
            else if (item->FindValue(VALNAME(FrameWidthSent))) //video tx
            {
                width = getLongValue(VALNAME(FrameWidthSent), item);
                auto& sample = mCurrSample->vstats;
                AVG(Rtt, sample.rtt);
                AVG(FrameRateSent, sample.s.fps);
                AVG(FrameRateInput, sample.s.cfps);
                sample.s.width = width;
                sample.s.height = getLongValue(VALNAME(FrameHeightSent), item);
                if (mStats->mConnInfo.mVcodec.empty())
                {
                    mStats->mConnInfo.mVcodec = getStringValue(VALNAME(CodecName), item);
                }
//              s.et = stat('googAvgEncodeMs');
                AVG(EncodeUsagePercent, sample.s.el); //(s.et*s.fps)/10; // (encTime*fps/1000ms)*100%
                sample.s.lcpu = getStringValue(VALNAME(CpuLimitedResolution), item) == "true";
                sample.s.lbw = getStringValue(VALNAME(BandwidthLimitedResolution), item) == "true";
                mVideoTxBwCalc.calculate(period, getLongValue(VALNAME(BytesSent), item));
            }
            else if (item->FindValue(VALNAME(AudioInputLevel))) //audio rx
            {
                mAudioRxBwCalc.calculate(period, getLongValue(VALNAME(BytesSent), item));
                if (item->FindValue(VALNAME(Rtt)))
                {
                    AVG(Rtt, mCurrSample->astats.rtt);
                }
            }
            else if (item->FindValue(VALNAME(AudioOutputLevel))) //audio tx
            {
                mAudioTxBwCalc.calculate(period, getLongValue(VALNAME(BytesReceived), item));
                AVG(JitterReceived, mCurrSample->astats.r.jtr);
                mCurrSample->astats.r.pl = getLongValue(VALNAME(PacketsLost), item);
            }
        }
        else if ((item->id()->type() == RPTYPE(CandidatePair)) && getStringValue(VALNAME(ActiveConnection), item) == "true")
        {
            if (!mHasConnInfo) //happens if peer is Firefox
            {
                mHasConnInfo = true;
                bool isRelay = getStringValue(VALNAME(LocalCandidateType), item) == "relay";
                if (isRelay)
                {
                    auto& rlySvr = mStats->mConnInfo.mRlySvr;
                    rlySvr = getStringValue(VALNAME(LocalAddress), item);
                    if (rlySvr.empty())
                        rlySvr = "<error getting relay server>";
                }
                mStats->mConnInfo.mCtype = getStringValue(VALNAME(RemoteCandidateType), item);
                mStats->mConnInfo.mProto = getStringValue(VALNAME(TransportType), item);
            }
            auto& cstat = mCurrSample->cstats;
            AVG(Rtt, cstat.rtt);
            mConnRxBwCalc.calculate(period, getLongValue(VALNAME(BytesReceived), item));
            mConnTxBwCalc.calculate(period, getLongValue(VALNAME(BytesSent), item));

        }
        else if (item->id()->type() == RPTYPE(Bwe))
        {
            mCurrSample->vstats.r.bwav = round((float)getLongValue(VALNAME(AvailableReceiveBandwidth), item)/1024);
            auto& sample = mCurrSample->vstats.s;
            sample.bwav = round((float)getLongValue(VALNAME(AvailableSendBandwidth), item)/1024);
            sample.gbps = round((float)getLongValue(VALNAME(TransmitBitrate), item)/1024); //chrome returns it in bits/s, should be near our calculated bps
            sample.targetEncBitrate = round((float)getLongValue(VALNAME(TargetEncBitrate), item)/1024);
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
        long d_dly = 0;
        if (last.vstats.r.dly)
        {
            d_dly = (mCurrSample->vstats.r.dly - last.vstats.r.dly);
            if (d_dly < 0)
                d_dly = -d_dly;
        }
        long d_vrtt = 0;
        if (last.vstats.rtt)
        {
            d_vrtt = (mCurrSample->vstats.rtt - last.vstats.rtt);
            if (d_vrtt < 0)
                d_vrtt = -d_vrtt;
        }
        long d_auRtt = 0;
        if (last.astats.rtt)
        {
            d_auRtt = mCurrSample->astats.rtt - last.astats.rtt;
            if (d_auRtt < 0)
                d_auRtt = -d_auRtt;
        }
        long d_auJtr = 0;
        if (last.astats.r.jtr)
        {
            d_auJtr = mCurrSample->astats.r.jtr - last.astats.r.jtr;
                    if (d_auJtr < 0)
                        d_auJtr = -d_auJtr;
        }

        auto d_ts = mCurrSample->ts - last.ts;
        auto d_apl = mCurrSample->astats.r.pl - last.astats.r.pl;
        mCurrSample->astats.plDifference = d_apl;
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
    mStats->mSessionId = mSession.sessionId();
    mStats->mOwnAnonId = mSession.call().manager().ownAnonId();
    mStats->mPeerAnonId = mSession.peerAnonId();
    mStats->mSper = mScanPeriod;
    mStats->mStartTs = karere::timestampMs();
    mTimer = setInterval([this]()
    {
        mSession.rtcConn()->GetStats(static_cast<webrtc::StatsObserver*>(this), nullptr, mStatsLevel);
        if (!mStats->mSamples.empty())
        {
            mSession.manageNetworkQuality(mStats->mSamples.back());
        }
    }, mScanPeriod, mSession.mManager.mClient.appCtx);
}

std::string Recorder::terminate(const StatSessInfo& info)
{
    cancelInterval(mTimer, mSession.mManager.mClient.appCtx);
    mTimer = 0;
    mStats->mDur = karere::timestampMs() - mStats->mStartTs;
    mStats->mTermRsn = info.mTermReason;
    mStats->mDeviceInfo = info.deviceInfo;
    std::string json;
    mStats->toJson(json);
    return json;
}

Recorder::~Recorder()
{
    if (mTimer)
        cancelInterval(mTimer, mSession.mManager.mClient.appCtx);
}

RtcStats::~RtcStats()
{
    for (unsigned int i = 0; i < mSamples.size(); i++)
    {
        delete mSamples[i];
    }

    mSamples.clear();
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
    JSON_ADD_STR(sid, mSessionId.toString());
    JSON_ADD_STR(caid, mIsCaller?mOwnAnonId.toString():mPeerAnonId.toString());
    JSON_ADD_STR(aaid, mIsCaller?mPeerAnonId.toString():mOwnAnonId.toString());
    JSON_ADD_INT(isCaller, mIsCaller);
    JSON_ADD_INT(ts, mStartTs);
    JSON_ADD_INT(sper, mSper);
    JSON_ADD_INT(dur, round((float)mDur/1000));
    JSON_ADD_STR(termRsn, mTermRsn);
    JSON_ADD_STR(bws, mDeviceInfo);

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
            JSON_ADD_SAMPLES(vstats., rtt);
            JSON_SUBOBJ("s");
                JSON_ADD_BWINFO(vstats.s);
                JSON_ADD_SAMPLES(vstats.s., gbps);
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
            JSON_SUBOBJ("s");
                JSON_ADD_BWINFO(astats.s);
            JSON_END_SUBOBJ();
            JSON_SUBOBJ("r");
                JSON_ADD_BWINFO(astats.r);
                JSON_ADD_SAMPLES(astats.r., jtr);
                JSON_ADD_SAMPLES(astats.r., pl);
            JSON_END_SUBOBJ();
        JSON_END_SUBOBJ(); //a
    JSON_END_SUBOBJ(); //samples
    json[json.size()-1]='}'; //all
}
}
}
