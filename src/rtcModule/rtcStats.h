#ifndef RTCSTATS_H
#define RTCSTATS_H
#include "IRtcModule.h" //needed for StatOptions only
#include "webrtcAdapter.h"
#include "IRtcStats.h"
#include "ITypesImpl.h"
#include <timers.h>

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

    virtual IString* ctype() const { return new IString_ref(mCtype); }
    virtual IString* proto() const { return new IString_ref(mProto); }
    virtual IString* rlySvr() const { return new IString_ref(mRlySvr); }
    virtual IString* vcodec() const { return new IString_ref(mVcodec); }
};

class RtcStats: public IRefCountedMixin<IRtcStats>
{
public:
    std::string mTermRsn;
    int mIsCaller;
    int mSper; //sample period
    int64_t mStartTs;
    int64_t mDur;
    std::string mCallId;
    std::vector<Sample*> mSamples;
    ConnInfo mConnInfo;
    //IRtcStats implementation
    virtual IString* termRsn() const { return new IString_ref(mTermRsn); }
    virtual int isCaller() const { return mIsCaller; }
    virtual IString* callId() const { return new IString_ref(mCallId); }
    virtual size_t sampleCnt() const { return mSamples.size(); }
    virtual const Sample* samples() const
    {
        if (mSamples.empty())
            return nullptr;
        return mSamples[0];
    }
    virtual const IConnInfo* connInfo() const { return &mConnInfo; }
    virtual IString* toJson() const;
};

class BasicStats: public IRefCountedMixin<IRtcStats>
{
public:
    bool mIsCaller;
    std::string mTermRsn;
    std::string mCallId;
    BasicStats(const IJingleSession& sess, const char* aTermRsn)
        :mIsCaller(sess.isCaller()), mTermRsn(aTermRsn?aTermRsn:""), mCallId(sess.getCallId()){}
    virtual IString* ctype() const { return nullptr; }
    virtual IString* proto() const { return nullptr; }
    virtual IString* rlySvr() const { return nullptr; }
    virtual IString* termRsn() const { return new IString_ref(mTermRsn); }
    virtual IString* vcodec() const { return nullptr; }
    virtual int isCaller() const { return mIsCaller; }
    virtual IString* callId() const { return new IString_ref(mCallId); }
    virtual size_t sampleCnt() const { return 0; }
    virtual const Sample* samples() const { return nullptr; }
    virtual const IConnInfo* connInfo() const { return nullptr; }
    virtual IString* toJson() const { return new IString_string("");}

};

class Recorder: public webrtc::StatsObserver
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
    ISharedPtr<RtcStats> mStats;
    Recorder(JingleSession& sess, const Options& options);
    ~Recorder();
    bool isRelay() const
    {
        if (!mHasConnInfo)
            throw std::runtime_error("No connection info yet");
        return !mStats->mConnInfo.mRlySvr.empty();
    }
    void start();
    void terminate(const char* termRsn);
    virtual void OnComplete(const std::vector<webrtc::StatsReport>& data);
    virtual int AddRef() { return 3; }
    virtual int Release() { return 2; }
    void onStats(const std::shared_ptr<artc::MappedStatsData>& data);
    std::function<void(void*, int)> onSample;
};
}
}
#endif
