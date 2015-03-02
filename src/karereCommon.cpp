#include "karereCommon.h"
#include <unistd.h>

namespace karere
{
int isatty_stdout = isatty(1);
int isatty_stderr = isatty(2);

#ifdef _WIN32
//needed to compensate for the wrap of the 32bit millisec value (every ~49 days)
    Ts _gLastTimeValue = 0;
    Ts _gTimeBase = 0;
#elif defined(__MACH__)
    double _gTimeConversionFactor = -1.0;
    void _init_mac_timefunc()
    {
        mach_timebase_info_data_t timebase;
        mach_timebase_info(&timebase);
        _gTimeConversionFactor = ((double)timebase.numer*1000000) / (double)timebase.denom;
    }
#endif
}

