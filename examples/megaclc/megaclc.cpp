/**
 * @file examples/megaclc.cpp
 * @brief Sample application, interactive command line chat client
 *
 * (c) 2018-2018 by Mega Limited, Auckland, New Zealand
 *
 * This file is part of the MEGA SDK - Client Access Engine.
 *
 * Applications using the MEGA API must present a valid application key
 * and comply with the the rules set forth in the Terms of Service.
 *
 * The MEGA SDK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @copyright Simplified (2-clause) BSD License.
 *
 * You should have received a copy of the license along with this
 * program.
 */

// This program is intended for exploring the chat API, performing testing and so on.
// It's not well tested and should be considered alpha at best. 

#if defined(WIN32)
#include <windows.h>
#include <winhttp.h>
#elif defined(__linux__)
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#include <atomic>
#include <iomanip>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>
#include <regex>

#define USE_VARARGS
#define PREFER_STDARG

#ifndef NO_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include <mega.h>
#include <megaapi.h>
#include <megachatapi.h>
#include <karereId.h>
#include <mega/autocomplete.h>

using namespace std;
namespace m = ::mega;
namespace ac = m::autocomplete;
namespace c = ::megachat;
namespace k = ::karere;

using m::SimpleLogger;
using m::logFatal;
using m::logError;
using m::logWarning;
using m::logInfo;
using m::logDebug;

#ifdef WIN32
#define strdup _strdup
#endif

#if (__cplusplus >= 201700L)
    #include <filesystem>
    namespace fs = std::filesystem;
#else
#ifdef WIN32
    #include <filesystem>
    namespace fs = std::experimental::filesystem;
#else
    #include <experimental/filesystem>
    namespace fs = std::experimental::filesystem;
#endif
#endif

bool g_detailHigh = false;

std::atomic<bool> g_reportMessagesDeveloper{false};

// These objects are helping to work around history loading problems for reviewing public chats
std::atomic<bool> g_reviewingPublicChat{false};
std::atomic<bool> g_startedPublicChatReview{false};
std::atomic<int> g_reviewPublicChatMsgCountRemaining{-1};
std::atomic<unsigned int> g_reviewPublicChatMsgCount{0};
std::atomic<c::MegaChatHandle> g_reviewPublicChatid{c::MEGACHAT_INVALID_HANDLE};
std::unique_ptr<std::ofstream> g_reviewPublicChatOutFile;
std::unique_ptr<std::ofstream> g_reviewPublicChatOutFileLinks;
std::mutex g_reviewPublicChatOutFileLogsMutex;
std::unique_ptr<std::ofstream> g_reviewPublicChatOutFileLogs;
class ReviewPublicChat_GetUserEmail_Listener;

static const int MAX_NUMBER_MESSAGES = 300;

#ifdef __APPLE__
// No std::fileystem before OSX10.15
string getExeDirectory()
{
    array<char, 513> path{};
    uint32_t size = 512;
    if (_NSGetExecutablePath(path.data(), &size))
    {
        cout << "Error: Unable to retrieve exe path" << endl;
        exit(1);
    }
    const std::string spath{path.data()};
    return spath.substr(0, spath.find_last_of('/'));
}
#else
fs::path getExeDirectory()
{
#ifdef WIN32
    array<wchar_t, MAX_PATH + 1> path{};
    if (!GetModuleFileNameW(NULL, path.data(), MAX_PATH))
    {
        cout << "Error: Unable to retrieve exe path" << endl;
        exit(1);
    }
    return fs::path{path.data()}.parent_path();
#else // linux
    const auto link = "/proc/" + to_string(getpid()) + "/exe";
    array<char, 513> path{};
    const auto count = readlink(link.c_str(), path.data(), 512);
    if (count == -1)
    {
        cout << "Error: Unable to retrieve exe path" << endl;
        exit(1);
    }
    path[count] = '\0';
    return fs::path{path.data()}.parent_path();
#endif
}
#endif

// Chat links look like this:
// https://mega.nz/chat/E1foobar#EFa7vexblahJwjNglfooxg
//                      ^handle  ^key
string extractChatLink(const char* message)
{
    constexpr size_t handleSize = 8;
    constexpr size_t keySize = 22;
    static const string base = "chat/";
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
    if (hashPtr - chatPtr - base.size() != handleSize)
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
    return "https://mega.nz/" + string(chatPtr, chatPtr + base.size() + handleSize + 1 + keySize);
}

void WaitMillisec(unsigned n)
{
#ifdef WIN32
    Sleep(n);
    #define strdup _strdup
#else
    usleep(n*1000);
#endif
}

class OneShotRequestListener : public m::MegaRequestListener
{
public:
    std::function<void(m::MegaApi* api, m::MegaRequest *request, m::MegaError* e)> onRequestFinishFunc;

    explicit OneShotRequestListener(std::function<void(m::MegaApi* api, m::MegaRequest *request, m::MegaError* e)> f = {})
        :onRequestFinishFunc(f)
    {
    }

    void onRequestFinish(m::MegaApi* api, m::MegaRequest *request, m::MegaError* e) override
    {
        if (onRequestFinishFunc) onRequestFinishFunc(api, request, e);
        delete this;  // one-shot is done so auto-delete
    }
};

class OneShotTransferListener : public m::MegaTransferListener
{
public:
    std::function<void(m::MegaApi* api, m::MegaTransfer *request, m::MegaError* e)> onTransferFinishFunc;

    explicit OneShotTransferListener(std::function<void(m::MegaApi* api, m::MegaTransfer* transfer, m::MegaError* e)> f = {})
        :onTransferFinishFunc(f)
    {
    }

    void onTransferFinish(m::MegaApi* api, m::MegaTransfer *request, m::MegaError* e) override
    {
        if (onTransferFinishFunc) onTransferFinishFunc(api, request, e);
        delete this;  // one-shot is done so auto-delete
    }
};

class OneShotChatRequestListener : public c::MegaChatRequestListener
{
public:
    std::function<void(c::MegaChatApi* api, c::MegaChatRequest *request)> onRequestStartFunc;
    std::function<void(c::MegaChatApi* api, c::MegaChatRequest *request, c::MegaChatError* e)> onRequestFinishFunc;
    std::function<void(c::MegaChatApi*api, c::MegaChatRequest *request)> onRequestUpdateFunc;
    std::function<void(c::MegaChatApi *api, c::MegaChatRequest *request, c::MegaChatError* error)> onRequestTemporaryErrorFunc;

    explicit OneShotChatRequestListener(std::function<void(c::MegaChatApi* api, c::MegaChatRequest *request, c::MegaChatError* e)> f = {})
        :onRequestFinishFunc(f)
    {
    }

    void onRequestStart(c::MegaChatApi* api, c::MegaChatRequest *request) override
    {
        if (onRequestStartFunc) onRequestStartFunc(api, request);
    }

    void onRequestFinish(c::MegaChatApi* api, c::MegaChatRequest *request, c::MegaChatError* e) override
    {
        if (onRequestFinishFunc) onRequestFinishFunc(api, request, e);
        delete this;  // one-shot is done so auto-delete
    }

    void onRequestUpdate(c::MegaChatApi*api, c::MegaChatRequest *request) override
    {
        if (onRequestUpdateFunc) onRequestUpdateFunc(api, request);
    }

    void onRequestTemporaryError(c::MegaChatApi *api, c::MegaChatRequest *request, c::MegaChatError* error) override
    {
        if (onRequestTemporaryErrorFunc) onRequestTemporaryErrorFunc(api, request, error);
    }
};

struct ConsoleLock
{
    static std::recursive_mutex outputlock;
    std::ostream& os;
    bool locking = false;
    inline ConsoleLock(std::ostream& o)
        : os(o), locking(true)
    {
        outputlock.lock();
    }
    ConsoleLock(ConsoleLock&& o)
        : os(o.os), locking(o.locking)
    {
        o.locking = false;
    }
    ~ConsoleLock()
    {
        if (locking)
        {
            outputlock.unlock();
        }
    }

    template<class T>
    ostream& operator<<(T&& arg)
    {
        return os << std::forward<T>(arg);
    }
};

std::recursive_mutex ConsoleLock::outputlock;

ConsoleLock conlock(std::ostream& o)
{
    // Returns a temporary object that has locked a mutex.  The temporary's destructor will unlock the object.
    // So you can get multithreaded non-interleaved console output with just conlock(cout) << "some " << "strings " << endl;
    // (as the temporary's destructor will run at the end of the outermost enclosing expression).
    // Or, move-assign the temporary to an lvalue to control when the destructor runs (to lock output over several statements).
    // Be careful not to have cout locked across a g_megaApi member function call, as any callbacks that also log could then deadlock.
    return ConsoleLock(o);
}

std::string timeToLocalTimeString(const int64_t time)
{
    struct tm dt;
    m::m_localtime(time, &dt);
    char buffer[40];
    std::strftime(buffer, 40, "%FT%T", &dt);
    return std::string{buffer};
}

class MegaCLLogger : public m::Logger {
public:
    void logMsg(const int loglevel, const std::string& message)
    {
        log(timeToLocalTimeString(time(0)).c_str(), loglevel, nullptr, message.c_str());
    }

private:
    void log(const char* time, int loglevel, const char*, const char *message) override
    {
#ifdef _WIN32
        OutputDebugStringA(message);
        OutputDebugStringA("\r\n");
#endif
        std::ostringstream os;
        os << "API [" << time << "] " << m::SimpleLogger::toStr(static_cast<m::LogLevel>(loglevel)) << ": " << message << endl;
        const auto msg = os.str();
        if (loglevel <= m::logError)
        {
            conlock(cout) << msg << flush;
        }
        if (g_reviewPublicChatOutFileLogs)
        {
            std::lock_guard<std::mutex> lock{g_reviewPublicChatOutFileLogsMutex};
            *g_reviewPublicChatOutFileLogs << msg << flush;
        }
    }
};

MegaCLLogger g_apiLogger;

struct MegaclcChatChatLogger : public c::MegaChatLogger
{
public:
    void logMsg(const int loglevel, const std::string& message)
    {
        const std::string msg = "[" + timeToLocalTimeString(time(0)) + "] Level(" + std::to_string(loglevel) + "): " + message;
        log(loglevel, msg.c_str());
    }

private:
    void log(int loglevel, const char *message) override
    {
#ifdef _WIN32
        if (message && *message)
        {
            OutputDebugStringA(message);
            if (message[strlen(message)-1] != '\n')
                OutputDebugStringA("\r\n");
        }
#endif
        std::ostringstream os;
        os << "CHAT " << message;
        if (*message && message[strlen(message) - 1] != '\n')
        {
            os << endl;
        }
        const auto msg = os.str();
        if (loglevel <= c::MegaChatApi::LOG_LEVEL_ERROR)
        {
            conlock(cout) << msg << flush;
        }
        if (g_reviewPublicChatOutFileLogs)
        {
            std::lock_guard<std::mutex> lock{g_reviewPublicChatOutFileLogsMutex};
            *g_reviewPublicChatOutFileLogs << msg << flush;
        }
    }
};

MegaclcChatChatLogger g_chatLogger;


// convert string to handle
c::MegaChatHandle s_ch(const string& s)
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
string ch_s(c::MegaChatHandle h)
{
    return (h == 0 || h == c::MEGACHAT_INVALID_HANDLE) ? "<Null>" : k::Id(h).toString();
}

enum ReportOnConsole { NoConsoleReport, ReportFailure, ReportResult };

bool check_err(const string& opName, m::MegaError* e, ReportOnConsole report = NoConsoleReport)
{
    if (e->getErrorCode() == c::MegaChatError::ERROR_OK)
    {
        const std::string message = opName + " succeeded.";
        g_apiLogger.logMsg(m::MegaApi::LOG_LEVEL_INFO, message);
        if (report == ReportResult)
        {
            conlock(cout) << message << endl;
        }
        return true;
    }
    else
    {
        const std::string message = opName + " failed. Error: " + string{e->getErrorString()};
        g_apiLogger.logMsg(m::MegaApi::LOG_LEVEL_ERROR, message);
        if (report != NoConsoleReport)
        {
            conlock(cout) << message << endl;
        }
        return false;
    }
}

bool check_err(const string& opName, c::MegaChatError* e, ReportOnConsole report = NoConsoleReport)
{
    if (e->getErrorCode() == c::MegaChatError::ERROR_OK)
    {
        const std::string message = opName + " succeeded.";
        g_chatLogger.logMsg(c::MegaChatApi::LOG_LEVEL_INFO, message);
        if (report == ReportResult)
        {
            conlock(cout) << message << endl;
        }
        return true;
    }
    else
    {
        const std::string message = opName + " failed. Error: " + string{e->getErrorString()};
        g_chatLogger.logMsg(c::MegaChatApi::LOG_LEVEL_ERROR, message);
        if (report != NoConsoleReport)
        {
            conlock(cout) << message << endl;
        }
        return false;
    }
}

string OwnStr(const char* s)
{
    // takes ownership of a string from MegaApi, prevents leaks
    string str(s ? s : "");
    delete[] s;
    return str;
}

string base64NodeHandle(m::MegaHandle h)
{
    if (h == m::INVALID_HANDLE) return "INVALID_HANDLE";
    return OwnStr(m::MegaApi::handleToBase64(h));
}

unique_ptr<m::Console> console;

static const char* prompts[] =
{
    "", "MEGAclc> ", "Password:", "Pin:"
};

enum prompttype
{
    NOPROMPT, COMMAND, LOGINPASSWORD, PIN
};

static prompttype prompt = COMMAND;

#if defined(WIN32) && defined(NO_READLINE)
static char pw_buf[512];  // double space for unicode
#define strdup _strdup
#else
static char pw_buf[256];  
#endif

static int pw_buf_pos;

// lock this for output since we are using cout on multiple threads
std::mutex g_outputMutex;

// console input line to process
static char* line = NULL;

static void setprompt(prompttype p)
{
    auto cl = conlock(cout);

    prompt = p;

    if (p == COMMAND)
    {
        console->setecho(true);
        line = strdup("");  // causes main loop to iterate and update the prompt
    }
    else
    {
        pw_buf_pos = 0;
#if defined(WIN32) && defined(NO_READLINE)
        static_cast<m::WinConsole*>(console.get())->updateInputPrompt(prompts[p]);
#else
        cout << prompts[p] << flush;
#endif
        console->setecho(false);
    }
}

// readline callback - exit if EOF, add to history unless password
#if !defined(WIN32) || !defined(NO_READLINE)
static void store_line(char* l)
{
    if (!l)
    {
        console.reset();
        exit(0);
    }

#ifndef NO_READLINE
    if (*l && prompt == COMMAND)
    {
        add_history(l);
    }
#endif

    line = l;
}
#endif

void reviewPublicChatLoadMessages(const c::MegaChatHandle chatid);

struct CLCRoomListener : public c::MegaChatRoomListener
{
    c::MegaChatHandle room = c::MEGACHAT_INVALID_HANDLE;

    void onChatRoomUpdate(c::MegaChatApi*, c::MegaChatRoom *chat) override;

    void onMessageLoaded(c::MegaChatApi*, c::MegaChatMessage *msg) override;

    void onMessageReceived(c::MegaChatApi*, c::MegaChatMessage *) override;

    void onMessageUpdate(c::MegaChatApi*, c::MegaChatMessage *msg) override;

    void onHistoryReloaded(c::MegaChatApi*, c::MegaChatRoom *chat) override;

    void onHistoryTruncatedByRetentionTime(c::MegaChatApi*, c::MegaChatMessage *msg) override;
};

struct RoomListenerRecord
{
    bool open = false;
    unique_ptr<CLCRoomListener> listener;
    RoomListenerRecord();
};
map<c::MegaChatHandle, RoomListenerRecord> g_roomListeners;

struct CLCListener : public c::MegaChatListener
{
    void onChatInitStateUpdate(c::MegaChatApi*, int newState) override
    {
        std::string message = "Status update : ";
        switch (newState)
        {
            case c::MegaChatApi::INIT_ERROR:
            {
                message += "INIT_ERROR";
                g_chatLogger.logMsg(c::MegaChatApi::LOG_LEVEL_ERROR, message);
                break;
            }
            case c::MegaChatApi::INIT_WAITING_NEW_SESSION:
            {
                message += "INIT_WAITING_NEW_SESSION";
                g_chatLogger.logMsg(c::MegaChatApi::LOG_LEVEL_INFO, message);
                break;
            }
            case c::MegaChatApi::INIT_OFFLINE_SESSION:
            {
                message += "INIT_OFFLINE_SESSION";
                g_chatLogger.logMsg(c::MegaChatApi::LOG_LEVEL_INFO, message);
                break;
            }
            case c::MegaChatApi::INIT_ONLINE_SESSION:
            {
                message += "INIT_ONLINE_SESSION";
                g_chatLogger.logMsg(c::MegaChatApi::LOG_LEVEL_INFO, message);
                break;
            }
            default:
            {
                message += "INIT_ERROR";
                g_chatLogger.logMsg(c::MegaChatApi::LOG_LEVEL_ERROR, message);
                break;
            }
        }
    }

