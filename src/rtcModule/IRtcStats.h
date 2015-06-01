#ifndef IRTCSTATS_H
#define IRTCSTATS_H

#include "ITypes.h"

namespace rtcModule
{
namespace stats
{
typedef long Val;

struct Sample
{
    struct
    {
        struct
        {
            Val bt;
            Val bps;
            Val abps;
            Val pl;
            Val fps;
            Val dly;
            Val jtr;
            Val width;
            Val height;
            Val bwav;
        } r;
        struct
        {
            Val bt;
            Val bps;
            Val abps;
            Val gbps;
            Val gabps;
            Val rtt;
            Val fps;
            Val cfps;
            Val cjtr;
            Val width;
            Val height;
            Val el;
            Val lcpu;
            Val lbw;
            Val bwav;
        } s;
    } vstats;
    struct
    {
        Val rtt;
        Val pl;
        Val jtr;
    } astats;
    struct
    {
        Val rtt;
        struct
        {
            Val bt;
            Val bps;
            Val abps;
        } r;
        struct
        {
            Val bt;
            Val bps;
            Val abps;
        } s;
    } cstats;
    int64_t ts;
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
}
}
#endif
