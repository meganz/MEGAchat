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

#include <array>
#include <set>

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

namespace
{

const std::string kRootNz = "https://mega.nz/";
const std::string kRootApp = "https://mega.app/";

std::string linkRootURL()
{
    auto siteFlag = clc_global::g_megaApi->getFlag("site");
    constexpr int USE_APP_URL = 1;
    return (siteFlag->getType() == mega::MegaFlag::FLAG_TYPE_FEATURE &&
            siteFlag->getGroup() == USE_APP_URL) ?
               "https://mega.app/" :
               "https://mega.nz/";
}

std::vector<std::string> extractLinksWithHandleAndKey(const char* message,
                                                      const std::string& base,
                                                      bool stopAtSlashAfterKey = false)
{
    constexpr size_t handleSize = 8;
    constexpr size_t keySize = 22;

    if (!message)
    {
        return {};
    }

    std::vector<std::string> links;
    std::set<std::string> seen;
    const std::array<std::pair<std::string, size_t>, 2> roots = {
        std::make_pair(kRootNz, kRootNz.size()),
        std::make_pair(kRootApp, kRootApp.size())};

    for (const auto& [root, rootSize]: roots)
    {
        const auto prefix = root + base;
        const char* current = message;
        while ((current = strstr(current, prefix.c_str())))
        {
            const auto hashPtr = strstr(current, "#");
            if (hashPtr &&
                static_cast<size_t>(hashPtr - current) - rootSize - base.size() == handleSize)
            {
                auto keyPtr = hashPtr + 1;
                size_t count = 0;
                while (count < keySize && *keyPtr != '\0' &&
                       (!stopAtSlashAfterKey || *keyPtr != '/'))
                {
                    ++count;
                    ++keyPtr;
                }
                if (count == keySize)
                {
                    const auto link =
                        linkRootURL() + std::string(current + rootSize,
                                                    stopAtSlashAfterKey ?
                                                        keyPtr :
                                                        current + rootSize + base.size() +
                                                            handleSize + 1 + keySize);
                    if (seen.insert(link).second)
                    {
                        links.push_back(link);
                    }
                    current = keyPtr;
                    continue;
                }
            }
            current += prefix.size();
        }
    }

    return links;
}

std::string extractLinkWithHandleAndKey(const char* message,
                                        const std::string& base,
                                        bool stopAtSlashAfterKey = false)
{
    const auto links = extractLinksWithHandleAndKey(message, base, stopAtSlashAfterKey);
    return links.empty() ? std::string{} : links.front();
}

std::vector<std::string> extractLinksWithHandle(const char* message, const std::string& base)
{
    constexpr size_t handleSize = 8;

    if (!message)
    {
        return {};
    }

    std::vector<std::string> links;
    std::set<std::string> seen;
    const std::array<std::pair<std::string, size_t>, 2> roots = {
        std::make_pair(kRootNz, kRootNz.size()),
        std::make_pair(kRootApp, kRootApp.size())};

    for (const auto& [root, rootSize]: roots)
    {
        const auto prefix = root + base;
        const char* current = message;
        while ((current = strstr(current, prefix.c_str())))
        {
            auto handlePtr = current + rootSize + base.size();
            size_t count = 0;
            while (count < handleSize && *handlePtr != '\0')
            {
                ++count;
                ++handlePtr;
            }
            if (count == handleSize)
            {
                const auto link =
                    linkRootURL() +
                    std::string(current + rootSize, current + rootSize + base.size() + handleSize);
                if (seen.insert(link).second)
                {
                    links.push_back(link);
                }
                current = handlePtr;
                continue;
            }
            current += prefix.size();
        }
    }

    return links;
}

std::string extractLinkWithHandle(const char* message, const std::string& base)
{
    const auto links = extractLinksWithHandle(message, base);
    return links.empty() ? std::string{} : links.front();
}
}

// Chat links look like this:
// https://mega.app/chat/E1foobar#EFa7vexblahJwjNglfooxg
//                      ^handle  ^key
std::string extractChatLink(const char* message)
{
    static const std::string base = "chat/";
    return extractLinkWithHandleAndKey(message, base);
}

std::vector<std::string> extractChatLinks(const char* message)
{
    static const std::string base = "chat/";
    return extractLinksWithHandleAndKey(message, base);
}

// Folder links look like this:
// https://mega.nz/folder/9m0xyKza#MloVQA3krMfdPep-CYEddg/folder/RrE2UbJS
//                        ^handle  ^key
std::string extractFolderLink(const char* message)
{
    static const std::string base = "folder/";
    return extractLinkWithHandleAndKey(message, base, true);
}

std::vector<std::string> extractFolderLinks(const char* message)
{
    static const std::string base = "folder/";
    return extractLinksWithHandleAndKey(message, base, true);
}

// File links look like this:
// https://mega.nz/file/zAJnUTYD#8YE5dXrnIEJ47NdDfFEvqtOefhuDMphyae0KY5zrhns
//                      ^handle  ^key
std::string extractFileLink(const char* message)
{
    static const std::string base = "file/";
    return extractLinkWithHandleAndKey(message, base);
}

std::vector<std::string> extractFileLinks(const char* message)
{
    static const std::string base = "file/";
    return extractLinksWithHandleAndKey(message, base);
}

// Contact links look like this:
// https://mega.nz/C!S6hTFahK
//                   ^handle
std::string extractContactLink(const char* message)
{
    static const std::string base = "C!";
    return extractLinkWithHandle(message, base);
}

std::vector<std::string> extractContactLinks(const char* message)
{
    static const std::string base = "C!";
    return extractLinksWithHandle(message, base);
}

// Album links look like this:
// https://mega.nz/collection/GqQDQDAJ#ZMKkdfm3HQjUa8WLT6nj2g
//                            ^handle  ^key
std::string extractAlbumLink(const char* message)
{
    static const std::string base = "collection/";
    return extractLinkWithHandleAndKey(message, base);
}

std::vector<std::string> extractAlbumLinks(const char* message)
{
    static const std::string base = "collection/";
    return extractLinksWithHandleAndKey(message, base);
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
