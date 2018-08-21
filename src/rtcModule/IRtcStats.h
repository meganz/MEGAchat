#ifndef IRTCSTATS_H
#define IRTCSTATS_H

#include "ITypes.h"
#include <karereId.h>
#include <functional>

namespace rtcModule
{
namespace stats
{

struct BwInfo
{
    long bt = 0;        // total bytes
    long bps = 0;       // bits-per-second
    long abps = 0;      // average bits-per-second
};

struct Sample
{
    int64_t ts;
    int lq;         // network quality
    long f;         // Av flags + cpu limit resolution + bandwith limit resolution
    struct
    {
        long rtt = 0;               // Round-Trip delay Time

        struct : BwInfo
        {
            long pl = 0;            // packets-lost
            long fps = 0;           // frames-per-second
            long dly = 0;           // current delay (ms)
            long jtr = 0;           // jitter
            short width = 0;        // width of frame
            short height = 0;       // height of frame
            long bwav = 0;          // bandwidth available
            long firtx = 0;         // full intra request
            long plitx = 0;         // picture loss indication
            long nacktx = 0;        // Negative Acknowledgement
        } r;    // receive

        struct : BwInfo
        {
            long gbps = 0;          // transmit bitrate
            short fps = 0;          // frames-per-second
            short cfps = 0;         // frame-rate input
            short width = 0;        // width of frame
            short height = 0;       // height of frame
            float el = 0.0;         // encode-usage percentage
            long bwav = 0;          // bandwidth available
            long targetEncBitrate = 0;
        } s;    // sent

    } vstats;   // video-stats

    struct
    {
        long rtt = 0;               // Round-Trip delay Time
        long plDifference = 0;      // packets lost difference
        struct : BwInfo
        {
            long pl = 0;            // packets lost
            long jtr = 0;           // jitter
            long dly = 0;           // current delay in milliseconds
            long al = 0;            // audio output level
        } r;
        BwInfo s;

    } astats;   // audio-stats

    struct
    {
        long rtt = 0;   // Round-Trip delay Time
        BwInfo r;       // reception
        BwInfo s;       // sending
    } cstats;   // connection-stats
};

class IConnInfo
{
public:
    virtual const std::string& ctype() const = 0;
    virtual const std::string& proto() const = 0;
    virtual const std::string& rlySvr() const = 0;
    virtual const std::string& vcodec() const = 0;
};

class IRtcStats
{
public:
    virtual const std::string& termRsn() const = 0;
    virtual bool isCaller() const = 0;
    virtual karere::Id callId() const = 0;
    virtual size_t sampleCnt() const = 0;
    virtual const std::vector<Sample*>* samples() const = 0;
    virtual const IConnInfo* connInfo() const = 0;
    virtual void toJson(std::string&) const = 0;
    virtual ~IRtcStats(){}
};

struct Options
{
    bool enableStats = true;
    int scanPeriod = -1;
    int maxSamplePeriod = -1;
    std::function<void(void*, int)> onSample;
};

}
}
#endif
