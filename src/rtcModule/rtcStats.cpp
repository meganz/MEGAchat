#include "rtcStats.h"
#include "webrtcPrivate.h"
#include <timers.hpp>
#include <string.h> //for memset
#include <karereCommon.h> //for timestampMs()
#include <chatClient.h>
#include <mega/utils.h>
#define RPTYPE(name) webrtc::StatsReport::kStatsReportType##name
#define VALNAME(name) webrtc::StatsReport::kStatsValueName##name

namespace rtcModule
{
using namespace artc;
using namespace std::placeholders;
using namespace karere;
#if WIN32
using ::mega::mega_snprintf;   // enables the calls to snprintf below which are #defined
#endif

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
    static bool failTypeLog = true;
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
        else if (failTypeLog)
        {
            //This fail is almost always produced due to incompatibilities between webRtc
            // in release mode and karere in debug mode. We only report once to avoid unnecessary log output
            KR_LOG_DEBUG("Incorrect type: Value with id %s is not an int, but has type %d", value->ToString().c_str(), value->type());
            failTypeLog = false;
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

bool Recorder::checkShouldAddSample()
{
    if (mStats->mSamples.empty())
    {
        return true;
    }

    Sample *last = mStats->mSamples.back();

    mCurrSample->astats.plDifference = mCurrSample->astats.r.pl - last->astats.r.pl;
    if (mCurrSample->astats.plDifference)
    {
        return true;
    }

    if (mCurrSample->f != last->f)
    {
        return true;
    }

    if ((mCurrSample->ts - last->ts) >= mMaxSamplePeriod)
    {
        return true;
    }

    if (mCurrSample->vstats.r.width != last->vstats.r.width)
    {
        return true;
    }

    if (mCurrSample->vstats.s.width != last->vstats.s.width)
    {
        return true;
    }

    if (abs(mCurrSample->vstats.r.dly - last->vstats.r.dly) >= 100)
    {
        return true;
    }

    if (abs(mCurrSample->vstats.rtt - last->vstats.rtt) >= 50)
    {
        return true;
    }

    if (abs(mCurrSample->astats.rtt - last->astats.rtt) >= 50)
    {
        return true;
    }

    if (abs(mCurrSample->astats.r.jtr - last->astats.r.jtr) >= 40)
    {
        return true;
    }

    return false;
}

void Recorder::BwCalculator::calculate(uint64_t periodMs, uint64_t newTotalBytes)
{
    uint64_t deltaBytes = newTotalBytes - mBwInfo->bt;
    mBwInfo->bps = (!periodMs) ? 0 : ((float)(deltaBytes) / 128.0) / (periodMs / 1000.0); //from bytes/s to kbits/s
    mBwInfo->bt = newTotalBytes;
    mBwInfo->abps = (mBwInfo->abps * 4 + mBwInfo->bps) / 5;
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
    mCurrSample->f = mSession.call().sentAv().value();
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
                sample.nacktx = getLongValue(VALNAME(NacksSent), item);
                sample.plitx = getLongValue(VALNAME(PlisSent), item);
                sample.firtx = getLongValue(VALNAME(FirsSent), item);
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
                if (getStringValue(VALNAME(CpuLimitedResolution), item) == "true")
                {
                    mCurrSample->f |= STATFLAG_SEND_CPU_LIMITED_RESOLUTION;
                }
                if (getStringValue(VALNAME(BandwidthLimitedResolution), item) == "true")
                {
                    mCurrSample->f |= STATFLAG_SEND_BANDWIDTH_LIMITED_RESOLUTION;
                }

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
                AVG(CurrentDelayMs, mCurrSample->astats.r.dly);
                mCurrSample->astats.r.al = ((((float)getLongValue(VALNAME(AudioOutputLevel), item))/327.67) >= 10) ? 1 : 0;
            }
        }
        else if ((item->id()->type() == RPTYPE(CandidatePair)) && (getStringValue(VALNAME(ActiveConnection), item) == "true"))
        {
            mStats->mConnInfo.mRly = (getStringValue(VALNAME(LocalCandidateType), item) == "relay");
            if (mStats->mConnInfo.mRly)
            {
                mStats->mConnInfo.mRlySvr = getStringValue(VALNAME(LocalAddress), item);
            }

            mStats->mConnInfo.mRRly = (getStringValue(VALNAME(RemoteCandidateType), item) == "relay");
            if (mStats->mConnInfo.mRRly)
            {
                mStats->mConnInfo.mRRlySvr = getStringValue(VALNAME(RemoteAddress), item);
            }

            mStats->mConnInfo.mCtype = getStringValue(VALNAME(RemoteCandidateType), item);
            mStats->mConnInfo.mProto = getStringValue(VALNAME(TransportType), item);

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


    mCurrSample->lq = mSession.calculateNetworkQuality(mCurrSample.get());

    bool shouldAddSample = checkShouldAddSample();
    if (shouldAddSample)
    {
        addSample();
    }

    if (onSample)
    {
        if ((mStats->mSamples.size() == 1) && shouldAddSample) //first sample that we just added
            onSample(&(mStats->mConnInfo), 0);
        onSample(mCurrSample.get(), 1);
    }
}

webrtc::PeerConnectionInterface::StatsOutputLevel Recorder::getStatsLevel() const
{
    return mStatsLevel;
}

void Recorder::start()
{
    assert(mSession.mRtcConn);
    mStats->mIsJoiner = !mSession.isCaller();
    mStats->mCallId = mSession.call().id();
    mStats->mSessionId = mSession.sessionId();
    mStats->mOwnAnonId = mSession.call().manager().ownAnonId();
    mStats->mPeerAnonId = mSession.peerAnonId();
    mStats->mSper = mScanPeriod;
    mStats->mStartTs = karere::timestampMs();
}

std::string Recorder::terminate(const StatSessInfo& info)
{
    mStats->mDur = karere::timestampMs() - mStats->mStartTs;
    mStats->mTermRsn = info.mTermReason;
    mStats->mDeviceInfo = info.deviceInfo;
    std::string json;
    mStats->toJson(json);
    return json;
}

Recorder::~Recorder()
{
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
    JSON_ADD_SAMPLES(path., bt);        \
    JSON_ADD_SAMPLES(path., bps);       \
    JSON_ADD_SAMPLES(path., abps)

void RtcStats::toJson(std::string& json) const
{
    json.reserve(10240);
    json ="{";
    JSON_ADD_STR(cid, mCallId.toString());
    JSON_ADD_STR(sid, mSessionId.toString());
    JSON_ADD_INT(ts, round((float)mStartTs/1000));
    JSON_ADD_INT(dur, round((float)mDur/1000));
    JSON_SUBOBJ("samples");
        JSON_ADD_SAMPLES(, ts);
        JSON_ADD_SAMPLES(, lq);
        JSON_ADD_SAMPLES(, f);
        JSON_SUBOBJ("v");
            JSON_ADD_SAMPLES(vstats., rtt);
            JSON_SUBOBJ("s");
                JSON_ADD_BWINFO(vstats.s);
                JSON_ADD_SAMPLES(vstats.s., fps);
                JSON_ADD_SAMPLES(vstats.s., cfps);
                JSON_ADD_SAMPLES(vstats.s., width);
                JSON_ADD_SAMPLES(vstats.s., height);
                JSON_ADD_DEC_SAMPLES(vstats.s., el);
                JSON_ADD_SAMPLES(vstats.s., bwav);
                JSON_ADD_SAMPLES(vstats.s., gbps);
            JSON_END_SUBOBJ();
            JSON_SUBOBJ("r");
                JSON_ADD_BWINFO(vstats.r);
                JSON_ADD_SAMPLES(vstats.r., pl);
                JSON_ADD_SAMPLES(vstats.r., jtr);
                JSON_ADD_SAMPLES(vstats.r., fps);
                JSON_ADD_SAMPLES(vstats.r., dly);
                JSON_ADD_SAMPLES(vstats.r., width);
                JSON_ADD_SAMPLES(vstats.r., height);
                JSON_ADD_SAMPLES(vstats.r., firtx);
                JSON_ADD_SAMPLES(vstats.r., plitx);
                JSON_ADD_SAMPLES(vstats.r., nacktx);
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
                JSON_ADD_SAMPLES(astats.r., dly);
                JSON_ADD_SAMPLES(astats.r., al);
            JSON_END_SUBOBJ();
        JSON_END_SUBOBJ(); //a
    JSON_END_SUBOBJ(); //samples
    JSON_ADD_STR(bws, mDeviceInfo);
    JSON_ADD_INT(rly, mConnInfo.mRly);
    JSON_ADD_INT(rrly, mConnInfo.mRRly);
    JSON_ADD_STR(proto, mConnInfo.mProto);
    JSON_ADD_INT(isJoiner, mIsJoiner);
    JSON_ADD_STR(caid, mIsJoiner ? mOwnAnonId.toString() : mPeerAnonId.toString());
    JSON_ADD_STR(aaid, mIsJoiner ? mPeerAnonId.toString() : mOwnAnonId.toString());
    JSON_ADD_STR(termRsn, mTermRsn);
    json[json.size()-1]='}'; //all
}
}
}
