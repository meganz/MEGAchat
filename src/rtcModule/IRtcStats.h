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
    long bs;
    long bps;
    long abps;
};

struct Sample
{
    int64_t ts;
    struct
    {
        long rtt = 0;
        struct : BwInfo
        {
            long pl = 0;
            long fps = 0;
            long dly = 0;
            long jtr = 0;
            short width = 0;
            short height = 0;
            long bwav = 0;
        } r;
        struct : BwInfo
        {
            long gbps = 0;
            short fps = 0;
            short cfps = 0;
            long cjtr = 0;
            short width = 0;
            short height = 0;
            float el = 0.0;
            short lcpu = 0;
            short lbw = 0;
            long bwav = 0;
            long targetEncBitrate = 0;
        } s;
    } vstats;
    struct
    {
        long rtt = 0;
        long plDifference = 0;
        struct : BwInfo
        {
            long pl = 0;
            long jtr = 0;
        } r;
        BwInfo s;
    } astats;
    struct
    {
        long rtt = 0;
        BwInfo r;
        BwInfo s;
    } cstats;
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