    void onChatConnectionStateUpdate(c::MegaChatApi* api, c::MegaChatHandle chatid, int newState) override
    {
        if (newState != c::MegaChatApi::CHAT_CONNECTION_ONLINE
                || !g_reviewingPublicChat
                || chatid != g_reviewPublicChatid
                || g_reviewPublicChatMsgCountRemaining == 0)
        {
            return;
        }

        // Load all user attributes with loadUserAttributes
        if (!g_startedPublicChatReview)
        {
            g_startedPublicChatReview = true;

            std::unique_ptr<c::MegaChatRoom> chatRoom(api->getChatRoom(chatid));
            unsigned int numParticipants = chatRoom->getPeerCount();

            unique_ptr<m::MegaHandleList> peerList = unique_ptr<m::MegaHandleList>(m::MegaHandleList::createInstance());
            for (unsigned int i = 0; i < numParticipants; i++)
            {
                peerList->addMegaHandle(chatRoom->getPeerHandle(i));
            }

            auto allEmailsReceived = new OneShotChatRequestListener;
            allEmailsReceived->onRequestFinishFunc =
            [numParticipants, chatid](c::MegaChatApi* api, c::MegaChatRequest * , c::MegaChatError* e)
            {
                std::unique_ptr<c::MegaChatRoom> chatRoom(api->getChatRoom(chatid));
                std::ostringstream os;
                os << "\n\t\t------------------ Load Particpants --------------------\n\n";
                for (unsigned int i = 0; i < numParticipants; i++)
                {
                    c::MegaChatHandle peerHandle = chatRoom->getPeerHandle(i);
                    std::unique_ptr<const char[]> email = std::unique_ptr<const char[]>(api->getUserEmailFromCache(peerHandle));
                    std::unique_ptr<const char[]> fullname = std::unique_ptr<const char[]>(api->getUserFullnameFromCache(peerHandle));
                    std::unique_ptr<const char[]> handleBase64 = std::unique_ptr<const char[]>(m::MegaApi::userHandleToBase64(peerHandle));
                    os << "\tParticipant: " << handleBase64.get() << "\tEmail: " << (email.get() ? email.get() : "No email") << "\t\t\tName: " << (fullname.get() ? fullname.get() : "No name") << "\n";
                }

                os << "\n\n\t\t------------------ Load Messages ----------------------\n\n";
                const auto msg = os.str();
                conlock(cout) << msg;
                conlock(*g_reviewPublicChatOutFile) << msg << flush;
                g_chatLogger.logMsg(c::MegaChatApi::LOG_LEVEL_INFO, msg);

                // Access to g_roomListeners is safe because no other thread accesses this map
                // while the Mega Chat API thread is using it here.
                auto& rec = g_roomListeners[chatid];
                assert(!rec.open);
                if (!api->openChatRoom(chatid, rec.listener.get()))
                {
                    g_chatLogger.logMsg(c::MegaChatApi::LOG_LEVEL_ERROR, "Failed to open chat room");
                    g_roomListeners.erase(chatid);
                    *g_reviewPublicChatOutFile << "Error: Failed to open chat room." << endl;
                }
                else
                {
                    rec.listener->room = chatid;
                    rec.open = true;
                }

                if (api->getChatConnectionState(chatid) == c::MegaChatApi::CHAT_CONNECTION_ONLINE)
                {
                    g_reportMessagesDeveloper = false;
                    reviewPublicChatLoadMessages(chatid);
                }
            };

            api->loadUserAttributes(chatid, peerList.get(), allEmailsReceived);
        }
        else
        {
            reviewPublicChatLoadMessages(chatid);
        }
    }
};

struct finishInfo
{
    c::MegaChatApi* api;
    c::MegaChatRequest *request;
    c::MegaChatError* e;
};

struct MegaclChatListener : public c::MegaChatRequestListener
{
    mutex m;
    map<int, function<void(finishInfo&)>> finishFn;

public:

    void onFinish(int n, function<void(finishInfo&)> f)
    {
        lock_guard<mutex> g(m);
        finishFn[n] = f;
    }

    void onRequestStart(c::MegaChatApi*, c::MegaChatRequest *) override
    {
    }

    void onRequestFinish(c::MegaChatApi* api, c::MegaChatRequest *request, c::MegaChatError* e) override
    {
        assert(request && e);
        function<void(finishInfo&)> f;
        {
            lock_guard<mutex> g(m);
            auto i = finishFn.find(request->getType());
            if (i != finishFn.end())
                f = i->second;
        }
        if (f)
        {
            finishInfo fi{ api, request, e };
            f(fi);
        }
        else
        {
            switch (request->getType())
            {
            case c::MegaChatRequest::TYPE_CONNECT:
                if (check_err("Connect", e))
                {
                    conlock(cout) << "Connection state " << api->getConnectionState() << endl;
                }
                break;

            case c::MegaChatRequest::TYPE_SET_ONLINE_STATUS:
                if (check_err("SetChatStatus", e))
                {
                }
                break;

            case c::MegaChatRequest::TYPE_SET_PRESENCE_AUTOAWAY:
                if (check_err("SetAutoAway", e))
                {
                    conlock(cout) << " autoaway: " << request->getFlag() << " timeout: " << request->getNumber() << endl;
                }
                break;

            case c::MegaChatRequest::TYPE_SET_PRESENCE_PERSIST:
                if (check_err("SetPresencePersist", e))
                {
                    conlock(cout) << " persist: " << request->getFlag() << endl;
                }
                break;

            case c::MegaChatRequest::TYPE_SET_BACKGROUND_STATUS:
                if (check_err("SetBackgroundStatus", e))
                {
                    conlock(cout) << " background: " << request->getFlag() << endl;
                }
                break;

            case c::MegaChatRequest::TYPE_LOAD_PREVIEW:
                if (check_err("OpenChatPreview", e))
                {
                    conlock(cout) << "openchatpreview: chatlink loaded. Chatid: " << k::Id(request->getChatHandle()).toString() << endl;
                }
                break;
            }
        }
    }

    void onRequestUpdate(c::MegaChatApi*, c::MegaChatRequest *) override
    {
    }

    void onRequestTemporaryError(c::MegaChatApi *, c::MegaChatRequest *, c::MegaChatError*) override
    {
    }
};


class MegaclcListener : public m::MegaListener
{
public:

    void onRequestStart(m::MegaApi* , m::MegaRequest *) override
    {
    }

    void onRequestFinish(m::MegaApi* api, m::MegaRequest *request, m::MegaError* e) override;

    virtual void onRequestUpdate(m::MegaApi*, m::MegaRequest *) override
    {
    }

    void onRequestTemporaryError(m::MegaApi *, m::MegaRequest *, m::MegaError* ) override
    {
    }

    void onTransferStart(m::MegaApi *, m::MegaTransfer *) override
    {
    }

    void onTransferFinish(m::MegaApi* , m::MegaTransfer *, m::MegaError* ) override
    {
    }

    void onTransferUpdate(m::MegaApi *, m::MegaTransfer *) override
    {
    }

    void onTransferTemporaryError(m::MegaApi *, m::MegaTransfer *, m::MegaError* ) override
    {
    }

    void onUsersUpdate(m::MegaApi* , m::MegaUserList *users) override
    {
        conlock(cout) << "User list updated:  " << (users ? users->size() : -1) << endl;
        if (users)
        {
            for (int i = 0; i < users->size(); ++i)
            {
                if (m::MegaUser* m = users->get(i))
                {
                    auto changebits = m->getChanges();
                    if (changebits)
                    {
                        auto cl = conlock(cout);
                        cout << "user " << ch_s(m->getHandle()) << " changes:";
                        if (changebits & m::MegaUser::CHANGE_TYPE_AUTHRING) cout << " AUTHRING";
                        if (changebits & m::MegaUser::CHANGE_TYPE_LSTINT) cout << " LSTINT";
                        if (changebits & m::MegaUser::CHANGE_TYPE_AVATAR) cout << " AVATAR";
                        if (changebits & m::MegaUser::CHANGE_TYPE_FIRSTNAME) cout << " FIRSTNAME";
                        if (changebits & m::MegaUser::CHANGE_TYPE_LASTNAME) cout << " LASTNAME";
                        if (changebits & m::MegaUser::CHANGE_TYPE_EMAIL) cout << " EMAIL";
                        if (changebits & m::MegaUser::CHANGE_TYPE_KEYRING) cout << " KEYRING";
                        if (changebits & m::MegaUser::CHANGE_TYPE_COUNTRY) cout << " COUNTRY";
                        if (changebits & m::MegaUser::CHANGE_TYPE_BIRTHDAY) cout << " BIRTHDAY";
                        if (changebits & m::MegaUser::CHANGE_TYPE_PUBKEY_CU255) cout << " PUBKEY_CU255";
                        if (changebits & m::MegaUser::CHANGE_TYPE_PUBKEY_ED255) cout << " PUBKEY_ED255";
                        if (changebits & m::MegaUser::CHANGE_TYPE_SIG_PUBKEY_RSA) cout << " SIG_PUBKEY_RSA";
                        if (changebits & m::MegaUser::CHANGE_TYPE_SIG_PUBKEY_CU255) cout << " SIG_PUBKEY_CU255";
                        if (changebits & m::MegaUser::CHANGE_TYPE_LANGUAGE) cout << " LANGUAGE";
                        if (changebits & m::MegaUser::CHANGE_TYPE_PWD_REMINDER) cout << " PWD_REMINDER";
                        if (changebits & m::MegaUser::CHANGE_TYPE_DISABLE_VERSIONS) cout << " DISABLE_VERSIONS";
                        cout << endl;
                    }
                }
            }
        }
    }

    void onNodesUpdate(m::MegaApi* , m::MegaNodeList *nodes) override
    {
        //conlock(cout) << "Node list updated:  " << (nodes ? nodes->size() : -1) << endl;
    }

    void onAccountUpdate(m::MegaApi *) override
    {
        conlock(cout) << "Account updated" << endl;
    }

    void onContactRequestsUpdate(m::MegaApi*, m::MegaContactRequestList* requests) override
    {
        conlock(cout) << "Contact requests list updated:  " << (requests ? requests->size() : -1) << endl;
    }

    void onReloadNeeded(m::MegaApi* api) override
    {
        {
            conlock(cout) << "Reload needed!  Submitting fetchNodes request" << endl;
        }
        api->fetchNodes();
    }

#ifdef ENABLE_SYNC
    void onSyncFileStateChanged(m::MegaApi *, m::MegaSync *, string *, int ) override
    {
    }

    void onSyncEvent(m::MegaApi *, m::MegaSync *, m::MegaSyncEvent *) override
    {
    }

    void onSyncStateChanged(m::MegaApi*, m::MegaSync*) override
    {
    }

    void onGlobalSyncStateChanged(m::MegaApi*) override
    {
        conlock(cout) << "Sync state changed";
    }
#endif

    void onChatsUpdate(m::MegaApi*, m::MegaTextChatList *chats) override
    {
        conlock(cout) << "Chats updated:  " << (chats ? chats->size() : -1) << endl;
    }

    void onEvent(m::MegaApi*, m::MegaEvent *e) override
    {
        if (e)
        {
            LOG_info << "Event: " << e->getEventString();
            LOG_info << "\tText: " << (e->getText() ? e->getText() : "(null)");
            LOG_info << "\tNumber: " << std::to_string(e->getNumber());
            LOG_info << "\tHandle: " << m::Base64Str<sizeof(m::MegaHandle)>(e->getHandle());
        }
        else
        {
            assert(false);
        }
    }
};


class ClcMegaGlobalListener : public m::MegaGlobalListener
{
public:
    void onUsersUpdate(m::MegaApi* api, m::MegaUserList *users) override {}

    void onUserAlertsUpdate(m::MegaApi* api, m::MegaUserAlertList *alerts) override {}

    void onNodesUpdate(m::MegaApi* api, m::MegaNodeList *nodes) override {}

    void onAccountUpdate(m::MegaApi *api) override {}

    void onContactRequestsUpdate(m::MegaApi* api, m::MegaContactRequestList* requests) override {}

    void onReloadNeeded(m::MegaApi* api) override {}

#ifdef ENABLE_SYNC
    void onGlobalSyncStateChanged(m::MegaApi* api) override {}
#endif

    void onChatsUpdate(m::MegaApi* api, m::MegaTextChatList *chats) override {}

    void onEvent(m::MegaApi* api, m::MegaEvent *event) override {}
};

CLCListener g_clcListener;
MegaclcListener g_megaclcListener;
MegaclChatListener g_chatListener;
ClcMegaGlobalListener g_globalListener;
unique_ptr<m::MegaApi> g_megaApi;
unique_ptr<c::MegaChatApi> g_chatApi;


void MegaclcListener::onRequestFinish(m::MegaApi* api, m::MegaRequest *request, m::MegaError* e)
{
    std::unique_lock<std::mutex> guard(g_outputMutex);
    switch (request->getType())
    {
    case m::MegaRequest::TYPE_LOGIN:
        if (check_err("Login", e))
        {
            conlock(cout) << "Loading Account with fetchNodes..." << endl;
            guard.unlock();
            api->fetchNodes();
            setprompt(NOPROMPT);
        }
        else if (e->getErrorCode() == m::MegaError::API_EMFAREQUIRED)
        {
            guard.unlock();
            setprompt(PIN);
        }
        else
        {
            guard.unlock();
            setprompt(COMMAND);
        }
        break;

    case m::MegaRequest::TYPE_FETCH_NODES:
        if (check_err("FetchNodes", e))
        {
            conlock(cout) << "Connecting to chat servers" << endl;
            guard.unlock();
            g_chatApi->connect(&g_chatListener);

            setprompt(COMMAND);
        }
        break;

    case m::MegaRequest::TYPE_LOGOUT:
        if (!check_err("Logout", e))
        {
            conlock(cout) << "Error in logout: "<< e->getErrorString() << endl;
        }

        guard.unlock();
        setprompt(COMMAND);

    default:
        break;
    }
}

bool oneOpenRoom(c::MegaChatHandle room);
void reviewPublicChatLoadMessages(const c::MegaChatHandle chatid);

std::string timeToStringUTC(const int64_t time)
{
    struct tm dt;
    m::m_gmtime(time, &dt);
    char buffer[40];
    std::strftime(buffer, 40, "%FT%H-%M-%S", &dt);
    return std::string{buffer};
}

std::string msgTypeToString(const int msgType)
{
    switch (msgType)
    {
        case c::MegaChatMessage::TYPE_UNKNOWN: return "TYPE_UNKNOWN";
        case c::MegaChatMessage::TYPE_INVALID: return "TYPE_INVALID";
        case c::MegaChatMessage::TYPE_NORMAL: return "TYPE_NORMAL";
        case c::MegaChatMessage::TYPE_ALTER_PARTICIPANTS: return "TYPE_ALTER_PARTICIPANTS";
        case c::MegaChatMessage::TYPE_TRUNCATE: return "TYPE_TRUNCATE";
        case c::MegaChatMessage::TYPE_PRIV_CHANGE: return "TYPE_PRIV_CHANGE";
        case c::MegaChatMessage::TYPE_CHAT_TITLE: return "TYPE_CHAT_TITLE";
        case c::MegaChatMessage::TYPE_CALL_ENDED: return "TYPE_CALL_ENDED";
        case c::MegaChatMessage::TYPE_CALL_STARTED: return "TYPE_CALL_STARTED";
        case c::MegaChatMessage::TYPE_PUBLIC_HANDLE_CREATE: return "TYPE_PUBLIC_HANDLE_CREATE";
        case c::MegaChatMessage::TYPE_PUBLIC_HANDLE_DELETE: return "TYPE_PUBLIC_HANDLE_DELETE";
        case c::MegaChatMessage::TYPE_SET_PRIVATE_MODE: return "TYPE_SET_PRIVATE_MODE";
        case c::MegaChatMessage::TYPE_SET_RETENTION_TIME: return "TYPE_SET_RETENTION_TIME";
        case c::MegaChatMessage::TYPE_NODE_ATTACHMENT: return "TYPE_NODE_ATTACHMENT";
        case c::MegaChatMessage::TYPE_REVOKE_NODE_ATTACHMENT: return "TYPE_REVOKE_NODE_ATTACHMENT";
        case c::MegaChatMessage::TYPE_CONTACT_ATTACHMENT: return "TYPE_CONTACT_ATTACHMENT";
        case c::MegaChatMessage::TYPE_CONTAINS_META: return "TYPE_CONTAINS_META";
        case c::MegaChatMessage::TYPE_VOICE_CLIP: return "TYPE_VOICE_CLIP";
        default: assert(false); return "Invalid Msg Type (" + std::to_string(msgType) + ")";
    }
    return {};
}

std::string msgStatusToString(const int msgStatus)
{
    switch (msgStatus)
    {
        case c::MegaChatMessage::STATUS_UNKNOWN: return "STATUS_UNKNOWN";
        case c::MegaChatMessage::STATUS_SENDING: return "STATUS_SENDING";
        case c::MegaChatMessage::STATUS_SENDING_MANUAL: return "STATUS_SENDING_MANUAL";
        case c::MegaChatMessage::STATUS_SERVER_RECEIVED: return "STATUS_SERVER_RECEIVED";
        case c::MegaChatMessage::STATUS_SERVER_REJECTED: return "STATUS_SERVER_REJECTED";
        case c::MegaChatMessage::STATUS_DELIVERED: return "STATUS_DELIVERED";
        case c::MegaChatMessage::STATUS_NOT_SEEN: return "STATUS_NOT_SEEN";
        case c::MegaChatMessage::STATUS_SEEN: return "STATUS_SEEN";
        default: assert(false); return "Invalid Msg Status (" + std::to_string(msgStatus) + ")";
    }
    return {};
}

std::string callTermCodeToString(const int termCode)
{
    switch (termCode)
    {
        case c::MegaChatMessage::END_CALL_REASON_ENDED: return "END_CALL_REASON_ENDED";
        case c::MegaChatMessage::END_CALL_REASON_REJECTED: return "END_CALL_REASON_REJECTED";
        case c::MegaChatMessage::END_CALL_REASON_NO_ANSWER: return "END_CALL_REASON_NO_ANSWER";
        case c::MegaChatMessage::END_CALL_REASON_FAILED: return "END_CALL_REASON_FAILED";
        case c::MegaChatMessage::END_CALL_REASON_CANCELLED: return "END_CALL_REASON_CANCELLED";
        default: assert(false); return "Invalid Call Term Code (" + std::to_string(termCode) + ")";
    }
    return {};
}

