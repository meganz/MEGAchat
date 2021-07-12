#ifndef RTCSTATS_H
#define RTCSTATS_H
#include <karereId.h>
#include <api/stats/rtc_stats_collector_callback.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <base/trackDelete.h>

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
        // Scalable video coding index
        std::vector<int32_t> mQ;
        // Audio video flags
        std::vector<int32_t> mAv;
        // number of high resolution active tracks
        std::vector<int32_t> mNrxh;
        // number of low resolution active tracks
        std::vector<int32_t> mNrxl;
        // number of audio active tracks
        std::vector<int32_t> mNrxa;
        // fps low res video
        std::vector<int32_t> mVtxLowResfps;
        // width low res video
        std::vector<int32_t> mVtxLowResw;
        // height low res video
        std::vector<int32_t> mVtxLowResh;
        // fps high res video
        std::vector<int32_t> mVtxHiResfps;
        // width high res video
        std::vector<int32_t> mVtxHiResw;
        // height high res video
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
        std::string mDevice;

    protected:
        void parseSamples(const std::vector<int32_t>& samples, rapidjson::Value& value, rapidjson::Document &json, bool diff, const std::vector<int32_t>* periods = nullptr);
    };

    class RtcStatCallback : public webrtc::RTCStatsCollectorCallback, public karere::DeleteTrackable
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

    class RemoteStatsCallBack : public RtcStatCallback
    {
    public:
        RemoteStatsCallBack(Stats* stats);
        ~RemoteStatsCallBack();
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
