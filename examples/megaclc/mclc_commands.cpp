#include "mclc_commands.h"

#include "mclc_autocompletion.h"
#include "mclc_chat_and_call_actions.h"
#include "mclc_general_utils.h"
#include "mclc_globals.h"
#include "mclc_listeners.h"
#include "mclc_logging.h"
#include "mclc_reports.h"

#include <async_utils.h>
#include <karereId.h>
namespace k = ::karere;

#include <regex>

namespace mclc::clc_cmds
{

using namespace mclc::clc_global;
using namespace mclc::clc_log;
using namespace mclc::clc_prompt;
using namespace mclc::clc_listen;
using namespace mclc::str_utils;
using namespace mclc::path_utils;
using namespace mclc::clc_time;
using namespace mclc::clc_report;
using namespace mclc::clc_ac;
using namespace mclc::clc_console;

using m::logDebug;
using m::logError;
using m::logInfo;
using m::logMax;
using m::logWarning;
using m::SimpleLogger;

void exec_initanonymous(ac::ACState&)
{
    if (g_chatApi->getInitState() == c::MegaChatApi::INIT_NOT_DONE)
    {
        g_chatApi->initAnonymous();
    }
    else
    {
        conlock(std::cout) << "Already initialized. Please log out first." << std::endl;
    }
}

void exec_login(ac::ACState& s)
{
    if (g_chatApi->getInitState() == c::MegaChatApi::INIT_NOT_DONE)
    {
        bool hasemail = s.words[1].s.find_first_of('@') != std::string::npos;
        if (s.words.size() == 3 && hasemail)
        {
            // full account login
            {
                conlock(std::cout) << "Initiating login attempt..." << std::endl;
            }
            g_chatApi->init(NULL);
            g_login = s.words[1].s;
            g_password = s.words[2].s;

            // Block prompt until the request has finished
            setprompt(NOPROMPT);
            g_megaApi->login(g_login.c_str(), g_password.c_str());
        }
        else if (s.words.size() == 2 && hasemail)
        {
            g_login = s.words[1].s;
            setprompt(LOGINPASSWORD);
        }
        else if ((s.words.size() == 2) ||
                 (s.words.size() == 3 && !hasemail && s.words[1].s == "autoresume"))
        {
            std::string session, filename = "mega_autoresume_session" +
                                            (s.words.size() == 3 ? "_" + s.words[2].s : "");
            std::ifstream file(filename.c_str());
            file >> session;
            if (file.is_open() && session.size())
            {
                conlock(std::cout) << "Resuming session..." << std::endl;
                g_chatApi->init(session.c_str());
                return g_megaApi->fastLogin(session.c_str());
            }
            conlock(std::cout) << "Failed to get a valid session id from file " << filename
                               << std::endl;
        }
        else if (s.words.size() == 2 && s.words[1].s.size() < 64 * 4 / 3)
        {
            {
                conlock(std::cout) << "Resuming session..." << std::endl;
            }
            g_chatApi->init(s.words[1].s.c_str());
            g_megaApi->fastLogin(s.words[1].s.c_str());
        }
        else
        {
            conlock(std::cout) << s.selectedSyntax << std::endl;
        }
    }
    else
    {
        conlock(std::cout) << "Already logged in. Please log out first." << std::endl;
    }
}

void exec_logout(ac::ACState&)
{
    std::unique_ptr<const char[]> session(g_megaApi->dumpSession());
    if (g_chatApi->getInitState() == c::MegaChatApi::INIT_ANONYMOUS)
    {
        g_chatApi->logout();
    }
    else if (g_chatApi->getInitState() != c::MegaChatApi::INIT_NOT_DONE)
    {
        setprompt(NOPROMPT);
#ifdef ENABLE_SYNC
        g_megaApi->logout(false, nullptr);
#else
        g_megaApi->logout();
#endif
    }
    else
    {
        conlock(std::cout) << "Not logged in." << std::endl;
    }
}

void exec_session(ac::ACState& s)
{
    std::unique_ptr<const char[]> session(g_megaApi->dumpSession());
    if (session)
    {
        if (s.words.size() > 1 && s.words[1].s == "autoresume")
        {
            std::string filename =
                "mega_autoresume_session" + (s.words.size() == 3 ? "_" + s.words[2].s : "");
            std::ofstream file(filename.c_str());
            if (file.fail() || !file.is_open())
            {
                conlock(std::cout) << "could not open file: " << filename << std::endl;
            }
            else
            {
                file << session.get();
                conlock(std::cout)
                    << "Your (secret) session is saved in file '" << filename << "'" << std::endl;
            }
        }
        else
        {
            conlock(std::cout) << "Your (secret) session is: " << session.get() << std::endl;
        }
    }
    else
    {
        conlock(std::cout) << "Not logged in." << std::endl;
    }
}

void exec_debug(ac::ACState& s)
{
    // Defaults
    SimpleLogger::setLogLevel(logWarning);
    g_debugOutpuWriter.disableLogToConsole();
    g_debugOutpuWriter.disableLogToFile();

    auto levelStrToInt = [](const std::string& s) -> m::LogLevel
    {
        if (s == "all")
            return m::logMax;
        if (s == "debug")
            return m::logDebug;
        if (s == "info")
            return m::logInfo;
        if (s == "warning")
            return m::logWarning;
        if (s == "error")
            return m::logError;
        return m::logFatal;
    };

    bool createPidDir = s.extractflag("-pid");
    if (s.extractflag("-noconsole"))
    {
        g_debugOutpuWriter.disableLogToConsole();
    }
    std::string logLevelStr;
    if (s.extractflagparam("-console", logLevelStr))
    {
        auto logLevel = levelStrToInt(logLevelStr);
        SimpleLogger::setLogLevel(logLevel);
        g_debugOutpuWriter.enableLogToConsole();
        g_debugOutpuWriter.setConsoleLogLevel(logLevel);
    }

    if (s.extractflag("-nofile"))
    {
        g_debugOutpuWriter.disableLogToFile();
    }
    if (s.extractflagparam("-file", logLevelStr))
    {
        auto logLevel = levelStrToInt(logLevelStr);
        assert(s.words.size() == 2); // At this point only the filename should remain
        fs::path filePath(s.words[1].s);
        if (createPidDir)
        {
            fs::path auxPath = filePath.parent_path() / std::to_string(path_utils::getProcessId());

            if (!fs::exists(auxPath))
            {
                fs::create_directories(auxPath);
            }
            filePath = auxPath / filePath.filename();
        }
        g_debugOutpuWriter.enableLogToFile(filePath);
        g_debugOutpuWriter.setFileLogLevel(logLevel);
    }

    // Feedback
    auto out = conlock(std::cout);
    if (g_debugOutpuWriter.isLoggingToConsole())
    {
        out << "Logging to console with level: " << g_debugOutpuWriter.getConsoleLogLevel() << "\n";
    }
    else
    {
        out << "Not logging to console\n";
    }
    if (g_debugOutpuWriter.isLoggingToFile())
    {
        out << "Logging to file (" << g_debugOutpuWriter.getLogFileName()
            << ") with level: " << g_debugOutpuWriter.getFileLogLevel() << "\n";
    }
    else
    {
        out << "Not logging to file\n";
    }
}

void exec_easy_debug(ac::ACState& s)
{
    std::string line = "debug -console warning -file all " + s.words[1].s;
    process_line(line.c_str());
}

void exec_setonlinestatus(ac::ACState& s)
{
    assert(s.words.size() == 2);
    int status;
    if (s.words[1].s == "offline")
        status = c::MegaChatApi::STATUS_OFFLINE;
    else if (s.words[1].s == "away")
        status = c::MegaChatApi::STATUS_AWAY;
    else if (s.words[1].s == "online")
        status = c::MegaChatApi::STATUS_ONLINE;
    else if (s.words[1].s == "busy")
        status = c::MegaChatApi::STATUS_BUSY;
    else
    {
        conlock(std::cout) << s.selectedSyntax << std::endl;
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
    auto cl = conlock(std::cout);
    switch (g_chatApi->getOnlineStatus())
    {
        case c::MegaChatApi::STATUS_OFFLINE:
            std::cout << "offline" << std::endl;
            break;
        case c::MegaChatApi::STATUS_AWAY:
            std::cout << "away" << std::endl;
            break;
        case c::MegaChatApi::STATUS_ONLINE:
            std::cout << "online" << std::endl;
            break;
        case c::MegaChatApi::STATUS_BUSY:
            std::cout << "busy" << std::endl;
            break;
        default:
            std::cout << g_chatApi->getOnlineStatus() << std::endl;
            break;
    }
}

void exec_setbackgroundstatus(ac::ACState& s)
{
    g_chatListener.onFinish(c::MegaChatRequest::TYPE_SET_BACKGROUND_STATUS,
                            [](CLCFinishInfo& f)
                            {
                                if (check_err("SetBackgroundStatus", f.e))
                                {
                                    conlock(std::cout)
                                        << " background: " << f.request->getFlag() << std::endl;
                                }
                            });

    g_chatApi->setBackgroundStatus(s.words[1].s == "on", &g_chatListener);
}

void exec_getuserfirstname(ac::ACState& s)
{
    c::MegaChatHandle userhandle(s_ch(s.words[1].s));

    g_chatListener.onFinish(c::MegaChatRequest::TYPE_GET_FIRSTNAME,
                            [userhandle](CLCFinishInfo& f)
                            {
                                if (check_err("getUserFirstname", f.e))
                                {
                                    conlock(std::cout) << ch_s(userhandle) << " -> "
                                                       << f.request->getText() << std::endl;
                                }
                            });

    g_chatApi->getUserFirstname(userhandle, NULL, &g_chatListener);
}

void exec_getuserlastname(ac::ACState& s)
{
    c::MegaChatHandle userhandle(s_ch(s.words[1].s));

    g_chatListener.onFinish(c::MegaChatRequest::TYPE_GET_LASTNAME,
                            [userhandle](CLCFinishInfo& f)
                            {
                                if (check_err("getUserLastname", f.e))
                                {
                                    conlock(std::cout) << ch_s(userhandle) << " -> "
                                                       << f.request->getText() << std::endl;
                                }
                            });

    g_chatApi->getUserLastname(userhandle, NULL, &g_chatListener);
}

void exec_getuseremail(ac::ACState& s)
{
    c::MegaChatHandle userhandle(s_ch(s.words[1].s));

    g_chatListener.onFinish(c::MegaChatRequest::TYPE_GET_EMAIL,
                            [userhandle](CLCFinishInfo& f)
                            {
                                if (check_err("getUserEmail", f.e))
                                {
                                    conlock(std::cout) << ch_s(userhandle) << " -> "
                                                       << f.request->getText() << std::endl;
                                }
                            });

    g_chatApi->getUserEmail(userhandle, &g_chatListener);
}

void exec_getcontactemail(ac::ACState& s)
{
    c::MegaChatHandle userhandle(s_ch(s.words[1].s));
    std::unique_ptr<char[]> email(g_chatApi->getContactEmail(userhandle));

    conlock(std::cout) << ch_s(userhandle) << " -> "
                       << (email ? email.get() : "<no contact relationship>") << std::endl;
}

void exec_getuserhandlebyemail(ac::ACState& s)
{
    c::MegaChatHandle userhandle = g_chatApi->getUserHandleByEmail(s.words[1].s.c_str());

    conlock(std::cout) << s.words[1].s << " -> " << ch_s(userhandle) << std::endl;
}

void exec_getmyuserhandle(ac::ACState&)
{
    conlock(std::cout) << ch_s(g_chatApi->getMyUserHandle()) << std::endl;
}

void exec_getmyfirstname(ac::ACState&)
{
    std::unique_ptr<char[]> t(g_chatApi->getMyFirstname());

    conlock(std::cout) << (t ? t.get() : "<no result>") << std::endl;
}

void exec_getmylastname(ac::ACState&)
{
    std::unique_ptr<char[]> t(g_chatApi->getMyLastname());

    conlock(std::cout) << (t ? t.get() : "<no result>") << std::endl;
}

void exec_getmyfullname(ac::ACState&)
{
    std::unique_ptr<char[]> t(g_chatApi->getMyFullname());

    conlock(std::cout) << (t ? t.get() : "<no result>") << std::endl;
}

void exec_getmyemail(ac::ACState&)
{
    std::unique_ptr<char[]> t(g_chatApi->getMyEmail());

    conlock(std::cout) << (t ? t.get() : "<no result>") << std::endl;
}

static std::string chatDetails(const c::MegaChatRoom& cr)
{
    std::stringstream s;

    s << "title: " << (cr.getTitle() ? cr.getTitle() : "") << " handle: " << ch_s(cr.getChatId())
      << " priv:" << cr.privToString(cr.getOwnPrivilege())
      << " s:" << (cr.isGroup() ? " isGroup " : " ") << "peers: ";
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
    std::unique_ptr<c::MegaChatRoomList> crl(g_chatApi->getChatRooms());
    if (crl)
    {
        auto cl = conlock(std::cout);
        for (unsigned i = 0; i < crl->size(); ++i)
        {
            if (const c::MegaChatRoom* cr = crl->get(i))
            {
                std::cout << chatDetails(*cr) << std::endl;
            }
        }
    }
}

void exec_getchatroom(ac::ACState& s)
{
    c::MegaChatHandle h = s_ch(s.words[1].s);
    std::unique_ptr<c::MegaChatRoom> p(g_chatApi->getChatRoom(h));

    conlock(std::cout) << (p ? chatDetails(*p) : "not found") << std::endl;
}

void exec_getchatroombyuser(ac::ACState& s)
{
    c::MegaChatHandle h = s_ch(s.words[1].s);
    std::unique_ptr<c::MegaChatRoom> p(g_chatApi->getChatRoomByUser(h));

    conlock(std::cout) << (p ? chatDetails(*p) : "not found") << std::endl;
}

static std::string chatlistDetails(const c::MegaChatListItem& cli)
{
    std::ostringstream s;

    s << "title: " << (cli.getTitle() ? cli.getTitle() : "") << " handle: " << ch_s(cli.getChatId())
      << " priv:" << c::MegaChatRoom::privToString(cli.getOwnPrivilege()) << " "
      << (cli.isGroup() ? " isGroup " : " ") << " " << (cli.isActive() ? " isActive " : " ");

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
        s << " last: " << str << " (type " << cli.getLastMessageType() << " from "
          << ch_s(cli.getLastMessageSender()) << ")";
    }
    return s.str();
};

void exec_getchatlistitems(ac::ACState&)
{
    std::unique_ptr<c::MegaChatListItemList> clil(g_chatApi->getChatListItems());
    if (clil)
    {
        auto cl = conlock(std::cout);
        for (unsigned i = 0; i < clil->size(); ++i)
        {
            if (const c::MegaChatListItem* cli = clil->get(i))
            {
                std::cout << chatlistDetails(*cli) << std::endl;
            }
        }
    }
}

void exec_getchatlistitem(ac::ACState& s)
{
    c::MegaChatHandle h = s_ch(s.words[1].s);
    std::unique_ptr<c::MegaChatListItem> p(g_chatApi->getChatListItem(h));

    conlock(std::cout) << (p ? chatlistDetails(*p) : "not found") << std::endl;
}

void exec_getunreadchats(ac::ACState&)
{
    conlock(std::cout) << "unread message count: " << g_chatApi->getUnreadChats() << std::endl;
}

void exec_getactivechatlistitems(ac::ACState&)
{
    std::unique_ptr<c::MegaChatListItemList> clil(g_chatApi->getActiveChatListItems());
    if (clil)
    {
        auto cl = conlock(std::cout);
        for (unsigned i = 0; i < clil->size(); ++i)
        {
            if (const c::MegaChatListItem* cli = clil->get(i))
            {
                std::cout << chatlistDetails(*cli) << std::endl;
            }
        }
    }
}

void exec_getinactivechatlistitems(ac::ACState&)
{
    std::unique_ptr<c::MegaChatListItemList> clil(g_chatApi->getInactiveChatListItems());
    if (clil)
    {
        auto cl = conlock(std::cout);
        for (unsigned i = 0; i < clil->size(); ++i)
        {
            if (const c::MegaChatListItem* cli = clil->get(i))
            {
                std::cout << chatlistDetails(*cli) << std::endl;
            }
        }
    }
}

void exec_getunreadchatlistitems(ac::ACState&)
{
    std::unique_ptr<c::MegaChatListItemList> clil(g_chatApi->getUnreadChatListItems());
    if (clil)
    {
        auto cl = conlock(std::cout);
        for (unsigned i = 0; i < clil->size(); ++i)
        {
            if (const c::MegaChatListItem* cli = clil->get(i))
            {
                std::cout << chatlistDetails(*cli) << std::endl;
            }
        }
    }
}

static void printChatInfoFromCache(const c::MegaChatRoom* room)
{
    conlock(std::cout) << "Chat ID: " << ch_s(room->getChatId()) << std::endl;
    conlock(std::cout) << "\tTitle: " << room->getTitle() << std::endl;
    conlock(std::cout) << "\tGroup chat: " << ((room->isGroup()) ? "yes" : "no") << std::endl;
    conlock(std::cout) << "\tPublic chat: " << ((room->isPublic()) ? "yes" : "no") << std::endl;
    conlock(std::cout) << "\tPreview mode: " << ((room->isPreview()) ? "yes" : "no") << std::endl;
    conlock(std::cout) << "\tOwn privilege: "
                       << c::MegaChatRoom::privToString(room->getOwnPrivilege()) << std::endl;
    conlock(std::cout) << "\tCreation ts: " << room->getCreationTs() << std::endl;
    conlock(std::cout) << "\tArchived: " << ((room->isArchived()) ? "yes" : "no") << std::endl;
    conlock(std::cout) << "\t" << room->getPeerCount() << " participants in chat:" << std::endl;
    for (unsigned i = 0; i < room->getPeerCount(); i++)
    {
        c::MegaChatHandle uh = room->getPeerHandle(i);
        conlock(std::cout)
            << "\t\t" << ch_s(uh) << "\t"
            << std::unique_ptr<const char[]>(g_chatApi->getUserFullnameFromCache(uh)).get();
        auto userEmailFromCache =
            std::unique_ptr<const char[]>(g_chatApi->getUserEmailFromCache(uh));
        if (userEmailFromCache)
        {
            conlock(std::cout) << " (" << userEmailFromCache.get() << ")";
        }
        conlock(std::cout) << "\tPriv: " << c::MegaChatRoom::privToString(room->getPeerPrivilege(i))
                           << std::endl;
    }
}

unsigned int g_remainingPrints;
std::mutex g_mutexPrintChatInfo;
std::condition_variable g_cvChatInfoPrinted;

static void printChatInfo(const c::MegaChatRoom* room)
{
    if (!room)
    {
        conlock(std::cout) << "Room not found" << std::endl;
    }
    else
    {
        // for the sake of keeping the order there are two iterations over the peers in the chat
        auto missingPeersList =
            std::unique_ptr<m::MegaHandleList>(m::MegaHandleList::createInstance());
        unsigned int totalPeers = room->getPeerCount();
        for (unsigned int peerIdx = 0; peerIdx < totalPeers; ++peerIdx)
        {
            auto uh = room->getPeerHandle(peerIdx);
            auto userCached =
                std::unique_ptr<const char[]>(g_chatApi->getUserFirstnameFromCache(uh));
            if (!userCached)
            {
                missingPeersList->addMegaHandle(uh);
            }
        }

        if (missingPeersList->size())
        {
            // lk it's already locked in exec_chatinfo
            ++g_remainingPrints;

            auto allUserDataReceivedListener = new OneShotChatRequestListener(
                [room](c::MegaChatApi*, c::MegaChatRequest*, c::MegaChatError* e)
                {
                    std::unique_lock<std::mutex> lk(g_mutexPrintChatInfo);
                    if (check_err("checkLoadUAForChatInfo", e))
                    {
                        printChatInfoFromCache(room);
                    }
                    --g_remainingPrints;
                    lk.unlock();
                    g_cvChatInfoPrinted.notify_one();
                });
            g_chatApi->loadUserAttributes(room->getChatId(),
                                          missingPeersList.get(),
                                          allUserDataReceivedListener);
        }
        else
        {
            printChatInfoFromCache(room);
        }
    }
}

void exec_chatinfo(ac::ACState& s)
{
    std::unique_ptr<c::MegaChatRoomList> chats;
    std::unique_ptr<c::MegaChatRoom> room;

    std::unique_lock<std::mutex> lk(g_mutexPrintChatInfo);
    g_remainingPrints = 0;
    if (s.words.size() == 1) // print all chats
    {
        chats.reset(g_chatApi->getChatRooms());
        for (unsigned int i = 0; i < chats->size(); i++)
        {
            printChatInfo(chats->get(i));
        }
    }
    else if (s.words.size() == 2)
    {
        c::MegaChatHandle chatid = s_ch(s.words[1].s);
        room.reset(g_chatApi->getChatRoom(chatid));
        printChatInfo(room.get());
    }
    else // just in case the parameter precon check changes at some point
    {
        conlock(std::cout) << "Incorrect number of parameters. Check help." << std::endl;
        return;
    }

    if (g_remainingPrints && !g_cvChatInfoPrinted.wait_for(lk,
                                                           std::chrono::milliseconds(500),
                                                           []
                                                           {
                                                               return !g_remainingPrints;
                                                           }))
    {
        conlock(std::cout) << "Timeout on request to get chat information" << std::endl;
    }
}

void exec_getchathandlebyuser(ac::ACState& s)
{
    c::MegaChatHandle h = s_ch(s.words[1].s);
    c::MegaChatHandle h2 = g_chatApi->getChatHandleByUser(h);

    conlock(std::cout) << ch_s(h2) << std::endl;
}

void exec_createchat(ac::ACState& s)
{
    g_chatListener.onFinish(
        c::MegaChatRequest::TYPE_CREATE_CHATROOM,
        [](CLCFinishInfo& f)
        {
            if (check_err("CreateChat", f.e))
            {
                auto cl = conlock(std::cout);
                std::cout << "Chat " << ch_s(f.request->getChatHandle())
                          << (f.request->getFlag() ? " is a group chat" : " is a permanent chat")
                          << std::endl;
                auto list = f.request->getMegaChatPeerList();
                for (int i = 0; i < list->size(); ++i)
                {
                    std::cout << "  peer " << ch_s(list->getPeerHandle(i)) << " "
                              << c::MegaChatRoom::privToString(list->getPeerPrivilege(i))
                              << std::endl;
                }
            }
        });

    bool isGroup = s.extractflag("-group");
    bool isPublic = s.extractflag("-public");
    bool isMeeting = s.extractflag("-meeting");
    auto peerList = c::MegaChatPeerList::createInstance();
    for (unsigned i = 1; i < s.words.size(); ++i)
    {
        peerList->addPeer(s_ch(s.words[i].s),
                          c::MegaChatPeerList::PRIV_STANDARD); // todo: accept privilege flags
    }

    if (isMeeting)
    {
        g_chatApi->createMeeting(nullptr, &g_chatListener);
    }
    else if (isPublic)
    {
        g_chatApi->createPublicChat(peerList, nullptr, &g_chatListener);
    }
    else // group and 1on1
    {
        g_chatApi->createChat(isGroup, peerList, &g_chatListener);
    }
}

void exec_invitetochat(ac::ACState& s)
{
    g_chatListener.onFinish(c::MegaChatRequest::TYPE_INVITE_TO_CHATROOM,
                            [](CLCFinishInfo& f)
                            {
                                if (check_err("InviteToChat", f.e))
                                {
                                    conlock(std::cout)
                                        << "Invited user " << ch_s(f.request->getUserHandle())
                                        << " to chat " << ch_s(f.request->getChatHandle()) << " as "
                                        << c::MegaChatRoom::privToString(f.request->getPrivilege())
                                        << std::endl;
                                }
                            });

    g_chatApi->inviteToChat(s_ch(s.words[1].s),
                            s_ch(s.words[2].s),
                            c::MegaChatPeerList::PRIV_STANDARD,
                            &g_chatListener); // todo
}

void exec_removefromchat(ac::ACState& s)
{
    g_chatListener.onFinish(c::MegaChatRequest::TYPE_REMOVE_FROM_CHATROOM,
                            [](CLCFinishInfo& f)
                            {
                                if (check_err("RemoveFromChat", f.e))
                                {
                                    conlock(std::cout)
                                        << "Removed user " << ch_s(f.request->getUserHandle())
                                        << " from chat " << ch_s(f.request->getChatHandle())
                                        << std::endl;
                                }
                            });

    g_chatApi->removeFromChat(s_ch(s.words[1].s), s_ch(s.words[2].s), &g_chatListener);
}

void exec_leavechat(ac::ACState& s)
{
    g_chatListener.onFinish(c::MegaChatRequest::TYPE_REMOVE_FROM_CHATROOM,
                            [](CLCFinishInfo& f)
                            {
                                if (check_err("LeaveChat", f.e))
                                {
                                    conlock(std::cout)
                                        << "Left chat " << ch_s(f.request->getChatHandle())
                                        << " (user " << ch_s(f.request->getUserHandle())
                                        << std::endl;
                                }
                            });

    g_chatApi->removeFromChat(s_ch(s.words[1].s), s_ch(s.words[2].s), &g_chatListener);
}

void exec_updatechatpermissions(ac::ACState& s)
{
    g_chatListener.onFinish(c::MegaChatRequest::TYPE_UPDATE_PEER_PERMISSIONS,
                            [](CLCFinishInfo& f)
                            {
                                if (check_err("UpdateChatPermissions", f.e))
                                {
                                    conlock(std::cout)
                                        << "Updated user " << ch_s(f.request->getUserHandle())
                                        << " in chat " << ch_s(f.request->getChatHandle()) << " to "
                                        << c::MegaChatRoom::privToString(f.request->getPrivilege())
                                        << std::endl;
                                }
                            });

    g_chatApi->updateChatPermissions(s_ch(s.words[1].s),
                                     s_ch(s.words[2].s),
                                     c::MegaChatPeerList::PRIV_STANDARD,
                                     &g_chatListener); // todo
}

void exec_truncatechat(ac::ACState& s)
{
    g_chatListener.onFinish(c::MegaChatRequest::TYPE_TRUNCATE_HISTORY,
                            [](CLCFinishInfo& f)
                            {
                                if (check_err("TruncateChat", f.e))
                                {
                                    conlock(std::cout)
                                        << "Truncated from " << ch_s(f.request->getUserHandle())
                                        << " in chat " << ch_s(f.request->getChatHandle())
                                        << std::endl;
                                }
                            });

    g_chatApi->truncateChat(s_ch(s.words[1].s), s_ch(s.words[2].s), &g_chatListener);
}

void exec_clearchathistory(ac::ACState& s)
{
    g_chatListener.onFinish(c::MegaChatRequest::TYPE_TRUNCATE_HISTORY,
                            [](CLCFinishInfo& f)
                            {
                                if (check_err("ClearChatHistory", f.e))
                                {
                                    conlock(std::cout)
                                        << "Truncated chat " << ch_s(f.request->getChatHandle())
                                        << ", sole message now " << ch_s(f.request->getUserHandle())
                                        << std::endl;
                                }
                            });

    g_chatApi->clearChatHistory(s_ch(s.words[1].s), &g_chatListener);
}

void exec_setRetentionTime(ac::ACState& s)
{
    g_chatListener.onFinish(c::MegaChatRequest::TYPE_SET_RETENTION_TIME,
                            [](CLCFinishInfo& f)
                            {
                                if (check_err("SetRetentionTime", f.e))
                                {
                                    // Clients will not learn about the retention time from the API
                                    conlock(std::cout)
                                        << "Retention time was set successfully for chat "
                                        << ch_s(f.request->getChatHandle()) << std::endl;
                                }
                            });

    g_chatApi->setChatRetentionTime(s_ch(s.words[1].s),
                                    static_cast<unsigned int>(atoi(s.words[2].s.c_str())),
                                    &g_chatListener);
}

void exec_getRetentionTime(ac::ACState& s)
{
    std::unique_ptr<megachat::MegaChatRoom> chatRoom(g_chatApi->getChatRoom(s_ch(s.words[1].s)));
    if (chatRoom)
    {
        conlock(std::cout) << " retentionTime "
                           << std::to_string(chatRoom->getRetentionTime()).c_str() << std::endl;
    }
}

void exec_setchattitle(ac::ACState& s)
{
    g_chatListener.onFinish(c::MegaChatRequest::TYPE_EDIT_CHATROOM_NAME,
                            [](CLCFinishInfo& f)
                            {
                                if (check_err("SetChatTitle", f.e))
                                {
                                    conlock(std::cout)
                                        << "Chat " << ch_s(f.request->getChatHandle())
                                        << " now titled" << f.request->getText() << std::endl;
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
            conlock(std::cout) << "Failed to open chat room." << std::endl;
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
        conlock(std::cout) << "Room " << ch_s(room) << " is already open." << std::endl;
    }
}

void exec_closechatroom(ac::ACState& s)
{
    c::MegaChatHandle room = s_ch(s.words[1].s);
    auto& rec = g_roomListeners[room];
    if (!rec.open)
    {
        conlock(std::cout) << "Room " << ch_s(room) << " was not open" << std::endl;
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

void exec_joinCallViaMeetingLink(ac::ACState& s)
{
    // Requirement at this point account must be logged out, this will simplify this method
    const bool video = !s.extractflag("-novideo");
    const bool audio = !s.extractflag("-noaudio");

    std::string waitTimeStr{"40"};
    s.extractflagparam("-wait", waitTimeStr);
    unsigned int waitTimeSec = static_cast<unsigned int>(std::stoi(waitTimeStr));
    if (waitTimeSec == 0)
    {
        waitTimeSec = clc_ccactions::callUnlimitedDuration;
    }

    std::string videoInputDevice;
    s.extractflagparam("-videoInputDevice", videoInputDevice);
    if (videoInputDevice.size() != 0)
    {
        logMsg(m::logInfo,
               "## Task0: Setting video input device (optional) ##",
               ELogWriter::MEGA_CHAT);
        if (!clc_ccactions::setChatVideoInDevice(videoInputDevice))
        {
            logMsg(m::logError,
                   "Invalid input video device, selecting the default one.",
                   ELogWriter::MEGA_CHAT);
        }
    }

    auto link = s.words[1].s;

    logMsg(m::logInfo, "## Task1: open chat link ##", ELogWriter::MEGA_CHAT);
    auto [chatId, errCode] = clc_ccactions::openChatLink(link);
    if (chatId == c::MEGACHAT_INVALID_HANDLE)
    {
        return;
    }

    logMsg(m::logInfo, "## Task2: Join chat ##", ELogWriter::MEGA_CHAT);
    if (!clc_ccactions::joinChat(chatId, errCode))
    {
        return;
    }

    // We assume that there should be an ongoing call in the chat
    // If we haven't received yet we'll wait a small period to receive it
    // If we still don't receive it we consider as an error.
    logMsg(m::logInfo, "## Task3: Wait for call receiving call ##", ELogWriter::MEGA_CHAT);
    if (!clc_ccactions::waitUntilCallIsReceived(chatId))
    {
        return;
    }

    logMsg(m::logInfo, "## Task4: Answer chat call ##", ELogWriter::MEGA_CHAT);
    if (!clc_ccactions::answerCall(chatId,
                                   audio,
                                   video,
                                   {megachat::MegaChatCall::CALL_STATUS_IN_PROGRESS}))
    {
        return;
    }
    // Log number of participants
    std::unique_ptr<megachat::MegaChatCall> call(g_chatApi->getChatCall(chatId));
    if (!call)
    {
        // The call must exists as it existed in answerCall function
        logMsg(m::logError, "Call cannot be retrieved for chatid", ELogWriter::MEGA_CHAT);
        assert(false);
        return;
    }
    logMsg(m::logInfo,
           "## Task4.1: You have joined a call with " + std::to_string(call->getNumParticipants()) +
               " ##",
           ELogWriter::MEGA_CHAT);

    logMsg(m::logInfo, "## Task5: waiting some time before hanging up ##", ELogWriter::MEGA_CHAT);
    clc_ccactions::waitInCallFor(chatId, waitTimeSec);
    logMsg(m::logInfo, "## Task5.1: wait time finished ##", ELogWriter::MEGA_CHAT);

    logMsg(m::logInfo, "## Task6: hanging up the call ##", ELogWriter::MEGA_CHAT);
    if (!clc_ccactions::hangUpCall(chatId))
    {
        return;
    }
    logMsg(m::logError, "Call finished properly", ELogWriter::MEGA_CHAT);
}

void exec_loadmessages(ac::ACState& s)
{
    g_reportMessagesDeveloper = s.words.size() > 3 && s.words[3].s == "developer";

    auto source = g_chatApi->loadMessages(s_ch(s.words[1].s), stoi(s.words[2].s));

    auto cl = conlock(std::cout);
    switch (source)
    {
        case c::MegaChatApi::SOURCE_ERROR:
            std::cout << "Load failed as we are offline." << std::endl;
            break;
        case c::MegaChatApi::SOURCE_NONE:
            std::cout << "No more messages." << std::endl;
            break;
        case c::MegaChatApi::SOURCE_LOCAL:
            std::cout << "Loading from local store." << std::endl;
            break;
        case c::MegaChatApi::SOURCE_REMOTE:
            std::cout << "Loading from server." << std::endl;
            break;
    }
}

static bool initFile(std::unique_ptr<std::ofstream>& file, const std::string& filename)
{
#ifdef __APPLE__
    const auto outputFilename = getExeDirectory() + "/" + filename;
#else
    const auto outputFilename = getExeDirectory() / filename;
#endif
    file.reset(new std::ofstream{outputFilename});
    if (!file->is_open())
    {
        conlock(std::cout) << "Error: Unable to open output file: " << outputFilename << std::endl;
        return false;
    }
    return true;
}

void exec_dumpchathistory(ac::ACState& s)
{
    if (g_chatApi->getInitState() != c::MegaChatApi::INIT_ONLINE_SESSION)
    {
        conlock(std::cout) << "Error: Not logged in" << std::endl;
        return;
    }

    if (g_dumpHistoryChatid != c::MEGACHAT_INVALID_HANDLE)
    {
        conlock(std::cout) << "There is other dumping history in progress" << std::endl;
        return;
    }

    g_dumpHistoryChatid = s_ch(s.words[1].s);
    if (g_dumpHistoryChatid == c::MEGACHAT_INVALID_HANDLE)
    {
        conlock(std::cout) << "Error: Invalid handle" << std::endl;
        return;
    }

    auto& rec = g_roomListeners[g_dumpHistoryChatid];
    if (rec.open)
    {
        g_chatApi->closeChatRoom(g_dumpHistoryChatid, rec.listener.get());
    }

    if (!g_chatApi->openChatRoom(g_dumpHistoryChatid, rec.listener.get()))
    {
        conlock(std::cout) << "Failed to open chat room." << std::endl;
        g_roomListeners.erase(g_dumpHistoryChatid);
        return;
    }
    else
    {
        rec.listener->room = g_dumpHistoryChatid;
        rec.open = true;
    }

    g_dumpingChatHistory = true;
    g_reportMessagesDeveloper = false;
    g_reviewChatMsgCountRemaining = -1;
    g_reviewChatMsgCount = 0;

    std::string baseFilename =
        "ChatRoom" + s.words[1].s + "_" + timeToStringUTC(std::time(nullptr)) + "UTC";
    if (s.words.size() >= 3)
    {
        baseFilename = s.words[2].s;
    }

    if (!initFile(g_reviewPublicChatOutFile, baseFilename + ".txt"))
    {
        g_dumpHistoryChatid = c::MEGACHAT_INVALID_HANDLE;
        g_dumpingChatHistory = false;
        return;
    }

    std::unique_ptr<c::MegaChatRoom> chatRoom(g_chatApi->getChatRoom(g_dumpHistoryChatid));
    std::unique_ptr<m::MegaHandleList> peerList =
        std::unique_ptr<m::MegaHandleList>(m::MegaHandleList::createInstance());
    for (unsigned int i = 0; i < chatRoom->getPeerCount(); i++)
    {
        peerList->addMegaHandle(chatRoom->getPeerHandle(i));
    }

    auto allEmailsReceived = new OneShotChatRequestListener;
    allEmailsReceived->onRequestFinishFunc =
        [](c::MegaChatApi*, c::MegaChatRequest*, c::MegaChatError*)
    {
        std::unique_ptr<c::MegaChatRoom> chatRoom(g_chatApi->getChatRoom(g_dumpHistoryChatid));
        reviewPublicChatLoadMessages(g_dumpHistoryChatid);
    };

    g_chatApi->loadUserAttributes(g_dumpHistoryChatid, peerList.get(), allEmailsReceived);
}

void exec_reviewpublicchat(ac::ACState& s)
{
    if (g_chatApi->getInitState() != c::MegaChatApi::INIT_ONLINE_SESSION)
    {
        conlock(std::cout) << "Error: Not logged in" << std::endl;
        return;
    }

    if (g_reviewPublicChatid != c::MEGACHAT_INVALID_HANDLE)
    {
        g_chatApi->closeChatRoom(g_reviewPublicChatid,
                                 g_roomListeners[g_reviewPublicChatid].listener.get());
        g_roomListeners.erase(g_reviewPublicChatid);
        g_chatApi->closeChatPreview(g_reviewPublicChatid);
    }

    g_reviewingPublicChat = true;
    g_reviewChatMsgCountRemaining = 0;
    g_reviewChatMsgCount = 0;
    g_startedPublicChatReview = false;
    g_reviewPublicChatid = c::MEGACHAT_INVALID_HANDLE;

    const auto chat_link = s.words[1].s;
    g_reviewChatMsgCountRemaining = s.words.size() > 2 ? stoi(s.words[2].s) : -1;

    const auto lastSlashIdx = chat_link.find_last_of("/");
    const auto lastHashIdx = chat_link.find_last_of("#");
    if (lastSlashIdx == std::string::npos || lastHashIdx == std::string::npos ||
        lastSlashIdx >= lastHashIdx)
    {
        conlock(std::cout) << "Error: Invalid link format: " << chat_link << std::endl;
        return;
    }
    const auto linkHandle = chat_link.substr(lastSlashIdx + 1, lastHashIdx - lastSlashIdx - 1);

    const auto baseFilename =
        "PublicChat_" + linkHandle + "_" + timeToStringUTC(std::time(nullptr)) + "UTC";
    if (!initFile(g_reviewPublicChatOutFile, baseFilename + ".txt"))
    {
        return;
    }
    if (!initFile(g_reviewPublicChatOutFileLinks, baseFilename + "_Links.txt"))
    {
        return;
    }
    *g_reviewPublicChatOutFile << chat_link << std::endl;
    *g_reviewPublicChatOutFileLinks << chat_link << std::endl;
    g_debugOutpuWriter.writeOutput(chat_link + "\n", logInfo);

    auto check_chat_preview_listener = new OneShotChatRequestListener;
    check_chat_preview_listener->onRequestFinishFunc =
        [](c::MegaChatApi* api, c::MegaChatRequest* request, c::MegaChatError* e)
    {
        // Called on Mega Chat API std::thread
        if (!check_err("checkChatLink", e))
        {
            *g_reviewPublicChatOutFile << "checkChatLink failed. Error: " << e->getErrorString()
                                       << std::endl;
            return;
        }

        g_reviewPublicChatid = request->getChatHandle();
        std::ostringstream os1;
        os1 << "\nReviewPublicChat: chatlink loaded succesfully.\n\tChatid: "
            << k::Id(g_reviewPublicChatid).toString() << std::endl;
        const auto msg1 = os1.str();
        conlock(std::cout) << msg1;
        conlock(*g_reviewPublicChatOutFile) << msg1 << std::flush;

        const int numPeers = static_cast<int>(request->getNumber());
        std::ostringstream os2;
        os2 << "\tUser count: " << numPeers << std::endl;
        const auto msg2 = os2.str();
        conlock(std::cout) << msg2;
        conlock(*g_reviewPublicChatOutFile) << msg2 << std::flush;

        const char* title = request->getText();
        std::ostringstream os3;
        os3 << "\tTitle: " << title << std::endl;
        const auto msg3 = os3.str();
        conlock(std::cout) << msg3;
        conlock(*g_reviewPublicChatOutFile) << msg3 << std::flush;

        // now we know the chatid, we register the listener
        auto open_chat_preview_listener = new OneShotChatRequestListener;
        open_chat_preview_listener->onRequestFinishFunc =
            [](c::MegaChatApi*, c::MegaChatRequest*, c::MegaChatError* e)
        {
            if (!check_err("openChatPreview", e))
            {
                *g_reviewPublicChatOutFile
                    << "openChatPreview failed. Error: " << e->getErrorString() << std::endl;
                return;
            }
        };

        const char* chatlink = request->getLink();
        api->openChatPreview(chatlink, open_chat_preview_listener);
        // now wait until logged in into the chatroom, so we know the peers and load their emails
    };

    g_chatApi->checkChatLink(chat_link.c_str(), check_chat_preview_listener);
}

void exec_isfullhistoryloaded(ac::ACState& s)
{
    conlock(std::cout) << (g_chatApi->isFullHistoryLoaded(s_ch(s.words[1].s)) ? "Yes" : "No")
                       << std::endl;
}

void exec_getmessage(ac::ACState& s)
{
    auto room = s_ch(s.words[1].s);
    std::unique_ptr<c::MegaChatMessage> msg(g_chatApi->getMessage(room, s_ch(s.words[2].s)));

    if (!msg)
    {
        conlock(std::cout) << "Not retrieved." << std::endl;
    }
    else
    {
        reportMessage(room, msg.get(), "got");
    }
}

void exec_getmanualsendingmessage(ac::ACState& s)
{
    auto room = s_ch(s.words[1].s);
    std::unique_ptr<c::MegaChatMessage> msg(
        g_chatApi->getManualSendingMessage(room, s_ch(s.words[2].s)));

    if (!msg)
    {
        conlock(std::cout) << "Not retrieved." << std::endl;
    }
    else
    {
        reportMessage(room, msg.get(), "got");
    }
}

void exec_sendmessage(ac::ACState& s)
{
    auto room = s_ch(s.words[1].s);
    std::unique_ptr<c::MegaChatMessage> msg(g_chatApi->sendMessage(room, s.words[2].s.c_str()));

    if (!msg)
    {
        conlock(std::cout) << "Failed." << std::endl;
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

    std::unique_ptr<c::MegaChatMessage> msg(g_chatApi->attachContacts(room, mhl)); // todo:
                                                                                   // ownership

    if (!msg)
    {
        conlock(std::cout) << "Failed." << std::endl;
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
    std::unique_ptr<c::MegaChatMessage> msg(
        g_chatApi->revokeAttachmentMessage(room, s_ch(s.words[2].s)));

    if (!msg)
    {
        conlock(std::cout) << "Failed." << std::endl;
    }
    else
    {
        reportMessage(room, msg.get(), "revoking attachment");
    }
}

void exec_editmessage(ac::ACState& s)
{
    auto room = s_ch(s.words[1].s);
    std::unique_ptr<c::MegaChatMessage> msg(
        g_chatApi->editMessage(room, s_ch(s.words[2].s), s.words[2].s.c_str()));

    if (!msg)
    {
        conlock(std::cout) << "Failed." << std::endl;
    }
    else
    {
        reportMessage(room, msg.get(), "editing");
    }
}

void exec_deletemessage(ac::ACState& s)
{
    auto room = s_ch(s.words[1].s);
    std::unique_ptr<c::MegaChatMessage> msg(g_chatApi->deleteMessage(room, s_ch(s.words[2].s)));

    if (!msg)
    {
        conlock(std::cout) << "Failed." << std::endl;
    }
    else
    {
        reportMessage(room, msg.get(), "deleting");
    }
}

void exec_setmessageseen(ac::ACState& s)
{
    conlock(std::cout) << (g_chatApi->setMessageSeen(s_ch(s.words[2].s), s_ch(s.words[2].s)) ?
                               "Done" :
                               "Failed")
                       << std::endl;
}

void exec_getLastMessageSeen(ac::ACState& s)
{
    auto room = s_ch(s.words[1].s);
    std::unique_ptr<c::MegaChatMessage> msg(g_chatApi->getLastMessageSeen(room));

    if (!msg)
    {
        conlock(std::cout) << "None." << std::endl;
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
    g_chatListener.onFinish(c::MegaChatRequest::TYPE_SEND_TYPING_NOTIF,
                            [](CLCFinishInfo& f)
                            {
                                if (check_err("SendTypingNotification", f.e))
                                {
                                    conlock(std::cout)
                                        << "Chat " << ch_s(f.request->getChatHandle())
                                        << " notified" << std::endl;
                                }
                            });

    g_chatApi->sendTypingNotification(s_ch(s.words[1].s), &g_chatListener);
}

void exec_ismessagereceptionconfirmationactive(ac::ACState&)
{
    conlock(std::cout) << (g_chatApi->isMessageReceptionConfirmationActive() ? "Yes" : "No")
                       << std::endl;
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
    static_cast<m::WinConsole*>(global::console.get())
        ->setAutocompleteStyle(s.words[1].s == "unix");
}
#endif

void exec_help(ac::ACState&)
{
    conlock(std::cout) << *g_autocompleteTemplate << std::flush;
}

#ifdef WIN32
void exec_history(ac::ACState&)
{
    static_cast<m::WinConsole*>(global::console.get())->outputHistory();
}
#endif

void exec_quit(ac::ACState&)
{
    g_promptQuitFlag = true;
}

#ifndef KARERE_DISABLE_WEBRTC

void exec_getchatvideoindevices(ac::ACState&)
{
    std::unique_ptr<m::MegaStringList> videoDevices(g_chatApi->getChatVideoInDevices());
    for (int i = 0; i < videoDevices->size(); ++i)
    {
        std::cout << videoDevices->get(i) << std::endl;
    }
}

void exec_setchatvideoindevice(ac::ACState& s)
{
    c::MegaChatRequestListener* listener = new c::MegaChatRequestListener; // todo
    g_chatApi->setChatVideoInDevice(s.words[1].s.c_str(), listener);
}

void exec_startchatcall(ac::ACState& s)
{
    const bool video = !s.extractflag("-novideo");
    const bool audio = !s.extractflag("-noaudio");
    c::MegaChatHandle chatId = s_ch(s.words[1].s);

    std::unique_ptr<c::MegaChatCall> call(g_chatApi->getChatCall(chatId));
    if (call)
    {
        logMsg(logError, "The call already exists", ELogWriter::MEGA_CHAT);
        return;
    }
    clc_ccactions::startChatCall(chatId, audio, video, false);
}

void exec_answerchatcall(ac::ACState& s)
{
    const bool video = !s.extractflag("-novideo");
    const bool audio = !s.extractflag("-noaudio");
    c::MegaChatHandle chatId = s_ch(s.words[1].s);

    std::unique_ptr<c::MegaChatCall> call(g_chatApi->getChatCall(chatId));
    if (!call)
    {
        logMsg(logError, "The call you are trying to anwer does not exist", ELogWriter::MEGA_CHAT);
        return;
    }
    clc_ccactions::answerCall(chatId,
                              audio,
                              video,
                              {megachat::MegaChatCall::CALL_STATUS_IN_PROGRESS});
}

void exec_hangchatcall(ac::ACState& s)
{
    c::MegaChatRequestListener* listener = new c::MegaChatRequestListener; // todo
    c::MegaChatHandle call = s_ch(s.words[1].s);
    g_chatApi->hangChatCall(call, listener);
}

void exec_enableaudio(ac::ACState& s)
{
    c::MegaChatRequestListener* listener = new c::MegaChatRequestListener; // todo
    c::MegaChatHandle room = s_ch(s.words[1].s);
    g_chatApi->enableAudio(room, listener);
}

void exec_disableaudio(ac::ACState& s)
{
    c::MegaChatRequestListener* listener = new c::MegaChatRequestListener; // todo
    c::MegaChatHandle room = s_ch(s.words[1].s);
    g_chatApi->disableAudio(room, listener);
}

void exec_enablevideo(ac::ACState& s)
{
    c::MegaChatRequestListener* listener = new c::MegaChatRequestListener; // todo
    c::MegaChatHandle room = s_ch(s.words[1].s);
    g_chatApi->enableVideo(room, listener);
}

void exec_disablevideo(ac::ACState& s)
{
    c::MegaChatRequestListener* listener = new c::MegaChatRequestListener; // todo
    c::MegaChatHandle room = s_ch(s.words[1].s);
    g_chatApi->disableVideo(room, listener);
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
    // MegaChatCall *getChatCallByCallId(MegaChatHandle callId);
}

void exec_getnumcalls(ac::ACState&)
{
    std::cout << g_chatApi->getNumCalls() << std::endl;
}

void exec_getchatcalls(ac::ACState&)
{
    std::unique_ptr<m::MegaHandleList> list(g_chatApi->getChatCalls());
    for (unsigned i = 0; i < list->size(); ++i)
    {
        std::cout << ch_s(list->get(i)) << std::endl;
    }
}

void exec_getchatcallsids(ac::ACState&)
{
    std::unique_ptr<m::MegaHandleList> list(g_chatApi->getChatCallsIds());
    for (unsigned i = 0; i < list->size(); ++i)
    {
        std::cout << ch_s(list->get(i)) << std::endl;
    }
}

#endif

void exec_smsverify(ac::ACState& s)
{
    if (s.words[1].s == "send")
    {
        auto listener = new OneShotRequestListener;
        listener->onRequestFinishFunc = [](m::MegaApi*, m::MegaRequest*, m::MegaError* e)
        {
            conlock(std::cout) << "SMS Verify Text Result: " << e->getErrorString() << std::endl;
        };
        g_megaApi->sendSMSVerificationCode(s.words[2].s.c_str(),
                                           listener,
                                           s.words.size() > 3 && s.words[3].s == "to");
    }
    else if (s.words[1].s == "code")
    {
        auto listener = new OneShotRequestListener;
        listener->onRequestFinishFunc = [](m::MegaApi*, m::MegaRequest*, m::MegaError* e)
        {
            conlock(std::cout) << "SMS Verify Text Result: " << e->getErrorString() << std::endl;
        };
        g_megaApi->checkSMSVerificationCode(s.words[2].s.c_str(), listener);
    }
    else if (s.words[1].s == "allowed")
    {
        conlock(std::cout) << "SMS Verify Text Result: " << g_megaApi->smsAllowedState()
                           << std::endl;
    }
    else if (s.words[1].s == "phone")
    {
        std::unique_ptr<char[]> number(g_megaApi->smsVerifiedPhoneNumber());
        conlock(std::cout) << "Verified phone: " << (number ? number.get() : "<none>") << std::endl;
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
            conlock(std::cout) << "Refreshing local cache due to change of APIURL" << std::endl;

            setprompt(NOPROMPT);

            const char* session = g_megaApi->dumpSession();
            g_megaApi->fastLogin(session);
            g_chatApi->refreshUrl();
            delete[] session;
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

        g_megaApi->catchup(new OneShotRequestListener(
            [id](m::MegaApi*, m::MegaRequest*, m::MegaError* e)
            {
                check_err("catchup " + std::to_string(id), e);
            }));

        conlock(std::cout) << "catchup " << id << " requested" << std::endl;
    }
}

std::map<std::string, std::unique_ptr<m::MegaBackgroundMediaUpload>> g_megaBackgroundMediaUploads;

static bool getNamedBackgroundMediaUpload(const std::string& name, m::MegaBackgroundMediaUpload*& p)
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

#ifdef WIN32 // functions to perform background-upload like http request

// handle WinHTTP callbacks (which can be in a worker std::thread context)
VOID CALLBACK asynccallback(HINTERNET hInternet,
                            DWORD_PTR dwContext,
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

void synchronousHttpRequest(const std::string& url,
                            const std::string& senddata,
                            std::string& responsedata)
{
    using namespace m;
    LOG_info << "Sending file to " << url << ", size: " << senddata.size();

    BOOL bResults = TRUE;
    HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;

    // Use WinHttpOpen to obtain a session handle.
    hSession = WinHttpOpen(L"testmega/1.0",
                           WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                           WINHTTP_NO_PROXY_NAME,
                           WINHTTP_NO_PROXY_BYPASS,
                           0);

    WCHAR szURL[8192];
    WCHAR szHost[256];
    URL_COMPONENTS urlComp = {sizeof urlComp};

    urlComp.lpszHostName = szHost;
    urlComp.dwHostNameLength = sizeof szHost / sizeof *szHost;
    urlComp.dwUrlPathLength = (DWORD)-1;
    urlComp.dwSchemeLength = (DWORD)-1;

    if (MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, szURL, sizeof szURL / sizeof *szURL) &&
        WinHttpCrackUrl(szURL, 0, 0, &urlComp))
    {
        if ((hConnect = WinHttpConnect(hSession, szHost, urlComp.nPort, 0)))
        {
            hRequest = WinHttpOpenRequest(
                hConnect,
                L"POST",
                urlComp.lpszUrlPath,
                NULL,
                WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES,
                (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
        }
    }

    // Send a Request.
    if (hRequest)
    {
        WinHttpSetTimeouts(hRequest, 58000, 58000, 0, 0);

        LPCWSTR pwszHeaders = L"Content-Type: application/octet-stream";

        // HTTPS connection: ignore certificate errors, send no data yet
        DWORD flags = SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                      SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_UNKNOWN_CA;

        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof flags);

        if (WinHttpSendRequest(hRequest,
                               pwszHeaders,
                               DWORD(wcslen(pwszHeaders)),
                               (LPVOID)senddata.data(),
                               (DWORD)senddata.size(),
                               (DWORD)senddata.size(),
                               NULL))
        {}
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
                printf("Error %u in WinHttpQueryDataAvailable.\n", GetLastError());

            size_t offset = responsedata.size();
            responsedata.resize(offset + dwSize);

            ZeroMemory(responsedata.data() + offset, dwSize);

            DWORD dwDownloaded = 0;
            if (!WinHttpReadData(hRequest, responsedata.data() + offset, dwSize, &dwDownloaded))
                printf("Error %u in WinHttpReadData.\n", GetLastError());
        }
        while (dwSize > 0);

    // Report errors.
    if (!bResults)
        printf("Error %d has occurred.\n", GetLastError());

    // Close open handles.
    if (hRequest)
        WinHttpCloseHandle(hRequest);
    if (hConnect)
        WinHttpCloseHandle(hConnect);
    if (hSession)
        WinHttpCloseHandle(hSession);
}
#endif

void exec_backgroundupload(ac::ACState& s)
{
    m::MegaBackgroundMediaUpload* mbmu = nullptr;

    if (s.words[1].s == "new" && s.words.size() == 3)
    {
        g_megaBackgroundMediaUploads[s.words[2].s].reset(
            m::MegaBackgroundMediaUpload::createInstance(g_megaApi.get()));
    }
    else if (s.words[1].s == "resume" && s.words.size() == 4)
    {
        g_megaBackgroundMediaUploads[s.words[2].s].reset(
            m::MegaBackgroundMediaUpload::unserialize(s.words[3].s.c_str(), g_megaApi.get()));
    }
    else if (s.words[1].s == "analyse" && s.words.size() == 4 &&
             getNamedBackgroundMediaUpload(s.words[2].s, mbmu))
    {
        mbmu->analyseMediaInfo(s.words[3].s.c_str());
    }
    else if (s.words[1].s == "encrypt" && s.words.size() == 8 &&
             getNamedBackgroundMediaUpload(s.words[2].s, mbmu))
    {
        int64_t startPos = atol(s.words[5].s.c_str());
        int64_t length = atol(s.words[6].s.c_str());
        bool adjustsizeonly = s.words[7].s == "true";
        std::string urlSuffix = OwnStr(mbmu->encryptFile(s.words[3].s.c_str(),
                                                         startPos,
                                                         &length,
                                                         s.words[4].s.c_str(),
                                                         adjustsizeonly));
        if (!urlSuffix.empty())
        {
            conlock(std::cout) << "Encrypt complete, URL suffix: " << urlSuffix
                               << " and updated length: " << length << std::endl;
        }
        else
        {
            conlock(std::cout) << "Encrypt failed" << std::endl;
        }
    }
    else if (s.words[1].s == "geturl" && s.words.size() == 4 &&
             getNamedBackgroundMediaUpload(s.words[2].s, mbmu))
    {
        auto ln = new OneShotRequestListener;
        ln->onRequestFinishFunc = [](m::MegaApi*, m::MegaRequest* request, m::MegaError* e)
        {
            if (check_err("Get upload URL", e))
            {
                conlock(std::cout)
                    << "Upload URL: "
                    << OwnStr(request->getMegaBackgroundMediaUploadPtr()->getUploadURL())
                    << std::endl;
            }
        };

        g_megaApi->backgroundMediaUploadRequestUploadURL(atoll(s.words[3].s.c_str()), mbmu, ln);
    }
    else if (s.words[1].s == "serialize" && s.words.size() == 3 &&
             getNamedBackgroundMediaUpload(s.words[2].s, mbmu))
    {
        std::unique_ptr<char[]> serialized(mbmu->serialize());
        conlock(std::cout) << serialized.get() << std::endl;
    }
    else if (s.words[1].s == "upload" && s.words.size() == 4)
    {
#ifdef WIN32
        std::string responsedata;
        synchronousHttpRequest(s.words[2].s, loadfile(s.words[3].s), responsedata);
        std::unique_ptr<char[]> base64(
            m::MegaApi::binaryToBase64(responsedata.data(), responsedata.size()));
        conlock(std::cout) << "Synchronous upload response (converted to base 64): "
                           << (responsedata.size() <= 3 ? responsedata : base64.get()) << std::endl;
#endif
    }
    else if (s.words[1].s == "putthumbnail" && s.words.size() == 4 &&
             getNamedBackgroundMediaUpload(s.words[2].s, mbmu))
    {
        g_megaApi->putThumbnail(
            mbmu,
            s.words[3].s.c_str(),
            new OneShotRequestListener(
                [](m::MegaApi*, m::MegaRequest* r, m::MegaError* e)
                {
                    if (check_err("putthumbnail", e))
                    {
                        conlock(std::cout)
                            << "thumbnail file attribute handle: "
                            << std::unique_ptr<char[]>(
                                   m::MegaApi::userHandleToBase64(r->getNodeHandle()))
                                   .get()
                            << std::endl;
                    }
                }));
    }
    else if (s.words[1].s == "putpreview" && s.words.size() == 4 &&
             getNamedBackgroundMediaUpload(s.words[2].s, mbmu))
    {
        g_megaApi->putPreview(mbmu,
                              s.words[3].s.c_str(),
                              new OneShotRequestListener(
                                  [](m::MegaApi*, m::MegaRequest* r, m::MegaError* e)
                                  {
                                      if (check_err("putpreview", e))
                                      {
                                          conlock(std::cout) << "preview file attribute handle: "
                                                             << std::unique_ptr<char[]>(
                                                                    m::MegaApi::userHandleToBase64(
                                                                        r->getNodeHandle()))
                                                                    .get()
                                                             << std::endl;
                                      }
                                  }));
    }
    else if (s.words[1].s == "setthumbnail" && s.words.size() == 4 &&
             getNamedBackgroundMediaUpload(s.words[2].s, mbmu))
    {
        mbmu->setThumbnail(m::MegaApi::base64ToUserHandle(s.words[3].s.c_str()));
    }
    else if (s.words[1].s == "setpreview" && s.words.size() == 4 &&
             getNamedBackgroundMediaUpload(s.words[2].s, mbmu))
    {
        mbmu->setPreview(m::MegaApi::base64ToUserHandle(s.words[3].s.c_str()));
    }
    else if (s.words[1].s == "setcoordinates" && s.words.size() == 5 &&
             getNamedBackgroundMediaUpload(s.words[2].s, mbmu))
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
            ln->onRequestFinishFunc = [](m::MegaApi*, m::MegaRequest*, m::MegaError* e)
            {
                check_err("Background upload completion", e);
            };

            g_megaApi->backgroundMediaUploadComplete(mbmu,
                                                     s.words[3].s.c_str(),
                                                     parent.get(),
                                                     fingerprint,
                                                     fingerprintoriginal,
                                                     uploadtoken64,
                                                     ln);
        }
    }
    else
    {
        conlock(std::cout) << "incorrect subcommand" << std::endl;
    }
}

void exec_setthumbnailbyhandle(ac::ACState& s)
{
    if (auto node = GetNodeByPath(s.words[1].s))
    {
        g_megaApi->setThumbnailByHandle(node.get(),
                                        s_ch(s.words[2].s),
                                        new OneShotRequestListener(
                                            [](m::MegaApi*, m::MegaRequest*, m::MegaError* e)
                                            {
                                                check_err("setThumbnailByHandle", e);
                                            }));
    }
}

void exec_setpreviewbyhandle(ac::ACState& s)
{
    if (auto node = GetNodeByPath(s.words[1].s))
    {
        g_megaApi->setPreviewByHandle(node.get(),
                                      s_ch(s.words[2].s),
                                      new OneShotRequestListener(
                                          [](m::MegaApi*, m::MegaRequest*, m::MegaError* e)
                                          {
                                              check_err("setThumbnailByHandle", e);
                                          }));
    }
    else
    {
        conlock(std::cout) << "Path not found" << std::endl;
    }
}

void exec_ensuremediainfo(ac::ACState&)
{
    bool b = g_megaApi->ensureMediaInfo();
    if (b)
    {
        conlock(std::cout) << "media info already available" << std::endl;
    }
    else
    {
        conlock(std::cout) << "media info request sent" << std::endl;
    }
}

void exec_getfingerprint(ac::ACState& s)
{
    if (s.words[1].s == "local" && s.words.size() == 3)
    {
        char* fp = g_megaApi->getFingerprint(s.words[2].s.c_str());
        conlock(std::cout) << (fp ? fp : "<NULL>") << std::endl;
        delete[] fp;
    }
    else if (s.words[1].s == "remote" && s.words.size() == 3)
    {
        if (auto n = GetNodeByPath(s.words[2].s))
        {
            char* fp = g_megaApi->getFingerprint(n.get());
            conlock(std::cout) << (fp ? fp : "<NULL>") << std::endl;
            delete[] fp;
        }
    }
    else if (s.words[1].s == "original" && s.words.size() == 3)
    {
        if (auto n = GetNodeByPath(s.words[2].s))
        {
            const char* fp = n->getOriginalFingerprint();
            conlock(std::cout) << (fp ? fp : "<NULL>") << std::endl;
        }
    }
}

void exec_createthumbnail(ac::ACState& s)
{
    std::string parallelcount;
    bool tempmegaapi = extractflag("-tempmegaapi", s.words);
    bool parallel = extractflagparam("-parallel", parallelcount, s.words);

    if (!parallel)
    {
        parallelcount = "1";
    }

    std::vector<std::unique_ptr<std::thread>> ts;

    // investigate thumbnal generation memory usage after reports of memory leaks in iOS
    int N = atoi(parallelcount.c_str());
    for (int i = N; i--;)
    {
        std::string path1 = s.words[1].s;
        std::string path2 = s.words[2].s + (N > 1 ? "-" + std::to_string(i) : std::string());

        ts.emplace_back(new std::thread(
            [path1, path2, tempmegaapi]()
            {
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
                conlock(std::cout) << (done ? "succeeded" : "failed") << std::endl;
            }));
    }

    for (size_t i = static_cast<size_t>(atoi(parallelcount.c_str())); i--;)
    {
        ts[i]->join();
    }
}

void exec_createpreview(ac::ACState& s)
{
    std::string path1 = s.words[1].s;
    std::string path2 = s.words[2].s;
    bool done = g_megaApi->createThumbnail(path1.c_str(), path2.c_str());
    conlock(std::cout) << (done ? "succeeded" : "failed") << std::endl;
}

void exec_getthumbnail(ac::ACState& s)
{
    std::string nodepath1 = s.words[1].s;
    std::string localpath2 = s.words[2].s;

    if (auto n = GetNodeByPath(nodepath1))
    {
        g_megaApi->getThumbnail(n.get(),
                                localpath2.c_str(),
                                new OneShotRequestListener(
                                    [](m::MegaApi*, m::MegaRequest*, m::MegaError* e)
                                    {
                                        check_err("getThumbnail", e, ReportResult);
                                    }));
    }
    else
    {
        conlock(std::cout) << "node not found" << std::endl;
    }
}

void exec_cancelgetthumbnail(ac::ACState& s)
{
    std::string nodepath1 = s.words[1].s;

    if (auto n = GetNodeByPath(nodepath1))
    {
        g_megaApi->cancelGetThumbnail(n.get(),
                                      new OneShotRequestListener(
                                          [](m::MegaApi*, m::MegaRequest*, m::MegaError* e)
                                          {
                                              check_err("cancelGetThumbnail", e, ReportResult);
                                          }));
    }
    else
    {
        conlock(std::cout) << "node not found" << std::endl;
    }
}

void exec_getpreview(ac::ACState& s)
{
    std::string nodepath1 = s.words[1].s;
    std::string localpath2 = s.words[2].s;

    if (auto n = GetNodeByPath(nodepath1))
    {
        g_megaApi->getPreview(n.get(),
                              localpath2.c_str(),
                              new OneShotRequestListener(
                                  [](m::MegaApi*, m::MegaRequest*, m::MegaError* e)
                                  {
                                      check_err("getPreview", e, ReportResult);
                                  }));
    }
    else
    {
        conlock(std::cout) << "node not found" << std::endl;
    }
}

void exec_cancelgetpreview(ac::ACState& s)
{
    std::string nodepath1 = s.words[1].s;

    if (auto n = GetNodeByPath(nodepath1))
    {
        g_megaApi->cancelGetPreview(n.get(),
                                    new OneShotRequestListener(
                                        [](m::MegaApi*, m::MegaRequest*, m::MegaError* e)
                                        {
                                            check_err("cancelGetPreview", e, ReportResult);
                                        }));
    }
    else
    {
        conlock(std::cout) << "node not found" << std::endl;
    }
}

void exec_testAllocation(ac::ACState& s)
{
    bool success = g_megaApi->testAllocation(unsigned(atoi(s.words[1].s.c_str())),
                                             size_t(atoll(s.words[2].s.c_str())));
    conlock(std::cout) << (success ? "succeeded" : "failed") << std::endl;
}

void exec_recentactions(ac::ACState& s)
{
    std::unique_ptr<m::MegaRecentActionBucketList> ra;

    if (s.words.size() == 3)
    {
        ra.reset(g_megaApi->getRecentActions(static_cast<unsigned>(atoi(s.words[1].s.c_str())),
                                             static_cast<unsigned>(atoi(s.words[2].s.c_str()))));
    }
    else
    {
        ra.reset(g_megaApi->getRecentActions());
    }

    auto l = conlock(std::cout);
    for (int b = 0; b < ra->size(); ++b)
    {
        m::MegaRecentActionBucket* bucket = ra->get(b);

        int64_t ts = bucket->getTimestamp();
        const char* em = bucket->getUserEmail();
        m::MegaHandle ph = bucket->getParentHandle();
        bool isupdate = bucket->isUpdate();
        bool ismedia = bucket->isMedia();
        const m::MegaNodeList* nodes = bucket->getNodes();

        std::cout << "Bucket " << ts << " email " << (em ? em : "NULL") << " parent " << ph
                  << (isupdate ? " update" : "") << (ismedia ? " media" : " files")
                  << " count: " << nodes->size() << std::endl;

        for (int i = 0; i < nodes->size(); ++i)
        {
            std::cout << "    ";
            std::unique_ptr<char[]> path(g_megaApi->getNodePath(nodes->get(i)));
            std::unique_ptr<char[]> handleStr(nodes->get(i)->getBase64Handle());
            if (path)
            {
                std::cout << path.get();
            }
            else
            {
                std::cout << "Path unknown but node name is: " << nodes->get(i)->getName();
            }
            std::cout << " size: " << nodes->get(i)->getSize()
                      << " handle: " << (handleStr ? handleStr.get() : "(NULL)") << std::endl;
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

    g_megaApi->getSpecificAccountDetails(
        storage,
        transfer,
        pro,
        -1,
        new OneShotRequestListener(
            [](m::MegaApi*, m::MegaRequest* r, m::MegaError* e)
            {
                if (check_err("getSpecificAccountDetails", e, ReportFailure))
                {
                    std::unique_ptr<m::MegaAccountDetails> ad(r->getMegaAccountDetails());
                    conlock(std::cout)
                        << "Storage used: " << ad->getStorageUsed()
                        << " free: " << (ad->getStorageMax() - ad->getStorageUsed())
                        << " max: " << ad->getStorageMax() << std::endl
                        << "Version bytes used: " << ad->getVersionStorageUsed() << std::endl;
                }
            }));
}

void exec_setnodecoordinates(ac::ACState& s)
{
    if (auto node = GetNodeByPath(s.words[1].s))
    {
        g_megaApi->setNodeCoordinates(node.get(),
                                      atof(s.words[2].s.c_str()),
                                      atof(s.words[3].s.c_str()),
                                      new OneShotRequestListener(
                                          [](m::MegaApi*, m::MegaRequest*, m::MegaError* e)
                                          {
                                              check_err("setNodeCoordinates", e);
                                          }));
    }
}

void exec_setunshareablenodecoordinates(ac::ACState& s)
{
    if (auto node = GetNodeByPath(s.words[1].s))
    {
        g_megaApi->setUnshareableNodeCoordinates(
            node.get(),
            atof(s.words[2].s.c_str()),
            atof(s.words[3].s.c_str()),
            new OneShotRequestListener(
                [](m::MegaApi*, m::MegaRequest*, m::MegaError* e)
                {
                    check_err("setUnshareableNodeCoordinates", e);
                }));
    }
}

void exec_getnodebypath(ac::ACState& s)
{
    if (auto node = GetNodeByPath(s.words[1].s))
    {
        auto guard = conlock(std::cout);

        std::cout << "type: " << node->getType() << std::endl;
        std::cout << "name: " << (node->getName() ? node->getName() : "<null>") << std::endl;
        std::cout << "fingerprint: " << (node->getFingerprint() ? node->getFingerprint() : "<null>")
                  << std::endl;
        std::cout << "original fingerprint: "
                  << (node->getOriginalFingerprint() ? node->getOriginalFingerprint() : "<null>")
                  << std::endl;
        std::cout << "has custom attrs: " << node->hasCustomAttrs() << std::endl;
        std::unique_ptr<m::MegaStringList> can(node->getCustomAttrNames());
        for (int i = 0; i < can->size(); ++i)
        {
            std::cout << "  " << can->get(i) << ": " << node->getCustomAttr(can->get(i))
                      << std::endl;
        }
        std::cout << "duration (seconds): " << node->getDuration() << std::endl;
        std::cout << "width: " << node->getWidth() << std::endl;
        std::cout << "height: " << node->getHeight() << std::endl;
        std::cout << "shortformat: " << node->getShortformat() << std::endl;
        std::cout << "videoCodecId: " << node->getVideocodecid() << std::endl;
        std::cout << "latitude: " << node->getLatitude() << std::endl;
        std::cout << "longitude: " << node->getLongitude() << std::endl;
        std::cout << "handle: " << node->getBase64Handle() << std::endl;
        std::cout << "size: " << node->getSize() << std::endl;
        std::cout << "creation time: " << node->getCreationTime() << std::endl;
        std::cout << "modification time: " << node->getModificationTime() << std::endl;
        std::cout << "handle: " << ch_s(node->getHandle()) << std::endl;
        std::cout << "restore handle: " << ch_s(node->getRestoreHandle()) << std::endl;
        std::cout << "parent handle: " << ch_s(node->getParentHandle()) << std::endl;
        // getBase64Key();
        std::cout << "expiration time: " << node->getExpirationTime() << std::endl;
        std::cout << "public handle: " << ch_s(node->getPublicHandle()) << std::endl;
        // getPublicNode();
        std::unique_ptr<char[]> publink(node->getPublicLink(true));
        std::cout << "public link: " << (publink.get() ? publink.get() : "<null>") << std::endl;
        std::cout << "is file: " << node->isFile() << std::endl;
        std::cout << "is folder: " << node->isFolder() << std::endl;
        std::cout << "is removed: " << node->isRemoved() << std::endl;
        std::cout << "changes: " << std::hex << node->getChanges() << std::dec << std::endl;
        std::cout << "has thumbnail: " << node->hasThumbnail() << std::endl;
        std::cout << "has preview: " << node->hasPreview() << std::endl;
        std::cout << "isPublic: " << node->isPublic() << std::endl;
        std::cout << "isShared: " << node->isShared() << std::endl;
        std::cout << "isOutShare: " << node->isOutShare() << std::endl;
        std::cout << "isInShare: " << node->isInShare() << std::endl;
        std::cout << "isExported: " << node->isExported() << std::endl;
        std::cout << "isExpired: " << node->isExpired() << std::endl;
        std::cout << "isTakenDown: " << node->isTakenDown() << std::endl;
        std::cout << "isForeign: " << node->isForeign() << std::endl;
        // getNodeKey();
        std::unique_ptr<char[]> fileattr(node->getFileAttrString());
        std::cout << "chatroom file attributes: " << (fileattr ? fileattr.get() : "<null>")
                  << std::endl;
        // getPrivateAuth();
        // setPrivateAuth(const char *privateAuth);
        // getPublicAuth();
        // getChatAuth();
        // getChildren();
        std::cout << "owner handle: " << ch_s(node->getOwner()) << std::endl;
        std::cout << "serialized: " << std::unique_ptr<char[]>(node->serialize()).get()
                  << std::endl;
        // unserialize(const char *d);
    }
}

struct ls_flags
{
    std::string regexfilterstring;
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

static void ls(m::MegaNode* node, const std::string& basepath, const ls_flags& flags, int depth)
{
    bool show = true;

    if (depth > 0 || node->getType() == m::MegaNode::TYPE_FILE)
    {
        std::string utf8path(g_megaApi->getNodePath(node));
        if (utf8path.size() > basepath.size() &&
            0 == memcmp(utf8path.data(), basepath.data(), basepath.size()))
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
            auto guard = conlock(std::cout);
            std::cout << utf8path;
            if (node->getType() == m::MegaNode::TYPE_FOLDER)
                std::cout << "/";

            if (flags.size)
                std::cout << " " << node->getSize();
            if (flags.ctime)
                std::cout << " " << node->getCreationTime();
            if (flags.mtime)
                std::cout << " " << node->getModificationTime();
            if (flags.handle)
                std::cout << " " << OwnStr(g_megaApi->handleToBase64(node->getHandle()));
        }
    }

    switch (node->getType())
    {
        case m::MegaNode::TYPE_UNKNOWN:
            if (show)
                std::cout << " TYPE_UNKNOWN" << std::endl;
            break;

        case m::MegaNode::TYPE_FILE:
            if (show)
                std::cout << std::endl;
            break;

        case m::MegaNode::TYPE_FOLDER:
        case m::MegaNode::TYPE_ROOT:
        case m::MegaNode::TYPE_INCOMING:
        case m::MegaNode::TYPE_RUBBISH:
            if (show && depth > 0)
                std::cout << std::endl;
            if (flags.recursive || depth == 0)
            {
                std::unique_ptr<m::MegaNodeList> children(
                    g_megaApi->getChildren(node, flags.order));
                if (children)
                    for (int i = 0; i < children->size(); ++i)
                    {
                        ls(children->get(i), basepath, flags, depth + 1);
                    }
            }
            break;
    }
}

void exec_ls(ac::ACState& s)
{
    std::string orderstring;
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
        std::string basepath = OwnStr(g_megaApi->getNodePath(node.get()));
        switch (node->getType())
        {
            case m::MegaNode::TYPE_FILE:
                basepath.clear();
                break;
            case m::MegaNode::TYPE_FOLDER:
            case m::MegaNode::TYPE_INCOMING:
            case m::MegaNode::TYPE_RUBBISH:
                basepath += "/";
                break;
            default:;
        }
        ls(node.get(), basepath, flags, 0);
    }
}

void exec_renamenode(ac::ACState& s)
{
    if (auto node = GetNodeByPath(s.words[1].s))
    {
        g_megaApi->renameNode(node.get(),
                              s.words[2].s.c_str(),
                              new OneShotRequestListener(
                                  [](m::MegaApi*, m::MegaRequest*, m::MegaError* e)
                                  {
                                      check_err("renamenode", e, ReportResult);
                                  }));
    }
}

void exec_createfolder(ac::ACState& s)
{
    if (auto node = GetNodeByPath(s.words[2].s))
    {
        g_megaApi->createFolder(s.words[1].s.c_str(),
                                node.get(),
                                new OneShotRequestListener(
                                    [](m::MegaApi*, m::MegaRequest*, m::MegaError* e)
                                    {
                                        check_err("createfolder", e, ReportResult);
                                    }));
    }
}

void exec_remove(ac::ACState& s)
{
    if (auto node = GetNodeByPath(s.words[1].s))
    {
        g_megaApi->remove(node.get(),
                          new OneShotRequestListener(
                              [](m::MegaApi*, m::MegaRequest*, m::MegaError* e)
                              {
                                  check_err("remove", e, ReportResult);
                              }));
    }
}

static m::MegaCancelToken* makeNewGlobalCancelToken()
{
    g_cancelTokens.emplace_back(m::MegaCancelToken::createInstance());
    conlock(std::cout) << "cancel token: " << g_cancelTokens.size() - 1 << std::endl;
    return g_cancelTokens.back().get();
}

void exec_cancelbytoken(ac::ACState& s)
{
    int id = 0;
    if (s.words.size() > 1)
    {
        id = atoi(s.words[1].s.c_str());
    }
    else
    {
        id = int(g_cancelTokens.size()) - 1;
    }
    if (id < 0 || id >= static_cast<int>(g_cancelTokens.size()))
    {
        conlock(std::cout) << "failed: cancel token id is out of range: " << id << std::endl;
    }
    else if (g_cancelTokens[static_cast<size_t>(id)].get() == nullptr)
    {
        conlock(std::cout) << "failed: cancel token no longer exists for id: " << id << std::endl;
    }
    else
    {
        g_cancelTokens[static_cast<size_t>(id)]->cancel();
        conlock(std::cout) << "cancel triggered for token id: " << id << std::endl;
    }
}

void exec_setmaxuploadspeed(ac::ACState& s)
{
    int bps = atoi(s.words[1].s.c_str());
    g_megaApi->setMaxUploadSpeed(bps);
}

void exec_setmaxdownloadspeed(ac::ACState& s)
{
    int bps = atoi(s.words[1].s.c_str());
    g_megaApi->setMaxDownloadSpeed(bps);
}

void exec_startupload(ac::ACState& s)
{
    std::string newfilename;
    bool set_filename = s.extractflagparam("-filename", newfilename);

    bool useCancelToken = s.extractflag("-withcanceltoken");
    m::MegaCancelToken* ct = useCancelToken ? makeNewGlobalCancelToken() : nullptr;

    bool logstage = s.extractflag("-logstage");

    if (auto node = GetNodeByPath(s.words[2].s))
    {
        if (!set_filename)
        {
            g_megaApi->startUpload(s.words[1].s.c_str(),
                                   node.get(),
                                   nullptr,
                                   0,
                                   nullptr,
                                   false,
                                   false,
                                   ct,
                                   new OneShotTransferListener(
                                       [](m::MegaApi*, m::MegaTransfer*, m::MegaError* e)
                                       {
                                           check_err("startUpload", e, ReportResult);
                                       },
                                       logstage));
        }
        else
        {
            g_megaApi->startUpload(s.words[1].s.c_str(),
                                   node.get(),
                                   newfilename.c_str(),
                                   0,
                                   nullptr,
                                   false,
                                   false,
                                   ct,
                                   new OneShotTransferListener(
                                       [](m::MegaApi*, m::MegaTransfer*, m::MegaError* e)
                                       {
                                           check_err("startUpload", e, ReportResult);
                                       },
                                       logstage));
        }
    }
}

void exec_startdownload(ac::ACState& s)
{
    bool useCancelToken = s.extractflag("-withcanceltoken");
    m::MegaCancelToken* ct = useCancelToken ? makeNewGlobalCancelToken() : nullptr;

    bool logstage = s.extractflag("-logstage");

    if (auto node = GetNodeByPath(s.words[1].s))
    {
        g_megaApi->startDownload(node.get(),
                                 s.words[2].s.c_str(),
                                 nullptr,
                                 nullptr,
                                 false,
                                 ct,
                                 ::mega::MegaTransfer::COLLISION_CHECK_FINGERPRINT,
                                 ::mega::MegaTransfer::COLLISION_RESOLUTION_OVERWRITE,
                                 false,
                                 new OneShotTransferListener(
                                     [](m::MegaApi*, m::MegaTransfer*, m::MegaError* e)
                                     {
                                         check_err("startDownload", e, ReportResult);
                                     },
                                     logstage));
    }
}

void exec_pausetransfers(ac::ACState& s)
{
    int paused = atoi(s.words[1].s.c_str());

    if (s.words.size() > 2)
    {
        int direction = atoi(s.words[2].s.c_str());

        g_megaApi->pauseTransfers(paused,
                                  direction,
                                  new OneShotRequestListener(
                                      [](m::MegaApi*, m::MegaRequest*, m::MegaError* e)
                                      {
                                          check_err("pauseTransfers", e, ReportResult);
                                      }));
    }
    else
    {
        g_megaApi->pauseTransfers(paused,
                                  new OneShotRequestListener(
                                      [](m::MegaApi*, m::MegaRequest*, m::MegaError* e)
                                      {
                                          check_err("pauseTransfers", e, ReportResult);
                                      }));
    }
}

void exec_pausetransferbytag(ac::ACState& s)
{
    int tag = atoi(s.words[1].s.c_str());
    int pause = atoi(s.words[2].s.c_str());

    g_megaApi->pauseTransferByTag(tag,
                                  pause,
                                  new OneShotRequestListener(
                                      [](m::MegaApi*, m::MegaRequest*, m::MegaError* e)
                                      {
                                          check_err("cancelTransferByTag", e, ReportResult);
                                      }));
}

void exec_canceltransfers(ac::ACState& s)
{
    auto direction = atoi(s.words[1].s.c_str());

    g_megaApi->cancelTransfers(direction,
                               new OneShotRequestListener(
                                   [](m::MegaApi*, m::MegaRequest*, m::MegaError* e)
                                   {
                                       check_err("cancelTransfers", e, ReportResult);
                                   }));
}

void exec_canceltransferbytag(ac::ACState& s)
{
    int tag = atoi(s.words[1].s.c_str());

    g_megaApi->cancelTransferByTag(tag,
                                   new OneShotRequestListener(
                                       [](m::MegaApi*, m::MegaRequest*, m::MegaError* e)
                                       {
                                           check_err("cancelTransferByTag", e, ReportResult);
                                       }));
}

void exec_gettransfers(ac::ACState& s)
{
    int type = atoi(s.words[1].s.c_str());

    std::unique_ptr<m::MegaTransferList> ts(g_megaApi->getTransfers(type));

    auto cl = conlock(std::cout);

    for (int i = 0; i < ts->size(); ++i)
    {
        m::MegaTransfer* t = ts->get(i);
        std::cout << t->getTag() << " : " << t->getPath() << std::endl;
    }
    std::cout << "(" << ts->size() << " transfers listed by tag, type " << type << ")" << std::endl;
}

void exec_exportNode(ac::ACState& s)
{
    std::string expire, writable;
    bool specifyWritable = s.extractflagparam("-writable", writable);
    bool specifyExpire = s.extractflagparam("-expire", expire);

    int64_t expireTime = atoll(expire.c_str());
    bool writableFlag = writable == "true";

    if (auto node = GetNodeByPath(s.words[1].s))
    {
        if (specifyWritable)
        {
            if (specifyExpire)
            {
                g_megaApi->exportNode(node.get(),
                                      expireTime,
                                      writableFlag,
                                      false,
                                      new OneShotRequestListener(
                                          [](m::MegaApi*, m::MegaRequest* r, m::MegaError* e)
                                          {
                                              if (check_err("exportnode", e, ReportFailure))
                                              {
                                                  conlock(std::cout)
                                                      << "Exported link: " << r->getLink()
                                                      << " and auth: " << r->getPrivateKey()
                                                      << std::endl;
                                              }
                                          }));
            }
            else
            {
                g_megaApi->exportNode(node.get(),
                                      writableFlag,
                                      false,
                                      new OneShotRequestListener(
                                          [](m::MegaApi*, m::MegaRequest* r, m::MegaError* e)
                                          {
                                              if (check_err("exportnode", e, ReportFailure))
                                              {
                                                  conlock(std::cout)
                                                      << "Exported link: " << r->getLink()
                                                      << " and auth: " << r->getPrivateKey()
                                                      << std::endl;
                                              }
                                          }));
            }
        }
        else
        {
            if (specifyExpire)
            {
                g_megaApi->exportNode(node.get(),
                                      expireTime,
                                      new OneShotRequestListener(
                                          [](m::MegaApi*, m::MegaRequest* r, m::MegaError* e)
                                          {
                                              if (check_err("exportnode", e, ReportFailure))
                                              {
                                                  conlock(std::cout)
                                                      << "Exported link: " << r->getLink()
                                                      << std::endl;
                                              }
                                          }));
            }
            else
            {
                g_megaApi->exportNode(node.get(),
                                      new OneShotRequestListener(
                                          [](m::MegaApi*, m::MegaRequest* r, m::MegaError* e)
                                          {
                                              if (check_err("exportnode", e, ReportFailure))
                                              {
                                                  conlock(std::cout)
                                                      << "Exported link: " << r->getLink()
                                                      << std::endl;
                                              }
                                          }));
            }
        }
    }
}

void exec_pushreceived(ac::ACState& s)
{
    bool beep = s.extractflag("-beep");

    if (s.words.size() == 2)
    {
        g_chatApi->pushReceived(beep,
                                s_ch(s.words[1].s),
                                new OneShotChatRequestListener(
                                    [](c::MegaChatApi*, c::MegaChatRequest*, c::MegaChatError* e)
                                    {
                                        check_err("pushReceived (iOS style)", e, ReportResult);
                                    }));
    }
    else
    {
        g_chatApi->pushReceived(beep,
                                new OneShotChatRequestListener(
                                    [](c::MegaChatApi*, c::MegaChatRequest*, c::MegaChatError* e)
                                    {
                                        check_err("pushReceived (Android style)", e, ReportResult);
                                    }));
    }
}

void exec_getcloudstorageused(ac::ACState&)
{
    g_megaApi->getCloudStorageUsed(new OneShotRequestListener(
        [](m::MegaApi*, m::MegaRequest* r, m::MegaError* e)
        {
            if (check_err("getcloudstorageused", e, ReportFailure))
            {
                conlock(std::cout)
                    << "Cloud storage used (locally calculated): " << r->getNumber() << std::endl;
            }
        }));
}

void exec_cp(ac::ACState& s)
{
    std::unique_ptr<m::MegaNode> srcnode(g_megaApi->getNodeByPath(s.words[1].s.c_str()));
    std::unique_ptr<m::MegaNode> dstnode(g_megaApi->getNodeByPath(s.words[2].s.c_str()));

    if (!srcnode)
    {
        conlock(std::cout) << "source not found" << std::endl;
    }
    else if (!dstnode)
    {
        conlock(std::cout) << "destination not found" << std::endl;
    }
    else if (dstnode->getType() <= m::MegaNode::TYPE_FILE)
    {
        conlock(std::cout) << "destination is not a folder" << std::endl;
    }
    else
    {
        g_megaApi->copyNode(srcnode.get(),
                            dstnode.get(),
                            new OneShotRequestListener(
                                [](m::MegaApi*, m::MegaRequest*, m::MegaError* e)
                                {
                                    check_err("copyNode", e, ReportResult);
                                }));
    }
}

void exec_mv(ac::ACState& s)
{
    std::string newname;
    bool rename = s.extractflagparam("-rename", newname);

    std::unique_ptr<m::MegaNode> srcnode(g_megaApi->getNodeByPath(s.words[1].s.c_str()));
    std::unique_ptr<m::MegaNode> dstnode(g_megaApi->getNodeByPath(s.words[2].s.c_str()));

    if (!srcnode)
    {
        conlock(std::cout) << "source not found" << std::endl;
    }
    else if (!dstnode)
    {
        conlock(std::cout) << "destination not found" << std::endl;
    }
    else if (dstnode->getType() <= m::MegaNode::TYPE_FILE)
    {
        conlock(std::cout) << "destination is not a folder" << std::endl;
    }
    else
    {
        if (rename)
        {
            g_megaApi->moveNode(srcnode.get(),
                                dstnode.get(),
                                newname.c_str(),
                                new OneShotRequestListener(
                                    [](m::MegaApi*, m::MegaRequest*, m::MegaError* e)
                                    {
                                        check_err("moveNode", e, ReportResult);
                                    }));
        }
        else
        {
            g_megaApi->moveNode(srcnode.get(),
                                dstnode.get(),
                                new OneShotRequestListener(
                                    [](m::MegaApi*, m::MegaRequest*, m::MegaError* e)
                                    {
                                        check_err("moveNode", e, ReportResult);
                                    }));
        }
    }
}

static void PrintAchievements(m::MegaAchievementsDetails& ad)
{
    auto cl = conlock(std::cout);

    cl << "getBaseStorage: " << ad.getBaseStorage() << std::endl;

    int classes[] = {m::MegaAchievementsDetails::MEGA_ACHIEVEMENT_WELCOME,
                     m::MegaAchievementsDetails::MEGA_ACHIEVEMENT_INVITE,
                     m::MegaAchievementsDetails::MEGA_ACHIEVEMENT_DESKTOP_INSTALL,
                     m::MegaAchievementsDetails::MEGA_ACHIEVEMENT_MOBILE_INSTALL,
                     m::MegaAchievementsDetails::MEGA_ACHIEVEMENT_ADD_PHONE};

    for (size_t i = 0; i < sizeof(classes) / sizeof(*classes); ++i)
    {
        cl << "class " << classes[i];
        cl << "  getClassStorage: " << ad.getClassStorage(classes[i]);
        cl << "  getClassTransfer: " << ad.getClassTransfer(classes[i]);
        cl << "  getClassExpire: " << ad.getClassExpire(classes[i]) << std::endl;
    }
    cl << "getAwardsCount: " << ad.getAwardsCount() << std::endl;
    for (unsigned i = 0; i < ad.getAwardsCount(); ++i)
    {
        cl << "Award " << i << std::endl;
        cl << "  getAwardClass: " << ad.getAwardClass(i) << std::endl;
        cl << "  getAwardId: " << ad.getAwardId(i) << std::endl;
        cl << "  getAwardTimestamp: " << ad.getAwardTimestamp(i) << std::endl;
        cl << "  getAwardExpirationTs: " << ad.getAwardExpirationTs(i) << std::endl;
        cl << "  getAwardClass: " << ad.getAwardClass(i) << std::endl;
        cl << "  getAwardEmails: <todo>" << std::endl; // << ad.getAwardEmails(i) << std::endl;
    }
    cl << "getRewardsCount: " << ad.getRewardsCount() << std::endl;
    for (unsigned i = 0; i < static_cast<unsigned>(ad.getRewardsCount()); ++i)
    {
        cl << "Reward " << i << std::endl;
        cl << "  getRewardAwardId: " << ad.getRewardAwardId(i) << std::endl;
        cl << "  getRewardStorage: " << ad.getRewardStorage(i) << std::endl;
        cl << "  getRewardTransfer: " << ad.getRewardTransfer(i) << std::endl;
        cl << "  getRewardStorageByAwardId: "
           << ad.getRewardStorageByAwardId(ad.getRewardAwardId(i)) << std::endl;
        cl << "  getRewardTransferByAwardId: "
           << ad.getRewardTransferByAwardId(ad.getRewardAwardId(i)) << std::endl;
        cl << "  getRewardExpire: " << ad.getRewardExpire(i) << std::endl;
    }
    cl << "currentStorage: " << ad.currentStorage() << std::endl;
    cl << "currentTransfer: " << ad.currentTransfer() << std::endl;
    cl << "currentStorageReferrals: " << ad.currentStorageReferrals() << std::endl;
    cl << "currentTransferReferrals: " << ad.currentTransferReferrals() << std::endl;
};

void exec_getaccountachievements(ac::ACState&)
{
    auto listener = new OneShotRequestListener;
    listener->onRequestFinishFunc = [](m::MegaApi*, m::MegaRequest* request, m::MegaError* e)
    {
        conlock(std::cout) << "getAccountAchievements Result: " << e->getErrorString() << std::endl;
        if (!e->getErrorCode())
        {
            std::unique_ptr<m::MegaAchievementsDetails> ad(request->getMegaAchievementsDetails());
            if (ad)
            {
                PrintAchievements(*ad);
            }
        }
    };

    g_megaApi->getAccountAchievements(listener);
}

void exec_getmegaachievements(ac::ACState&)
{
    auto listener = new OneShotRequestListener;
    listener->onRequestFinishFunc = [](m::MegaApi*, m::MegaRequest* request, m::MegaError* e)
    {
        conlock(std::cout) << "getAccountAchievements Result: " << e->getErrorString() << std::endl;
        if (!e->getErrorCode())
        {
            std::unique_ptr<m::MegaAchievementsDetails> ad(request->getMegaAchievementsDetails());
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
        conlock(std::cout) << "Folder not found.";
    }
    else
    {
        g_megaApi->setCameraUploadsFolder(
            srcnode->getHandle(),
            new OneShotRequestListener(
                [](m::MegaApi*, m::MegaRequest* r, m::MegaError* e)
                {
                    conlock(std::cout)
                        << "Camera upload folder request flag: " << r->getFlag() << std::endl;
                    conlock(std::cout) << "Camera upload folder request handle: "
                                       << base64NodeHandle(r->getNodeHandle()) << std::endl;
                    check_err("setCameraUploadsFolder", e, ReportResult);
                }));
    }
}

void exec_getCameraUploadsFolder(ac::ACState&)
{
    g_megaApi->getCameraUploadsFolder(new OneShotRequestListener(
        [](m::MegaApi*, m::MegaRequest* r, m::MegaError* e)
        {
            conlock(std::cout) << "Camera upload folder flag: " << r->getFlag() << std::endl;
            if (check_err("getCameraUploadsFolder", e, ReportFailure))
            {
                std::unique_ptr<m::MegaNode> node(g_megaApi->getNodeByHandle(r->getNodeHandle()));
                if (!node)
                {
                    conlock(std::cout) << "No node found by looking up handle: "
                                       << base64NodeHandle(r->getNodeHandle()) << std::endl;
                }
                else
                {
                    conlock(std::cout)
                        << "Camera upload folder: " << OwnStr(g_megaApi->getNodePath(node.get()))
                        << std::endl;
                }
            }
        }));
}

void exec_setCameraUploadsFolderSecondary(ac::ACState& s)
{
    std::unique_ptr<m::MegaNode> srcnode(g_megaApi->getNodeByPath(s.words[1].s.c_str()));

    if (!srcnode)
    {
        conlock(std::cout) << "Folder not found.";
    }
    else
    {
        g_megaApi->setCameraUploadsFolderSecondary(
            srcnode->getHandle(),
            new OneShotRequestListener(
                [](m::MegaApi*, m::MegaRequest* r, m::MegaError* e)
                {
                    conlock(std::cout)
                        << "Camera upload folder request flag: " << r->getFlag() << std::endl;
                    conlock(std::cout) << "Camera upload folder request handle: "
                                       << base64NodeHandle(r->getNodeHandle()) << std::endl;
                    check_err("setCameraUploadsFolderSecondary", e, ReportResult);
                }));
    }
}

void exec_getCameraUploadsFolderSecondary(ac::ACState&)
{
    g_megaApi->getCameraUploadsFolderSecondary(new OneShotRequestListener(
        [](m::MegaApi*, m::MegaRequest* r, m::MegaError* e)
        {
            conlock(std::cout) << "Camera upload folder flag: " << r->getFlag() << std::endl;
            if (check_err("getCameraUploadsFolderSecondary", e, ReportFailure))
            {
                std::unique_ptr<m::MegaNode> node(g_megaApi->getNodeByHandle(r->getNodeHandle()));
                if (!node)
                {
                    conlock(std::cout) << "No node found by looking up handle: "
                                       << base64NodeHandle(r->getNodeHandle()) << std::endl;
                }
                else
                {
                    conlock(std::cout) << "Camera upload folder (secondary): "
                                       << OwnStr(g_megaApi->getNodePath(node.get())) << std::endl;
                }
            }
        }));
}

void exec_getContact(ac::ACState& s)
{
    std::unique_ptr<m::MegaUser> user(g_megaApi->getContact(s.words[1].s.c_str()));
    if (user)
    {
        conlock(std::cout) << "found with handle: " << ch_s(user->getHandle())
                           << " timestamp: " << user->getTimestamp() << std::endl;
    }
    else
    {
        conlock(std::cout) << "No user found with that email" << std::endl;
    }
}

void exec_getDefaultTZ(ac::ACState& s)
{
    auto listener = new OneShotRequestListener;
    listener->onRequestFinishFunc = [](m::MegaApi*, m::MegaRequest* request, m::MegaError* e)
    {
        auto cl = conlock(std::cout);
        cl << "Get Default Time Zone Result: " << e->getErrorString() << std::endl;

        if (!e->getErrorCode())
        {
            ::mega::MegaTimeZoneDetails* tz = request->getMegaTimeZoneDetails();
            assert(tz);

            int defaulttz = tz->getDefault();
            assert(defaulttz < tz->getNumTimeZones());

            // print relevant info
            cl << "Default Time Zone: " << tz->getTimeZone(defaulttz) << std::endl
               << "Time offset: " << tz->getTimeOffset(defaulttz) << std::endl;
        }
    };

    // send the request
    conlock(std::cout) << "  Command `" << s.words[0].s << "` is executing in the background..."
                       << std::endl;
    g_megaApi->fetchTimeZone(listener);
}

void exec_isGeolocOn(ac::ACState& s)
{
    if (!g_megaApi->isLoggedIn())
    {
        conlock(std::cout) << "Invalid operation, needs successful login." << std::endl;
        return;
    }

    auto listener = new OneShotRequestListener;
    listener->onRequestFinishFunc = [](m::MegaApi*, m::MegaRequest*, m::MegaError* e)
    {
        const char* on = e->getErrorCode() ? "false" : "true";
        conlock(std::cout) << "Is Geolocation Enabled Result: " << on << std::endl;
    };

    // send the request
    conlock(std::cout) << "  Command `" << s.words[0].s << "` is executing in the background..."
                       << std::endl;
    g_megaApi->isGeolocationEnabled(listener);
}

void exec_setGeolocOn(ac::ACState& s)
{
    if (!g_megaApi->isLoggedIn())
    {
        conlock(std::cout) << "Invalid operation, needs successful login." << std::endl;
        return;
    }

    auto listener = new OneShotRequestListener;
    listener->onRequestFinishFunc = [](m::MegaApi*, m::MegaRequest*, m::MegaError* e)
    {
        const char* on = e->getErrorCode() ? "false" : "true";
        conlock(std::cout) << "Enable Geolocation Result: " << on << std::endl;
    };

    // send the request
    conlock(std::cout) << "  Command `" << s.words[0].s << "` is executing in the background..."
                       << std::endl;
    g_megaApi->enableGeolocation(listener);
}

static bool typematchesnodetype(int pathtype, int nodetype)
{
    switch (pathtype)
    {
        case m::MegaNode::TYPE_FOLDER:
        case m::MegaNode::TYPE_FILE:
            return nodetype == pathtype;
        default:
            return false;
    }
}

static bool recursiveCompare(m::MegaNode& mn, fs::path p)
{
    auto pathtype = fs::is_directory(p)    ? m::MegaNode::TYPE_FOLDER :
                    fs::is_regular_file(p) ? m::MegaNode::TYPE_FILE :
                                             m::MegaNode::TYPE_UNKNOWN;
    if (!typematchesnodetype(pathtype, mn.getType()))
    {
        std::cout << "Path type mismatch: " << OwnStr(g_megaApi->getNodePath(&mn)) << ":"
                  << mn.getType() << " " << p.u8string() << ":" << pathtype << std::endl;
        return false;
    }

    if (pathtype == m::MegaNode::TYPE_FILE)
    {
        int64_t size = (int64_t)fs::file_size(p);
        if (size != (int64_t)mn.getSize())
        {
            std::cout << "File size mismatch: " << OwnStr(g_megaApi->getNodePath(&mn)) << ":"
                      << mn.getType() << " " << p.u8string() << ":" << size << std::endl;
        }
    }

    if (pathtype != m::MegaNode::TYPE_FOLDER)
    {
        return true;
    }

    std::unique_ptr<m::MegaNodeList> children(g_megaApi->getChildren(&mn, 0));

    std::multimap<std::string, m::MegaNode*> ms;
    std::multimap<std::string, fs::path> ps;
    for (auto i = children->size(); i--;)
    {
        ms.emplace(children->get(i)->getName(), children->get(i));
    }
    for (fs::directory_iterator pi(p); pi != fs::directory_iterator(); ++pi)
    {
        ps.emplace(pi->path().filename().u8string(), pi->path());
    }

    for (auto p_iter = ps.begin(); p_iter != ps.end();)
    {
        auto er = ms.equal_range(p_iter->first);
        auto next_p = p_iter;
        ++next_p;
        for (auto i = er.first; i != er.second; ++i)
        {
            if (recursiveCompare(*i->second, p_iter->second))
            {
                ms.erase(i);
                ps.erase(p_iter);
                break;
            }
        }
        p_iter = next_p;
    }
    if (ps.empty() && ms.empty())
    {
        return true;
    }
    else
    {
        std::cout << "Extra content detected between " << OwnStr(g_megaApi->getNodePath(&mn))
                  << " and " << p.u8string() << std::endl;
        for (auto& mi: ms)
            std::cout << "Extra remote: " << mi.first << std::endl;
        for (auto& pi: ps)
            std::cout << "Extra local: " << pi.second << std::endl;
        return false;
    };
}

void exec_treecompare(ac::ACState& s)
{
    fs::path p = pathFromLocalPath(s.words[1].s, true);
    std::unique_ptr<m::MegaNode> n(g_megaApi->getNodeByPath(s.words[2].s.c_str()));
    if (n && !p.empty())
    {
        recursiveCompare(*n, p);
    }
}

static bool buildLocalFolders(fs::path targetfolder,
                              const std::string& prefix,
                              int foldersperfolder,
                              int recurselevel,
                              int filesperfolder,
                              int filesize,
                              int& totalfilecount,
                              int& totalfoldercount)
{
    fs::path p = targetfolder / fs::u8path(prefix);
    if (!fs::is_directory(p) && !fs::create_directory(p))
        return false;
    ++totalfoldercount;

    for (int i = 0; i < filesperfolder; ++i)
    {
        std::string filename = prefix + "_file_" + std::to_string(++totalfilecount);
        fs::path fp = p / fs::u8path(filename);
        std::ofstream fs(fp.u8string(), std::ios::binary);

        for (unsigned j = static_cast<unsigned>(filesize) / static_cast<unsigned>(sizeof(int));
             j--;)
        {
            fs.write((char*)&totalfilecount, sizeof(int));
        }
        fs.write((char*)&totalfilecount, static_cast<unsigned>(filesize) % sizeof(int));
    }

    if (recurselevel > 1)
    {
        for (int i = 0; i < foldersperfolder; ++i)
        {
            if (!buildLocalFolders(p,
                                   prefix + "_" + std::to_string(i),
                                   foldersperfolder,
                                   recurselevel - 1,
                                   filesperfolder,
                                   filesize,
                                   totalfilecount,
                                   totalfoldercount))
                return false;
        }
    }
    return true;
}

void exec_generatetestfilesfolders(ac::ACState& s)
{
    std::string param, nameprefix = "test";
    int folderdepth = 1, folderwidth = 1, filecount = 100, filesize = 1024;
    if (s.extractflagparam("-folderdepth", param))
        folderdepth = atoi(param.c_str());
    if (s.extractflagparam("-folderwidth", param))
        folderwidth = atoi(param.c_str());
    if (s.extractflagparam("-filecount", param))
        filecount = atoi(param.c_str());
    if (s.extractflagparam("-filesize", param))
        filesize = atoi(param.c_str());
    if (s.extractflagparam("-nameprefix", param))
        nameprefix = param;

    fs::path p = pathFromLocalPath(s.words[1].s, true);
    if (!p.empty())
    {
        int totalfilecount = 0, totalfoldercount = 0;
        buildLocalFolders(p,
                          nameprefix,
                          folderwidth,
                          folderdepth,
                          filecount,
                          filesize,
                          totalfilecount,
                          totalfoldercount);
        conlock(std::cout) << "created " << totalfilecount << " files and " << totalfoldercount
                           << " folders" << std::endl;
    }
    else
    {
        conlock(std::cout) << "invalid directory: " << p.u8string() << std::endl;
    }
}

void exec_syncadd(ac::ACState& s)
{
    std::string drive, name;
    bool backup = s.extractflag("-backup");
    bool external = s.extractflagparam("-external", drive);
    bool named = s.extractflagparam("-name", name);

    // sync add source target
    std::string drivePath = drive;
    std::string sourcePath = s.words[2].s;
    std::string targetPath = s.words[3].s;

    // Does the target node exist?
    std::unique_ptr<m::MegaNode> targetNode(g_megaApi->getNodeByPath(targetPath.c_str()));

    if (!targetNode)
    {
        std::cerr << targetPath << ": Not found." << std::endl;
        return;
    }

    // Try and add the new sync.
    g_megaApi->syncFolder(backup ? m::MegaSync::TYPE_BACKUP : m::MegaSync::TYPE_TWOWAY,
                          sourcePath.c_str(),
                          named ? name.c_str() : nullptr,
                          targetNode->getHandle(),
                          external ? drive.c_str() : nullptr,
                          new OneShotRequestListener(
                              [](m::MegaApi*, m::MegaRequest*, m::MegaError* e)
                              {
                                  conlock(std::cout)
                                      << "syncFolder result: " << e->getErrorString() << std::endl;
                              }));
}

void exec_syncclosedrive(ac::ACState& s)
{
    std::string drive = s.words[2].s;
    g_megaApi->closeExternalBackupSyncsFromExternalDrive(
        drive.c_str(),
        new OneShotRequestListener(
            [](m::MegaApi*, m::MegaRequest*, m::MegaError* e)
            {
                conlock(std::cout)
                    << "closeExternalBackupSyncsFromExternalDrive result: " << e->getErrorString()
                    << std::endl;
            }));
}

void exec_syncexport(ac::ACState& s)
{
    auto configs = std::unique_ptr<const char[]>(g_megaApi->exportSyncConfigs());

    if (s.words.size() == 2)
    {
        conlock(std::cout) << "Configs exported as: " << configs.get() << std::endl;
        return;
    }

    auto flags = std::ios::binary | std::ios::out | std::ios::trunc;
    std::ofstream ostream(s.words[2].s, flags);

    ostream.write(configs.get(), static_cast<std::streamsize>(strlen(configs.get())));
    ostream.close();

    if (!ostream.good())
    {
        conlock(std::cout) << "Failed to write exported configs to: " << s.words[2].s << std::endl;
    }
}

void exec_syncimport(ac::ACState& s)
{
    auto flags = std::ios::binary | std::ios::in;
    std::ifstream istream(s.words[2].s, flags);

    if (!istream)
    {
        conlock(std::cout) << "Unable to open " << s.words[2].s << " for reading.";
        return;
    }

    std::string data;

    for (char buffer[512]; istream;)
    {
        istream.read(buffer, sizeof(buffer));

        if (auto nRead = istream.gcount())
        {
            data.append(buffer, static_cast<size_t>(nRead));
        }
    }

    if (!istream.eof())
    {
        conlock(std::cout) << "Unable to read " << s.words[2].s << std::endl;
        return;
    }

    auto completion = [](m::MegaApi*, m::MegaRequest*, m::MegaError* result)
    {
        assert(result);

        if (result->getErrorCode())
        {
            conlock(std::cout) << "Unable to import sync configs: " << result->getErrorString()
                               << std::endl;
            return;
        }

        conlock(std::cout) << "Syncs configs successfully imported." << std::endl;
    };

    conlock(std::cout) << "Importing sync configs..." << std::endl;

    auto* listener = new OneShotRequestListener(std::move(completion));
    g_megaApi->importSyncConfigs(data.c_str(), listener);
}

void exec_syncopendrive(ac::ACState& s)
{
    std::string drive = s.words[2].s;
    g_megaApi->loadExternalBackupSyncsFromExternalDrive(
        drive.c_str(),
        new OneShotRequestListener(
            [](m::MegaApi*, m::MegaRequest*, m::MegaError* e)
            {
                conlock(std::cout)
                    << "loadExternalBackupSyncsFromExternalDrive result: " << e->getErrorString()
                    << std::endl;
            }));
}

void exec_synclist(ac::ACState&)
{
    std::unique_ptr<m::MegaSyncList> syncs(g_megaApi->getSyncs());

    for (int i = 0; i < syncs->size(); ++i)
    {
        auto sync = syncs->get(i);

        // Display name.
        conlock(std::cout) << "Sync " << ch_s(sync->getBackupId()) << ": " << sync->getName()
                           << "\n";

        std::unique_ptr<m::MegaNode> node(g_megaApi->getNodeByHandle(sync->getMegaHandle()));
        std::unique_ptr<char[]> nodepath(node ? g_megaApi->getNodePath(node.get()) :
                                                g_megaApi->strdup(""));

        // Display source/target mapping.
        conlock(std::cout) << "  Mapping: " << sync->getLocalFolder() << " -> "
                           << (strlen(nodepath.get()) ? nodepath.get() :
                                                        sync->getLastKnownMegaFolder())
                           << "\n";

        if (sync)
        {
            //// Display status info.
            // conlock(std::cout) << "  State: "
            //     << SyncConfig::syncstatename(sync->state)
            //     << "\n";

            //// Display some usage stats.
            // conlock(std::cout) << "  Statistics: "
            //     << sync->localbytes
            //     << " byte(s) across "
            //     << sync->localnodes[FILENODE]
            //     << " file(s) and "
            //     << sync->localnodes[FOLDERNODE]
            //     << " folder(s).\n";
        }
        else
        {
            // Display what status info we can.
            conlock(std::cout) << "  State: " << sync->getRunState() << "\n"
                               << "  Last Error: " << sync->getMegaSyncErrorCode(sync->getError())
                               << "\n";
        }

        // Display sync type.
        conlock(std::cout)
            //<< (config.isExternal() ? "EX" : "IN")
            //<< "TERNAL "
            << " type " << sync->getType() << "\n"
            << std::endl;
    }
    conlock(std::cout) << syncs->size() << " syncs listed." << std::endl;
}

void exec_syncremove(ac::ACState& s)
{
    std::string id, path;
    bool byId = s.extractflagparam("-id", id);
    bool byPath = s.extractflagparam("-path", path);

    if (byPath)
    {
        std::unique_ptr<m::MegaNode> targetNode(g_megaApi->getNodeByPath(path.c_str()));
        if (!targetNode)
        {
            conlock(std::cout) << "cloud folder not found" << std::endl;
            return;
        }

        std::unique_ptr<m::MegaSync> sync(g_megaApi->getSyncByNode(targetNode.get()));
        m::MegaHandle backupId = sync ? sync->getBackupId() : m::INVALID_HANDLE;

        g_megaApi->removeSync(backupId,
                              new OneShotRequestListener(
                                  [](m::MegaApi*, m::MegaRequest*, m::MegaError* e)
                                  {
                                      conlock(std::cout)
                                          << "removeSync result: " << e->getErrorString()
                                          << std::endl;
                                  }));
    }
    else if (byId)
    {
        g_megaApi->removeSync(g_megaApi->base64ToHandle(id.c_str()),
                              new OneShotRequestListener(
                                  [](m::MegaApi*, m::MegaRequest*, m::MegaError* e)
                                  {
                                      conlock(std::cout)
                                          << "removeSync result: " << e->getErrorString()
                                          << std::endl;
                                  }));
    }
}

void exec_syncxable(ac::ACState& s)
{
    const auto command = s.words[1].s;
    const auto id = s.words[2].s;

    auto backupId = m::MegaApi::base64ToBackupId(id.c_str());

    if (command == "enable")
    {
        auto completion = [id](m::MegaApi*, m::MegaRequest*, m::MegaError* result)
        {
            if (result->getErrorCode())
            {
                conlock(std::cout) << "Unable to enable sync " << id << ": "
                                   << result->getErrorString() << std::endl;
                return;
            }

            conlock(std::cout) << "Sync " << id << " enabled." << std::endl;
        };

        conlock(std::cout) << "Enabling sync " << id << "..." << std::endl;

        auto* listener = new OneShotRequestListener(std::move(completion));
        g_megaApi->setSyncRunState(backupId, m::MegaSync::RUNSTATE_RUNNING, listener);

        return;
    }

    auto completion = [id](m::MegaApi*, m::MegaRequest*, m::MegaError* result)
    {
        if (result->getErrorCode())
        {
            conlock(std::cout) << "Unable to disable sync " << id << ": " << result->getErrorCode()
                               << std::endl;
            return;
        }

        conlock(std::cout) << "Sync " << id << " disabled." << std::endl;
    };

    conlock(std::cout) << "Disabling sync " << id << "..." << std::endl;

    auto* listener = new OneShotRequestListener(std::move(completion));
    g_megaApi->setSyncRunState(backupId, m::MegaSync::RUNSTATE_DISABLED, listener);
}

void exec_getmybackupsfolder(ac::ACState&)
{
    g_megaApi->getUserAttribute(
        m::ATTR_MY_BACKUPS_FOLDER,
        new OneShotRequestListener(
            [](m::MegaApi*, m::MegaRequest* request, m::MegaError* e)
            {
                ConsoleLock outstr(std::cout);
                outstr << "getUserAttribute ATTR_MY_BACKUPS_FOLDER result: ";

                if (e->getErrorCode() != m::MegaError::API_OK)
                {
                    outstr << e->getErrorString() << std::endl;
                }
                else
                {
                    outstr << OwnStr(g_megaApi->getNodePathByNodeHandle(request->getNodeHandle()))
                           << std::endl;
                }
            }));
}

void exec_setmybackupsfolder(ac::ACState& s)
{
    g_megaApi->setMyBackupsFolder(s.words[1].s.c_str(),
                                  new OneShotRequestListener(
                                      [](m::MegaApi*, m::MegaRequest*, m::MegaError* e)
                                      {
                                          conlock(std::cout) << "setMyBackupsFolder result: "
                                                             << e->getErrorString() << std::endl;
                                      }));
}

}