void reportMessageHuman(c::MegaChatHandle chatid, c::MegaChatMessage *msg, const char* loadorreceive)
{
    if (!msg)
    {
        cout << "Room " << ch_s(chatid) << " - end of " << loadorreceive << " messages" << endl;
        if (g_reviewingPublicChat && g_reviewPublicChatMsgCountRemaining)
        {
            reviewPublicChatLoadMessages(chatid);
        }
        else
        {
            std::string message = "Loaded all messages requested: " + std::to_string(g_reviewPublicChatMsgCount);
            conlock(cout) << message << flush;
            if (g_reviewPublicChatOutFile)
            {
                conlock(*g_reviewPublicChatOutFile) << message << flush;
            }
        }
        return;
    }

    if (g_reviewingPublicChat && g_reviewPublicChatMsgCountRemaining > 0)
    {
        --g_reviewPublicChatMsgCountRemaining;
    }

    g_reviewPublicChatMsgCount ++;

    const c::MegaChatRoom* room = g_chatApi->getChatRoom(chatid);
    const std::string room_title = room ? room->getTitle() : "<No Title>";

    auto firstname = [room](const c::MegaChatHandle handle) -> std::string
    {
        std::unique_ptr<const char []> firstnamePtr(room ? g_chatApi->getUserFirstnameFromCache(handle) : nullptr);
        if (firstnamePtr)
        {
            return firstnamePtr.get();
        }

        return std::string{"<No Firstname>"};
    };

    auto lastname = [room](const c::MegaChatHandle handle) -> std::string
    {
        std::unique_ptr<const char []> lastnamePtr(room ? g_chatApi->getUserLastnameFromCache(handle) : nullptr);
        if (lastnamePtr)
        {
            return lastnamePtr.get();
        }

        return std::string{"<No Lastname>"};
    };

    auto email = [room](const c::MegaChatHandle handle) -> std::string
    {
        std::unique_ptr<const char []> emailPtr(room ? g_chatApi->getUserEmailFromCache(handle) : nullptr);
        if (emailPtr)
        {
            return emailPtr.get();
        }

        return std::string{"<No Email>"};
    };

    auto nodeinfo = [](m::MegaNodeList* list)
    {
        if (!list || list->size() == 0)
        {
            return std::string{"<No Attachement>"};
        }
        std::stringstream ss;
        for (int i = 0; i < list->size(); ++i)
        {
            const auto node = list->get(i);
            ss << node->getName() << "(" << node->getSize() << ")";
            if (i + 1 < list->size())
            {
                ss << ", ";
            }
        }
        return ss.str();
    };

    auto metainfo = [](const c::MegaChatContainsMeta* meta)
    {
        if (!meta || !meta->getTextMessage())
        {
            return "<No Meta>";
        }
        return meta->getTextMessage();
    };

    auto callinfo = [](const int msgType, const int duration, const int termCode)
    {
        if (msgType != c::MegaChatMessage::TYPE_CALL_ENDED)
        {
            return std::string{"<Not an ending call>"};
        }
        return "Call ended: " + callTermCodeToString(termCode) + " - " + std::to_string(duration);
    };

    std::ostringstream os;
    os << room_title
       << " | " << timeToStringUTC(msg->getTimestamp()) << " UTC"
       << " | " << msgTypeToString(msg->getType())
       << " | " << ch_s(msg->getMsgId())
       << " | " << ch_s(msg->getHandleOfAction())
       << " | " << ch_s(msg->getUserHandle())
       << " | " << (msg->hasConfirmedReactions() ? "reacted to" : "not reacted to")
       << " | " << (msg->isEdited() ? "edited" : "not edited")
       << " | " << (msg->isDeleted() ? "deleted" : "not deleted")
       << " | " << nodeinfo(msg->getMegaNodeList())
       << " | " << metainfo(msg->getContainsMeta())
       << " | " << callinfo(msg->getType(), msg->getDuration(), msg->getTermCode())
       << " | " << firstname(msg->getUserHandle())
       << " | " << lastname(msg->getUserHandle())
       << " | " << email(msg->getUserHandle())
       << " | " << (msg->getContent() ? msg->getContent() : "<No Content>")
       << endl;
    const auto outMsg = os.str();

    if (g_reviewPublicChatOutFileLinks && msg->getContent())
    {
        const auto subChatLink = extractChatLink(msg->getContent());
        if (!subChatLink.empty())
        {
            conlock(*g_reviewPublicChatOutFileLinks) << outMsg << flush;
        }
    }

    conlock(cout) << outMsg;
    if (g_reviewPublicChatOutFile)
    {
        conlock(*g_reviewPublicChatOutFile) << outMsg << flush;
    }
}

void reportMessage(c::MegaChatHandle chatid, c::MegaChatMessage *msg, const char* loadorreceive)
{
    if (!g_reportMessagesDeveloper)
    {
        reportMessageHuman(chatid, msg, loadorreceive);
        return;
    }

    auto cl = conlock(cout);

    if (!msg)
    {
        cout << "Room " << ch_s(chatid) << " - end of " << loadorreceive << " messages" << endl;
        return;
    }

    if (!g_detailHigh && msg->getType() == c::MegaChatMessage::TYPE_NORMAL && msg->getContent())
    {
        cout << ch_s(msg->getUserHandle());
        if (!oneOpenRoom(chatid))
        {
            cout << " (room " << ch_s(chatid) << ")";
        }
        cout << ": " << msg->getContent() << endl;
        return;
    }

    cout << "Room " << ch_s(chatid) << " " << loadorreceive << " message " << msg->getMsgIndex() << " from " << ch_s(msg->getUserHandle()) << " type: ";

    cout << msgTypeToString(msg->getType());

    if (msg->getMsgId() != c::MEGACHAT_INVALID_HANDLE)
    {
        cout << " id " << ch_s(msg->getMsgId());
    }

    if (msg->getTempId() != c::MEGACHAT_INVALID_HANDLE)
    {
        cout << " tempid " << ch_s(msg->getTempId());
    }

    if (msg->getRowId() != c::MEGACHAT_INVALID_HANDLE)
    {
        cout << " (manual row id " << ch_s(msg->getRowId()) << ")";
    }
    if (msg->getChanges() != 0)
    {
        cout << " (change flags: " << msg->getChanges()
            << (msg->hasChanged(c::MegaChatMessage::CHANGE_TYPE_STATUS) ? " status" : "")
            << (msg->hasChanged(c::MegaChatMessage::CHANGE_TYPE_CONTENT) ? " content" : "")
            << (msg->hasChanged(c::MegaChatMessage::CHANGE_TYPE_ACCESS) ? " access" : "")
            << ")";
    }

    cout << endl << "content: '" << (msg->getContent() ? msg->getContent() : "<Null>")
        << "' status: " << msg->getStatus() << " timestamp " << msg->getTimestamp()
        << (msg->isEdited() ? " (edited)" : "")
        << (msg->isDeleted() ? " (deleted)" : "")
        << (msg->isEditable() ? " (editable)" : "")
        << (msg->isDeletable() ? " (deletable)" : "");

    if (msg->isManagementMessage())
    {
        cout << " (management, user " << ch_s(msg->getHandleOfAction()) << " privilege " << c::MegaChatRoom::privToString(msg->getPrivilege()) << ")";
    }

    if (msg->getCode() != 0)
    {
        cout << " (reason: ";
        switch (msg->getCode())
        {
        case c::MegaChatMessage::REASON_PEERS_CHANGED: cout << "REASON_PEERS_CHANGED"; break;
        case c::MegaChatMessage::REASON_TOO_OLD: cout << "REASON_TOO_OLD"; break;
        case c::MegaChatMessage::REASON_GENERAL_REJECT: cout << "REASON_GENERAL_REJECT"; break;
        case c::MegaChatMessage::REASON_NO_WRITE_ACCESS: cout << "REASON_NO_WRITE_ACCESS"; break;
        case c::MegaChatMessage::REASON_NO_CHANGES: cout << "REASON_NO_CHANGES"; break;
        default: cout << msg->getCode();
        }
        cout << ")";
    }

    if (msg->getUsersCount() > 0)
    {
        cout << " (attached users: " << msg->getUsersCount() << ")";
    }
    cout << endl;
}

bool oneOpenRoom(c::MegaChatHandle room)
{
    return g_roomListeners.size() == 1 && g_roomListeners.begin()->first == room;
}

void reviewPublicChatLoadMessages(const c::MegaChatHandle chatid)
{
    int source;
    if (g_chatApi->isFullHistoryLoaded(chatid))
    {
        source = c::MegaChatApi::SOURCE_NONE;
    }
    else
    {
        int numberOfMessages = g_reviewPublicChatMsgCountRemaining.load() > 0 ? g_reviewPublicChatMsgCountRemaining.load() : MAX_NUMBER_MESSAGES;
        source = g_chatApi->loadMessages(chatid, numberOfMessages);
    }

    auto cl = conlock(cout);
    switch (source)
    {
        case c::MegaChatApi::SOURCE_ERROR:
        {
            cout << "Loading messages..." << endl;
            break;
        }
        case c::MegaChatApi::SOURCE_NONE:
        {
            std::string message = "No more messages. Message loaded: " + std::to_string(g_reviewPublicChatMsgCount);
            conlock(cout) << message << flush;
            if (g_reviewPublicChatOutFile)
            {
                conlock(*g_reviewPublicChatOutFile) << message << flush;
            }

            g_reviewingPublicChat = false;
            g_reviewPublicChatMsgCountRemaining = 0;
            g_reviewPublicChatMsgCount = 0;
            g_startedPublicChatReview = false;
            return;
        }
        default: return;
    }
}

bool extractflag(const string& flag, vector<ac::ACState::quoted_word>& words)
{
    for (auto i = words.begin(); i != words.end(); ++i)
    {
        if (i->s == flag && !i->q.quoted)
        {
            words.erase(i);
            return true;
        }
    }
    return false;
}

bool extractflagparam(const string& flag, string& param, vector<ac::ACState::quoted_word>& words)
{
    for (auto i = words.begin(); i != words.end(); ++i)
    {
        if (i->s == flag)
        {
            auto j = i;
            ++j;
            if (j != words.end())
            {
                param = j->s;
                words.erase(i, ++j);
                return true;
            }
        }
    }
    return false;
}

unique_ptr<m::MegaNode> GetNodeByPath(const string& path)
{
    if (path.find("//handle/") == 0)
    {
        m::MegaHandle h = g_megaApi->base64ToHandle(path.c_str() + 9);
        unique_ptr<m::MegaNode> node(g_megaApi->getNodeByHandle(h));
        if (!node)
        {
            conlock(cout) << "No node found by looking up handle: '" << (path.c_str() + 9) << "'" << endl;
        }
        return node;
    }

    unique_ptr<m::MegaNode> node(g_megaApi->getNodeByPath(path.c_str()));
    if (!node)
    {
        conlock(cout) << "No node found at path: '" << path << "'" << endl;
    }
    return node;
}

static bool quit_flag = false;
static string login;
static string password;

void exec_initanonymous(ac::ACState& s)
{
    if (g_chatApi->getInitState() == c::MegaChatApi::INIT_NOT_DONE)
    {
        g_chatApi->initAnonymous();
        g_chatApi->connect(&g_chatListener);
    }
    else
    {
        conlock(cout) << "Already initialized. Please log out first." << endl;
    }
}

void exec_login(ac::ACState& s)
{
    if (g_chatApi->getInitState() == c::MegaChatApi::INIT_NOT_DONE)
    {
        bool hasemail = s.words[1].s.find_first_of('@') != string::npos;
        if (s.words.size() == 3 && hasemail)
        {
            // full account login
            {
                conlock(cout) << "Initiating login attempt..." << endl;
            }
            g_chatApi->init(NULL);
            login = s.words[1].s;
            password = s.words[2].s;

            // Block prompt until the request has finished
            setprompt(NOPROMPT);
            g_megaApi->login(login.c_str(), password.c_str());
        }
        else if (s.words.size() == 2 && hasemail)
        {
            login = s.words[1].s;
            setprompt(LOGINPASSWORD);
        }
        else if ((s.words.size() == 2) || (s.words.size() == 3 && !hasemail && s.words[1].s == "autoresume"))
        {
            string session, filename = "mega_autoresume_session" + (s.words.size() == 3 ? "_" + s.words[2].s : "");
            ifstream file(filename.c_str());
            file >> session;
            if (file.is_open() && session.size())
            {
                conlock(cout) << "Resuming session..." << endl;
                g_chatApi->init(session.c_str());
                return g_megaApi->fastLogin(session.c_str());
            }
            conlock(cout) << "Failed to get a valid session id from file " << filename << endl;
        }
        else if (s.words.size() == 2 && s.words[1].s.size() < 64 * 4 / 3)
        {
            {
                conlock(cout) << "Resuming session..." << endl;
            }
            g_chatApi->init(s.words[1].s.c_str());
            g_megaApi->fastLogin(s.words[1].s.c_str());
        }
        else
        {
            conlock(cout) << s.selectedSyntax << endl;
        }
    }
    else
    {
        conlock(cout) << "Already logged in. Please log out first." << endl;
    }
}

void exec_logout(ac::ACState& s)
{
    unique_ptr<const char[]>session(g_megaApi->dumpSession());
    if (g_chatApi->getInitState() == c::MegaChatApi::INIT_ANONYMOUS)
    {
        g_chatApi->logout();
    }
    else if (g_chatApi->getInitState() != c::MegaChatApi::INIT_NOT_DONE)
    {
        setprompt(NOPROMPT);
        g_megaApi->logout();
    }
    else
    {
        conlock(cout) << "Not logged in." << endl;
    }
}

void exec_session(ac::ACState& s)
{
    unique_ptr<const char[]>session(g_megaApi->dumpSession());
    if (session)
    {
        if (s.words.size() > 1 && s.words[1].s == "autoresume")
        {
            string filename = "mega_autoresume_session" + (s.words.size() == 3 ? "_" + s.words[2].s : "");
            ofstream file(filename.c_str());
            if (file.fail() || !file.is_open())
            {
                conlock(cout) << "could not open file: " << filename << endl;
            }
            else
            {
                file << session.get();
                conlock(cout) << "Your (secret) session is saved in file '" << filename << "'" << endl;
            }
        }
        else
        {
            conlock(cout) << "Your (secret) session is: " << session.get() << endl;
        }
    }
    else 
    {
        conlock(cout) << "Not logged in." << endl;
    }
}

void exec_setonlinestatus(ac::ACState& s)
{
    assert(s.words.size() == 2);
    int status;
    if (s.words[1].s == "offline")  status = c::MegaChatApi::STATUS_OFFLINE;
    else if (s.words[1].s == "away")  status = c::MegaChatApi::STATUS_AWAY;
    else if (s.words[1].s == "online")  status = c::MegaChatApi::STATUS_ONLINE;
    else if (s.words[1].s == "busy")  status = c::MegaChatApi::STATUS_BUSY;
    else
    {
        conlock(cout) << s.selectedSyntax << endl;
        return;
    }
    g_chatApi->setOnlineStatus(status, &g_chatListener);
}

void exec_setpresenceautoaway(ac::ACState& s)
{
    assert(s.words.size() == 3);
    g_chatApi->setPresenceAutoaway(s.words[1].s == "on", stoi(s.words[2].s), &g_chatListener);
}

void exec_setpresencepersist(ac::ACState& s)
{
    g_chatApi->setPresencePersist(s.words[1].s == "on", &g_chatListener);
}

int g_signalPresencePeriod = 0;
time_t g_signalPresenceLastSent = 0;

int g_repeatPeriod = 5;
time_t g_repeatLastSent = 0;
string g_repeatCommand;

void exec_signalpresenceperiod(ac::ACState& s)
{
    // use this one to call signalPresenceActivity automatically on that period
    g_signalPresencePeriod = stoi(s.words[1].s);
    g_signalPresenceLastSent = 0;
}

void exec_repeat(ac::ACState& s)
{
    // use this one to call signalPresenceActivity automatically on that period
    g_repeatPeriod = stoi(s.words[1].s);
    g_repeatLastSent = 0;
    g_repeatCommand = s.words[2].s;
}



void exec_getonlinestatus(ac::ACState&)
{
    auto cl = conlock(cout);
    switch (g_chatApi->getOnlineStatus())
    {
    case c::MegaChatApi::STATUS_OFFLINE:    cout << "offline" << endl; break;
    case c::MegaChatApi::STATUS_AWAY:       cout << "away" << endl; break;
    case c::MegaChatApi::STATUS_ONLINE:     cout << "online" << endl; break;
    case c::MegaChatApi::STATUS_BUSY:       cout << "busy" << endl; break;
    default:   cout << g_chatApi->getOnlineStatus() << endl; break;
    }
}


void exec_setbackgroundstatus(ac::ACState& s)
{
    g_chatListener.onFinish(c::MegaChatRequest::TYPE_SET_BACKGROUND_STATUS, [](finishInfo& f) 
    {
        if (check_err("SetBackgroundStatus", f.e))
        {
            conlock(cout) << " background: " << f.request->getFlag() << endl;
        }
    });

    g_chatApi->setBackgroundStatus(s.words[1].s == "on", &g_chatListener);
}

void exec_getuserfirstname(ac::ACState& s)
{
    c::MegaChatHandle userhandle(s_ch(s.words[1].s));

    g_chatListener.onFinish(c::MegaChatRequest::TYPE_GET_FIRSTNAME, [userhandle](finishInfo& f)
    {
        if (check_err("getUserFirstname", f.e))
        {
            conlock(cout) << ch_s(userhandle) << " -> " << f.request->getText() << endl;
        }
    });

    g_chatApi->getUserFirstname(userhandle, NULL, &g_chatListener);
}


void exec_getuserlastname(ac::ACState& s)
{
    c::MegaChatHandle userhandle(s_ch(s.words[1].s));

    g_chatListener.onFinish(c::MegaChatRequest::TYPE_GET_LASTNAME, [userhandle](finishInfo& f)
    {
        if (check_err("getUserLastname", f.e))
        {
            conlock(cout) << ch_s(userhandle) << " -> " << f.request->getText() << endl;
        }
    });

    g_chatApi->getUserLastname(userhandle, NULL, &g_chatListener);
}

void exec_getuseremail(ac::ACState& s)
{
    c::MegaChatHandle userhandle(s_ch(s.words[1].s));

    g_chatListener.onFinish(c::MegaChatRequest::TYPE_GET_EMAIL, [userhandle](finishInfo& f)
    {
        if (check_err("getUserEmail", f.e))
        {
            conlock(cout) << ch_s(userhandle) << " -> " << f.request->getText() << endl;
        }
    });

    g_chatApi->getUserEmail(userhandle, &g_chatListener);
}

void exec_getcontactemail(ac::ACState& s)
{
    c::MegaChatHandle userhandle(s_ch(s.words[1].s));
    unique_ptr<char[]> email(g_chatApi->getContactEmail(userhandle));

    conlock(cout) << ch_s(userhandle) << " -> " << (email ? email.get() : "<no contact relationship>") << endl;
}


void exec_getuserhandlebyemail(ac::ACState& s)
{
    c::MegaChatHandle userhandle = g_chatApi->getUserHandleByEmail(s.words[1].s.c_str());

    conlock(cout) << s.words[1].s << " -> " << ch_s(userhandle) << endl;
}

void exec_getmyuserhandle(ac::ACState&)
{
    conlock(cout) << ch_s(g_chatApi->getMyUserHandle()) << endl;
}

void exec_getmyfirstname(ac::ACState&)
{
    unique_ptr<char[]> t(g_chatApi->getMyFirstname());

    conlock(cout) << (t ? t.get() : "<no result>") << endl;
}

void exec_getmylastname(ac::ACState&)
{
    unique_ptr<char[]> t(g_chatApi->getMyLastname());

    conlock(cout) << (t ? t.get() : "<no result>") << endl;
}

void exec_getmyfullname(ac::ACState&)
{
    unique_ptr<char[]> t(g_chatApi->getMyFullname());

    conlock(cout) << (t ? t.get() : "<no result>") << endl;
}

