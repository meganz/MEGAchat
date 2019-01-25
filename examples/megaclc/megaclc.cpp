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

#include <iomanip>
#include <fstream>
#include <mutex>

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

void WaitMillisec(unsigned n)
{
#ifdef WIN32
    Sleep(n);
#else
    usleep(n*1000);
#endif
}
	

struct ConsoleLock
{
    static std::mutex outputlock;
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

std::mutex ConsoleLock::outputlock;

ConsoleLock conlock(std::ostream& o)
{
    // Returns a temporary object that has locked a mutex.  The temporary's destructor will unlock the object.
    // So you can get multithreaded non-interleaved console output with just conlock(cout) << "some " << "strings " << endl;
    // (as the temporary's destructor will run at the end of the outermost enclosing expression).
    // Or, move-assign the temporary to an lvalue to control when the destructor runs (to lock output over several statements).
    return ConsoleLock(o);
}




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

bool check_err(const char* opName, m::MegaError* e)
{
    bool success = e->getErrorCode() == c::MegaChatError::ERROR_OK;
    conlock(cout) << opName << (success ? " succeeded." : " failed. Error: " + string(e->getErrorString())) << endl;
    return success;
}

bool check_err(const char* opName, c::MegaChatError* e)
{
    bool success = e->getErrorCode() == c::MegaChatError::ERROR_OK;
    conlock(cout) << opName << (success ? " succeeded." : " failed. Error: " + string(e->getErrorString())) << endl;
    return success;
}


unique_ptr<m::Console> console;

static const char* prompts[] =
{
    "", "MEGAclc> ", "Password:"
};

enum prompttype
{
    NOPROMPT, COMMAND, LOGINPASSWORD
};

static prompttype prompt = COMMAND;

#if defined(WIN32) && defined(NO_READLINE)
static char pw_buf[512];  // double space for unicode
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

struct CLCListener : public c::MegaChatListener
{
    void onChatInitStateUpdate(c::MegaChatApi*, int newState) override
    {
        auto cl = conlock(cout);
        cout << "Status update : ";
        switch (newState)
        {
        case c::MegaChatApi::INIT_ERROR: cout << "INIT_ERROR" << endl; break;
        case c::MegaChatApi::INIT_WAITING_NEW_SESSION: cout << "INIT_WAITING_NEW_SESSION"; break;
        case c::MegaChatApi::INIT_OFFLINE_SESSION: cout << "INIT_OFFLINE_SESSION"; break;
        case c::MegaChatApi::INIT_ONLINE_SESSION: cout << "INIT_ONLINE_SESSION"; break;
        default: cout << "INIT_ERROR"; break;
        }
        cout << endl;
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
        conlock(cout) << "Node list updated:  " << (nodes ? nodes->size() : -1) << endl;
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

    const char* eventName(int i)
    {
        switch (i)
        {
        case m::MegaEvent::EVENT_COMMIT_DB: return "EVENT_COMMIT_DB";
        case m::MegaEvent::EVENT_ACCOUNT_CONFIRMATION: return "EVENT_ACCOUNT_CONFIRMATION";
        case m::MegaEvent::EVENT_CHANGE_TO_HTTPS: return "EVENT_CHANGE_TO_HTTPS";
        case m::MegaEvent::EVENT_DISCONNECT: return "EVENT_DISCONNECT";
        case m::MegaEvent::EVENT_ACCOUNT_BLOCKED: return "EVENT_ACCOUNT_BLOCKED";
        default: return "new event type";
        }
    }

    void onEvent(m::MegaApi*, m::MegaEvent *e) override
    {
        conlock(cout) << "Event: " << (e ? eventName(e->getType()) : "(null)") << endl;
    }

};

CLCListener g_clcListener;
MegaclcListener g_megaclcListener;
MegaclChatListener g_chatListener;
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
        }
        break;

