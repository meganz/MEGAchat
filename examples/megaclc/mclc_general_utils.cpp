#include "mclc_general_utils.h"

#include "mclc_globals.h"

#include <karereId.h>
namespace k = ::karere;

#if defined(WIN32)
#include <windows.h>
#include <winhttp.h>
#elif defined(__linux__)
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace mclc
{

namespace cli_utils
{
std::vector<std::string> argsToVec(int argc, char* argv[])
{
    if (argc < 0)
    {
        return {};
    }
    size_t nElems = static_cast<size_t>(argc);
    std::vector<std::string> result;
    result.reserve(nElems);
    for (size_t i = 0; i < nElems; ++i)
    {
        result.push_back(argv[i]);
    }
    return result;
}
}

namespace path_utils
{

unsigned long getProcessId()
{
#if defined(_WIN32) || defined(_WIN64)
    return static_cast<unsigned long>(GetCurrentProcessId());
#else
    return static_cast<unsigned long>(getpid());
#endif
}

fs::path getExeDirectory()
{
#ifdef WIN32
    std::array<wchar_t, MAX_PATH + 1> path{};
    if (!GetModuleFileNameW(NULL, path.data(), MAX_PATH))
    {
        std::cout << "Error: Unable to retrieve exe path" << std::endl;
        exit(1);
    }
#elif defined(__APPLE__)
    std::array<char, 513> path{};
    uint32_t size = 512;
    if (_NSGetExecutablePath(path.data(), &size))
    {
        std::cout << "Error: Unable to retrieve exe path" << std::endl;
        exit(1);
    }
#else // linux
    const auto link = "/proc/" + std::to_string(getpid()) + "/exe";
    std::array<char, 513> path{};
    const auto count = readlink(link.c_str(), path.data(), 512);
    if (count == -1)
    {
        std::cout << "Error: Unable to retrieve exe path" << std::endl;
        exit(1);
    }
    path[static_cast<size_t>(count)] = '\0';
#endif
    return fs::path{path.data()}.parent_path();
}

fs::path getHomeDirectory()
{
    return fs::path(getenv(
#ifdef WIN32
        "USERPROFILE"
#else
        "HOME"
#endif
        ));
}

std::unique_ptr<m::MegaNode> GetNodeByPath(const std::string& path)
{
    if (path.find("//handle/") == 0)
    {
        m::MegaHandle h = clc_global::g_megaApi->base64ToHandle(path.c_str() + 9);
        std::unique_ptr<m::MegaNode> node(clc_global::g_megaApi->getNodeByHandle(h));
        if (!node)
        {
            clc_console::conlock(std::cout) << "No node found by looking up handle: '"
                                            << (path.c_str() + 9) << "'" << std::endl;
        }
        return node;
    }

    std::unique_ptr<m::MegaNode> node(clc_global::g_megaApi->getNodeByPath(path.c_str()));
    if (!node)
    {
        clc_console::conlock(std::cout) << "No node found at path: '" << path << "'" << std::endl;
    }
    return node;
}

fs::path pathFromLocalPath(const std::string& s, bool mustexist)
{
    fs::path p = s.empty() ? fs::current_path() : fs::u8path(s);
#ifdef WIN32
    p = fs::u8path("\\\\?\\" + p.u8string());
#endif
    if (mustexist && !fs::exists(p))
    {
        std::cout << "local path not found: '" << s << "'";
        return fs::path();
    }
    return p;
}

}

namespace str_utils
{

// Chat links look like this: https://<mega-domain>/chat/<public-handle>#<chat-key>
std::string extractChatLink(const char* message)
{
    constexpr size_t handleSize = 8;
    constexpr size_t keySize = 22;
    static const std::string base = "chat/";
    const auto chatPtr = strstr(message, base.c_str());
    if (!chatPtr)
    {
        return {};
    }
    const auto hashPtr = strstr(chatPtr, "#");
    if (!hashPtr)
    {
        return {};
    }
    if (static_cast<size_t>(hashPtr - chatPtr) - base.size() != handleSize)
    {
        return {};
    }
    auto keyPtr = hashPtr + 1;
    size_t count = 0;
    while (count < keySize && *keyPtr != '\0')
    {
        ++count;
        ++keyPtr;
    }
    if (count < keySize)
    {
        return {};
    }
    return "https://mega.nz/" +
           std::string(chatPtr, chatPtr + base.size() + handleSize + 1 + keySize);
}

// convert string to handle
c::MegaChatHandle s_ch(const std::string& s)
{
    c::MegaChatHandle ret;
    if (s == "<Null>")
    {
        ret = c::MEGACHAT_INVALID_HANDLE;
    }
    else
    {
        ret = k::Id(s.c_str(), s.size());
    }
    return ret;
}

// convert handle to string
std::string ch_s(c::MegaChatHandle h)
{
    return (h == 0 || h == c::MEGACHAT_INVALID_HANDLE) ? "<Null>" : k::Id(h).toString();
}

std::string OwnStr(const char* s)
{
    // takes ownership of a string from MegaApi, prevents leaks
    std::string str(s ? s : "");
    delete[] s;
    return str;
}

std::string base64NodeHandle(m::MegaHandle h)
{
    if (h == m::INVALID_HANDLE)
        return "INVALID_HANDLE";
    return OwnStr(m::MegaApi::handleToBase64(h));
}

std::string base64ChatHandle(m::MegaHandle h)
{
    if (h == m::INVALID_HANDLE)
    {
        return "INVALID_HANDLE";
    }
    return OwnStr(m::MegaApi::userHandleToBase64(h));
}

std::string tohex(const std::string& binary)
{
    std::ostringstream s;
    s << std::hex;

    for (const char c: binary)
    {
        s << std::setw(2) << std::setfill('0') << (unsigned)c;
    }

    return s.str();
}

unsigned char tobinary(unsigned char c)
{
    if (c >= '0' && c <= '9')
        return static_cast<unsigned char>(c - '0');
    if (c >= 'a' && c <= 'z')
        return static_cast<unsigned char>(c - 'a' + 10);
    return 0;
}

std::string tobinary(const std::string& hex)
{
    std::string bin;
    for (std::string::const_iterator i = hex.cbegin(); i != hex.cend(); ++i)
    {
        unsigned c = tobinary(static_cast<unsigned char>(*i));
        c <<= 4;
        ++i;
        if (i != hex.cend())
        {
            c |= tobinary(static_cast<unsigned char>(*i));
        }
        bin.push_back(static_cast<char>(c));
    }
    return bin;
}

std::string loadfile(const std::string& filename)
{
    std::string filedata;
    std::ifstream f(filename, std::ios::binary);
    f.seekg(0, std::ios::end);
    filedata.resize(unsigned(f.tellg()));
    f.seekg(0, std::ios::beg);
    f.read(const_cast<char*>(filedata.data()), static_cast<std::streamsize>(filedata.size()));
    return filedata;
}

std::string joinStringList(const m::MegaStringList& msl, const std::string& separator)
{
    std::string s;
    if (msl.size() > 0)
    {
        s += msl.get(0) ? msl.get(0) : "<null>"; // Añade el primer elemento sin separador delante
        for (int i = 1; i < msl.size(); ++i)
        {
            s += separator; // Añade el separador antes de los elementos siguientes
            s += msl.get(i) ? msl.get(i) : "<null>";
        }
    }
    return s;
}

}

namespace clc_console
{

ConsoleLock::ConsoleLock(std::ostream& o):
    os(o),
    locking(true)
{
    outputlock.lock();
}

ConsoleLock::ConsoleLock(ConsoleLock&& o):
    os(o.os),
    locking(o.locking)
{
    o.locking = false;
}

ConsoleLock::~ConsoleLock()
{
    if (locking)
    {
        outputlock.unlock();
    }
}

std::recursive_mutex ConsoleLock::outputlock;

ConsoleLock conlock(std::ostream& o)
{
    return ConsoleLock(o);
}

}

namespace clc_time
{

void WaitMillisec(unsigned n)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(n));
}

std::string timeToLocalTimeString(const int64_t time)
{
    struct tm dt;
    m::m_localtime(time, &dt);
    char buffer[40];
    std::strftime(buffer, 40, "%FT%T", &dt);
    return std::string{buffer};
}

std::string timeToStringUTC(const int64_t time)
{
    struct tm dt;
    m::m_gmtime(time, &dt);
    char buffer[40];
    std::strftime(buffer, 40, "%FT%H-%M-%S", &dt);
    return std::string{buffer};
}

}
}