void exec_getmyemail(ac::ACState&)
{
    unique_ptr<char[]> t(g_chatApi->getMyEmail());

    conlock(cout) << (t ? t.get() : "<no result>") << endl;
}

string chatDetails(const c::MegaChatRoom& cr)
{
    ostringstream s;

    s << "title: " << (cr.getTitle() ? cr.getTitle() : "") << " handle: " << ch_s(cr.getChatId()) 
      << " priv:" << cr.privToString(cr.getOwnPrivilege()) << " s:" << (cr.isGroup() ? " isGroup " : " ")
      << "peers: ";
    for (unsigned i = 0; i < cr.getPeerCount(); ++i)
    {
        s << ch_s(cr.getPeerHandle(i)) << " ";
    }
    if (cr.getUnreadCount())
    {
        s << "unread: " << cr.getUnreadCount();
    }
    return s.str();
};


void exec_getchatrooms(ac::ACState&)
{
    unique_ptr<c::MegaChatRoomList> crl(g_chatApi->getChatRooms());
    if (crl)
    {
        auto cl = conlock(cout);
        for (unsigned i = 0; i < crl->size(); ++i)
        {
            if (const c::MegaChatRoom* cr = crl->get(i))
            {
                cout << chatDetails(*cr) << endl;
            }
        }
    }
}


void exec_getchatroom(ac::ACState& s)
{
    c::MegaChatHandle h = s_ch(s.words[1].s);
    unique_ptr<c::MegaChatRoom> p(g_chatApi->getChatRoom(h));

    conlock(cout) << (p ? chatDetails(*p) : "not found") << endl;
}


void exec_getchatroombyuser(ac::ACState& s)
{
    c::MegaChatHandle h = s_ch(s.words[1].s);
    unique_ptr<c::MegaChatRoom> p(g_chatApi->getChatRoomByUser(h));

    conlock(cout) << (p ? chatDetails(*p) : "not found") << endl;
}

string chatlistDetails(const c::MegaChatListItem& cli)
{
    ostringstream s;

    s << "title: " << (cli.getTitle() ? cli.getTitle() : "") 
        << " handle: " << ch_s(cli.getChatId())
        << " priv:" << c::MegaChatRoom::privToString(cli.getOwnPrivilege()) 
        << " " << (cli.isGroup() ? " isGroup " : " ") 
        << " " << (cli.isActive() ? " isActive " : " ");

    if (cli.getPeerHandle() != c::MEGACHAT_INVALID_HANDLE)
    {
        s << "peer: " << ch_s(cli.getPeerHandle());
    }
    if (cli.getUnreadCount())
    {
        s << " unread: " << cli.getUnreadCount();
    }
    if (auto str = cli.getLastMessage())
    {
        s << " last: " << str << " (type " << cli.getLastMessageType() << " from " << ch_s(cli.getLastMessageSender()) << ")";
    }
    return s.str();
};

void exec_getchatlistitems(ac::ACState&)
{
    unique_ptr<c::MegaChatListItemList> clil(g_chatApi->getChatListItems());
    if (clil)
    {
        auto cl = conlock(cout);
        for (unsigned i = 0; i < clil->size(); ++i)
        {
            if (const c::MegaChatListItem* cli = clil->get(i))
            {
                cout << chatlistDetails(*cli) << endl;
            }
        }
    }
}

void exec_getchatlistitem(ac::ACState& s)
{
    c::MegaChatHandle h = s_ch(s.words[1].s);
    unique_ptr<c::MegaChatListItem> p(g_chatApi->getChatListItem(h));

    conlock(cout) << (p ? chatlistDetails(*p) : "not found") << endl;
}

void exec_getunreadchats(ac::ACState&)
{
    conlock(cout) << "unread message count: " << g_chatApi->getUnreadChats() << endl;
}

void exec_getactivechatlistitems(ac::ACState&)
{
    unique_ptr<c::MegaChatListItemList> clil(g_chatApi->getActiveChatListItems());
    if (clil)
    {
        auto cl = conlock(cout);
        for (unsigned i = 0; i < clil->size(); ++i)
        {
            if (const c::MegaChatListItem* cli = clil->get(i))
            {
                cout << chatlistDetails(*cli) << endl;
            }
        }
    }
}

void exec_getinactivechatlistitems(ac::ACState&)
{
    unique_ptr<c::MegaChatListItemList> clil(g_chatApi->getInactiveChatListItems());
    if (clil)
    {
        auto cl = conlock(cout);
        for (unsigned i = 0; i < clil->size(); ++i)
        {
            if (const c::MegaChatListItem* cli = clil->get(i))
            {
                cout << chatlistDetails(*cli) << endl;
            }
        }
    }
}

void exec_getunreadchatlistitems(ac::ACState&)
{
    unique_ptr<c::MegaChatListItemList> clil(g_chatApi->getUnreadChatListItems());
    if (clil)
    {
        auto cl = conlock(cout);
        for (unsigned i = 0; i < clil->size(); ++i)
        {
            if (const c::MegaChatListItem* cli = clil->get(i))
            {
                cout << chatlistDetails(*cli) << endl;
            }
        }
    }
}

void exec_chatinfo(ac::ACState& s)
{
    c::MegaChatHandle chatid = s_ch(s.words[1].s);
    c::MegaChatRoom *room = g_chatApi->getChatRoom(chatid);
    if (room)
    {
        conlock(cout) << room->getPeerCount() << " participants in chat " << s.words[1].s << endl;
        for (unsigned i = 0; i < room->getPeerCount(); i++)
        {
            conlock(cout) << ch_s(room->getPeerHandle(i)) << "\t" << room->getPeerFullname(i);
            if (room->getPeerEmail(i))
            {
                conlock(cout) << " (" << room->getPeerEmail(i) << ")";
            }
            conlock(cout) << "\tPriv: " << c::MegaChatRoom::privToString(room->getPeerPrivilege(i)) << endl;
        }
    }
    else
    {
         conlock(cout) << "Room not found" << endl;
    }
}

void exec_getchathandlebyuser(ac::ACState& s)
{
    c::MegaChatHandle h = s_ch(s.words[1].s);
    c::MegaChatHandle h2 = g_chatApi->getChatHandleByUser(h);

    conlock(cout) << ch_s(h2) << endl;
}

void exec_createchat(ac::ACState& s)
{
    g_chatListener.onFinish(c::MegaChatRequest::TYPE_CREATE_CHATROOM, [](finishInfo& f)
    {
        if (check_err("CreateChat", f.e))
        {
            auto cl = conlock(cout);
            cout << "Chat " << ch_s(f.request->getChatHandle()) << (f.request->getFlag() ? " is a group chat" : " is a permanent chat") << endl;
            auto list = f.request->getMegaChatPeerList();
            for (int i = 0; i < list->size(); ++i)
            {
                cout << "  peer " << ch_s(list->getPeerHandle(i)) << " " << c::MegaChatRoom::privToString(list->getPeerPrivilege(i)) << endl;
            }
        }
    });

    bool group = s.words[1].s == "-group";
    auto pl = c::MegaChatPeerList::createInstance();  
    for (unsigned i = group ? 2 : 1; i < s.words.size(); ++i)
    {
        pl->addPeer(s_ch(s.words[i].s), c::MegaChatPeerList::PRIV_STANDARD); // todo: accept privilege flags
    }
    g_chatApi->createChat(group, pl, &g_chatListener);  
}

void exec_invitetochat(ac::ACState& s)
{
    g_chatListener.onFinish(c::MegaChatRequest::TYPE_INVITE_TO_CHATROOM, [](finishInfo& f)
    {
        if (check_err("InviteToChat", f.e))
        {
            conlock(cout) << "Invited user " << ch_s(f.request->getUserHandle()) << " to chat " << ch_s(f.request->getChatHandle()) << " as " << c::MegaChatRoom::privToString(f.request->getPrivilege()) << endl;
        }
    });

    g_chatApi->inviteToChat(s_ch(s.words[1].s), s_ch(s.words[2].s), c::MegaChatPeerList::PRIV_STANDARD, &g_chatListener);  // todo
}

void exec_removefromchat(ac::ACState& s)
{
    g_chatListener.onFinish(c::MegaChatRequest::TYPE_REMOVE_FROM_CHATROOM, [](finishInfo& f)
    {
        if (check_err("RemoveFromChat", f.e))
        {
            conlock(cout) << "Removed user " << ch_s(f.request->getUserHandle()) << " from chat " << ch_s(f.request->getChatHandle()) << endl;
        }
    });

    g_chatApi->removeFromChat(s_ch(s.words[1].s), s_ch(s.words[2].s), &g_chatListener);  
}

void exec_leavechat(ac::ACState& s)
{
    g_chatListener.onFinish(c::MegaChatRequest::TYPE_REMOVE_FROM_CHATROOM, [](finishInfo& f)
    {
        if (check_err("LeaveChat", f.e))
        {
            conlock(cout) << "Left chat " << ch_s(f.request->getChatHandle()) << " (user " << ch_s(f.request->getUserHandle()) << endl;
        }
    });

    g_chatApi->removeFromChat(s_ch(s.words[1].s), s_ch(s.words[2].s), &g_chatListener);  
}

void exec_updatechatpermissions(ac::ACState& s)
{
    g_chatListener.onFinish(c::MegaChatRequest::TYPE_UPDATE_PEER_PERMISSIONS, [](finishInfo& f)
    {
        if (check_err("UpdateChatPermissions", f.e))
        {
            conlock(cout) << "Updated user " << ch_s(f.request->getUserHandle()) << " in chat " << ch_s(f.request->getChatHandle()) << " to " << c::MegaChatRoom::privToString(f.request->getPrivilege()) << endl;
        }
    });

    g_chatApi->updateChatPermissions(s_ch(s.words[1].s), s_ch(s.words[2].s), c::MegaChatPeerList::PRIV_STANDARD, &g_chatListener);  // todo
}

void exec_truncatechat(ac::ACState& s)
{
    g_chatListener.onFinish(c::MegaChatRequest::TYPE_TRUNCATE_HISTORY, [](finishInfo& f)
    {
        if (check_err("TruncateChat", f.e))
        {
            conlock(cout) << "Truncated from " << ch_s(f.request->getUserHandle()) << " in chat "  << ch_s(f.request->getChatHandle()) << endl;
        }
    });

    g_chatApi->truncateChat(s_ch(s.words[1].s), s_ch(s.words[2].s), &g_chatListener);  
}

void exec_clearchathistory(ac::ACState& s)
{
    g_chatListener.onFinish(c::MegaChatRequest::TYPE_TRUNCATE_HISTORY, [](finishInfo& f)
    {
        if (check_err("ClearChatHistory", f.e))
        {
            conlock(cout) << "Truncated chat " << ch_s(f.request->getChatHandle()) << ", sole message now " << ch_s(f.request->getUserHandle()) << endl;
        }
    });

    g_chatApi->clearChatHistory(s_ch(s.words[1].s), &g_chatListener);  
}

void exec_setRetentionTime(ac::ACState& s)
{
    g_chatListener.onFinish(c::MegaChatRequest::TYPE_SET_RETENTION_TIME, [](finishInfo& f)
    {
        if (check_err("SetRetentionTime", f.e))
        {
            // Clients will not learn about the retention time from the API
            conlock(cout) << "Retention time was set successfully for chat " << ch_s(f.request->getChatHandle()) << endl;
        }
    });

    g_chatApi->setChatRetentionTime(s_ch(s.words[1].s), atoi(s.words[2].s.c_str()), &g_chatListener);
}


void exec_getRetentionTime(ac::ACState& s)
{
    ::mega::unique_ptr <megachat::MegaChatRoom> chatRoom(g_chatApi->getChatRoom(s_ch(s.words[1].s)));
    if (chatRoom)
    {
        conlock(cout) << " retentionTime " << std::to_string(chatRoom->getRetentionTime()).c_str()<< endl;
    }
}


void exec_setchattitle(ac::ACState& s)
{
    g_chatListener.onFinish(c::MegaChatRequest::TYPE_EDIT_CHATROOM_NAME, [](finishInfo& f)
    {
        if (check_err("SetChatTitle", f.e))
        {
            conlock(cout) << "Chat " << ch_s(f.request->getChatHandle()) << " now titled" << f.request->getText() << endl;
        }
    });

    g_chatApi->setChatTitle(s_ch(s.words[1].s), s.words[2].s.c_str(), &g_chatListener);
}

void exec_openchatroom(ac::ACState& s)
{
    c::MegaChatHandle room = s_ch(s.words[1].s);
    auto& rec = g_roomListeners[room];
    if (!rec.open)
    {
        if (!g_chatApi->openChatRoom(room, rec.listener.get()))
        {
            conlock(cout) << "Failed to open chat room." << endl;
            g_roomListeners.erase(room);
        }
        else
        {
            rec.listener->room = room;
            rec.open = true;
        }
    }
    else
    {
        conlock(cout) << "Room " << ch_s(room) << " is already open." << endl;
    }
}

void exec_closechatroom(ac::ACState& s)
{
    c::MegaChatHandle room = s_ch(s.words[1].s);
    auto& rec = g_roomListeners[room];
    if (!rec.open)
    {
        conlock(cout) << "Room " << ch_s(room) << " was not open" << endl;
    }
    else
    {
        g_chatApi->closeChatRoom(room, rec.listener.get());
    }
    g_roomListeners.erase(room);
}

void exec_openchatpreview(ac::ACState& s)
{
    g_chatApi->openChatPreview(s.words[1].s.c_str(), &g_chatListener);
}

void exec_closechatpreview(ac::ACState& s)
{
    c::MegaChatHandle room = s_ch(s.words[1].s);
    g_chatApi->closeChatPreview(room);
}

void exec_loadmessages(ac::ACState& s)
{
    g_reportMessagesDeveloper = s.words.size() > 3 && s.words[3].s == "developer";

    auto source = g_chatApi->loadMessages(s_ch(s.words[1].s), stoi(s.words[2].s));

    auto cl = conlock(cout);
    switch (source)
    {
    case c::MegaChatApi::SOURCE_ERROR: cout << "Load failed as we are offline." << endl; break;
    case c::MegaChatApi::SOURCE_NONE: cout << "No more messages." << endl; break;
    case c::MegaChatApi::SOURCE_LOCAL: cout << "Loading from local store." << endl; break;
    case c::MegaChatApi::SOURCE_REMOTE: cout << "Loading from server." << endl; break;
    }
}

bool initFile(std::unique_ptr<std::ofstream>& file, const std::string& filename)
{
#ifdef __APPLE__
    const auto outputFilename = getExeDirectory() + "/" + filename;
#else
    const auto outputFilename = getExeDirectory() / filename;
#endif
    file.reset(new std::ofstream{outputFilename});
    if (!file->is_open())
    {
        conlock(cout) << "Error: Unable to open output file: " << outputFilename << endl;
        return false;
    }
    return true;
}

void exec_reviewpublicchat(ac::ACState& s)
{
    if (g_chatApi->getInitState() != c::MegaChatApi::INIT_ONLINE_SESSION)
    {
        conlock(cout) << "Error: Not logged in" << endl;
        return;
    }

    if (g_reviewPublicChatid != c::MEGACHAT_INVALID_HANDLE)
    {
        g_chatApi->closeChatRoom(g_reviewPublicChatid, g_roomListeners[g_reviewPublicChatid].listener.get());
        g_roomListeners.erase(g_reviewPublicChatid);
        g_chatApi->closeChatPreview(g_reviewPublicChatid);
    }

    g_reviewingPublicChat = true;
    g_reviewPublicChatMsgCountRemaining = 0;
    g_reviewPublicChatMsgCount = 0;
    g_startedPublicChatReview = false;
    g_reviewPublicChatid = c::MEGACHAT_INVALID_HANDLE;

    const auto chat_link = s.words[1].s;
    g_reviewPublicChatMsgCountRemaining = s.words.size() > 2 ? stoi(s.words[2].s) : -1;

    const auto lastSlashIdx = chat_link.find_last_of("/");
    const auto lastHashIdx = chat_link.find_last_of("#");
    if (lastSlashIdx == std::string::npos || lastHashIdx == std::string::npos || lastSlashIdx >= lastHashIdx)
    {
        conlock(cout) << "Error: Invalid link format: " << chat_link << endl;
        return;
    }
    const auto linkHandle = chat_link.substr(lastSlashIdx + 1, lastHashIdx - lastSlashIdx - 1);

    const auto baseFilename = "PublicChat_" + linkHandle + "_" + timeToStringUTC(time(nullptr)) + "UTC";
    if (!initFile(g_reviewPublicChatOutFile, baseFilename + ".txt"))
    {
        return;
    }
    if (!initFile(g_reviewPublicChatOutFileLinks, baseFilename + "_Links.txt"))
    {
        return;
    }
    if (!initFile(g_reviewPublicChatOutFileLogs, baseFilename + "_Logs.txt"))
    {
        return;
    }
    *g_reviewPublicChatOutFile << chat_link << endl;
    *g_reviewPublicChatOutFileLinks << chat_link << endl;
    *g_reviewPublicChatOutFileLogs << chat_link << endl;

    auto check_chat_preview_listener = new OneShotChatRequestListener;
    check_chat_preview_listener->onRequestFinishFunc =
    [](c::MegaChatApi* api, c::MegaChatRequest *request, c::MegaChatError* e)
    {
        // Called on Mega Chat API thread
        if (!check_err("checkChatLink", e))
        {
            *g_reviewPublicChatOutFile << "checkChatLink failed. Error: " << e->getErrorString() << endl;
            return;
        }

        const c::MegaChatHandle chatid = g_reviewPublicChatid = request->getChatHandle();
        std::ostringstream os1;
        os1 << "\nReviewPublicChat: chatlink loaded succesfully.\n\tChatid: " << k::Id(g_reviewPublicChatid).toString() << endl;
        const auto msg1 = os1.str();
        conlock(cout) << msg1;
        conlock(*g_reviewPublicChatOutFile) << msg1 << flush;

        const int numPeers = static_cast<int>(request->getNumber());
        std::ostringstream os2;
        os2 << "\tUser count: " << numPeers << endl;
        const auto msg2 = os2.str();
        conlock(cout) << msg2;
        conlock(*g_reviewPublicChatOutFile) << msg2 << flush;

        const char *title = request->getText();
        std::ostringstream os3;
        os3 << "\tTitle: " << title << endl;
        const auto msg3 = os3.str();
        conlock(cout) << msg3;
        conlock(*g_reviewPublicChatOutFile) << msg3 << flush;

        // now we know the chatid, we register the listener
        auto open_chat_preview_listener = new OneShotChatRequestListener;
        open_chat_preview_listener->onRequestFinishFunc =
        [chatid](c::MegaChatApi*, c::MegaChatRequest *request, c::MegaChatError* e)
        {
            if (!check_err("openChatPreview", e))
            {
                *g_reviewPublicChatOutFile << "openChatPreview failed. Error: " << e->getErrorString() << endl;
                return;
            }
        };

        const char *chatlink = request->getLink();
        api->openChatPreview(chatlink, open_chat_preview_listener);
        // now wait until logged in into the chatroom, so we know the peers and load their emails
    };

    g_chatApi->checkChatLink(chat_link.c_str(), check_chat_preview_listener);
}

