#ifndef IRTCSTATS_H
#define IRTCSTATS_H

#include "ITypes.h"

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
    virtual IString* ctype() const = 0;
    virtual IString* proto() const = 0;
    virtual IString* rlySvr() const = 0;
    virtual IString* vcodec() const = 0;
};

class IRtcStats: public IDestroy
{
public:
    virtual IString* termRsn() const = 0;
    virtual int isCaller() const = 0;
    virtual IString* callId() const = 0;
    virtual size_t sampleCnt() const = 0;
    virtual const Sample* samples() const = 0;
    virtual const IConnInfo* connInfo() const = 0;
};

struct Options
{
    int enableStats = 1;
    int scanPeriod = -1;
    int maxSamplePeriod = -1;
};

}
}
#endif
