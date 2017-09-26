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
        struct : BwInfo
        {
            long pl;
            long fps;
            long dly;
            long jtr;
            short width;
            short height;
            long bwav;
        } r;
        struct : BwInfo
        {
            long gbps;
            long gabps;
            long rtt;
            short fps;
            short cfps;
            long cjtr;
            short width;
            short height;
            float el;
            unsigned char lcpu;
            unsigned char lbw;
            long bwav;
            long targetEncBitrate;
        } s;
    } vstats;
    struct
    {
        long rtt;
        long pl;
        long jtr;
        BwInfo r;
        BwInfo s;
    } astats;
    struct
    {
        long rtt;
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