void exec_isfullhistoryloaded(ac::ACState& s)
{
    conlock(cout) << (g_chatApi->isFullHistoryLoaded(s_ch(s.words[1].s)) ? "Yes" : "No") << endl;
}

void exec_getmessage(ac::ACState& s)
{
    auto room = s_ch(s.words[1].s);
    unique_ptr<c::MegaChatMessage> msg(g_chatApi->getMessage(room, s_ch(s.words[2].s)));

    if (!msg)
    {
        conlock(cout) << "Not retrieved." << endl;
    }
    else
    {
        reportMessage(room, msg.get(), "got");
    }
}

void exec_getmanualsendingmessage(ac::ACState& s)
{
    auto room = s_ch(s.words[1].s);
    unique_ptr<c::MegaChatMessage> msg(g_chatApi->getManualSendingMessage(room, s_ch(s.words[2].s)));

    if (!msg)
    {
        conlock(cout) << "Not retrieved." << endl;
    }
    else
    {
        reportMessage(room, msg.get(), "got");
    }
}


void exec_sendmessage(ac::ACState& s)
{
    auto room = s_ch(s.words[1].s);
    unique_ptr<c::MegaChatMessage> msg(g_chatApi->sendMessage(room, s.words[2].s.c_str()));

    if (!msg)
    {
        conlock(cout) << "Failed." << endl;
    }
    else
    {
        reportMessage(room, msg.get(), "sending");
    }
}

void exec_attachcontacts(ac::ACState& s)
{
    auto room = s_ch(s.words[1].s);
    auto mhl = m::MegaHandleList::createInstance();
    for (unsigned i = 2; i < s.words.size(); ++i)
    {
        mhl->addMegaHandle(s_ch(s.words[i].s));
    }

    unique_ptr<c::MegaChatMessage> msg(g_chatApi->attachContacts(room, mhl));  //todo: ownership

    if (!msg)
    {
        conlock(cout) << "Failed." << endl;
    }
    else
    {
        reportMessage(room, msg.get(), "sending contacts");
    }
}

void exec_attachnode(ac::ACState& s)
{
    auto room = s_ch(s.words[1].s);
    g_chatApi->attachNode(room, s_ch(s.words[2].s), &g_chatListener);
}

void exec_revokeattachmentmessage(ac::ACState& s)
{
    auto room = s_ch(s.words[1].s);
    unique_ptr<c::MegaChatMessage> msg(g_chatApi->revokeAttachmentMessage(room, s_ch(s.words[2].s)));

    if (!msg)
    {
        conlock(cout) << "Failed." << endl;
    }
    else
    {
        reportMessage(room, msg.get(), "revoking attachment");
    }
}

void exec_editmessage(ac::ACState& s)
{
    auto room = s_ch(s.words[1].s);
    unique_ptr<c::MegaChatMessage> msg(g_chatApi->editMessage(room, s_ch(s.words[2].s), s.words[2].s.c_str()));

    if (!msg)
    {
        conlock(cout) << "Failed." << endl;
    }
    else
    {
        reportMessage(room, msg.get(), "editing");
    }
}

void exec_deletemessage(ac::ACState& s)
{
    auto room = s_ch(s.words[1].s);
    unique_ptr<c::MegaChatMessage> msg(g_chatApi->deleteMessage(room, s_ch(s.words[2].s)));

    if (!msg)
    {
        conlock(cout) << "Failed." << endl;
    }
    else
    {
        reportMessage(room, msg.get(), "deleting");
    }
}

void exec_setmessageseen(ac::ACState& s)
{
    conlock(cout) << (g_chatApi->setMessageSeen(s_ch(s.words[2].s), s_ch(s.words[2].s)) ? "Done" : "Failed") << endl;
}

void exec_getLastMessageSeen(ac::ACState& s)
{
    auto room = s_ch(s.words[1].s);
    unique_ptr<c::MegaChatMessage> msg(g_chatApi->getLastMessageSeen(room));

    if (!msg)
    {
        conlock(cout) << "None." << endl;
    }
    else
    {
        reportMessage(room, msg.get(), "last seen");
    }
}

void exec_removeunsentmessage(ac::ACState& s)
{
    g_chatApi->removeUnsentMessage(s_ch(s.words[1].s), s_ch(s.words[2].s));
}

void exec_sendtypingnotification(ac::ACState& s)
{
    g_chatListener.onFinish(c::MegaChatRequest::TYPE_SEND_TYPING_NOTIF, [](finishInfo& f)
    {
        if (check_err("SendTypingNotification", f.e))
        {
            conlock(cout) << "Chat " << ch_s(f.request->getChatHandle()) << " notified"  << endl;
        }
    });

    g_chatApi->sendTypingNotification(s_ch(s.words[1].s), &g_chatListener);
}

void exec_ismessagereceptionconfirmationactive(ac::ACState&)
{
    conlock(cout) << (g_chatApi->isMessageReceptionConfirmationActive() ? "Yes" : "No") << endl;
}


void exec_savecurrentstate(ac::ACState&)
{
    g_chatApi->saveCurrentState();
}

void exec_detail(ac::ACState& s)
{
    g_detailHigh = s.words[1].s == "high";
}

#ifdef WIN32
void exec_dos_unix(ac::ACState& s)
{
    static_cast<m::WinConsole*>(console.get())->setAutocompleteStyle(s.words[1].s == "unix");
}
#endif

ac::ACN autocompleteTemplate;

void exec_help(ac::ACState&)
{
    conlock(cout) << *autocompleteTemplate << flush;
}

#ifdef WIN32
void exec_history(ac::ACState& s)
{
    static_cast<m::WinConsole*>(console.get())->outputHistory();
}
#endif

void exec_quit(ac::ACState&)
{
    quit_flag = true;
}

#ifndef KARERE_DISABLE_WEBRTC

void exec_getchatvideoindevices(ac::ACState&)
{
    unique_ptr<m::MegaStringList> videoDevices(g_chatApi->getChatVideoInDevices());
    for (int i = 0; i < videoDevices->size(); ++i)
    {
        cout << videoDevices->get(i) << endl;
    }
}

void exec_setchatvideoindevice(ac::ACState& s)
{
    c::MegaChatRequestListener *listener = new c::MegaChatRequestListener; // todo
    g_chatApi->setChatVideoInDevice(s.words[1].s.c_str(), listener);
}

void exec_startchatcall(ac::ACState& s)
{
    c::MegaChatRequestListener *listener = new c::MegaChatRequestListener; // todo
    c::MegaChatHandle room = s_ch(s.words[1].s);
    bool enableVideo = s.words.size() > 2 && s.words[2].s == "true";  
    g_chatApi->startChatCall(room, enableVideo, listener);
}

void exec_answerchatcall(ac::ACState& s)
{
    c::MegaChatRequestListener *listener = new c::MegaChatRequestListener; // todo
    c::MegaChatHandle room = s_ch(s.words[1].s);
    bool enableVideo = s.words.size() < 2 || s.words[2].s == "true";
    g_chatApi->answerChatCall(room, enableVideo, listener);
}


void exec_hangchatcall(ac::ACState& s)
{
    c::MegaChatRequestListener *listener = new c::MegaChatRequestListener; // todo
    c::MegaChatHandle room = s_ch(s.words[1].s);
    g_chatApi->hangChatCall(room, listener);
}

void exec_hangallchatcalls(ac::ACState&)
{
    c::MegaChatRequestListener *listener = new c::MegaChatRequestListener; // todo
    g_chatApi->hangAllChatCalls(listener);
}

void exec_enableaudio(ac::ACState& s)
{
    c::MegaChatRequestListener *listener = new c::MegaChatRequestListener; // todo
    c::MegaChatHandle room = s_ch(s.words[1].s);
    g_chatApi->enableAudio(room, listener);
}

void exec_disableaudio(ac::ACState& s)
{
    c::MegaChatRequestListener *listener = new c::MegaChatRequestListener; // todo
    c::MegaChatHandle room = s_ch(s.words[1].s);
    g_chatApi->disableAudio(room, listener);
}

void exec_enablevideo(ac::ACState& s)
{
    c::MegaChatRequestListener *listener = new c::MegaChatRequestListener; // todo
    c::MegaChatHandle room = s_ch(s.words[1].s);
    g_chatApi->enableVideo(room, listener);
}

void exec_disablevideo(ac::ACState& s)
{
    c::MegaChatRequestListener *listener = new c::MegaChatRequestListener; // todo
    c::MegaChatHandle room = s_ch(s.words[1].s);
    g_chatApi->disableVideo(room, listener);
}

void exec_loadaudiovideodevicelist(ac::ACState&)
{
    c::MegaChatRequestListener *listener = new c::MegaChatRequestListener; // todo
    g_chatApi->loadAudioVideoDeviceList(listener);
}

void exec_getchatcall(ac::ACState&)
{
    /**
     * @brief Get the MegaChatCall associated with a chatroom
     *
     * If \c chatid is invalid or there isn't any MegaChatCall associated with the chatroom,
     * this function returns NULL.
     *
     * You take the ownership of the returned value.
     *
     * @param chatid MegaChatHandle that identifies the chat room
     * @return MegaChatCall object associated with chatid or NULL if it doesn't exist
     */
   // MegaChatCall *getChatCall(MegaChatHandle chatid);
}

void exec_setignoredcall(ac::ACState& s)
{
    c::MegaChatHandle room = s_ch(s.words[1].s);
    g_chatApi->setIgnoredCall(room);
}


void exec_getchatcallbycallid(ac::ACState&)
{
    /**
 * @brief Get the MegaChatCall that has a specific id
 *
 * You can get the id of a MegaChatCall using MegaChatCall::getId().
 *
 * You take the ownership of the returned value.
 *
 * @param callId MegaChatHandle that identifies the call
 * @return MegaChatCall object for the specified \c callId. NULL if call doesn't exist
 */
  //  MegaChatCall *getChatCallByCallId(MegaChatHandle callId);
}

void exec_getnumcalls(ac::ACState&)
{
    cout << g_chatApi->getNumCalls() << endl;
}

void exec_getchatcalls(ac::ACState&)
{
    unique_ptr<m::MegaHandleList> list(g_chatApi->getChatCalls());
    for (unsigned i = 0; i < list->size(); ++i)
    {
        cout << ch_s(list->get(i)) << endl;
    }
}

void exec_getchatcallsids(ac::ACState&)
{
    unique_ptr<m::MegaHandleList> list(g_chatApi->getChatCallsIds());
    for (unsigned i = 0; i < list->size(); ++i)
    {
        cout << ch_s(list->get(i)) << endl;
    }
}

#endif

void exec_smsverify(ac::ACState& s)
{
    if (s.words[1].s == "send")
    {
        auto listener = new OneShotRequestListener;
        listener->onRequestFinishFunc = [](m::MegaApi* api, m::MegaRequest *request, m::MegaError* e) 
            { 
                conlock(cout) << "SMS Verify Text Result: " << e->getErrorString() << endl;
            };
        g_megaApi->sendSMSVerificationCode(s.words[2].s.c_str(), listener, s.words.size() > 3 && s.words[3].s == "to");
    }
    else if (s.words[1].s == "code")
    {
        auto listener = new OneShotRequestListener;
        listener->onRequestFinishFunc = [](m::MegaApi* api, m::MegaRequest *request, m::MegaError* e)
        {
            conlock(cout) << "SMS Verify Text Result: " << e->getErrorString() << endl;
        };
        g_megaApi->checkSMSVerificationCode(s.words[2].s.c_str(), listener);
    }
    else if (s.words[1].s == "allowed")
    {
        conlock(cout) << "SMS Verify Text Result: " << g_megaApi->smsAllowedState() << endl;
    }
    else if (s.words[1].s == "phone")
    {
        unique_ptr<char[]> number(g_megaApi->smsVerifiedPhoneNumber());
        conlock(cout) << "Verified phone: " << (number ? number.get() : "<none>") << endl;
    }
}

void exec_apiurl(ac::ACState& s)
{
    if (s.words.size() == 3 || s.words.size() == 2)
    {
        if (s.words[1].s.size() < 8 || s.words[1].s.substr(0, 8) != "https://")
        {
            s.words[1].s = "https://" + s.words[1].s;
        }
        if (s.words[1].s.empty() || s.words[1].s.back() != '/')
        {
            s.words[1].s += '/';
        }
        g_megaApi->changeApiUrl(s.words[1].s.c_str(), s.words.size() > 2 && s.words[2].s == "true");
        if (g_megaApi->isLoggedIn())
        {
            conlock(cout) << "Refreshing local cache due to change of APIURL" << endl;

            setprompt(NOPROMPT);

            const char *session = g_megaApi->dumpSession();
            g_megaApi->fastLogin(session);
            g_chatApi->refreshUrl();
            delete [] session;
        }
    }
}


void exec_catchup(ac::ACState& s)
{
    int count = s.words.size() > 1 ? atoi(s.words[1].s.c_str()) : 1;

    for (int i = 0; i < count; ++i)
    {
        static int next_catchup_id = 0;
        int id = next_catchup_id++;

        g_megaApi->catchup(new OneShotRequestListener([id](m::MegaApi*, m::MegaRequest *, m::MegaError* e)
            {
                check_err("catchup " + to_string(id), e);
            }));

        conlock(cout) << "catchup " << id << " requested" << endl;
    }
}

map<string, unique_ptr<m::MegaBackgroundMediaUpload>> g_megaBackgroundMediaUploads;

bool getNamedBackgroundMediaUpload(const string& name, m::MegaBackgroundMediaUpload*& p)
{
    p = NULL;
    auto i = g_megaBackgroundMediaUploads.find(name);
    if (i != g_megaBackgroundMediaUploads.end())
    {
        p = i->second.get();
        return true;
    }
    return false;
}

string toHex(const string& binary)
{
    ostringstream s;
    s << std::hex;

    for (unsigned char c : binary)
    {
        s << setw(2) << setfill('0') << (unsigned)c;
    }

    return s.str();
}

unsigned char toBinary(unsigned char c)
{
    if (c >= 0 && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    return 0;
}

string toBinary(const string& hex)
{
    string bin;
    for (string::const_iterator i = hex.cbegin(); i != hex.cend(); ++i)
    {
        unsigned char c = toBinary(*i);
        c <<= 4;
        ++i;
        if (i != hex.cend())
        {
            c |= toBinary(*i);
        }
        bin.push_back(c);
    }
    return bin;
}

#ifdef WIN32  // functions to perform background-upload like http request

// handle WinHTTP callbacks (which can be in a worker thread context)
VOID CALLBACK asynccallback(HINTERNET hInternet, DWORD_PTR dwContext,
    DWORD dwInternetStatus,
    LPVOID lpvStatusInformation,
    DWORD dwStatusInformationLength)
{
    using namespace m;

    if (dwInternetStatus == WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING)
    {
        LOG_verbose << "Closing request";
        return;
    }

    switch (dwInternetStatus)
    {
    case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE:
    {
        LOG_verbose << "WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE";
        break;
    }

    case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
        LOG_verbose << "WINHTTP_CALLBACK_STATUS_READ_COMPLETE";
        break;

    case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
    {
        LOG_verbose << "WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE";
        break;
    }

    case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
    {
        LOG_verbose << "WINHTTP_CALLBACK_STATUS_REQUEST_ERROR";
        break;
    }
    case WINHTTP_CALLBACK_STATUS_SECURE_FAILURE:
        LOG_verbose << "WINHTTP_CALLBACK_STATUS_SECURE_FAILURE";
        break;

    case WINHTTP_CALLBACK_STATUS_SENDING_REQUEST:
    {
        LOG_verbose << "WINHTTP_CALLBACK_STATUS_SENDING_REQUEST";
        break;
    }

    case WINHTTP_CALLBACK_STATUS_REQUEST_SENT:
    {
        LOG_verbose << "WINHTTP_CALLBACK_STATUS_REQUEST_SENT";
        break;
    }

    case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
        LOG_verbose << "WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE";
        break;
    case WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE:
        LOG_verbose << "WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE";
        break;
    default:
        LOG_verbose << dwInternetStatus;
    }
}

void synchronousHttpRequest(const string& url, const string& senddata, string& responsedata)
{
    using namespace m;
    LOG_info << "Sending file to " << url << ", size: " << senddata.size();

    BOOL  bResults = TRUE;
    HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;

    // Use WinHttpOpen to obtain a session handle.
    hSession = WinHttpOpen(L"testmega/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);

    WCHAR szURL[8192];
    WCHAR szHost[256];
    URL_COMPONENTS urlComp = { sizeof urlComp };

    urlComp.lpszHostName = szHost;
    urlComp.dwHostNameLength = sizeof szHost / sizeof *szHost;
    urlComp.dwUrlPathLength = (DWORD)-1;
    urlComp.dwSchemeLength = (DWORD)-1;

    if (MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, szURL,
        sizeof szURL / sizeof *szURL)
        && WinHttpCrackUrl(szURL, 0, 0, &urlComp))
    {
        if ((hConnect = WinHttpConnect(hSession, szHost, urlComp.nPort, 0)))
        {
            hRequest = WinHttpOpenRequest(hConnect, L"POST",
                urlComp.lpszUrlPath, NULL,
                WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES,
                (urlComp.nScheme == INTERNET_SCHEME_HTTPS)
                ? WINHTTP_FLAG_SECURE
                : 0);
        }
    }

    // Send a Request.
    if (hRequest)
    {
        WinHttpSetTimeouts(hRequest, 58000, 58000, 0, 0);

        LPCWSTR pwszHeaders = L"Content-Type: application/octet-stream";

        // HTTPS connection: ignore certificate errors, send no data yet
        DWORD flags = SECURITY_FLAG_IGNORE_CERT_CN_INVALID
            | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID
            | SECURITY_FLAG_IGNORE_UNKNOWN_CA;

        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof flags);

        if (WinHttpSendRequest(hRequest, pwszHeaders,
            DWORD(wcslen(pwszHeaders)),
            (LPVOID)senddata.data(),
            (DWORD)senddata.size(),
            (DWORD)senddata.size(),
            NULL))
        {
        }
    }

    DWORD dwSize = 0;

    // End the request.
    if (bResults)
        bResults = WinHttpReceiveResponse(hRequest, NULL);

    // Continue to verify data until there is nothing left.
    if (bResults)
        do
        {
            // Verify available data.
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
                printf("Error %u in WinHttpQueryDataAvailable.\n",
                    GetLastError());

            size_t offset = responsedata.size();
            responsedata.resize(offset + dwSize);

            ZeroMemory(responsedata.data() + offset, dwSize);

            DWORD dwDownloaded = 0;
            if (!WinHttpReadData(hRequest, responsedata.data() + offset, dwSize, &dwDownloaded))
                printf("Error %u in WinHttpReadData.\n", GetLastError());

        } while (dwSize > 0);

        // Report errors.
        if (!bResults)
            printf("Error %d has occurred.\n", GetLastError());

        // Close open handles.
        if (hRequest) WinHttpCloseHandle(hRequest);
        if (hConnect) WinHttpCloseHandle(hConnect);
        if (hSession) WinHttpCloseHandle(hSession);
}
#endif

