#ifndef RTCSTATS_H
#define RTCSTATS_H
#include "webrtcAdapter.h"
#include "IRtcStats.h"
#include "ITypesImpl.h"
#include <timers.hpp>
#include <karereId.h>

namespace rtcModule
{
class Session;
class Call;

namespace stats
{
struct StatSessInfo
{
    karere::Id sid;
    std::string mTermReason;
    std::string errInfo;
    karere::Id caid;
    karere::Id aaid;
    bool isCaller;
    std::string deviceInfo;
    StatSessInfo(karere::Id aSid, uint8_t code, const std::string& aErrInfo, const std::string &aDeviceInfo);
};

class ConnInfo: public IConnInfo
{
public:
    std::string mCtype;
    std::string mProto;
    std::string mRlySvr;
    std::string mRRlySvr;
    std::string mVcodec;
    bool mRly = false;
    bool mRRly = false;

    virtual const std::string& ctype() const { return mCtype; }
    virtual const std::string& proto() const { return mProto; }
    virtual const std::string& rlySvr() const { return mRlySvr; }
    virtual const std::string& rRlySvr() const { return mRRlySvr; }
    virtual const std::string& vcodec() const { return mVcodec; }
};

class RtcStats: public IRefCountedMixin<IRtcStats>
{
public:
    std::string mTermRsn;
    bool mIsJoiner;
    int mSper; //sample period
    int64_t mStartTs;
    int64_t mDur;
    karere::Id mCallId;
    karere::Id mSessionId;
    karere::Id mOwnAnonId;
    karere::Id mPeerAnonId;
    std::string mDeviceInfo;
    std::vector<Sample*> mSamples;
    ConnInfo mConnInfo;
    ~RtcStats();
    //IRtcStats implementation
    virtual const std::string& termRsn() const { return mTermRsn; }
    virtual bool isCaller() const { return !mIsJoiner; }
    virtual karere::Id callId() const { return mCallId; }
    virtual size_t sampleCnt() const { return mSamples.size(); }
    virtual const std::vector<Sample*>* samples() const { return &mSamples; }
    virtual const IConnInfo* connInfo() const { return &mConnInfo; }
    virtual void toJson(std::string& out) const;
};

class Recorder: public rtc::RefCountedObject<webrtc::StatsObserver>
{
protected:
    struct BwCalculator
    {
        BwInfo* mBwInfo = nullptr;
        void reset(BwInfo* aBwInfo)
        {
            assert(aBwInfo);
            mBwInfo = aBwInfo;
            mBwInfo->bt = 0;
        }
        void calculate(uint64_t periodMs, uint64_t newTotalBytes);
    };

    int mScanPeriod;
    int mMaxSamplePeriod;
    webrtc::PeerConnectionInterface::StatsOutputLevel mStatsLevel =
            webrtc::PeerConnectionInterface::kStatsOutputLevelStandard;
    static const int STATFLAG_SEND_CPU_LIMITED_RESOLUTION = 4;
    static const int STATFLAG_SEND_BANDWIDTH_LIMITED_RESOLUTION = 8;
    std::unique_ptr<Sample> mCurrSample;
    BwCalculator mVideoRxBwCalc;
    BwCalculator mVideoTxBwCalc;
    BwCalculator mAudioRxBwCalc;
    BwCalculator mAudioTxBwCalc;
    BwCalculator mConnRxBwCalc;
    BwCalculator mConnTxBwCalc;
    void addSample();
    void resetBwCalculators();
    int64_t getLongValue(webrtc::StatsReport::StatsValueName name, const webrtc::StatsReport* item);
    std::string getStringValue(webrtc::StatsReport::StatsValueName name, const webrtc::StatsReport* item);
    bool checkShouldAddSample();
public:
    Session& mSession;
    std::unique_ptr<RtcStats> mStats;
    Recorder(Session& sess, int scanPeriod, int maxSamplePeriod);
    ~Recorder();
    void start();
    std::string terminate(const StatSessInfo &info);
    virtual void OnComplete(const webrtc::StatsReports& data);
    void onStats(const webrtc::StatsReports &data);
    webrtc::PeerConnectionInterface::StatsOutputLevel getStatsLevel() const;
    std::function<void(void*, int)> onSample;
};
}
}
#endif
