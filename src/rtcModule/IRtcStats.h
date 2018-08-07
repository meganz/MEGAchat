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
    long bs;        // delta-bytes
    long bps;       // bits-per-second
    long abps;      // average bits-per-second
};

struct Sample
{
    int64_t ts;
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
            long bwav = 0;          // bandwidth average
        } r;    // receive

        struct : BwInfo
        {
            long gbps = 0;          // transmit bitrate
            short fps = 0;          // frames-per-second
            short cfps = 0;         // frame-rate input
            long cjtr = 0;
            short width = 0;        // width of frame
            short height = 0;       // height of frame
            float el = 0.0;         // encode-usage percentage
            short lcpu = 0;         // CPU-limited resolution
            short lbw = 0;          // bandwidth-limited resolution
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