string loadfile(const string& filename)
{
    string filedata;
    ifstream f(filename, ios::binary);
    f.seekg(0, std::ios::end);
    filedata.resize(unsigned(f.tellg()));
    f.seekg(0, std::ios::beg);
    f.read((char*)filedata.data(), filedata.size());
    return filedata;
}

void exec_backgroundupload(ac::ACState& s)
{
    m::MegaBackgroundMediaUpload* mbmu = nullptr;

    if (s.words[1].s == "new" && s.words.size() == 3)
    {
        g_megaBackgroundMediaUploads[s.words[2].s].reset(m::MegaBackgroundMediaUpload::createInstance(g_megaApi.get()));
    }
    else if (s.words[1].s == "resume" && s.words.size() == 4)
    {
        g_megaBackgroundMediaUploads[s.words[2].s].reset(m::MegaBackgroundMediaUpload::unserialize(s.words[3].s.c_str(), g_megaApi.get()));
    }
    else if (s.words[1].s == "analyse" && s.words.size() == 4 && getNamedBackgroundMediaUpload(s.words[2].s, mbmu))
    {
        mbmu->analyseMediaInfo(s.words[3].s.c_str());
    }
    else if (s.words[1].s == "encrypt" && s.words.size() == 8 && getNamedBackgroundMediaUpload(s.words[2].s, mbmu))
    {
        int64_t startPos = atol(s.words[5].s.c_str());
        int64_t length = atol(s.words[6].s.c_str());
        bool adjustsizeonly = s.words[7].s == "true";
        string urlSuffix = OwnStr(mbmu->encryptFile(s.words[3].s.c_str(), startPos, &length, s.words[4].s.c_str(), adjustsizeonly));
        if (!urlSuffix.empty())
        {
            conlock(cout) << "Encrypt complete, URL suffix: " << urlSuffix << " and updated length: " << length << endl;
        }
        else
        {
            conlock(cout) << "Encrypt failed" << endl;
        }
    }
    else if (s.words[1].s == "geturl" && s.words.size() == 4 && getNamedBackgroundMediaUpload(s.words[2].s, mbmu))
    {
        auto ln = new OneShotRequestListener;
        ln->onRequestFinishFunc = [](m::MegaApi* api, m::MegaRequest *request, m::MegaError* e) 
        { 
            if (check_err("Get upload URL", e))
            {
                conlock(cout) << "Upload URL: " << OwnStr(request->getMegaBackgroundMediaUploadPtr()->getUploadURL()) << endl;
            }
        };

        g_megaApi->backgroundMediaUploadRequestUploadURL(atoll(s.words[3].s.c_str()), mbmu, ln);
    }
    else if (s.words[1].s == "serialize"  && s.words.size() == 3 && getNamedBackgroundMediaUpload(s.words[2].s, mbmu))
    {
        unique_ptr<char[]> serialized(mbmu->serialize());
        conlock(cout) << serialized.get() << endl;
    }
    else if (s.words[1].s == "upload"  && s.words.size() == 4)
    {
#ifdef WIN32
        string responsedata;
        synchronousHttpRequest(s.words[2].s, loadfile(s.words[3].s), responsedata);
        unique_ptr<char[]> base64(m::MegaApi::binaryToBase64(responsedata.data(), responsedata.size()));
        conlock(cout) << "Synchronous upload response (converted to base 64): " << (responsedata.size() <= 3 ? responsedata : base64.get()) << endl;
#endif
    }
    else if (s.words[1].s == "putthumbnail"  && s.words.size() == 4 && getNamedBackgroundMediaUpload(s.words[2].s, mbmu))
    {
        g_megaApi->putThumbnail(mbmu, s.words[3].s.c_str(), new OneShotRequestListener([](m::MegaApi*, m::MegaRequest *r, m::MegaError* e)
        {
            if (check_err("putthumbnail", e))
            {
                conlock(cout) << "thumbnail file attribute handle: " << unique_ptr<char[]>(m::MegaApi::userHandleToBase64(r->getNodeHandle())).get() << endl;
            }
        }));
    }
    else if (s.words[1].s == "putpreview"  && s.words.size() == 4 && getNamedBackgroundMediaUpload(s.words[2].s, mbmu))
    {
        g_megaApi->putPreview(mbmu, s.words[3].s.c_str(), new OneShotRequestListener([](m::MegaApi*, m::MegaRequest *r, m::MegaError* e)
        {
            if (check_err("putpreview", e))
            {
                conlock(cout) << "preview file attribute handle: " << unique_ptr<char[]>(m::MegaApi::userHandleToBase64(r->getNodeHandle())).get() << endl;
            }
        }));
    }
    else if (s.words[1].s == "setthumbnail"  && s.words.size() == 4 && getNamedBackgroundMediaUpload(s.words[2].s, mbmu))
    {
        mbmu->setThumbnail(m::MegaApi::base64ToUserHandle(s.words[3].s.c_str()));
    }
    else if (s.words[1].s == "setpreview"  && s.words.size() == 4 && getNamedBackgroundMediaUpload(s.words[2].s, mbmu))
    {
        mbmu->setPreview(m::MegaApi::base64ToUserHandle(s.words[3].s.c_str()));
    }
    else if (s.words[1].s == "setcoordinates" && s.words.size() == 5 && getNamedBackgroundMediaUpload(s.words[2].s, mbmu))
    {
        bool shareable = extractflag("-shareable", s.words);
        mbmu->setCoordinates(atof(s.words[3].s.c_str()), atof(s.words[4].s.c_str()), !shareable);
    }
    else if (s.words[1].s == "complete" && getNamedBackgroundMediaUpload(s.words[2].s, mbmu))
    {
        const char* fingerprint = s.words[5].s.empty() ? NULL : s.words[5].s.c_str();
        const char* fingerprintoriginal = s.words[6].s.empty() ? NULL : s.words[6].s.c_str();
        const char* uploadtoken64 = s.words[7].s.c_str();
        if (auto parent = GetNodeByPath(s.words[4].s))
        {
            auto ln = new OneShotRequestListener;
            ln->onRequestFinishFunc = [](m::MegaApi* api, m::MegaRequest *request, m::MegaError* e)
            {
                check_err("Background upload completion", e);
            };

            g_megaApi->backgroundMediaUploadComplete(mbmu, s.words[3].s.c_str(), parent.get(), fingerprint, fingerprintoriginal, uploadtoken64, ln);
        }
    }
    else
    {
        conlock(cout) << "incorrect subcommand" << endl;
    }
}

void exec_setthumbnailbyhandle(ac::ACState& s)
{
    if (auto node = GetNodeByPath(s.words[1].s))
    {
        g_megaApi->setThumbnailByHandle(node.get(), s_ch(s.words[2].s), new OneShotRequestListener([](m::MegaApi*, m::MegaRequest *, m::MegaError* e)
        {
            check_err("setThumbnailByHandle", e);
        }));
    }
}

void exec_setpreviewbyhandle(ac::ACState& s)
{
    if (auto node = GetNodeByPath(s.words[1].s))
    {
        g_megaApi->setPreviewByHandle(node.get(), s_ch(s.words[2].s), new OneShotRequestListener([](m::MegaApi*, m::MegaRequest *, m::MegaError* e)
        {
            check_err("setThumbnailByHandle", e);
        }));
    }
    else
    {
        conlock(cout) << "Path not found" << endl;
    }
}

void exec_ensuremediainfo(ac::ACState& s)
{
    bool b = g_megaApi->ensureMediaInfo();
    if (b)
    {
        conlock(cout) << "media info already available" << endl;
    }
    else
    {
        conlock(cout) << "media info request sent" << endl;
    }
}

void exec_getfingerprint(ac::ACState& s)
{
    if (s.words[1].s == "local" && s.words.size() == 3)
    {
        char* fp = g_megaApi->getFingerprint(s.words[2].s.c_str());
        conlock(cout) << (fp ? fp : "<NULL>") << endl;
        delete[] fp;
    }
    else if (s.words[1].s == "remote" && s.words.size() == 3)
    {
        if (auto n = GetNodeByPath(s.words[2].s))
        {
            char* fp = g_megaApi->getFingerprint(n.get());
            conlock(cout) << (fp ? fp : "<NULL>") << endl;
            delete[] fp;
        }
    }
    else if (s.words[1].s == "original" && s.words.size() == 3)
    {
        if (auto n = GetNodeByPath(s.words[2].s))
        {
            const char* fp = n->getOriginalFingerprint();
            conlock(cout) << (fp ? fp : "<NULL>") << endl;
        }
    }
}

void exec_createthumbnail(ac::ACState& s)
{
    string parallelcount;
    bool tempmegaapi = extractflag("-tempmegaapi", s.words);
    bool parallel = extractflagparam("-parallel", parallelcount, s.words);

    if (!parallel)
    {
        parallelcount = "1";
    }

    vector<unique_ptr<thread>> ts;

    // investigate thumbnal generation memory usage after reports of memory leaks in iOS
    int N = atoi(parallelcount.c_str());
    for (int i = N; i--; )
    {
        string path1 = s.words[1].s;
        string path2 = s.words[2].s + (N > 1 ? "-" + to_string(i) : string());

        ts.emplace_back(new thread([path1, path2, tempmegaapi]() {
            bool done = false;

            if (tempmegaapi)
            {
                ::mega::MegaApi megaApi("temp");
                done = megaApi.createThumbnail(path1.c_str(), path2.c_str());
            }
            else
            {
                done = g_megaApi->createThumbnail(path1.c_str(), path2.c_str());
            }
            conlock(cout) << (done ? "succeeded" : "failed") << endl;
        }));
    }

    for (int i = atoi(parallelcount.c_str()); i--; )
    {
        ts[i]->join();
    }
}

void exec_createpreview(ac::ACState& s)
{
    string path1 = s.words[1].s;
    string path2 = s.words[2].s;
    bool done = g_megaApi->createThumbnail(path1.c_str(), path2.c_str());
    conlock(cout) << (done ? "succeeded" : "failed") << endl;
}

void exec_testAllocation(ac::ACState& s)
{
    bool success = g_megaApi->testAllocation(unsigned(atoi(s.words[1].s.c_str())), size_t(atoll(s.words[2].s.c_str())));
    conlock(cout) << (success ? "succeeded" : "failed") << endl;
}



void exec_recentactions(ac::ACState& s)
{
    unique_ptr<m::MegaRecentActionBucketList> ra;

    if (s.words.size() == 3)
    {
        ra.reset(g_megaApi->getRecentActions(atoi(s.words[1].s.c_str()), atoi(s.words[2].s.c_str())));
    }
    else
    {
        ra.reset(g_megaApi->getRecentActions());
    }
    
    auto l = conlock(cout);
    for (int b = 0; b < ra->size(); ++b)
    {
        m::MegaRecentActionBucket* bucket = ra->get(b);

        int64_t ts = bucket->getTimestamp();
        const char* em = bucket->getUserEmail();
        m::MegaHandle ph = bucket->getParentHandle();
        bool isupdate = bucket->isUpdate();
        bool ismedia = bucket->isMedia();
        const m::MegaNodeList* nodes = bucket->getNodes();
        
        cout << "Bucket " << ts << " email " << (em ? em : "NULL") << " parent " << ph << (isupdate ? " update" : "") << (ismedia ? " media" : " files") << " count: " << nodes->size() << endl;

        for (int i = 0; i < nodes->size(); ++i)
        {
            cout << "    ";
            unique_ptr<char[]> path(g_megaApi->getNodePath(nodes->get(i)));
            unique_ptr<char[]> handleStr(nodes->get(i)->getBase64Handle());
            if (path)
            {
                cout << path.get();
            }
            else
            {
                cout << "Path unknown but node name is: " << nodes->get(i)->getName();
            }
            cout << " size: " << nodes->get(i)->getSize() << " handle: " << (handleStr ? handleStr.get() : "(NULL)") << endl;
        }
    }
}

void exec_getspecificaccountdetails(ac::ACState& s)
{
    bool storage = extractflag("storage", s.words);
    bool transfer = extractflag("transfer", s.words);
    bool pro = extractflag("pro", s.words);

    if (!storage && !transfer && !pro)
    {
        storage = transfer = pro = true;
    }

    g_megaApi->getSpecificAccountDetails(storage, transfer, pro, -1, new OneShotRequestListener([](m::MegaApi*, m::MegaRequest *r, m::MegaError* e)
            {
                if (check_err("getSpecificAccountDetails", e, ReportFailure))
                {
                    unique_ptr<m::MegaAccountDetails> ad(r->getMegaAccountDetails());
                    conlock(cout) << "Storage used: " << ad->getStorageUsed() << " free: " << (ad->getStorageMax() - ad->getStorageUsed()) << " max: " << ad->getStorageMax() <<  endl
                                  << "Version bytes used: " << ad->getVersionStorageUsed() << endl;
                }
            }));
}


string joinStringList(m::MegaStringList& msl, const string& separator)
{
    string s;
    for (int i = 0; i < msl.size(); ++i)
    {
        if (s.empty()) s += separator;
        s += msl.get(i) ? msl.get(i) : "<null>";
    }
    return s;
}

void exec_setnodecoordinates(ac::ACState& s)
{
    if (auto node = GetNodeByPath(s.words[1].s))
    {
        g_megaApi->setNodeCoordinates(node.get(), atof(s.words[2].s.c_str()), atof(s.words[3].s.c_str()), new OneShotRequestListener([](m::MegaApi*, m::MegaRequest *, m::MegaError* e)
        {
            check_err("setNodeCoordinates", e);
        }));
    }
}

void exec_setunshareablenodecoordinates(ac::ACState& s)
{
    if (auto node = GetNodeByPath(s.words[1].s))
    {
        g_megaApi->setUnshareableNodeCoordinates(node.get(), atof(s.words[2].s.c_str()), atof(s.words[3].s.c_str()), new OneShotRequestListener([](m::MegaApi*, m::MegaRequest *, m::MegaError* e)
        {
            check_err("setUnshareableNodeCoordinates", e);
        }));
    }
}

void exec_getnodebypath(ac::ACState& s)
{
    if (auto node = GetNodeByPath(s.words[1].s))
    {
        auto guard = conlock(cout);

        cout << "type: " << node->getType() << endl;
        cout << "name: " << (node->getName() ? node->getName() : "<null>") << endl;
        cout << "fingerprint: " << (node->getFingerprint() ? node->getFingerprint() : "<null>") << endl;
        cout << "original fingerprint: " << (node->getOriginalFingerprint() ? node->getOriginalFingerprint() : "<null>") << endl;
        cout << "has custom attrs: " << node->hasCustomAttrs() << endl;
        unique_ptr<m::MegaStringList> can(node->getCustomAttrNames());
        for (int i = 0; i < can->size(); ++i)
        {
            cout << "  " << can->get(i) << ": " << node->getCustomAttr(can->get(i)) << endl;
        }
        cout << "duration (seconds): " << node->getDuration() << endl;
        cout << "width: " << node->getWidth() << endl;
        cout << "height: " << node->getHeight() << endl;
        cout << "shortformat: " << node->getShortformat() << endl;
        cout << "videoCodecId: " << node->getVideocodecid() << endl;
        cout << "latitude: " << node->getLatitude() << endl;
        cout << "longitude: " << node->getLongitude() << endl;
        cout << "handle: " << node->getBase64Handle() << endl;
        cout << "size: " << node->getSize() << endl;
        cout << "creation time: " << node->getCreationTime() << endl;
        cout << "modification time: " << node->getModificationTime() << endl;
        cout << "handle: " << ch_s(node->getHandle()) << endl;
        cout << "restore handle: " << ch_s(node->getRestoreHandle()) << endl;
        cout << "parent handle: " << ch_s(node->getParentHandle()) << endl;
        //getBase64Key();
        cout << "tag: " << node->getTag() << endl;
        cout << "expiration time: " << node->getExpirationTime() << endl;
        cout << "public handle: " << ch_s(node->getPublicHandle()) << endl;
        //getPublicNode();
        unique_ptr<char[]> publink(node->getPublicLink(true));
        cout << "public link: " << (publink.get() ? publink.get() : "<null>") << endl;
        cout << "is file: " << node->isFile() << endl;
        cout << "is folder: " << node->isFolder() << endl;
        cout << "is removed: " << node->isRemoved() << endl;
        cout << "changes: " << hex << node->getChanges() << dec << endl;
        cout << "has thumbnail: " << node->hasThumbnail() << endl;
        cout << "has preview: " << node->hasPreview() << endl;
        cout << "isPublic: " << node->isPublic() << endl;
        cout << "isShared: " << node->isShared() << endl;
        cout << "isOutShare: " << node->isOutShare() << endl;
        cout << "isInShare: " << node->isInShare() << endl;
        cout << "isExported: " << node->isExported() << endl;
        cout << "isExpired: " << node->isExpired() << endl;
        cout << "isTakenDown: " << node->isTakenDown() << endl;
        cout << "isForeign: " << node->isForeign() << endl;
        //getNodeKey();
        cout << "binary attriutes (hexed): " << (node->getAttrString() ? toHex(*node->getAttrString()) : "<null>") << endl;
        unique_ptr<char[]> fileattr(node->getFileAttrString());
        cout << "chatroom file attributes: " << (fileattr ? fileattr.get() : "<null>") << endl;
        //getPrivateAuth();
        //setPrivateAuth(const char *privateAuth);
        //getPublicAuth();
        //getChatAuth();
        //getChildren();
#ifdef ENABLE_SYNC
        //virtual bool isSyncDeleted();
        cout << "local sync path: " << node->getLocalPath() << endl;
#endif
        cout << "owner handle: " << ch_s(node->getOwner()) << endl;
        cout << "serialized: " << unique_ptr<char[]>(node->serialize()).get() << endl;
        //unserialize(const char *d);
    }
}

