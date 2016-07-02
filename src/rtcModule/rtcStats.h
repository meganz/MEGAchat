#ifndef RTCSTATS_H
#define RTCSTATS_H
#include "webrtcAdapter.h"
#include "IRtcStats.h"
#include "ITypesImpl.h"
#include <timers.hpp>
#include <strophe.jingle.session.h>

namespace rtcModule
{
class JingleSession;
namespace stats
{
class ConnInfo: public IConnInfo
{
public:
    std::string mCtype;
    std::string mProto;
    std::string mRlySvr;
    std::string mVcodec;
    bool isRelay() const { return !mRlySvr.empty(); }

    virtual const std::string& ctype() const { return mCtype; }
    virtual const std::string& proto() const { return mProto; }
    virtual const std::string& rlySvr() const { return mRlySvr; }
    virtual const std::string& vcodec() const { return mVcodec; }
};

class RtcStats: public IRefCountedMixin<IRtcStats>
{
public:
    std::string mTermRsn;
    bool mIsCaller;
    int mSper; //sample period
    int64_t mStartTs;
    int64_t mDur;
    std::string mCallId;
    std::string mOwnAnonId;
    std::string mPeerAnonId;
    std::vector<Sample*> mSamples;
    ConnInfo mConnInfo;
    //IRtcStats implementation
    virtual const std::string& termRsn() const { return mTermRsn; }
    virtual bool isCaller() const { return mIsCaller; }
    virtual const std::string& callId() const { return mCallId; }
    virtual size_t sampleCnt() const { return mSamples.size(); }
    virtual const std::vector<Sample*>* samples() const { return &mSamples; }
    virtual const IConnInfo* connInfo() const { return &mConnInfo; }
    virtual void toJson(std::string& out) const;
};

class BasicStats: public IRtcStats
{
public:
    bool mIsCaller;
    std::string mTermRsn;
    std::string mCallId;
    std::string mEmpty;
    BasicStats(const Call& call, const std::string& aTermRsn);
    virtual const std::string& ctype() const { return mEmpty; }
    virtual const std::string& proto() const { return mEmpty; }
    virtual const std::string& rlySvr() const { return mEmpty; }
    virtual const std::string& termRsn() const { return mTermRsn; }
    virtual const std::string& vcodec() const { return mEmpty; }
    virtual bool isCaller() const { return mIsCaller; }
    virtual const std::string& callId() const { return mCallId; }
    virtual size_t sampleCnt() const { return 0; }
    virtual const std::vector<Sample*>* samples() const { return nullptr; }
    virtual const IConnInfo* connInfo() const { return nullptr; }
    virtual void toJson(std::string&) const;
};

class Recorder: public rtc::RefCountedObject<webrtc::StatsObserver>
{
protected:
    struct BwCalculator
    {
        BwInfo* mBwInfo = nullptr;
        int64_t mTotalBytes = 0;
        void reset(BwInfo* aBwInfo)
        {
            assert(aBwInfo);
            mBwInfo = aBwInfo;
            mBwInfo->bs = 0;
        }
        void calculate(long periodMs, long newTotalBytes);
    };

    JingleSession& mSession;
    Options mOptions;
    webrtc::PeerConnectionInterface::StatsOutputLevel mStatsLevel =
            webrtc::PeerConnectionInterface::kStatsOutputLevelStandard;
    std::unique_ptr<Sample> mCurrSample;
    bool mHasConnInfo = false;
    megaHandle mTimer = 0;
    BwCalculator mVideoRxBwCalc;
    BwCalculator mVideoTxBwCalc;
    BwCalculator mAudioRxBwCalc;
    BwCalculator mAudioTxBwCalc;
    BwCalculator mConnRxBwCalc;
    BwCalculator mConnTxBwCalc;
    void addSample();
    void resetBwCalculators();
public:
    std::unique_ptr<RtcStats> mStats;
    Recorder(JingleSession& sess, const Options& options);
    ~Recorder();
    bool isRelay() const
    {
        if (!mHasConnInfo)
            throw std::runtime_error("No connection info yet");
        return !mStats->mConnInfo.mRlySvr.empty();
    }
    void start();
    void terminate(const std::string& termRsn);
    virtual void OnComplete(const webrtc::StatsReports& data);
    void onStats(const std::shared_ptr<artc::MyStatsReports>& data);
    std::function<void(void*, int)> onSample;
};
}
}
#endif