    case m::MegaRequest::TYPE_FETCH_NODES:
        if (check_err("FetchNodes", e))
        {
            conlock(cout) << "Connecting to chat servers" << endl;
            guard.unlock();
            g_chatApi->connect(&g_chatListener);
        }
        break;

    default:
        break;
    }
}


bool oneOpenRoom(c::MegaChatHandle room);

bool g_detailHigh = false;

void reportMessage(c::MegaChatHandle room, c::MegaChatMessage *msg, const char* loadorreceive)
{
    auto cl = conlock(cout);

    if (!msg)
    {
        cout << "Room " << ch_s(room) << " - end of " << loadorreceive << " messages" << endl;
        return;
    }

    if (!g_detailHigh && msg->getType() == c::MegaChatMessage::TYPE_NORMAL && msg->getContent())
    {
        cout << ch_s(msg->getUserHandle());
        if (!oneOpenRoom(room))
        {
            cout << " (room " << ch_s(room) << ")";
        }
        cout << ": " << msg->getContent() << endl;
        return;
    }

    cout << "Room " << ch_s(room) << " " << loadorreceive << " message " << msg->getMsgIndex() << " from " << ch_s(msg->getUserHandle()) << " type: ";

    switch (msg->getType())
    {
    case c::MegaChatMessage::TYPE_INVALID: cout << "TYPE_INVALID"; break;
    case c::MegaChatMessage::TYPE_NORMAL: cout << "TYPE_NORMAL"; break;
    case c::MegaChatMessage::TYPE_ALTER_PARTICIPANTS: cout << "TYPE_ALTER_PARTICIPANTS"; break;
    case c::MegaChatMessage::TYPE_TRUNCATE: cout << "TYPE_TRUNCATE"; break;
    case c::MegaChatMessage::TYPE_PRIV_CHANGE: cout << "TYPE_PRIV_CHANGE"; break;
    case c::MegaChatMessage::TYPE_CHAT_TITLE: cout << "TYPE_CHAT_TITLE"; break;
    case c::MegaChatMessage::TYPE_NODE_ATTACHMENT: cout << "TYPE_NODE_ATTACHMENT"; break;
    case c::MegaChatMessage::TYPE_REVOKE_NODE_ATTACHMENT: cout << "TYPE_REVOKE_NODE_ATTACHMENT"; break;
    case c::MegaChatMessage::TYPE_CONTACT_ATTACHMENT: cout << "TYPE_CONTACT_ATTACHMENT"; break;
    default: cout << msg->getType();
    }

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

    //MegaChatHandle getUserHandle(unsigned int index) const;
    //const char *getUserName(unsigned int index) const;
    //const char *getUserEmail(unsigned int index) const;
    //mega::MegaNodeList *getMegaNodeList() const;
    cout << endl;

}


struct CLCRoomListener : public c::MegaChatRoomListener
{
    c::MegaChatHandle room = c::MEGACHAT_INVALID_HANDLE;

    void onChatRoomUpdate(c::MegaChatApi*, c::MegaChatRoom *chat) override
    {
        conlock(cout) << "Room " << ch_s(chat->getChatId()) << " updated" << endl;
    }

    void onMessageLoaded(c::MegaChatApi*, c::MegaChatMessage *msg) override
    {
        reportMessage(room, msg, "loaded");
    }

    virtual void onMessageReceived(c::MegaChatApi*, c::MegaChatMessage *msg)
    {
        reportMessage(room, msg, "received");
    }

    virtual void onMessageUpdate(c::MegaChatApi*, c::MegaChatMessage *msg)
    {
        reportMessage(room, msg, "updated");
    }

    virtual void onHistoryReloaded(c::MegaChatApi*, c::MegaChatRoom *chat)
    {
        conlock(cout) << "Room " << room << " notification that room " << chat->getChatId() << " is reloading" << endl;
    }
};


struct RoomListenerRecord
{
    bool open = false;
    unique_ptr<CLCRoomListener> listener;
    RoomListenerRecord() : listener(new CLCRoomListener) {}
};