struct ls_flags
{
    string regexfilterstring;
    std::regex re;
    bool recursive = false;
    bool regexfilter = false;
    bool handle = false;
    bool ctime = false;
    bool mtime = false;
    bool size = false;
    bool versions = false;
    int order = 1;
};



void ls(m::MegaNode* node, const string& basepath, const ls_flags& flags, int depth)
{
    bool show = true;

    if (depth > 0 || node->getType() == m::MegaNode::TYPE_FILE)
    {
        string utf8path(g_megaApi->getNodePath(node));
        if (utf8path.size() > basepath.size() && 0 == memcmp(utf8path.data(), basepath.data(), basepath.size()))
        {
            utf8path.erase(0, basepath.size());
        }

        if (flags.regexfilter)
        {
            if (!std::regex_search(utf8path, flags.re))
            {
                show = false;
            }
        }

        if (show)
        {
            auto guard = conlock(cout);
            cout << utf8path;
            if (node->getType() == m::MegaNode::TYPE_FOLDER) cout << "/";

            if (flags.size) cout << " " << node->getSize();
            if (flags.ctime) cout << " " << node->getCreationTime();
            if (flags.mtime) cout << " " << node->getModificationTime();
            if (flags.handle) cout << " " << OwnStr(g_megaApi->handleToBase64(node->getHandle()));
        }
    }

    switch (node->getType())
    {
    case m::MegaNode::TYPE_UNKNOWN:
        if (show) cout << " TYPE_UNKNOWN" << endl;
        break;

    case m::MegaNode::TYPE_FILE:
        if (show) cout << endl;
        break;

    case m::MegaNode::TYPE_FOLDER:
    case m::MegaNode::TYPE_ROOT:
    case m::MegaNode::TYPE_INCOMING:
    case m::MegaNode::TYPE_RUBBISH:
        if (show && depth > 0) cout << endl;
        if (flags.recursive || depth == 0)
        {
            unique_ptr<m::MegaNodeList> children(g_megaApi->getChildren(node, flags.order));
            if (children) for (int i = 0; i < children->size(); ++i)
            {
                ls(children->get(i), basepath, flags, depth + 1);
            }
        }
        break;
    }
}

void exec_ls(ac::ACState& s)
{
    string orderstring;
    ls_flags flags;
    flags.recursive = s.extractflag("-recursive");
    flags.regexfilter = s.extractflagparam("-refilter", flags.regexfilterstring);
    flags.handle = s.extractflag("-handles");
    flags.ctime = s.extractflag("-ctime");
    flags.mtime = s.extractflag("-mtime");
    flags.size = s.extractflag("-size");
    flags.versions = s.extractflag("-versions");
    if (s.extractflagparam("-order", orderstring))
    {
        flags.order = std::stoi(orderstring);
    }

    if (flags.regexfilter)
    {
        flags.re = std::regex(flags.regexfilterstring);
    }

    if (auto node = GetNodeByPath(s.words[1].s))
    {
        string basepath = OwnStr(g_megaApi->getNodePath(node.get()));
        switch (node->getType())
        {
        case m::MegaNode::TYPE_FILE: basepath.clear(); break;
        case m::MegaNode::TYPE_FOLDER:
        case m::MegaNode::TYPE_INCOMING:
        case m::MegaNode::TYPE_RUBBISH: basepath += "/"; break;
        default:;
        }
        ls(node.get(), basepath, flags, 0);
    }
}

void exec_renamenode(ac::ACState& s)
{
    if (auto node = GetNodeByPath(s.words[1].s))
    {
        g_megaApi->renameNode(node.get(), s.words[2].s.c_str(), new OneShotRequestListener([](m::MegaApi*, m::MegaRequest *, m::MegaError* e)
        {
            check_err("renamenode", e, ReportResult);
        }));
    }
}

void exec_startupload(ac::ACState& s)
{
    if (auto node = GetNodeByPath(s.words[2].s))
    {
        g_megaApi->startUpload(s.words[1].s.c_str(), node.get(), new OneShotTransferListener([](m::MegaApi*, m::MegaTransfer*, m::MegaError* e)
        {
            check_err("startUpload", e, ReportResult);
        }));
    }
}

void exec_pushreceived(ac::ACState& s)
{
    bool beep = s.extractflag("-beep");

    if (s.words.size() == 2)
    {
        g_chatApi->pushReceived(beep, s_ch(s.words[1].s), new OneShotChatRequestListener([](c::MegaChatApi*, c::MegaChatRequest *, c::MegaChatError* e)
        {
            check_err("pushReceived (iOS style)", e, ReportResult);
        }));
    }
    else
    {
        g_chatApi->pushReceived(beep, new OneShotChatRequestListener([](c::MegaChatApi*, c::MegaChatRequest *, c::MegaChatError* e)
        {
            check_err("pushReceived (Android style)", e, ReportResult);
        }));
    }
}

void exec_getcloudstorageused(ac::ACState& s)
{
    g_megaApi->getCloudStorageUsed(new OneShotRequestListener([](m::MegaApi*, m::MegaRequest *r, m::MegaError* e)
    {
        if (check_err("getcloudstorageused", e, ReportFailure))
        {
            conlock(cout) << "Cloud storage used (locally calculated): " << r->getNumber() << endl;
        }
    }));
}

void exec_cp(ac::ACState& s)
{
    std::unique_ptr<m::MegaNode> srcnode(g_megaApi->getNodeByPath(s.words[1].s.c_str()));
    std::unique_ptr<m::MegaNode> dstnode(g_megaApi->getNodeByPath(s.words[2].s.c_str()));

    if (!srcnode)
    {
        conlock(cout) << "source not found" << endl;
    }
    else if (!dstnode)
    {
        conlock(cout) << "destination not found" << endl;
    }
    else if (dstnode->getType() <= m::MegaNode::TYPE_FILE)
    {
        conlock(cout) << "destination is not a folder" << endl;
    }
    else
    {
        g_megaApi->copyNode(srcnode.get(), dstnode.get(), new OneShotRequestListener([](m::MegaApi*, m::MegaRequest *, m::MegaError* e)
        {
            check_err("copyNode", e, ReportResult);
        }));
    }
}

void exec_mv(ac::ACState& s)
{
    string newname;
    bool rename = s.extractflagparam("-rename", newname);

    std::unique_ptr<m::MegaNode> srcnode(g_megaApi->getNodeByPath(s.words[1].s.c_str()));
    std::unique_ptr<m::MegaNode> dstnode(g_megaApi->getNodeByPath(s.words[2].s.c_str()));
                                                                                                                                                     
    if (!srcnode)
    {
        conlock(cout) << "source not found" << endl;
    }
    else if (!dstnode)
    {
        conlock(cout) << "destination not found" << endl;
    }
    else if (dstnode->getType() <= m::MegaNode::TYPE_FILE)
    {
        conlock(cout) << "destination is not a folder" << endl;
    }
    else
    {
        if (rename)
        {
            g_megaApi->moveNode(srcnode.get(), dstnode.get(), newname.c_str(), new OneShotRequestListener([](m::MegaApi*, m::MegaRequest *, m::MegaError* e)
            {
                check_err("moveNode", e, ReportResult);
            }));
        }
        else
        {
            g_megaApi->moveNode(srcnode.get(), dstnode.get(), new OneShotRequestListener([](m::MegaApi*, m::MegaRequest *, m::MegaError* e)
            {
                check_err("moveNode", e, ReportResult);
            }));
        }
    }
}

void PrintAchievements(m::MegaAchievementsDetails & ad)
{
    auto cl = conlock(cout);

    cl << "getBaseStorage: " << ad.getBaseStorage() << endl;

    int classes[] = {   m::MegaAchievementsDetails::MEGA_ACHIEVEMENT_WELCOME, 
                        m::MegaAchievementsDetails::MEGA_ACHIEVEMENT_INVITE, 
                        m::MegaAchievementsDetails::MEGA_ACHIEVEMENT_DESKTOP_INSTALL, 
                        m::MegaAchievementsDetails::MEGA_ACHIEVEMENT_MOBILE_INSTALL, 
                        m::MegaAchievementsDetails::MEGA_ACHIEVEMENT_ADD_PHONE };

    for (int i = 0; i < sizeof(classes) / sizeof(*classes); ++i)
    {
        cl << "class " << classes[i];
        cl << "  getClassStorage: " << ad.getClassStorage(classes[i]);
        cl << "  getClassTransfer: " << ad.getClassTransfer(classes[i]);
        cl << "  getClassExpire: " << ad.getClassExpire(classes[i]) << endl;
    }
    cl << "getAwardsCount: " << ad.getAwardsCount() << endl;
    for (unsigned i = 0; i < ad.getAwardsCount(); ++i)
    {
        cl << "Award " << i << endl;
        cl << "  getAwardClass: " << ad.getAwardClass(i) << endl;
        cl << "  getAwardId: " << ad.getAwardId(i) << endl;
        cl << "  getAwardTimestamp: " << ad.getAwardTimestamp(i) << endl;
        cl << "  getAwardExpirationTs: " << ad.getAwardExpirationTs(i) << endl;
        cl << "  getAwardClass: " << ad.getAwardClass(i) << endl;
        cl << "  getAwardEmails: <todo>" << endl; // << ad.getAwardEmails(i) << endl;
    }
    cl << "getRewardsCount: " << ad.getRewardsCount() << endl;
    for (int i = 0; i < ad.getRewardsCount(); ++i)
    {
        cl << "Reward " << i << endl;
        cl << "  getRewardAwardId: " << ad.getRewardAwardId(i) << endl;
        cl << "  getRewardStorage: " << ad.getRewardStorage(i) << endl;
        cl << "  getRewardTransfer: " << ad.getRewardTransfer(i) << endl;
        cl << "  getRewardStorageByAwardId: " << ad.getRewardStorageByAwardId(ad.getRewardAwardId(i)) << endl;
        cl << "  getRewardTransferByAwardId: " << ad.getRewardTransferByAwardId(ad.getRewardAwardId(i)) << endl;
        cl << "  getRewardExpire: " << ad.getRewardExpire(i) << endl;
    }
    cl << "currentStorage: " << ad.currentStorage() << endl;
    cl << "currentTransfer: " << ad.currentTransfer() << endl;
    cl << "currentStorageReferrals: " << ad.currentStorageReferrals() << endl;
    cl << "currentTransferReferrals: " << ad.currentTransferReferrals() << endl;
};

void exec_getaccountachievements(ac::ACState& s)
{
    auto listener = new OneShotRequestListener;
    listener->onRequestFinishFunc = [](m::MegaApi* api, m::MegaRequest *request, m::MegaError* e)
    {
        conlock(cout) << "getAccountAchievements Result: " << e->getErrorString() << endl;
        if (!e->getErrorCode())
        {
            unique_ptr<m::MegaAchievementsDetails> ad(request->getMegaAchievementsDetails());
            if (ad)
            {
                PrintAchievements(*ad);
            }
        }
    };

    g_megaApi->getAccountAchievements(listener);
}


void exec_getmegaachievements(ac::ACState& s)
{
    auto listener = new OneShotRequestListener;
    listener->onRequestFinishFunc = [](m::MegaApi* api, m::MegaRequest *request, m::MegaError* e)
    {
        conlock(cout) << "getAccountAchievements Result: " << e->getErrorString() << endl;
        if (!e->getErrorCode())
        {
            unique_ptr<m::MegaAchievementsDetails> ad(request->getMegaAchievementsDetails());
            if (ad)
            {
                PrintAchievements(*ad);
            }
        }
    };

    g_megaApi->getMegaAchievements(listener);
}

void exec_setCameraUploadsFolder(ac::ACState& s)
{
    std::unique_ptr<m::MegaNode> srcnode(g_megaApi->getNodeByPath(s.words[1].s.c_str()));
    
    if (!srcnode)
    {
        conlock(cout) << "Folder not found.";
    }
    else
    {
        g_megaApi->setCameraUploadsFolder(srcnode->getHandle(), new OneShotRequestListener([](m::MegaApi*, m::MegaRequest * r, m::MegaError* e)
        {
            conlock(cout) << "Camera upload folder request flag: " << r->getFlag() << endl;
            conlock(cout) << "Camera upload folder request handle: " << base64NodeHandle(r->getNodeHandle()) << endl;
            check_err("setCameraUploadsFolder", e, ReportResult);
        }));
    }

}

void exec_getCameraUploadsFolder(ac::ACState& s)
{
    g_megaApi->getCameraUploadsFolder(new OneShotRequestListener([](m::MegaApi*, m::MegaRequest *r, m::MegaError* e)
    {
        conlock(cout) << "Camera upload folder flag: " <<r->getFlag() << endl;
        if (check_err("getCameraUploadsFolder", e, ReportFailure))
        {
            unique_ptr<m::MegaNode> node(g_megaApi->getNodeByHandle(r->getNodeHandle()));
            if (!node)
            {
                conlock(cout) << "No node found by looking up handle: " << base64NodeHandle(r->getNodeHandle()) << endl;
            }
            else
            {
                conlock(cout) << "Camera upload folder: " << OwnStr(g_megaApi->getNodePath(node.get())) << endl;
            }
        }
    }));
}


void exec_setCameraUploadsFolderSecondary(ac::ACState& s)
{
    std::unique_ptr<m::MegaNode> srcnode(g_megaApi->getNodeByPath(s.words[1].s.c_str()));

    if (!srcnode)
    {
        conlock(cout) << "Folder not found.";
    }
    else
    {
        g_megaApi->setCameraUploadsFolderSecondary(srcnode->getHandle(), new OneShotRequestListener([](m::MegaApi*, m::MegaRequest *r, m::MegaError* e)
        {
            conlock(cout) << "Camera upload folder request flag: " << r->getFlag() << endl;
            conlock(cout) << "Camera upload folder request handle: " << base64NodeHandle(r->getNodeHandle()) << endl;
            check_err("setCameraUploadsFolderSecondary", e, ReportResult);
        }));
    }

}

void exec_getCameraUploadsFolderSecondary(ac::ACState& s)
{
    g_megaApi->getCameraUploadsFolderSecondary(new OneShotRequestListener([](m::MegaApi*, m::MegaRequest *r, m::MegaError* e)
    {
        conlock(cout) << "Camera upload folder flag: " << r->getFlag() << endl;
        if (check_err("getCameraUploadsFolderSecondary", e, ReportFailure))
        {
            unique_ptr<m::MegaNode> node(g_megaApi->getNodeByHandle(r->getNodeHandle()));
            if (!node)
            {
                conlock(cout) << "No node found by looking up handle: " << base64NodeHandle(r->getNodeHandle()) << endl;
            }
            else
            {
                conlock(cout) << "Camera upload folder (secondary): " << OwnStr(g_megaApi->getNodePath(node.get())) << endl;
            }
        }
    }));
}


void exec_getContact(ac::ACState& s)
{

    unique_ptr<m::MegaUser> user(g_megaApi->getContact(s.words[1].s.c_str()));
    if (user)
    {
        conlock(cout) << "found with handle: " << ch_s(user->getHandle()) << " timestamp: " << user->getTimestamp() << endl;
    }
    else
    {
        conlock(cout) << "No user found with that email" << endl;
    }
}



