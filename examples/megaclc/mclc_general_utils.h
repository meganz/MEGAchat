#pragma once

#include <mega.h>
namespace m = ::mega;

#include <megachatapi.h>
namespace c = ::megachat;

#include <karereId.h>


#include <string>

#include <filesystem>
namespace fs = std::filesystem;

namespace mclc
{
namespace path_utils
{

#ifdef __APPLE__
// No std::fileystem before OSX10.15
std::string getExeDirectory();
#else
fs::path getExeDirectory();
#endif

std::unique_ptr<m::MegaNode> GetNodeByPath(const std::string& path);

fs::path pathFromLocalPath(const std::string& s, bool mustexist);

}

namespace str_utils
{

// Chat links look like this:
// https://mega.nz/chat/E1foobar#EFa7vexblahJwjNglfooxg
//                      ^handle  ^key
std::string extractChatLink(const char* message);

// convert string to handle
c::MegaChatHandle s_ch(const std::string& s);

// convert handle to string
std::string ch_s(c::MegaChatHandle h);

std::string OwnStr(const char* s);

std::string base64NodeHandle(m::MegaHandle h);

std::string tohex(const std::string& binary);

unsigned char tobinary(unsigned char c);

std::string tobinary(const std::string& hex);

std::string loadfile(const std::string& filename);

std::string joinStringList(m::MegaStringList& msl, const std::string& separator);

}

namespace clc_console
{

struct ConsoleLock
{
    static std::recursive_mutex outputlock;
    std::ostream& os;
    bool locking = false;
    ConsoleLock(std::ostream& o);

    ConsoleLock(ConsoleLock&& o);
    ~ConsoleLock();

    template<class T>
    std::ostream& operator<<(T&& arg)
    {
        return os << std::forward<T>(arg);
    }
};

// Returns a temporary object that has locked a mutex.  The temporary's destructor will unlock the object.
// So you can get multithreaded non-interleaved console output with just conlock(cout) << "some " << "strings " << endl;
// (as the temporary's destructor will run at the end of the outermost enclosing expression).
// Or, move-assign the temporary to an lvalue to control when the destructor runs (to lock output over several statements).
// Be careful not to have cout locked across a g_megaApi member function call, as any callbacks that also log could then deadlock.
ConsoleLock conlock(std::ostream& o);

}

namespace clc_time
{

void WaitMillisec(unsigned n);

std::string timeToLocalTimeString(const int64_t time);

std::string timeToStringUTC(const int64_t time);


}
}