map<c::MegaChatHandle, RoomListenerRecord> g_roomListeners;

bool oneOpenRoom(c::MegaChatHandle room)
{
    return g_roomListeners.size() == 1 && g_roomListeners.begin()->first == room;
}


static bool quit_flag = false;
static string login;

void exec_login(ac::ACState& s)
{
    if (!g_megaApi->isLoggedIn())
    {
        bool hasemail = s.words[1].s.find_first_of('@') != string::npos;
        if (s.words.size() == 3 && hasemail)
        {
            // full account login
            {
                conlock(cout) << "Initiating login attempt..." << endl;
            }
            g_chatApi->init(NULL);
            g_megaApi->login(s.words[1].s.c_str(), s.words[2].s.c_str());
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

    g_chatApi->getUserFirstname(userhandle, &g_chatListener);
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

    g_chatApi->getUserLastname(userhandle, &g_chatListener);
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


void exec_loadmessages(ac::ACState& s)
{
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

void exec_getchataudioindevices(ac::ACState&)
{
    unique_ptr<m::MegaStringList> audioDevices(g_chatApi->getChatAudioInDevices());
    for (int i = 0; i < audioDevices->size(); ++i)
    {
        cout << audioDevices->get(i) << endl;
    }
}

void exec_getchatvideoindevices(ac::ACState&)
{
    unique_ptr<m::MegaStringList> videoDevices(g_chatApi->getChatVideoInDevices());
    for (int i = 0; i < videoDevices->size(); ++i)
    {
        cout << videoDevices->get(i) << endl;
    }
}

void exec_setchataudioindevice(ac::ACState& s)
{
    if (!g_chatApi->setChatAudioInDevice(s.words[1].s.c_str()))
    {
        cout << "setChatAudioInDevice failed" << endl;
    }
}

void exec_setchatvideoindevice(ac::ACState& s)
{
    if (!g_chatApi->setChatVideoInDevice(s.words[1].s.c_str()))
    {
        cout << "setChatVideoInDevice failed" << endl;
    }
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

class OneShotRequestListener : public m::MegaRequestListener
{
public:
    std::function<void(m::MegaApi* api, m::MegaRequest *request)> onRequestStartFunc;
    std::function<void(m::MegaApi* api, m::MegaRequest *request, m::MegaError* e)> onRequestFinishFunc;
    std::function<void(m::MegaApi*api, m::MegaRequest *request)> onRequestUpdateFunc;
    std::function<void(m::MegaApi *api, m::MegaRequest *request, m::MegaError* error)> onRequestTemporaryErrorFunc;

    void onRequestStart(m::MegaApi* api, m::MegaRequest *request) override
    {
        if (onRequestStartFunc) onRequestStartFunc(api, request);
    }

    void onRequestFinish(m::MegaApi* api, m::MegaRequest *request, m::MegaError* e) override
    {
        if (onRequestFinishFunc) onRequestFinishFunc(api, request, e);
        delete this;  // one-shot is done so auto-delete
    }

    void onRequestUpdate(m::MegaApi*api, m::MegaRequest *request) override
    {
        if (onRequestUpdateFunc) onRequestUpdateFunc(api, request);
    }

    void onRequestTemporaryError(m::MegaApi *api, m::MegaRequest *request, m::MegaError* error) override
    {
        if (onRequestTemporaryErrorFunc) onRequestTemporaryErrorFunc(api, request, error);
    }
};


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
            conlock(cout) << "Re-fetching nodes due to change of APIURL" << endl;

            setprompt(NOPROMPT);

            auto listener = new OneShotRequestListener;
            listener->onRequestFinishFunc = [](m::MegaApi*, m::MegaRequest *, m::MegaError* e) 
            {
                conlock(cout) << "Fetchnodes finished: " << e->getErrorString() << endl;
                setprompt(COMMAND);
            };

            g_megaApi->fetchNodes(listener);
        }
    }
}


ac::ACN autocompleteSyntax()
{
    using namespace ac;
    unique_ptr<Either> p(new Either("      "));

    p->Add(exec_login,      sequence(text("login"), either(sequence(param("email"), opt(param("password"))), param("session"), sequence(text("autoresume"), opt(param("id"))) )));
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
    p->Add(exec_createchat,         sequence(text("createchat"), opt(flag("-group")), repeat(param("userid"))));
    p->Add(exec_invitetochat,       sequence(text("invitetochat"), param("roomid"), param("userid")));
    p->Add(exec_removefromchat,     sequence(text("removefromchat"), param("roomid"), param("userid")));
    p->Add(exec_leavechat,          sequence(text("leavechat"), param("roomid")));
    p->Add(exec_updatechatpermissions, sequence(text("updatechatpermissions"), param("roomid"), param("userid")));
    p->Add(exec_truncatechat,       sequence(text("truncatechat"), param("roomid"), param("msgid")));
    p->Add(exec_clearchathistory,   sequence(text("clearchathistory"), param("roomid")));
    p->Add(exec_setchattitle,       sequence(text("setchattitle"), param("roomid"), param("title")));
    p->Add(exec_openchatroom,       sequence(text("openchatroom"), param("roomid")));
    p->Add(exec_closechatroom,      sequence(text("closechatroom"), param("roomid")));
    p->Add(exec_loadmessages,       sequence(text("loadmessages"), param("roomid"), wholenumber(10)));
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
     
#ifndef KARERE_DISABLE_WEBRTC
    p->Add(exec_getchataudioindevices, sequence(text("getchataudioindevices")));
    p->Add(exec_getchatvideoindevices, sequence(text("getchatvideoindevices")));
    p->Add(exec_setchataudioindevice, sequence(text("setchataudioindevice"), param("device")));
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
    p->Add(exec_apiurl, sequence(text("apiurl"), param("url"), opt(param("disablepkp"))));
    
    return p;
}


// execute command
static void process_line(const char* l)
{
    switch (prompt)
    {
    case LOGINPASSWORD:
        g_chatApi->init(NULL);
        g_megaApi->login(login.c_str(), l);
        {
            conlock(cout) << "\nLogging in..." << endl;
        }
        setprompt(COMMAND);
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


class MegaCLLogger : public m::Logger {
public:
    virtual void log(const char *, int loglevel, const char *, const char *message)
    {
#ifdef _WIN32
        OutputDebugStringA(message);
        OutputDebugStringA("\r\n");
#endif

        if (loglevel <= m::logWarning)
        {
            auto cl = conlock(cout);
            cl << message;
            if (*message && message[strlen(message) - 1] != '\n')
            {
                cout << endl;
            }
        }
    }
};

MegaCLLogger g_apiLogger;

struct MegaclcChatChatLogger : public c::MegaChatLogger
{
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
        if (loglevel <= c::MegaChatApi::LOG_LEVEL_WARNING)
        {
            auto cl = conlock(cout);
            cl << message;
            if (*message && message[strlen(message) - 1] != '\n')
            {
                cout << endl;
            }
        }
    }
};

MegaclcChatChatLogger g_chatLogger;

int main()
{
#ifdef _WIN32
    m::SimpleLogger::setLogLevel(m::logMax);  // warning and stronger to console; info and lesser to debug output
    m::SimpleLogger::setOutputClass(&g_apiLogger);
#else
    m::SimpleLogger::setAllOutputs(&cout);
#endif 

#ifdef WIN32
    fs::path basePath = fs::u8path(getenv("USERPROFILE"));
#else
    fs::path basePath = fs::u8path(getenv("HOME"));
#endif
    basePath /= "temp_MEGAclc";
    fs::create_directory(basePath);

    g_megaApi.reset(new m::MegaApi("VmhTTToK", basePath.u8string().c_str(), "MEGAclc"));
    g_megaApi->addListener(&g_megaclcListener);
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
    g_chatApi.reset();
    g_megaApi.reset();
}