ac::ACN autocompleteSyntax()
{
    using namespace ac;
    unique_ptr<Either> p(new Either("      "));

    p->Add(exec_initanonymous, sequence(text("initanonymous")));
    p->Add(exec_login,      sequence(text("login"), either(sequence(param("email"), opt(param("password"))), param("session"), sequence(text("autoresume"), opt(param("id"))) )));
    p->Add(exec_logout, sequence(text("logout")));
    p->Add(exec_session,    sequence(text("session"), opt(sequence(text("autoresume"), opt(param("id")))) ));

    p->Add(exec_setonlinestatus,    sequence(text("setonlinestatus"), either(text("offline"), text("away"), text("online"), text("busy"))));
    p->Add(exec_setpresenceautoaway, sequence(text("setpresenceautoaway"), either(text("on"), text("off")), wholenumber(30)));
    p->Add(exec_setpresencepersist, sequence(text("setpresencepersist"), either(text("on"), text("off"))));
    p->Add(exec_signalpresenceperiod, sequence(text("signalpresenceperiod"), wholenumber(5)));
    p->Add(exec_getonlinestatus, sequence(text("getonlinestatus")));

    p->Add(exec_getuserfirstname,   sequence(text("getuserfirstname"), param("userid")));
    p->Add(exec_getuserlastname,    sequence(text("getuserlastname"), param("userid")));
    p->Add(exec_getuseremail,       sequence(text("getuseremail"), param("userid")));
    p->Add(exec_getcontactemail,    sequence(text("getcontactemail"), param("userid")));
    p->Add(exec_getuserhandlebyemail, sequence(text("getuserhandlebyemail"), param("email")));
    p->Add(exec_getmyuserhandle,      sequence(text("getmyuserhandle")));
    p->Add(exec_getmyfirstname,     sequence(text("getmyfirstname")));
    p->Add(exec_getmylastname,      sequence(text("getmylastname")));
    p->Add(exec_getmyfullname,      sequence(text("getmyfullname")));
    p->Add(exec_getmyemail,         sequence(text("getmyemail")));

    p->Add(exec_getchatrooms,       sequence(text("getchatrooms")));
    p->Add(exec_getchatroom,        sequence(text("getchatroom"), param("roomid")));
    p->Add(exec_getchatroombyuser,  sequence(text("getchatroombyuser"), param("userid")));
    p->Add(exec_getchatlistitems,   sequence(text("getchatlistitems")));
    p->Add(exec_getchatlistitem,    sequence(text("getchatlistitem"), param("roomid")));
    p->Add(exec_getunreadchats,     sequence(text("getunreadchats"), param("roomid")));
    p->Add(exec_getinactivechatlistitems, sequence(text("getinactivechatlistitems"), param("roomid")));
    p->Add(exec_getunreadchatlistitems, sequence(text("getunreadchatlistitems"), param("roomid")));
    p->Add(exec_getchathandlebyuser, sequence(text("getchathandlebyuser"), param("userid")));
    p->Add(exec_chatinfo,           sequence(text("chatinfo"), param("roomid")));

    p->Add(exec_createchat,         sequence(text("createchat"), opt(flag("-group")), repeat(param("userid"))));
    p->Add(exec_invitetochat,       sequence(text("invitetochat"), param("roomid"), param("userid")));
    p->Add(exec_removefromchat,     sequence(text("removefromchat"), param("roomid"), param("userid")));
    p->Add(exec_leavechat,          sequence(text("leavechat"), param("roomid")));
    p->Add(exec_updatechatpermissions, sequence(text("updatechatpermissions"), param("roomid"), param("userid")));
    p->Add(exec_truncatechat,       sequence(text("truncatechat"), param("roomid"), param("msgid")));
    p->Add(exec_clearchathistory,   sequence(text("clearchathistory"), param("roomid")));
    p->Add(exec_setchattitle,       sequence(text("setchattitle"), param("roomid"), param("title")));
    p->Add(exec_setRetentionTime,   sequence(text("setretentiontime"), param("roomid"), param("period")));
    p->Add(exec_getRetentionTime,   sequence(text("getretentiontime"), param("roomid")));

    p->Add(exec_openchatroom,       sequence(text("openchatroom"), param("roomid")));
    p->Add(exec_closechatroom,      sequence(text("closechatroom"), param("roomid")));
    p->Add(exec_loadmessages,       sequence(text("loadmessages"), param("roomid"), wholenumber(10), opt(either(text("human"), text("developer")))));
    p->Add(exec_reviewpublicchat,   sequence(text("rpc"), param("chatlink"), opt(wholenumber(5000))));
    p->Add(exec_isfullhistoryloaded, sequence(text("isfullhistoryloaded"), param("roomid")));
    p->Add(exec_getmessage,         sequence(text("getmessage"), param("roomid"), param("msgid")));
    p->Add(exec_getmanualsendingmessage, sequence(text("getmanualsendingmessage"), param("roomid"), param("tempmsgid")));
    p->Add(exec_sendmessage,        sequence(text("sendmessage"), param("roomid"), param("text")));
    p->Add(exec_attachcontacts,     sequence(text("attachcontacts"), param("roomid"), repeat(param("userid"))));
    p->Add(exec_attachnode,         sequence(text("attachnode"), param("roomid"), param("nodeid")));
    p->Add(exec_revokeattachmentmessage, sequence(text("revokeattachmentmessage"), param("roomid"), param("msgid")));
    p->Add(exec_editmessage,        sequence(text("editmessage"), param("roomid"), param("msgid"), param("text")));
    p->Add(exec_setmessageseen,     sequence(text("setmessageseen"), param("roomid"), param("msgid")));
    p->Add(exec_getLastMessageSeen, sequence(text("getLastMessageSeen"), param("roomid")));
    p->Add(exec_removeunsentmessage, sequence(text("removeunsentmessage"), param("roomid"), param("tempid")));
    p->Add(exec_sendtypingnotification, sequence(text("sendtypingnotification"), param("roomid")));
    p->Add(exec_ismessagereceptionconfirmationactive, sequence(text("ismessagereceptionconfirmationactive")));
    p->Add(exec_savecurrentstate, sequence(text("savecurrentstate")));

    p->Add(exec_openchatpreview,    sequence(text("openchatpreview"), param("chatlink")));
    p->Add(exec_closechatpreview,   sequence(text("closechatpreview"), param("chatid")));
     
#ifndef KARERE_DISABLE_WEBRTC
    p->Add(exec_getchatvideoindevices, sequence(text("getchatvideoindevices")));
    p->Add(exec_setchatvideoindevice, sequence(text("setchatvideoindevice"), param("device")));
    p->Add(exec_startchatcall, sequence(text("startchatcall"), param("roomid"), opt(either(text("true"), text("false")))));
    p->Add(exec_answerchatcall, sequence(text("answerchatcall"), param("roomid"), opt(either(text("true"), text("false")))));
    p->Add(exec_hangchatcall, sequence(text("hangchatcall"), param("roomid")));
    p->Add(exec_hangallchatcalls, sequence(text("hangallchatcalls")));
    p->Add(exec_enableaudio, sequence(text("enableaudio"), param("roomid")));
    p->Add(exec_disableaudio, sequence(text("disableaudio"), param("roomid")));
    p->Add(exec_enablevideo, sequence(text("enablevideo"), param("roomid")));
    p->Add(exec_disablevideo, sequence(text("disablevideo"), param("roomid")));
    p->Add(exec_loadaudiovideodevicelist, sequence(text("loadaudiovideodevicelist")));
    p->Add(exec_getchatcall, sequence(text("getchatcall"), param("roomid")));
    p->Add(exec_setignoredcall, sequence(text("setignoredcall"), param("roomid")));
    p->Add(exec_getchatcallbycallid, sequence(text("getchatcallbycallid"), param("callid")));
    p->Add(exec_getnumcalls, sequence(text("getnumcalls")));
    p->Add(exec_getchatcalls, sequence(text("getchatcalls")));
    p->Add(exec_getchatcallsids, sequence(text("getchatcallsids")));
#endif

    p->Add(exec_detail,     sequence(text("detail"), opt(either(text("high"), text("low")))));
#ifdef WIN32
    p->Add(exec_dos_unix,   sequence(text("autocomplete"), opt(either(text("unix"), text("dos")))));
#endif
    p->Add(exec_help,       sequence(either(text("help"), text("?"))));
#ifdef WIN32
    p->Add(exec_history,    sequence(text("history")));
#endif
    p->Add(exec_repeat,     sequence(text("repeat"), wholenumber(5), param("command")));
    p->Add(exec_quit,       sequence(either(text("quit"), text("q"))));
    p->Add(exec_quit,       sequence(text("exit")));

    // sdk level commands (intermediate layer of megacli commands)
    p->Add(exec_catchup, sequence(text("catchup"), opt(wholenumber(3))));
    p->Add(exec_smsverify, sequence(text("smsverify"), either(sequence(text("send"), param("phoneNumber"), opt(text("to"))), sequence(text("code"), param("code")), text("allowed"), text("phone"))));
    p->Add(exec_apiurl, sequence(text("apiurl"), param("url"), opt(param("disablepkp"))));
    p->Add(exec_getaccountachievements, sequence(text("getaccountachievements")));
    p->Add(exec_getmegaachievements, sequence(text("getmegaachievements")));

    p->Add(exec_recentactions, sequence(text("recentactions"), opt(sequence(param("days"), param("nodecount")))));
    p->Add(exec_getspecificaccountdetails, sequence(text("getspecificaccountdetails"), repeat(either(flag("-storage"), flag("-transfer"), flag("-pro")))));


    p->Add(exec_backgroundupload, sequence(text("backgroundupload"), either(
        sequence(text("new"), param("name")),
        sequence(text("resume"), param("name"), param("serializeddata")),
        sequence(text("analyse"), param("name"), localFSFile()),
        sequence(text("encrypt"), param("name"), localFSFile(), localFSFile(), param("startPos"), param("length"), either(text("false"), text("true"))),
        sequence(text("geturl"), param("name"), param("filesize")),
        sequence(text("serialize"), param("name")),
        sequence(text("upload"), param("url"), localFSFile()),
        sequence(text("putthumbnail"), param("name"), localFSFile()),
        sequence(text("putpreview"), param("name"), localFSFile()),
        sequence(text("setthumbnail"), param("name"), param("handle")),
        sequence(text("setpreview"), param("name"), param("handle")),
        sequence(text("setcoordinates"), param("name"), opt(flag("-shareable")), param("latitude"), param("longitude")),
        sequence(text("complete"), param("name"), param("nodename"), param("remoteparentpath"), param("fingerprint"), param("originalfingerprint"), param("uploadtoken")))));

    p->Add(exec_ensuremediainfo, sequence(text("ensuremediainfo")));

    p->Add(exec_getfingerprint, sequence(text("getfingerprint"), either(
        sequence(text("local"), localFSFile()),
        sequence(text("remote"), param("remotefile")),
        sequence(text("original"), param("remotefile")))));

    p->Add(exec_setthumbnailbyhandle, sequence(text("setthumbnailbyhandle"), param("remotepath"), param("attributehandle")));
    p->Add(exec_setpreviewbyhandle, sequence(text("setpreviewbyhandle"), param("remotepath"), param("attributehandle")));
    p->Add(exec_setnodecoordinates, sequence(text("setnodecoordinates"), param("remotepath"), param("latitude"), param("longitude")));
    p->Add(exec_setunshareablenodecoordinates, sequence(text("setunshareablenodecoordinates"), param("remotepath"), param("latitude"), param("longitude")));
    p->Add(exec_createthumbnail, sequence(text("createthumbnail"), opt(flag("-tempmegaapi")), opt(sequence(flag("-parallel"), param("count"))), localFSFile(), localFSFile()));
    p->Add(exec_createpreview, sequence(text("createpreview"), localFSFile(), localFSFile()));
    p->Add(exec_testAllocation, sequence(text("testAllocation"), param("count"), param("size")));
    p->Add(exec_getnodebypath, sequence(text("getnodebypath"), param("remotepath")));
    p->Add(exec_ls, sequence(text("ls"), repeat(either(flag("-recursive"), flag("-handles"), flag("-ctime"), flag("-mtime"), flag("-size"), flag("-versions"), sequence(flag("-order"), param("order")), sequence(flag("-refilter"), param("regex")))), param("path")));
    p->Add(exec_renamenode, sequence(text("renamenode"), param("remotepath"), param("newname")));
    p->Add(exec_startupload, sequence(text("startupload"), param("localpath"), param("remotepath")));

    p->Add(exec_pushreceived, sequence(text("pushreceived"), opt(flag("-beep")), opt(param("chatid"))));
    p->Add(exec_getcloudstorageused, sequence(text("getcloudstorageused")));

    p->Add(exec_cp, sequence(text("cp"), param("remotesrc"), param("remotedst")));
    p->Add(exec_mv, sequence(text("mv"), param("remotesrc"), param("remotedst"), opt(sequence(flag("-rename"), param("newname")))));

    p->Add(exec_setCameraUploadsFolder, sequence(text("setcamerauploadsfolder"), param("remotedst")));
    p->Add(exec_getCameraUploadsFolder, sequence(text("getcamerauploadsfolder")));
    p->Add(exec_setCameraUploadsFolderSecondary, sequence(text("setcamerauploadsfoldersecondary"), param("remotedst")));
    p->Add(exec_getCameraUploadsFolderSecondary, sequence(text("getcamerauploadsfoldersecondary")));

    p->Add(exec_getContact, sequence(text("getcontact"), param("email")));

    return p;
}


// execute command
static void process_line(const char* l)
{
    switch (prompt)
    {
    case PIN:
    {
        std::string pin = l;
        g_chatApi->init(NULL);
        g_megaApi->multiFactorAuthLogin(login.c_str(), password.c_str(), !pin.empty() ? pin.c_str() : NULL);
        {
            conlock(cout) << "\nLogging in with 2FA..." << endl << flush;
        }
        setprompt(NOPROMPT);
        return;
    }

    case LOGINPASSWORD:
        password = l;
        g_chatApi->init(NULL);
        g_megaApi->login(login.c_str(), password.c_str());
        {
            conlock(cout) << "\nLogging in..." << endl;
        }
        setprompt(NOPROMPT);
        return;

    case COMMAND:
        try
        {
            std::string consoleOutput;
            ac::autoExec(string(l), string::npos, autocompleteTemplate, false, consoleOutput, true); // todo: pass correct unixCompletions flag
            if (!consoleOutput.empty())
            {
                conlock(cout) << consoleOutput << flush;
            }
        }
        catch (exception& e)
        {
            conlock(cout) << "Command failed: " << e.what() << endl;
        }
        return;
    }
}

#ifndef NO_READLINE
#ifdef HAVE_AUTOCOMPLETE
char* longestCommonPrefix(ac::CompletionState& acs)
{
    string s = acs.completions[0].s;
    for (int i = acs.completions.size(); i--; )
    {
        for (unsigned j = 0; j < s.size() && j < acs.completions[i].s.size(); ++j)
        {
            if (s[j] != acs.completions[i].s[j])
            {
                s.erase(j, string::npos);
                break;
            }
        }
    }
    return strdup(s.c_str());
}

char** my_rl_completion(const char *, int , int end)
{
    rl_attempted_completion_over = 1;

    std::string line(rl_line_buffer, end);
    ac::CompletionState acs = ac::autoComplete(line, line.size(), autocompleteTemplate, true);

    if (acs.completions.empty())
    {
        return NULL;
    }

    if (acs.completions.size() == 1 && !acs.completions[0].couldExtend)
    {
        acs.completions[0].s += " ";
    }

    char** result = (char**)malloc((sizeof(char*)*(2 + acs.completions.size())));
    for (int i = acs.completions.size(); i--; )
    {
        result[i + 1] = strdup(acs.completions[i].s.c_str());
    }
    result[acs.completions.size() + 1] = NULL;
    result[0] = longestCommonPrefix(acs);
    //for (int i = 0; i <= acs.completions.size(); ++i)
    //{
    //    cout << "i " << i << ": " << result[i] << endl;
    //}
    rl_completion_suppress_append = true;
    rl_basic_word_break_characters = " \r\n";
    rl_completer_word_break_characters = strdup(" \r\n");
    rl_completer_quote_characters = "";
    rl_special_prefixes = "";
    return result;
}
#endif
#endif

int responseprogress = -1;  // loading progress of lengthy API responses

// main loop
void megaclc()
{
#ifndef NO_READLINE
    char *saved_line = NULL;
    int saved_point = 0;
    rl_attempted_completion_function = my_rl_completion;

    rl_save_prompt();
#elif defined(WIN32) && defined(NO_READLINE)
    static_cast<m::WinConsole*>(console.get())->setShellConsole(CP_UTF8, GetConsoleOutputCP());
#else
    #error non-windows platforms must use the readline library
#endif

    for (;;)
    {
        if (prompt == COMMAND)
        {

#if defined(WIN32) && defined(NO_READLINE)
            static_cast<m::WinConsole*>(console.get())->updateInputPrompt(prompts[COMMAND]);
#else
            rl_callback_handler_install(prompts[COMMAND], store_line);

            // display prompt
            if (saved_line)
            {
                rl_replace_line(saved_line, 0);
                free(saved_line);
            }

            rl_point = saved_point;
            rl_redisplay();
#endif
        }

        // command editing loop - exits when a line is submitted or the engine requires the CPU
        while (!line)
        {
            WaitMillisec(1);

#ifdef NO_READLINE
            {
                auto cl = conlock(cout);
                static_cast<m::WinConsole*>(console.get())->consolePeek();
                if (prompt >= COMMAND && !line)
                {
                    line = static_cast<m::WinConsole*>(console.get())->checkForCompletedInputLine();
                }
            }
#else
            if (prompt == COMMAND)
            {
                rl_callback_read_char();
            }
            else if (prompt > COMMAND)
            {
                console->readpwchar(pw_buf, sizeof pw_buf, &pw_buf_pos, &line);
            }
#endif

            if (g_signalPresencePeriod > 0 && g_signalPresenceLastSent + g_signalPresencePeriod < time(NULL))
            {
                g_chatApi->signalPresenceActivity(&g_chatListener);
                g_signalPresenceLastSent = time(NULL);
            }

            if (g_repeatPeriod > 0 && !g_repeatCommand.empty() && g_repeatLastSent + g_repeatPeriod < time(NULL))
            {
                g_repeatLastSent = time(NULL);
                process_line(g_repeatCommand.c_str());
            }

        }

#ifndef NO_READLINE
        // save line
        saved_point = rl_point;
        saved_line = rl_copy_text(0, rl_end);

        // remove prompt
        rl_save_prompt();
        rl_replace_line("", 0);
        rl_redisplay();
#endif

        if (line)
        {
            // execute user command
            process_line(line);
            free(line);
            line = NULL;

            if (quit_flag)
            {
                return;
            }
        }
    }
}


int main()
{
    m::SimpleLogger::setOutputClass(&g_apiLogger);

    const std::string megaclc_path = "temp_MEGAclc";
#ifdef WIN32
    const std::string basePath = (fs::u8path(getenv("USERPROFILE")) / megaclc_path).u8string();
    fs::create_directories(basePath);
#else
    // No std::fileystem before OSX10.15
    const std::string basePath = getenv("HOME") + std::string{'/'} + megaclc_path;
    mkdir(basePath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#endif

    g_megaApi.reset(new m::MegaApi("VmhTTToK", basePath.c_str(), "MEGAclc"));
    g_megaApi->addListener(&g_megaclcListener);
    g_megaApi->addGlobalListener(&g_globalListener);
    g_chatApi.reset(new c::MegaChatApi(g_megaApi.get()));
    g_chatApi->setLoggerObject(&g_chatLogger);
    g_chatApi->setLogLevel(c::MegaChatApi::LOG_LEVEL_MAX);
    g_chatApi->setLogWithColors(false);
    g_chatApi->setLogToConsole(false);
    g_chatApi->addChatListener(&g_clcListener);

    console.reset(new m::CONSOLE_CLASS);

    autocompleteTemplate = autocompleteSyntax();
#ifdef WIN32
    static_cast<m::WinConsole*>(console.get())->setAutocompleteSyntax(autocompleteTemplate);
#endif

    megaclc();

    g_megaApi->removeListener(&g_megaclcListener);
    g_megaApi->removeGlobalListener(&g_globalListener);
    g_chatApi->removeChatListener(&g_clcListener);

    g_chatApi.reset();
    g_megaApi.reset();
}

RoomListenerRecord::RoomListenerRecord() : listener(new CLCRoomListener) {}

void CLCRoomListener::onChatRoomUpdate(megachat::MegaChatApi *, megachat::MegaChatRoom *chat)
{
    g_chatLogger.logMsg(c::MegaChatApi::LOG_LEVEL_INFO, "Room " + ch_s(chat->getChatId()) + " updated");
}

void CLCRoomListener::onMessageLoaded(megachat::MegaChatApi *, megachat::MegaChatMessage *msg)
{
    reportMessage(room, msg, "loaded");
}

void CLCRoomListener::onMessageReceived(megachat::MegaChatApi *, megachat::MegaChatMessage *) {}

void CLCRoomListener::onMessageUpdate(megachat::MegaChatApi *, megachat::MegaChatMessage *msg) {}

void CLCRoomListener::onHistoryReloaded(megachat::MegaChatApi *, megachat::MegaChatRoom *chat) {}

void CLCRoomListener::onHistoryTruncatedByRetentionTime(c::MegaChatApi*, c::MegaChatMessage *msg) {}
