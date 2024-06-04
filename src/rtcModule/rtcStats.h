#ifndef RTCSTATS_H
#define RTCSTATS_H
#include <karereId.h>
// disable warnings in webrtc headers
// the same pragma works with both GCC and Clang
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif
#include <api/stats/rtc_stats_collector_callback.h>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#include <base/trackDelete.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include <array>

namespace rtcModule
{

class QualityLimitationReport
{
public:
    enum class EReason : unsigned int 
    {
        NONE = 0,
        CPU = 1,
        BANDWIDTH = 2,
        OTHER = 4,
    };
    static constexpr unsigned int NUMBER_OF_REASONS = 4u;

    using StrToReasonMap = std::array<std::pair<std::string, EReason>, NUMBER_OF_REASONS>;
    const StrToReasonMap mStrReasonMap{
        std::make_pair("none", QualityLimitationReport::EReason::NONE),
        std::make_pair("cpu", QualityLimitationReport::EReason::CPU),
        std::make_pair("bandwidth", QualityLimitationReport::EReason::BANDWIDTH),
        std::make_pair("other", QualityLimitationReport::EReason::OTHER),
    };

    /**
     * @brief Restarts all the incidents counters.
     */
    void clear()
    {
        mIncidentCounter.fill(0u);
    }

    /**
     * @brief Increments in one the internal counter of incidents matching the given type
     *
     * @param reason The string indicating the quality limitation reason. Should be contained in
     * mStrReasonMap or be empty (interpreted as "none")
     */
    void addIncident(const std::string& reason);

    /**
     * @brief Writes the incidents counts in json format:
     *
     * - Json Output: [[0, 1], [1, 3], [2, 2], [3, 5]]
     *
     *   The first element of each pair is the numeric value associated to each type (see EReason)
     *   and the second is the number of reported incidents for that type.
     */
    void toJson(rapidjson::Value& value, rapidjson::Document& json);

private:
    std::array<uint32_t, NUMBER_OF_REASONS> mIncidentCounter{};
};

class StatSamples
{
public:
    std::vector<int32_t> mT;
    std::vector<int32_t> mPacketLost;
    std::vector<int32_t> mRoundTripTime;
    std::vector<int32_t> mOutGoingBitrate;
    std::vector<int32_t> mBytesReceived;
    std::vector<int32_t> mBytesSend;
    std::vector<int32_t> mAudioJitter;
    std::vector<uint32_t> mPacketSent;
    std::vector<double> mTotalPacketSendDelay;
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
    // Number of quality limitation per reason
    QualityLimitationReport mQualityLimitations;
};

class Stats
{
public:
    enum class statsErr : int
    {
        kStatsCallIdErr = -7,
        kStatsSfuHostErr = -6,
        kStatsDeviceErr = -5,
        kStatsDurErr = -4,
        kStatsCidErr = -3,
        kStatsMyPeerErr = -2,
        kStatsProtoErr = -1,
        kStatsOk = 0,
    };

    static std::string statsErrToString(statsErr e)
    {
        switch (e)
        {
            case statsErr::kStatsCallIdErr:
                return "Invalid CallId";
                break;
            case statsErr::kStatsSfuHostErr:
                return "Invalid SFU host";
                break;
            case statsErr::kStatsDeviceErr:
                return "Invalid device Id";
                break;
            case statsErr::kStatsDurErr:
                return "Invalid call duration";
                break;
            case statsErr::kStatsCidErr:
                return "Invalid Cid";
                break;
            case statsErr::kStatsMyPeerErr:
                return "Invalid PeerId";
                break;
            case statsErr::kStatsProtoErr:
                return "Invalid protocol version";
                break;
            case statsErr::kStatsOk:
                return "Stats Ok";
                break;
            default:
                return "Invalid statsErr";
                assert(false);
                break;
        }
    }

    statsErr validateStatsInfo() const;
    std::pair<statsErr, std::string> getJson();
    void clear();
    bool isEmptyStats();

    karere::Id mPeerId;
    uint32_t mCid = 0;
    uint32_t mSfuProtoVersion = UINT32_MAX;
    karere::Id mCallid;
    // Duration of the call before our connection to sfu
    uint64_t mTimeOffset = 0;
    // Duration of the call while we are participating (no call duration)
    uint64_t mDuration = 0;
    // maximum number of peers (excluding yourself), seen throughout the call
    uint8_t mMaxPeers = 0;
    StatSamples mSamples;
    int32_t mTermCode = 0;
    bool mIsGroup = false;
    int64_t mInitialTs = 0;
    std::string mDevice;
    std::string mSfuHost;

protected:
    static constexpr int kUnassignedCid =
        -1; // default value for unassigned CID (still not JOINED to SFU)
    void parseSamples(const std::vector<int32_t>& samples,
                      rapidjson::Value& value,
                      rapidjson::Document& json,
                      bool diff,
                      const std::vector<float>* periods = nullptr);
};

class ConnStatsCallBack:
    public rtc::RefCountedObject<webrtc::RTCStatsCollectorCallback>,
    public karere::DeleteTrackable
{
public:
    ConnStatsCallBack(Stats* stats, uint32_t hiResId, uint32_t lowResId, void* appCtx);
    ~ConnStatsCallBack();
    void removeStats();

    void OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) override;

protected:
    void getConnStats(const webrtc::RTCStatsReport::ConstIterator& it,
                      double& rtt,
                      double& txBwe,
                      int64_t& bytesRecv,
                      int64_t& bytesSend);

    Stats* mStats = nullptr; // Doesn't take ownership (Belongs to Call)
    uint32_t mHiResId;
    uint32_t mLowResId;
    void* mAppCtx;
};
}

#endif
