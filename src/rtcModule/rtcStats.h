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
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <base/trackDelete.h>
#include <array>

namespace rtcModule
{

enum class EQualityLimitationReason: unsigned int
{
    NONE = 0,
    CPU = 1,
    BANDWIDTH = 2,
    OTHER = 4,
};
static constexpr unsigned int NUMBER_OF_QUALITY_LIMITATION_REASONS = 4u;

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
    std::array<uint32_t, NUMBER_OF_QUALITY_LIMITATION_REASONS> mQualityLimitationReasons{};

    /**
     * @brief Auxiliary method to convert a quality limitation reason into an index to access the
     * right element of the mQualityLimitationReasons member.
     *
     * Check the allowed values here:
     * https://developer.mozilla.org/en-US/docs/Web/API/RTCOutboundRtpStreamStats/qualityLimitationReason
     *
     * @param reason The reason to map
     * @return The index to access the mQualityLimitationReasons array.
     */
    static EQualityLimitationReason parseQualityLimitReason(const std::string& reason);

    /**
     * @brief Converts an instance of EQualityLimitationReason to an index to access the
     * corresponding value in the mQualityLimitationReasons array.
     */
    static size_t qualityLimitationReasonToIndex(const EQualityLimitationReason limitReason);

    /**
     * @brief Converts an index to access the value in the mQualityLimitationReasons array to the
     * corresponding EQualityLimitationReason.
     */
    static EQualityLimitationReason indexToQualityLimitationReason(const size_t index);
};

class Stats
{
public:
    std::string getJson();
    void clear();
    bool isEmptyStats();

    karere::Id mPeerId;
    uint32_t mCid = 0;
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
    static constexpr int kUnassignedCid = -1; // default value for unassigned CID (still not JOINED to SFU)
    void parseSamples(const std::vector<int32_t>& samples, rapidjson::Value& value, rapidjson::Document &json, bool diff, const std::vector<float> *periods = nullptr);

    /**
     * @brief Converts the mQualityLimitationReason member of a Stats instance into json.
     * 
     * Example:
     * - Input: [1, 3, 2, 5]
     * - Json Output: [[0, 1], [1, 3], [2, 2], [3, 5]]
     */
    void parseSamplesQualityLimitations(const std::array<uint32_t, 4>& limitationReasons,
                                        rapidjson::Value& value,
                                        rapidjson::Document& json);
};

class ConnStatsCallBack : public rtc::RefCountedObject<webrtc::RTCStatsCollectorCallback>, public karere::DeleteTrackable
{
public:
    ConnStatsCallBack(Stats* stats, uint32_t hiResId, uint32_t lowResId, void* appCtx);
    ~ConnStatsCallBack();
    void removeStats();

    void OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) override;
protected:
    void getConnStats(const webrtc::RTCStatsReport::ConstIterator& it, double& rtt, double& txBwe, int64_t& bytesRecv, int64_t& bytesSend);

    Stats* mStats = nullptr; // Doesn't take ownership (Belongs to Call)
    uint32_t mHiResId;
    uint32_t mLowResId;
    void* mAppCtx;
};
}

#endif
