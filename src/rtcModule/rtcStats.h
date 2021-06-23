#ifndef RTCSTATS_H
#define RTCSTATS_H
#include <karereId.h>
#include <api/stats/rtc_stats_collector_callback.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>

namespace rtcModule
{
    //typedef std::pair<int64_t, int64_t> StatData;
    class StatSamples
    {
    public:
        std::vector<int32_t> mT;
        std::vector<int32_t> mPacketLost;
        std::vector<int32_t> mRoundTripTime;
        std::vector<int32_t> mOutGoingBitrate;
        std::vector<int32_t> mBytesReceived;
        std::vector<int32_t> mBytesSend;
        std::vector<int32_t> mQ;
        std::vector<int32_t> mAv;
        std::vector<int32_t> mNrxh;
        std::vector<int32_t> mNrxl;
        std::vector<int32_t> mNrxa;
        std::vector<int32_t> mVtxLowResfps;
        std::vector<int32_t> mVtxLowResw;
        std::vector<int32_t> mVtxLowResh;
        std::vector<int32_t> mVtxHiResfps;
        std::vector<int32_t> mVtxHiResw;
        std::vector<int32_t> mVtxHiResh;
    };

    class Stats
    {
    public:
        std::string getJson();
        void clear();

        karere::Id mPeerId;
        uint32_t mCid = 0;
        karere::Id mCallid;
        uint64_t mTimeOffset = 0;
        uint64_t mDuration = 0;
        StatSamples mSamples;
        int32_t mTerCode = 0;
        bool mIsGroup = false;
        int64_t mInitialTs = 0;

    protected:
        void parseSamples(const std::vector<int32_t>& samples, rapidjson::Value& value, rapidjson::Document &json, bool diff, const std::vector<int32_t>* periods = nullptr);
    };

    class ConnStats
    {
    public:
        int64_t mPeriodConnStats = 0;
        uint64_t mTxBwe = 0;
        uint64_t mRx = 0;
        uint64_t mTx = 0;
    };

    class RxStat : public ConnStats
    {
    public:
        uint64_t mPacketLost = 0;
        uint64_t mNackCount = 0;
        double mBytesRecv = 0.0;
        double mKeyFramesEncoded = 0.0;
    };

    class TxStat : public ConnStats
    {
    public:
        uint32_t mWidth = 0;
        uint32_t mHeight = 0;
        float mFps = 0.0;
        double mBytesSend = 0.0;
        double mKeyFramesEncoded = 0.0;
    };

    class RtcStatCallback : public webrtc::RTCStatsCollectorCallback
    {
    public:
        RtcStatCallback(Stats* stats);
        void removeStats();

        void AddRef() const override;
        rtc::RefCountReleaseStatus Release() const override;

    protected:
        Stats* mStats;

    private:
        mutable webrtc::webrtc_impl::RefCounter mRefCount{0};
    };

    class LocalVideoStatsCallBack : public RtcStatCallback
    {
    public:
        LocalVideoStatsCallBack(Stats *stats, bool hiRes);
        void OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) override;
    private:
        bool mHiRes;
    };

    class RemoteVideoStatsCallBack : public RtcStatCallback
    {
    public:
        RemoteVideoStatsCallBack(Stats* stats);
        ~RemoteVideoStatsCallBack();
        void OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) override;
    };

    class ConnStatsCallBack : public RtcStatCallback
    {
    public:
        ConnStatsCallBack(Stats* stats);
        ~ConnStatsCallBack();
        void OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) override;
    protected:
        void getConnStats(const webrtc::RTCStatsReport::ConstIterator& it, double &rtt, double txBwe, int64_t &bytesRecv, int64_t &bytesSend);
    };
}

#endif
