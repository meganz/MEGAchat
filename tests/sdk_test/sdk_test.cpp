#include "sdk_test.h"

#include <mega.h>
#include <megaapi.h>

#ifdef _WIN32
#include <direct.h>
#endif

using namespace mega;
using namespace megachat;
using namespace std;

const std::string MegaChatApiTest::DEFAULT_PATH = "../../tests/sdk_test/";
const std::string MegaChatApiTest::FILE_IMAGE_NAME = "logo.png";
const std::string MegaChatApiTest::PATH_IMAGE = "PATH_IMAGE";

const std::string MegaChatApiTest::LOCAL_PATH = "./tmp"; // no ending slash
const std::string MegaChatApiTest::REMOTE_PATH = "/";
const std::string MegaChatApiTest::DOWNLOAD_PATH = LOCAL_PATH + "/download/";

class GTestLogger : public ::testing::EmptyTestEventListener
{
public:
    void OnTestStart(const ::testing::TestInfo& info) override
    {
        LOG_info << "GTEST: " << info.test_suite_name() << '.' << info.name() << " RUNNING";
    }

    void OnTestEnd(const ::testing::TestInfo& info) override
    {
        LOG_info << "GTEST: " << info.test_suite_name() << '.' << info.name() << ' '
                 << (info.result()->Passed() ? "PASSED" : "FAILED");
    }

    void OnTestPartResult(const ::testing::TestPartResult& result) override
    {
        if (result.type() == ::testing::TestPartResult::kSuccess) return;

        std::string location = result.file_name() ? result.file_name() : "unknown";
        if (result.line_number() >= 0)
        {
            location += ':' + std::to_string(result.line_number());
        }

        LOG_info << "GTEST: " << location << ": Failure";

        std::istringstream istream(result.message());
        for (std::string s; std::getline(istream, s); )
        {
            LOG_info << "GTEST: " << s;
        }
    }
}; // GTestLogger

int main(int argc, char **argv)
{
    remove("test.log");

    std::vector<char*> myargv1(argv, argv + argc);
    for (auto it = myargv1.begin(); it != myargv1.end(); ++it)
    {
        if (std::string(*it).substr(0, 9) == "--APIURL:")
        {
            std::lock_guard<std::mutex> g(g_APIURL_default_mutex);
            g_APIURL_default = std::string(*it).substr(9);
            if (!g_APIURL_default.empty() && g_APIURL_default.back() != '/')
                g_APIURL_default += '/';
        }
    }
    MegaChatApiTest::init();
    testing::InitGoogleTest(&argc, argv);
    testing::UnitTest::GetInstance()->listeners().Append(new GTestLogger());

    int rc = RUN_ALL_TESTS(); // returns 0 (success) or 1 (failed tests)

    MegaChatApiTest::terminate();

    MegaChatApiUnitaryTest unitaryTest;
    std::cout << "[========] Unitary tests " << std::endl;
    unitaryTest.UNITARYTEST_ParseUrl();
#ifndef KARERE_DISABLE_WEBRTC
    unitaryTest.UNITARYTEST_SfuDataReception();
#endif

    std::cout << "[========] End Unitary tests " << std::endl;

    return rc + unitaryTest.mFailedTests;
}

Account::Account()
{
}

Account::Account(const std::string &email, const std::string &password)
    : mEmail(email)
    , mPassword(password)
{
}

std::string Account::getEmail() const
{
    return mEmail;
}

std::string Account::getPassword() const
{
    return mPassword;
}

MegaChatApiTest::MegaChatApiTest()
{
    for (unsigned i = 0u; i < NUM_ACCOUNTS; i++)
    {
        megaApi[i] = NULL;
        megaChatApi[i] = NULL;
    }
}

MegaChatApiTest::~MegaChatApiTest()
{
}

char *MegaChatApiTest::login(unsigned int accountIndex, const char *session, const char *email, const char *password)
{
    std::string mail;
    std::string pwd;
    if (email == NULL || password == NULL)
    {
        mail = account(accountIndex).getEmail();
        pwd = account(accountIndex).getPassword();
    }
    else
    {
        mail = email;
        pwd = password;
    }

    // 1. Initialize chat engine
    bool *flagInit = &initStateChanged[accountIndex]; *flagInit = false;
    int initializationState = megaChatApi[accountIndex]->init(session);
    EXPECT_GE(initializationState, 0) << "MegaChatApiImpl::init returned error";
    if (initializationState < 0) return nullptr;
    MegaApi::removeLoggerObject(logger());
    bool responseOk = waitForResponse(flagInit);
    EXPECT_TRUE(responseOk) << "Initialization failed";
    if (!responseOk) return nullptr;
    int initStateValue = initState[accountIndex];
    if (!session)
    {
        EXPECT_EQ(initStateValue, MegaChatApi::INIT_WAITING_NEW_SESSION) << "Wrong chat initialization state.";
        if (initStateValue != MegaChatApi::INIT_WAITING_NEW_SESSION) return nullptr;
    }
    else
    {
        EXPECT_EQ(initStateValue, MegaChatApi::INIT_OFFLINE_SESSION) << "Wrong chat initialization state.";
        if (initStateValue != MegaChatApi::INIT_OFFLINE_SESSION) return nullptr;
    }

    // 2. login
    RequestTracker loginTracker;
    session ? megaApi[accountIndex]->fastLogin(session, &loginTracker)
              : megaApi[accountIndex]->login(mail.c_str(), pwd.c_str(), &loginTracker);
    int loginResult = loginTracker.waitForResult();
    EXPECT_EQ(loginResult, API_OK) << "Login failed. Error: " << loginResult << ' ' << loginTracker.getErrorString();
    if (loginResult != API_OK) return nullptr;

    // 3. fetchnodes
    flagInit = &initStateChanged[accountIndex]; *flagInit = false;
    bool *loggedInFlag = &mLoggedInAllChats[accountIndex]; *loggedInFlag = false;
    RequestTracker fetchNodesTracker;
    megaApi[accountIndex]->fetchNodes(&fetchNodesTracker);
    int fetchNodesResult = fetchNodesTracker.waitForResult();
    EXPECT_EQ(fetchNodesResult, API_OK) << "Error fetch nodes. Error: " << fetchNodesResult << ' ' << fetchNodesTracker.getErrorString();
    if (fetchNodesResult != API_OK) return nullptr;
    // after fetchnodes, karere should be ready for offline, at least
    responseOk = waitForResponse(flagInit);
    EXPECT_TRUE(responseOk) << "Expired timeout for change init state";
    if (!responseOk) return nullptr;
    initStateValue = initState[accountIndex];
    EXPECT_EQ(initStateValue, MegaChatApi::INIT_ONLINE_SESSION) << "Wrong chat initialization state.";
    if (initStateValue != MegaChatApi::INIT_ONLINE_SESSION) return nullptr;

    // if there are chatrooms in this account, wait to be joined to all of them
    std::unique_ptr<MegaChatListItemList> items(megaChatApi[accountIndex]->getChatListItems());
    if (items->size())
    {
        responseOk = waitForResponse(loggedInFlag, 120);
        EXPECT_TRUE(responseOk) << "Expired timeout for login to all chats in account '" << mail << "'. (DDOS protection triggered?)";
        if (!responseOk) return nullptr;
    }

    return megaApi[accountIndex]->dumpSession();
}

void MegaChatApiTest::logout(unsigned int accountIndex, bool closeSession)
{
    bool *flagRequestLogoutChat = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_LOGOUT]; *flagRequestLogoutChat = false;
    RequestTracker logoutTracker;
    if (closeSession)
    {
#ifdef ENABLE_SYNC
        megaApi[accountIndex]->logout(false, &logoutTracker);
#else
        megaApi[accountIndex]->logout(logoutTracker.get());
#endif
    }
    else
    {
        megaApi[accountIndex]->localLogout(&logoutTracker);
    }

    int logoutResult = logoutTracker.waitForResult();
    ASSERT_TRUE(logoutResult == API_OK || logoutResult == API_ESID) <<
                     "Error sdk logout. Error: " << logoutResult << ' ' << logoutTracker.getErrorString();


    if (!closeSession)  // for closed session, karere automatically logs out itself
    {
        megaChatApi[accountIndex]->localLogout();

    }

    ASSERT_TRUE(waitForResponse(flagRequestLogoutChat)) << "Expired timeout for chat logout";
    ASSERT_TRUE(!lastErrorChat[accountIndex]) << "Error chat logout. Error: " << lastErrorMsgChat[accountIndex] << " (" << lastErrorChat[accountIndex] << ")";
    MegaApi::addLoggerObject(logger());   // need to restore customized logger

}

void MegaChatApiTest::init()
{
    std::cout << "[========] Global test environment initialization" << endl;

    getEnv().setLogFile("test.log");
    MegaApi::addLoggerObject(logger());
    MegaApi::setLogToConsole(false);    // already disabled by default
    MegaChatApi::setLoggerObject(logger());
    MegaChatApi::setLogToConsole(false);
    MegaChatApi::setCatchException(false);

    for (unsigned int i = 0; i < NUM_ACCOUNTS; i++)
    {
        // get credentials from environment variables
        std::string varName = "MEGA_EMAIL";
        varName += std::to_string(i);
        char *buf = getenv(varName.c_str());
        std::string email;
        if (buf)
        {
            email.assign(buf);
        }
        if (!email.length())
        {
            cout << "TEST - Set your username at the environment variable $" << varName << endl;
            exit(-1);
        }

        varName.assign("MEGA_PWD");
        varName += std::to_string(i);
        buf = getenv(varName.c_str());
        std::string pwd;
        if (buf)
        {
            pwd.assign(buf);
        }
        if (!pwd.length())
        {
            cout << "TEST - Set your password at the environment variable $" << varName << endl;
            exit(-1);
        }

        getEnv().addAccount(email, pwd);
    }
}

void MegaChatApiTest::terminate()
{
    std::cout << "[==========] Global test environment termination" << endl; \

    MegaApi::removeLoggerObject(logger());
    MegaChatApi::setLoggerObject(NULL);
}

void MegaChatApiTest::SetUp()
{
    const ::testing::TestInfo* ti = ::testing::UnitTest::GetInstance()->current_test_info();
    string name = string(ti->test_suite_name()) + '.' + ti->name();
    LOG_info << "Test " << name << ": SetUp starting.";

    struct stat st = {}; // init all members to default values (0)
    if (stat(LOCAL_PATH.c_str(), &st) == -1)
    {
#ifdef _WIN32
        _mkdir(LOCAL_PATH.c_str());
#else
        mkdir(LOCAL_PATH.c_str(), 0700);
#endif
    }

    for (unsigned i = 0u; i < NUM_ACCOUNTS; i++)
    {
        char path[1024];
#ifdef _WIN32
        _getcwd(path, sizeof path);
#else
        getcwd(path, sizeof path);
#endif
        megaApi[i] = new MegaApi(APPLICATION_KEY.c_str(), path, USER_AGENT_DESCRIPTION.c_str());
        megaApi[i]->setLogLevel(MegaApi::LOG_LEVEL_DEBUG);
        megaApi[i]->addListener(this);

        megaChatApi[i] = new MegaChatApi(megaApi[i]);
        megaChatApi[i]->setLogLevel(MegaChatApi::LOG_LEVEL_DEBUG);
        megaChatApi[i]->addChatRequestListener(this);
        megaChatApi[i]->addChatListener(this);

#ifndef KARERE_DISABLE_WEBRTC
        megaChatApi[i]->addChatCallListener(this);
        megaChatApi[i]->addSchedMeetingListener(this);
#endif

        // kill all sessions to ensure no interferences from other tests running in parallel
        RequestTracker loginTracker;
        megaApi[i]->login(account(i).getEmail().c_str(), account(i).getPassword().c_str(),
                          &loginTracker);
        ASSERT_EQ(loginTracker.waitForResult(), API_OK) << "Login failed in SetUp(). Error: " << loginTracker.getErrorString();

        RequestTracker killSessionTracker;
        megaApi[i]->killSession(INVALID_HANDLE, &killSessionTracker);
        ASSERT_EQ(killSessionTracker.waitForResult(), API_OK) << "Kill sessions failed in SetUp(). Error: " << killSessionTracker.getErrorString();
        RequestTracker logoutTracker;
#ifdef ENABLE_SYNC
        megaApi[i]->logout(false, &logoutTracker);
#else
        megaApi[i]->logout(&logoutTracker);
#endif
        int logoutResult = logoutTracker.waitForResult();
        ASSERT_TRUE(logoutResult == API_OK || logoutResult == API_ESID) <<
                         "Logout failed in SetUp(). Error: " << logoutResult << ' ' << logoutTracker.getErrorString();

        for (int j = 0; j < ::mega::MegaRequest::TOTAL_OF_REQUEST_TYPES; ++j)
        {
            requestFlags[i][j] = false;
        }

        for (int j = 0; j < megachat::MegaChatRequest::TOTAL_OF_REQUEST_TYPES; ++j)
        {
            requestFlagsChat[i][j] = false;
        }

        initStateChanged[i] = false;
        initState[i] = -1;
        mChatConnectionOnline[i] = false;
        mLoggedInAllChats[i] = false;
        mChatsUpdated[i] = false;
        mChatListUpdated[i].clear();
        lastErrorChat[i] = -1;
        lastErrorMsgChat[i].clear();
        lastErrorTransfer[i] = -1;

        chatid[i] = MEGACHAT_INVALID_HANDLE;  // chatroom id from request
        chatroom[i] = NULL;
        chatUpdated[i] = false;
        chatItemUpdated[i] = false;
        chatItemClosed[i] = false;
        peersUpdated[i] = false;
        titleUpdated[i] = false;
        chatArchived[i] = false;

        mFirstname = "";
        mLastname = "";
        mEmail = "";
        nameReceived[i] = false;

        mNotTransferRunning[i] = true;
        mPresenceConfigUpdated[i] = false;

#ifndef KARERE_DISABLE_WEBRTC
        mCallReceived[i] = false;
        mCallReceivedRinging[i] = false;
        mCallInProgress[i] = false;
        mCallDestroyed[i] = false;
        mCallConnecting[i] = false;
        mSchedMeetingUpdated[i] = false;
        mSchedOccurrUpdated[i] = false;
        mChatIdRingInCall[i] = MEGACHAT_INVALID_HANDLE;
        mTerminationCode[i] = 0;
        mChatIdInProgressCall[i] = MEGACHAT_INVALID_HANDLE;
        mCallIdRingIn[i] = MEGACHAT_INVALID_HANDLE;
        mCallIdJoining[i] = MEGACHAT_INVALID_HANDLE;
        mSchedIdUpdated[i] = MEGACHAT_INVALID_HANDLE;
        mSchedIdRemoved[i] = MEGACHAT_INVALID_HANDLE;
        mCallIdExpectedReceived[i] = MEGACHAT_INVALID_HANDLE;
        mLocalVideoListener[i] = NULL;
        mRemoteVideoListener[i] = NULL;
#endif

        mChatFirstname = "";
        mChatLastname = "";
        mChatEmail = "";
    }

    LOG_info << "Test " << name << ": SetUp finished.";
}

void MegaChatApiTest::TearDown()
{
    const ::testing::TestInfo* ti = ::testing::UnitTest::GetInstance()->current_test_info();
    string name = string(ti->test_suite_name()) + '.' + ti->name();
    LOG_info << "Test " << name << ": TearDown starting.";

    for (unsigned int i = 0; i < NUM_ACCOUNTS; i++)
    {
        if (megaChatApi[i])
        {
            if (megaChatApi[i]->getInitState() == MegaChatApi::INIT_ONLINE_SESSION ||
                    megaChatApi[i]->getInitState() == MegaChatApi::INIT_OFFLINE_SESSION )
            {
                int a2 = (i == 0) ? 1 : 0;  // FIXME: find solution for more than 2 accounts
                MegaChatHandle chatToSkip = MEGACHAT_INVALID_HANDLE;
                MegaChatHandle uh = megaChatApi[i]->getUserHandleByEmail(account(a2).getEmail().c_str());
                if (uh != MEGACHAT_INVALID_HANDLE)
                {
                    MegaChatPeerList *peers = MegaChatPeerList::createInstance();
                    peers->addPeer(uh, MegaChatPeerList::PRIV_STANDARD);
                    chatToSkip = getGroupChatRoom(i, a2, peers, MegaChatPeerList::PRIV_UNKNOWN, false);
                    delete peers;
                }

                clearAndLeaveChats(i, chatToSkip);

                bool *flagRequestLogout = &requestFlagsChat[i][MegaChatRequest::TYPE_LOGOUT]; *flagRequestLogout = false;
                megaChatApi[i]->logout();
                TEST_LOG_ERROR(waitForResponse(flagRequestLogout), "Time out MegaChatApi logout");
                TEST_LOG_ERROR(!lastErrorChat[i], "Failed to logout from Chat. Error: " + lastErrorMsgChat[i] + " (" + std::to_string(lastErrorChat[i]) + ")");
                MegaApi::addLoggerObject(logger());   // need to restore customized logger
            }

#ifndef KARERE_DISABLE_WEBRTC
            megaChatApi[i]->removeChatCallListener(this);
            megaChatApi[i]->removeSchedMeetingListener(this);
#endif
            megaChatApi[i]->removeChatRequestListener(this);
            megaChatApi[i]->removeChatListener(this);

            delete megaChatApi[i];
            megaChatApi[i] = NULL;
        }

        if (megaApi[i])
        {
            if (megaApi[i]->isLoggedIn())
            {
                MegaNode* cloudNode = megaApi[i]->getRootNode();
                purgeCloudTree(i, cloudNode);
                delete cloudNode;
                cloudNode = NULL;
                MegaNode* rubbishNode = megaApi[i]->getRubbishNode();
                purgeCloudTree(i, rubbishNode);
                delete rubbishNode;
                rubbishNode = NULL;

                removePendingContactRequest(i);

                RequestTracker logoutTracker;
#ifdef ENABLE_SYNC
                megaApi[i]->logout(false, &logoutTracker);
#else
                megaApi[i]->logout(&logoutTracker);
#endif
                TEST_LOG_ERROR(logoutTracker.waitForResult() == API_OK, "Failed to logout from SDK. Error: " + logoutTracker.getErrorString());
            }

            delete megaApi[i];
            megaApi[i] = NULL;
        }
    }

    purgeLocalTree(LOCAL_PATH);

    LOG_info << "Test " << name << ": TearDown finished.";
}

const char* MegaChatApiTest::printChatRoomInfo(const MegaChatRoom *chat)
{
    if (!chat)
    {
        return MegaApi::strdup("");
    }

    MegaChatHandle chatid = chat->getChatId();
    const char *hstr = MegaApi::userHandleToBase64(chatid);

    std::stringstream buffer;
    buffer << "Chat ID: " << hstr << " (" << chatid << ")" << endl;
    delete [] hstr;
    hstr = NULL;

    buffer << "\tOwn privilege level: " << MegaChatRoom::privToString(chat->getOwnPrivilege()) << endl;
    if (chat->isActive())
    {
        buffer << "\tActive: yes" << endl;
    }
    else
    {
        buffer << "\tActive: no" << endl;
    }
    if (chat->isGroup())
    {
        buffer << "\tGroup chat: yes" << endl;
    }
    else
    {
        buffer << "\tGroup chat: no" << endl;
    }
    if (chat->isArchived())
    {
        buffer << "\tArchived chat: yes" << endl;
    }
    else
    {
        buffer << "\tArchived chat: no" << endl;
    }
    if (chat->isPublic())
    {
        buffer << "\tPublic chat: yes" << endl;
    }
    else
    {
        buffer << "\tPublic chat: no" << endl;
    }
    buffer << "\tPeers:";

    if (chat->getPeerCount())
    {
        buffer << "\t\t(userhandle)\t(privilege)\t(firstname)\t(lastname)\t(fullname)" << endl;
        for (unsigned i = 0; i < chat->getPeerCount(); i++)
        {
            const char *fullName = chat->getPeerFullname(i);
            MegaChatHandle uh = chat->getPeerHandle(i);
            hstr = MegaApi::userHandleToBase64(uh);
            buffer << "\t\t\t" << hstr;
            delete [] hstr;
            hstr = NULL;
            buffer << "\t" << MegaChatRoom::privToString(chat->getPeerPrivilege(i));
            buffer << "\t\t" << chat->getPeerFirstname(i);
            buffer << "\t" << chat->getPeerLastname(i);
            buffer << "\t" << fullName << endl;
            delete [] fullName;
        }
    }
    else
    {
        buffer << " no peers (only you as participant)" << endl;
    }
    if (chat->getTitle())
    {
        buffer << "\tTitle: " << chat->getTitle() << endl;
    }
    buffer << "\tUnread count: " << chat->getUnreadCount() << " message/s" << endl;
    buffer << "-------------------------------------------------" << endl;

    return MegaApi::strdup(buffer.str().c_str());
}

const char* MegaChatApiTest::printMessageInfo(const MegaChatMessage *msg)
{
    if (!msg)
    {
        return MegaApi::strdup("");
    }

    const char *content = msg->getContent() ? msg->getContent() : "<empty>";

    std::stringstream buffer;
    buffer << "id: " << msg->getMsgId() << ", content: " << content;
    buffer << ", tempId: " << msg->getTempId() << ", index:" << msg->getMsgIndex();
    buffer << ", status: " << msg->getStatus() << ", uh: " << msg->getUserHandle();
    buffer << ", type: " << msg->getType() << ", edited: " << msg->isEdited();
    buffer << ", deleted: " << msg->isDeleted() << ", changes: " << msg->getChanges();
    buffer << ", ts: " << msg->getTimestamp() << endl;

    return MegaApi::strdup(buffer.str().c_str());
}

const char* MegaChatApiTest::printChatListItemInfo(const MegaChatListItem *item)
{
    if (!item)
    {
        return MegaApi::strdup("");
    }

    const char *title = item->getTitle() ? item->getTitle() : "<empty>";

    std::stringstream buffer;
    buffer << "id: " << item->getChatId() << ", title: " << title;
    buffer << ", ownPriv: " << item->getOwnPrivilege();
    buffer << ", unread: " << item->getUnreadCount() << ", changes: " << item->getChanges();
    buffer << ", lastMsg: " << item->getLastMessage() << ", lastMsgType: " << item->getLastMessageType();
    buffer << ", lastTs: " << item->getLastTimestamp();

    return MegaApi::strdup(buffer.str().c_str());
}

void MegaChatApiTest::postLog(const std::string &msg)
{
    logger()->postLog(msg.c_str());
}

bool MegaChatApiTest::exitWait(const std::vector<bool *>&responsesReceived, bool waitForAll) const
{
    for (auto r: responsesReceived)
    {
        if (!waitForAll && (*r))  { return true; };   // any response must be received
        if (waitForAll && !(*r))  { return false; };  // all responses must be received
    }
    return waitForAll; // true (all received) false (none received)
};

bool MegaChatApiTest::waitForMultiResponse(std::vector<bool *>responsesReceived, bool waitForAll, unsigned int timeout) const
{
    timeout *= 1000000; // convert to micro-seconds
    unsigned int tWaited = 0;    // microseconds
    bool connRetried = false;
    while (!exitWait(responsesReceived, waitForAll))
    {
        std::this_thread::sleep_for(std::chrono::microseconds(pollingT));

        if (timeout)
        {
            tWaited += pollingT;
            if (tWaited >= timeout)
            {
                return false;   // timeout is expired
            }
            else if (!connRetried && tWaited > (pollingT * 10))
            {
                for (unsigned int i = 0; i < NUM_ACCOUNTS; i++)
                {
                    if (megaApi[i] && megaApi[i]->isLoggedIn())
                    {
                        megaApi[i]->retryPendingConnections();
                    }

                    if (megaChatApi[i] && megaChatApi[i]->getInitState() == MegaChatApi::INIT_ONLINE_SESSION)
                    {
                        megaChatApi[i]->retryPendingConnections();
                    }
                }
                connRetried = true;
            }
        }
    }

    return true;    // responses have been received
}

// this method could be deprecated in favor of waitForMultiResponse, when it's proven it works as expected
bool MegaChatApiTest::waitForResponse(bool *responseReceived, unsigned int timeout) const
{
    timeout *= 1000000; // convert to micro-seconds
    unsigned int tWaited = 0;    // microseconds
    bool connRetried = false;
    while(!(*responseReceived))
    {
        std::this_thread::sleep_for(std::chrono::microseconds(pollingT));

        if (timeout)
        {
            tWaited += pollingT;
            if (tWaited >= timeout)
            {
                return false;   // timeout is expired
            }
            else if (!connRetried && tWaited > (pollingT * 10))
            {
                for (unsigned int i = 0; i < NUM_ACCOUNTS; i++)
                {
                    if (megaApi[i] && megaApi[i]->isLoggedIn())
                    {
                        megaApi[i]->retryPendingConnections();
                    }

                    if (megaChatApi[i] && megaChatApi[i]->getInitState() == MegaChatApi::INIT_ONLINE_SESSION)
                    {
                        megaChatApi[i]->retryPendingConnections();
                    }
                }
                connRetried = true;
            }
        }
    }

    return true;    // response is received
}

void MegaChatApiTest::waitForAction(int maxAttempts, std::vector<bool*> exitFlags, const std::vector<std::string>& flagsStr, const std::string& actionMsg, bool waitForAll, bool resetFlags, unsigned int timeout, std::function<void()>action)
{
    ASSERT_TRUE(exitFlags.size() == flagsStr.size() || flagsStr.empty()) << "waitForCallAction: no valid action provided";
    ASSERT_TRUE(action) << "waitForCallAction: no valid action provided";

    if (resetFlags)
    {
        for (auto f: exitFlags)
        {
            if (f) { *f = false; }
        }
    }

    int retries = 0;
    while (!exitWait(exitFlags, waitForAll))
    {
        action();
        if (!waitForMultiResponse(exitFlags, waitForAll, timeout))
        {
            std::string msg = "Attempt ["; msg.append(std::to_string(retries)).append("] for ").append(actionMsg).append(": ");
            for (size_t i = 0; i < exitFlags.size(); i++)
            {
                (i > flagsStr.size())
                        ? msg.append("Flag_").append(std::to_string(i))
                        : msg.append(flagsStr.at(i));

                msg.append(" = ").append(*exitFlags.at(i) ? "true" : "false").append(" ");
            }
            LOG_debug << msg;
            ASSERT_LE(++retries, maxAttempts) << "Max attempts exceeded for " << actionMsg;
        }
    }
}

/**
 * @brief MegaChatApiTest.ResumeSession
 *
 * This test does the following:
 *
 * - Create a new session
 * - Resume with previous sesion
 * - Resume an existing session without karere cache
 * - Re-create Karere cache without login out from SDK
 * - Close session
 * - Login with chat enabled, transition to disabled and back to enabled
 * - Login with chat disabled, transition to enabled
 * - Go into background, sleep and back to foreground
 * - Disconnect from chat server and reconnect
 *
 */
TEST_F(MegaChatApiTest, ResumeSession)
{
    unsigned accountIndex = 0;

    // ___ Create a new session ___
    char *session = login(accountIndex);
    ASSERT_TRUE(session);

    ASSERT_NO_FATAL_FAILURE({ checkEmail(accountIndex); });

    // Test for management of ESID:
    // (uncomment the following block)
//    {
//        bool *flag = &requestFlagsChat[0][MegaChatRequest::TYPE_LOGOUT]; *flag = false;
//        // ---> NOW close session remotely ---
//        sleep(30);
//        // and wait for forced logout of megachatapi due to ESID
//        ASSERT_TRUE(waitForResponse(flag));
//        session = login(0);
//        ASSERT_TRUE(session);
//    }

    // ___ Resume an existing session ___
    ASSERT_NO_FATAL_FAILURE({ logout(accountIndex, false); }); // keeps session alive
    char *tmpSession = login(accountIndex, session);
    ASSERT_TRUE(tmpSession);
    ASSERT_STREQ(session, tmpSession) << "Bad session key";
    delete [] tmpSession;   tmpSession = NULL;

    ASSERT_NO_FATAL_FAILURE({ checkEmail(accountIndex); });

    // ___ Resume an existing session without karere cache ___
    // logout from SDK keeping cache
    RequestTracker logoutTracker;
    megaApi[accountIndex]->localLogout(&logoutTracker);
    ASSERT_EQ(logoutTracker.waitForResult(), API_OK) << "Error local sdk logout. Error: " << logoutTracker.getErrorString();

    // logout from Karere removing cache
    bool *flagChatLogout = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_LOGOUT]; *flagChatLogout = false;
    megaChatApi[accountIndex]->logout();
    ASSERT_TRUE(waitForResponse(flagChatLogout)) << "Expired timeout for chat logout";
    ASSERT_TRUE(!lastErrorChat[accountIndex]) << "Error chat logout. Error: " << lastErrorMsgChat[accountIndex] <<
                                                 " (" << lastErrorChat[accountIndex] << ")";
    MegaApi::addLoggerObject(logger());   // need to restore customized logger
    // try to initialize chat engine with cache --> should fail
    ASSERT_EQ(megaChatApi[accountIndex]->init(session), MegaChatApi::INIT_NO_CACHE) <<
                     "Wrong chat initialization state.";
    MegaApi::removeLoggerObject(logger());
    megaApi[accountIndex]->invalidateCache();

    // ___ Re-create Karere cache without login out from SDK___
    bool *flagInit = &initStateChanged[accountIndex]; *flagInit = false;
    // login in SDK
    RequestTracker loginTracker;
    session ? megaApi[accountIndex]->fastLogin(session, &loginTracker)
            : megaApi[accountIndex]->login(account(accountIndex).getEmail().c_str(),
                                           account(accountIndex).getPassword().c_str(),
                                           &loginTracker);
    ASSERT_EQ(loginTracker.waitForResult(), API_OK) << "Error sdk fast login. Error: " << loginTracker.getErrorString();

    // fetchnodes in SDK
    RequestTracker fetchNodesTracker;
    megaApi[accountIndex]->fetchNodes(&fetchNodesTracker);
    ASSERT_EQ(fetchNodesTracker.waitForResult(), API_OK) << "Error fetchnodes. Error: " << fetchNodesTracker.getErrorString();
    ASSERT_TRUE(waitForResponse(flagInit)) << "Expired timeout for change init state";
    int initStateValue = initState[accountIndex];
    ASSERT_EQ(initStateValue, MegaChatApi::INIT_ONLINE_SESSION) << "Wrong chat initialization state.";

    // check there's a list of chats already available
    MegaChatListItemList *list = megaChatApi[accountIndex]->getChatListItems();
    ASSERT_TRUE(list->size()) << "Chat list item is empty";
    delete list; list = NULL;

    // ___ Close session ___
    ASSERT_NO_FATAL_FAILURE({ logout(accountIndex, true); });
    delete [] session; session = NULL;

    // ___ Login with chat enabled, transition to disabled and back to enabled
    session = login(accountIndex);
    ASSERT_TRUE(session) << "Empty session key";
    // fully disable chat: logout + remove logger + delete MegaChatApi instance
    flagChatLogout = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_LOGOUT]; *flagChatLogout = false;
    megaChatApi[accountIndex]->logout();
    ASSERT_TRUE(waitForResponse(flagChatLogout)) << "Expired timeout for megachat logout";
    ASSERT_TRUE(!lastErrorChat[accountIndex]) << "Error megachat logout. Error: " << lastErrorMsgChat[accountIndex] <<
                                                 " (" << lastErrorChat[accountIndex] << ")";
    MegaApi::addLoggerObject(logger());   // need to restore customized logger
    delete megaChatApi[accountIndex];
    // create a new MegaChatApi instance
    megaChatApi[accountIndex] = new MegaChatApi(megaApi[accountIndex]);
    megaChatApi[accountIndex]->setLogLevel(MegaChatApi::LOG_LEVEL_DEBUG);
    megaChatApi[accountIndex]->addChatRequestListener(this);
    megaChatApi[accountIndex]->addChatListener(this);
    MegaChatApi::setLoggerObject(logger());
    // back to enabled: init + fetchnodes + connect
    ASSERT_EQ(megaChatApi[accountIndex]->init(session), MegaChatApi::INIT_NO_CACHE) <<
                     "Wrong chat initialization state.";

    MegaApi::removeLoggerObject(logger());
    flagInit = &initStateChanged[accountIndex]; *flagInit = false;
    RequestTracker fetchNodesTracker2;
    megaApi[accountIndex]->fetchNodes(&fetchNodesTracker2);
    ASSERT_EQ(fetchNodesTracker2.waitForResult(), API_OK) << "Error fetchnodes. Error: " << fetchNodesTracker2.getErrorString();
    ASSERT_TRUE(waitForResponse(flagInit)) << "Expired timeout for change init state";
    initStateValue = initState[accountIndex];
    ASSERT_EQ(initStateValue, MegaChatApi::INIT_ONLINE_SESSION) <<
                     "Wrong chat initialization state.";

    // check there's a list of chats already available
    list = megaChatApi[accountIndex]->getChatListItems();
    ASSERT_TRUE(list->size()) << "Chat list item is empty";
    delete list; list = NULL;
    // close session and remove cache
    ASSERT_NO_FATAL_FAILURE({ logout(accountIndex, true); });
    delete [] session; session = NULL;

    // ___ Login with chat disabled, transition to enabled ___
    // fully disable chat: remove logger + delete MegaChatApi instance
    delete megaChatApi[accountIndex];
    // create a new MegaChatApi instance
    megaChatApi[accountIndex] = new MegaChatApi(megaApi[accountIndex]);
    megaChatApi[accountIndex]->setLogLevel(MegaChatApi::LOG_LEVEL_DEBUG);
    megaChatApi[accountIndex]->addChatRequestListener(this);
    megaChatApi[accountIndex]->addChatListener(this);
    MegaChatApi::setLoggerObject(logger());
    // login in SDK
    RequestTracker loginTracker2;
    megaApi[accountIndex]->login(account(accountIndex).getEmail().c_str(),
                                 account(accountIndex).getPassword().c_str(),
                                 &loginTracker2);
    ASSERT_EQ(loginTracker2.waitForResult(), API_OK) << "Error fast login. Error: " << loginTracker2.getErrorString();
    session = megaApi[accountIndex]->dumpSession();
    // fetchnodes in SDK
    RequestTracker fetchNodesTracker3;
    megaApi[accountIndex]->fetchNodes(&fetchNodesTracker3);
    ASSERT_EQ(fetchNodesTracker3.waitForResult(), API_OK) << "Error fetch nodes. Error: " << fetchNodesTracker3.getErrorString();
    // init in Karere
    ASSERT_EQ(megaChatApi[accountIndex]->init(session), MegaChatApi::INIT_NO_CACHE) << "Bad Megachat state.";
    MegaApi::removeLoggerObject(logger());
    // full-fetchndoes in SDK to regenerate cache in Karere
    flagInit = &initStateChanged[accountIndex]; *flagInit = false;
    RequestTracker fetchNodesTracker4;
    megaApi[accountIndex]->fetchNodes(&fetchNodesTracker4);
    ASSERT_EQ(fetchNodesTracker4.waitForResult(), API_OK) << "Error fetch nodes. Error: " << fetchNodesTracker4.getErrorString();
    ASSERT_TRUE(waitForResponse(flagInit)) << "Expired timeout for change init state";
    initStateValue = initState[accountIndex];
    ASSERT_EQ(initStateValue, MegaChatApi::INIT_ONLINE_SESSION) << "Bad Megachat state.";

    // check there's a list of chats already available
    list = megaChatApi[accountIndex]->getChatListItems();
    ASSERT_TRUE(list->size()) << "Chat list item is empty";
    delete list;
    list = NULL;

    // ___ Test going into background, sleep and back to foreground ___
    for(int i = 0; i < 3; i++)
    {
        bool *flag = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_SET_BACKGROUND_STATUS]; *flag = false;
        megaChatApi[accountIndex]->setBackgroundStatus(true);
        ASSERT_TRUE(waitForResponse(flag)) << "Failed to set background status after " << maxTimeout << " seconds";

        logger()->postLog("========== Enter background status ================= ");
        std::this_thread::sleep_for(std::chrono::seconds(15));

        flag = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_SET_BACKGROUND_STATUS]; *flag = false;
        megaChatApi[accountIndex]->setBackgroundStatus(false);
        ASSERT_TRUE(waitForResponse(flag)) << "Failed to set background status after " << maxTimeout << " seconds";

        logger()->postLog("========== Enter foreground status ================= ");
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    delete [] session; session = NULL;
}

/**
 * @brief MegaChatApiTest.SetOnlineStatus
 *
 * This test does the following:
 *
 * - Login
 * - Set status busy
 *
 */
TEST_F(MegaChatApiTest, SetOnlineStatus)
{
    unsigned accountIndex = 0;

    bool *flagPresence = &mPresenceConfigUpdated[accountIndex]; *flagPresence = false;

    char *sesion = login(accountIndex);
    ASSERT_TRUE(sesion);

    ASSERT_TRUE(waitForResponse(flagPresence)) << "Presence config not received after " << maxTimeout << " seconds";

    // Reset status to online before starting the test
    bool *flagStatus = &mOnlineStatusUpdated[accountIndex]; *flagStatus = false;
    bool *flag = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_SET_ONLINE_STATUS]; *flag = false;
    if (megaChatApi[accountIndex]->getPresenceConfig()->getOnlineStatus() != MegaChatApi::STATUS_ONLINE)
    {
        megaChatApi[accountIndex]->setOnlineStatus(MegaChatApi::STATUS_ONLINE);
        ASSERT_TRUE(waitForResponse(flag)) << "Failed to set online status after " << maxTimeout << " seconds";
        ASSERT_TRUE(!lastErrorChat[accountIndex]) << "Failed to set online status. Error: " << lastErrorMsgChat[accountIndex]
                                                     << " (" << lastErrorChat[accountIndex] << ")";
    }

    flagPresence = &mPresenceConfigUpdated[accountIndex]; *flagPresence = false;
    flagStatus = &mOnlineStatusUpdated[accountIndex]; *flagStatus = false;
    flag = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_SET_ONLINE_STATUS]; *flag = false;
    megaChatApi[accountIndex]->setOnlineStatus(MegaChatApi::STATUS_BUSY);
    ASSERT_TRUE(waitForResponse(flag)) << "Failed to set online status after " << maxTimeout << " seconds";
    ASSERT_TRUE(!lastErrorChat[accountIndex]) << "Failed to set online status. Error: " << lastErrorMsgChat[accountIndex]
                                                 << " (" << lastErrorChat[accountIndex] << ")";
    ASSERT_TRUE(waitForResponse(flagPresence)) << "Presence config not received after " << maxTimeout << " seconds";
    ASSERT_TRUE(waitForResponse(flagStatus)) << "Online status not received after " << maxTimeout << " seconds";

    // set online status
    flagStatus = &mOnlineStatusUpdated[accountIndex]; *flagStatus = false;
    flagPresence = &mPresenceConfigUpdated[accountIndex]; *flagPresence = false;
    flag = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_SET_ONLINE_STATUS]; *flag = false;
    megaChatApi[accountIndex]->setOnlineStatus(MegaChatApi::STATUS_ONLINE);
    ASSERT_TRUE(waitForResponse(flag)) << "Failed to set online status after " << maxTimeout << " seconds";
    ASSERT_TRUE(!lastErrorChat[accountIndex]) << "Failed to set online status. Error: " << lastErrorMsgChat[accountIndex]
                                                 << " (" << lastErrorChat[accountIndex] << ")";
    ASSERT_TRUE(waitForResponse(flagPresence)) << "Presence config not received after " << maxTimeout << " seconds";
    ASSERT_TRUE(waitForResponse(flagStatus)) << "Online status not received after " << maxTimeout << " seconds";

    // Update autoway timeout to force to send values to the server
    int64_t autowayTimeout = 5;
    if (megaChatApi[accountIndex]->getPresenceConfig()->getAutoawayTimeout() == autowayTimeout)
    {
        autowayTimeout ++;
    }

    // enable auto-away with 5 seconds timeout
    flagPresence = &mPresenceConfigUpdated[accountIndex]; *flagPresence = false;
    megaChatApi[accountIndex]->setPresenceAutoaway(true, autowayTimeout);
    ASSERT_TRUE(waitForResponse(flagPresence)) << "Presence config not received after " << maxTimeout << " seconds";

    // disable persist
    if (megaChatApi[accountIndex]->getPresenceConfig()->isPersist())
    {
        flagPresence = &mPresenceConfigUpdated[accountIndex]; *flagPresence = false;
        megaChatApi[accountIndex]->setPresencePersist(false);
        ASSERT_TRUE(waitForResponse(flagPresence)) << "Presence config not received after " << maxTimeout << " seconds";
    }

    // Set signal activity true, signal activity to false is sent automatically by presenced client
    megaChatApi[accountIndex]->signalPresenceActivity();
    ASSERT_TRUE(!lastErrorChat[accountIndex]) << "Failed to set activity. Error: " << lastErrorMsgChat[accountIndex]
                                                 << " (" << lastErrorChat[accountIndex] << ")";

    // now wait for timeout to expire
    flagStatus = &mOnlineStatusUpdated[accountIndex]; *flagStatus = false;

    LOG_debug << "Going to sleep for longer than autoaway timeout";
    MegaChatPresenceConfig *config = megaChatApi[accountIndex]->getPresenceConfig();
    std::this_thread::sleep_for(std::chrono::seconds(static_cast<unsigned int>(config->getAutoawayTimeout() + 12)));   // +12 to ensure at least one heartbeat (every 10s), where the `USERACTIVE 0` is sent for transition to Away

    // and check the status is away
    ASSERT_EQ(mOnlineStatus[accountIndex], MegaChatApi::STATUS_AWAY) <<
                     "Online status didn't changed to away automatically after timeout";
    int onlineStatus = megaChatApi[accountIndex]->getOnlineStatus();
    ASSERT_EQ(onlineStatus, MegaChatApi::STATUS_AWAY) <<
                     "Online status didn't changed to away automatically after timeout. Received: " << MegaChatRoom::statusToString(onlineStatus);

    // now signal user's activity to become online again
    flagStatus = &mOnlineStatusUpdated[accountIndex]; *flagStatus = false;
    megaChatApi[accountIndex]->signalPresenceActivity();
    ASSERT_TRUE(waitForResponse(flagStatus)) << "Online status not received after " << maxTimeout << " seconds";

    // and check the status is online
    ASSERT_EQ(mOnlineStatus[accountIndex], MegaChatApi::STATUS_ONLINE) <<
                     "Online status didn't changed to online from autoaway after signaling activity";
    onlineStatus = megaChatApi[accountIndex]->getOnlineStatus();
    ASSERT_EQ(onlineStatus, MegaChatApi::STATUS_ONLINE) <<
                     "Online status didn't changed to online from autoaway after signaling activity. Received: " << MegaChatRoom::statusToString(onlineStatus);


    delete [] sesion;
    sesion = NULL;
}

/**
 * @brief MegaChatApiTest.GetChatRoomsAndMessages
 *
 * This test does the following:
 *
 * - Print chatrooms information
 * - Load history from one chatroom
 * - Close chatroom
 * - Load history from cache
 *
 */
TEST_F(MegaChatApiTest, GetChatRoomsAndMessages)
{
    unsigned accountIndex = 0;

    char *session = login(accountIndex);
    ASSERT_TRUE(session);

    MegaChatRoomList *chats = megaChatApi[accountIndex]->getChatRooms();
    std::stringstream buffer;
    buffer << chats->size() << " chat/s received: " << endl;
    postLog(buffer.str());

    // Open chats and print history
    for (unsigned i = 0; i < chats->size(); i++)
    {
        // Open a chatroom
        const MegaChatRoom *chatroom = chats->get(i);
        if (chatroom->isPublic())
        {
            continue;
        }

        MegaChatHandle chatid = chatroom->getChatId();
        TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
        ASSERT_TRUE(megaChatApi[accountIndex]->openChatRoom(chatid, chatroomListener))
                << "Can't open chatRoom account " << (accountIndex+1);

        // Print chatroom information and peers' names
        const char *info = MegaChatApiTest::printChatRoomInfo(chatroom);
        postLog(info);
        delete [] info; info = NULL;
        if (chatroom->getPeerCount())
        {
            for (unsigned i = 0; i < chatroom->getPeerCount(); i++)
            {
                MegaChatHandle uh = chatroom->getPeerHandle(i);

                bool *flagNameReceived = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_GET_FIRSTNAME]; *flagNameReceived = false; mChatFirstname = "";
                megaChatApi[accountIndex]->getUserFirstname(uh, NULL);
                ASSERT_TRUE(waitForResponse(flagNameReceived)) << "Failed to retrieve firstname after " << maxTimeout << " seconds";
                ASSERT_TRUE(!lastErrorChat[accountIndex]) << "Failed to retrieve firstname. Error: " << lastErrorMsgChat[accountIndex] << " (" << lastErrorChat[accountIndex] << ")";
                buffer << "Peer firstname (" << uh << "): " << mChatFirstname << " (len: " << mChatFirstname.length() << ")" << endl;

                flagNameReceived = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_GET_LASTNAME]; *flagNameReceived = false; mChatLastname = "";
                megaChatApi[0]->getUserLastname(uh, NULL);
                ASSERT_TRUE(waitForResponse(flagNameReceived)) << "Failed to retrieve lastname after " << maxTimeout << " seconds";
                ASSERT_TRUE(!lastErrorChat[accountIndex]) << "Failed to retrieve lastname. Error: " << lastErrorMsgChat[accountIndex] << " (" << lastErrorChat[accountIndex] << ")";
                buffer << "Peer lastname (" << uh << "): " << mChatLastname << " (len: " << mChatLastname.length() << ")" << endl;

                char *email = megaChatApi[accountIndex]->getContactEmail(uh);
                if (email)
                {
                    buffer << "Contact email (" << uh << "): " << email << " (len: " << strlen(email) << ")" << endl;
                    delete [] email;
                }
                else
                {
                    flagNameReceived = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_GET_EMAIL]; *flagNameReceived = false; mChatEmail = "";
                    megaChatApi[accountIndex]->getUserEmail(uh);
                    ASSERT_TRUE(waitForResponse(flagNameReceived)) << "Failed to retrieve email after " << maxTimeout << " seconds";
                    ASSERT_TRUE(!lastErrorChat[accountIndex]) << "Failed to retrieve email. Error: " << lastErrorMsgChat[accountIndex] << " (" << lastErrorChat[accountIndex] << ")";
                    buffer << "Peer email (" << uh << "): " << mChatEmail << " (len: " << mChatEmail.length() << ")" << endl;
                }
            }
        }

        // Load history
        buffer << "Loading messages for chat " << chatroom->getTitle() << " (id: " << chatroom->getChatId() << ")" << endl;
        loadHistory(accountIndex, chatid, chatroomListener);

        // Close the chatroom
        megaChatApi[accountIndex]->closeChatRoom(chatid, chatroomListener);
        delete chatroomListener;

        // Now, load history locally (it should be cached by now)
        chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
        ASSERT_TRUE(megaChatApi[accountIndex]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (accountIndex+1);
        buffer << "Loading messages locally for chat " << chatroom->getTitle() << " (id: " << chatroom->getChatId() << ")" << endl;
        loadHistory(accountIndex, chatid, chatroomListener);

        // Close the chatroom
        megaChatApi[accountIndex]->closeChatRoom(chatid, chatroomListener);
        delete chatroomListener;

        delete chatroom;
        chatroom = NULL;
    }

    logger()->postLog(buffer.str().c_str());

    delete [] session;
    session = NULL;
}

/**
 * @brief MegaChatApiTest.EditAndDeleteMessages
 *
 * Requirements:
 * - Both accounts should be conctacts
 * - The 1on1 chatroom between them should exist
 * (if not accomplished, the test automatically solves the above)
 *
 * This test does the following:
 *
 * - Send a message
 * + Receive the message
 * - Update the messages
 *
 */
TEST_F(MegaChatApiTest, EditAndDeleteMessages)
{
    unsigned a1 = 0;
    unsigned a2 = 1;

    char *primarySession = login(a1);
    ASSERT_TRUE(primarySession);
    char *secondarySession = login(a2);
    ASSERT_TRUE(secondarySession);

    MegaUser *user = megaApi[a1]->getContact(account(a2).getEmail().c_str());
    if (!user || user->getVisibility() != MegaUser::VISIBILITY_VISIBLE)
    {
        ASSERT_NO_FATAL_FAILURE({ makeContact(a1, a2); });
    }
    delete user;
    user = NULL;

    MegaChatHandle chatid = getPeerToPeerChatRoom(a1, a2);
    ASSERT_NE(chatid, MEGACHAT_INVALID_HANDLE);

    // 1. A sends a message to B while B has the chat opened.
    // --> check the confirmed in A, the received message in B, the delivered in A
    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a1+1);
    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a2+1);

    // Load some message to feed history
    loadHistory(a1, chatid, chatroomListener);
    loadHistory(a2, chatid, chatroomListener);

    std::string messageToSend = "HI " + account(a1).getEmail() + " - This is a testing message automatically sent to you";
    MegaChatMessage *msgSent = sendTextMessageOrUpdate(a1, a2, chatid, messageToSend, chatroomListener);
    ASSERT_TRUE(msgSent);

    // edit the message
    std::string messageToUpdate = "This is an edited message to " + account(a1).getEmail();
    MegaChatMessage *msgUpdated = sendTextMessageOrUpdate(a1, a2, chatid, messageToUpdate, chatroomListener, msgSent->getMsgId());
    ASSERT_TRUE(msgUpdated);
    delete msgUpdated; msgUpdated = NULL;
    delete msgSent; msgSent = NULL;

    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);

    delete chatroomListener;

    // 2. A sends a message to B while B doesn't have the chat opened.
    // Then, B opens the chat --> check the received message in B, the delivered in A

    delete [] primarySession;
    primarySession = NULL;
    delete [] secondarySession;
    secondarySession = NULL;
}

/**
 * @brief MegaChatApiTest.GroupChatManagement
 *
 * Requirements:
 * - Both accounts should be conctacts
 * (if not accomplished, the test automatically solves the above)
 *
 * This test does the following:
 * - Create a group chat room or select an existing one
 * - Remove memeber
 * - Invite a new member
 * - Invite same account (error)
 * - Change chatroom title
 * - Change privileges to admin
 * - Changes privileges to read only
 * + Send message (error)
 * - Archive chatroom
 * - Send message (automatically unarchives)
 * - Archive chatroom
 * - Unarchive chatroom
 * - Remove peer from groupchat
 * - Invite another account
 */
TEST_F(MegaChatApiTest, GroupChatManagement)
{
    unsigned a1 = 0;
    unsigned a2 = 1;

    char *sessionPrimary = login(a1);
    ASSERT_TRUE(sessionPrimary);
    char *sessionSecondary = login(a2);
    ASSERT_TRUE(sessionSecondary);

    // Prepare peers, privileges...
    MegaUser *user = megaApi[a1]->getContact(account(a2).getEmail().c_str());
    if (!user || (user->getVisibility() != MegaUser::VISIBILITY_VISIBLE))
    {
        ASSERT_NO_FATAL_FAILURE({ makeContact(a1, a2); });
        delete user;
        user = megaApi[a1]->getContact(account(a2).getEmail().c_str());
    }

    MegaChatHandle uh = user->getHandle();
    delete user;
    user = NULL;

    MegaChatPeerList *peers = MegaChatPeerList::createInstance();
    peers->addPeer(uh, MegaChatPeerList::PRIV_STANDARD);

    MegaChatHandle chatid = getGroupChatRoom(a1, a2, peers);
    delete peers;
    ASSERT_NE(chatid, MEGACHAT_INVALID_HANDLE);

    // --> Open chatroom
    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a1+1);
    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a2+1);

    // --> Remove from chat
    bool *flagRemoveFromChat = &requestFlagsChat[a1][MegaChatRequest::TYPE_REMOVE_FROM_CHATROOM]; *flagRemoveFromChat = false;
    bool *chatItemLeft0 = &chatItemUpdated[a1]; *chatItemLeft0 = false;
    bool *chatItemLeft1 = &chatItemUpdated[a2]; *chatItemLeft1 = false;
    bool *chatItemClosed1 = &chatItemClosed[a2]; *chatItemClosed1 = false;
    bool *chatLeft0 = &chatroomListener->chatUpdated[a1]; *chatLeft0 = false;
    bool *chatLeft1 = &chatroomListener->chatUpdated[a2]; *chatLeft1 = false;
    bool *mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
    bool *flagChatsUpdated1 = &mChatsUpdated[a2]; *flagChatsUpdated1 = false;
    mChatListUpdated[a2].clear();
    MegaChatHandle *uhAction = &chatroomListener->uhAction[a1]; *uhAction = MEGACHAT_INVALID_HANDLE;
    int *priv = &chatroomListener->priv[a1]; *priv = MegaChatRoom::PRIV_UNKNOWN;
    megaChatApi[a1]->removeFromChat(chatid, uh);
    ASSERT_TRUE(waitForResponse(flagRemoveFromChat)) << "Failed to remove peer from chatroom after " << maxTimeout << " seconds";
    ASSERT_TRUE(!lastErrorChat[a1]) << "Failed to remove peer from chatroom " << lastErrorChat[a1];
    ASSERT_TRUE(waitForResponse(mngMsgRecv)) << "Failed to receive management message after " << maxTimeout << " seconds";
    ASSERT_EQ(*uhAction, uh) << "User handle from message doesn't match";
    ASSERT_EQ(*priv, MegaChatRoom::PRIV_RM) << "Privilege is incorrect";
    ASSERT_TRUE(waitForResponse(flagChatsUpdated1)) << "Failed to receive onChatsUpdate after " << maxTimeout << " seconds";
    ASSERT_TRUE(isChatroomUpdated(a2, chatid)) << "Chatroom " << chatid << " is not included in onChatsUpdate";
    mChatListUpdated[a2].clear();

    MegaChatRoom *chatroom = megaChatApi[a2]->getChatRoom(chatid);
    ASSERT_TRUE(chatroom) << "Cannot get chatroom for id " << chatid;
    ASSERT_EQ(chatroom->getOwnPrivilege(), MegaChatRoom::PRIV_RM) << "Invalid own privilege";
    delete chatroom;

    chatroom = megaChatApi[a1]->getChatRoom(chatid);
    ASSERT_TRUE(chatroom) << "Cannot get chatroom for id" << chatid;
    ASSERT_EQ(chatroom->getPeerCount(), 0) << "Wrong number of peers in chatroom " << chatid;
    delete chatroom;

    ASSERT_TRUE(waitForResponse(chatItemLeft0)) << "Chat list item update not received for main account after " << maxTimeout << " seconds";
    ASSERT_TRUE(waitForResponse(chatItemLeft1)) << "Chat list item update not received for auxiliar account after " << maxTimeout << " seconds";
    ASSERT_TRUE(waitForResponse(chatItemClosed1)) << "Chat list item close notification for auxiliar account not received after " << maxTimeout << " seconds";
    ASSERT_TRUE(waitForResponse(chatLeft0)) << "Chat list item leave notification for main account not received after " << maxTimeout << " seconds";

    ASSERT_TRUE(waitForResponse(chatLeft1)) << "Chat list item leave notification for auxiliar account not received after " << maxTimeout << " seconds";
    chatroom = megaChatApi[a1]->getChatRoom(chatid);
    ASSERT_TRUE(chatroom) << "Cannot get chatroom for id " << chatid;
    ASSERT_EQ(chatroom->getPeerCount(), 0) << "Wrong number of peers in chatroom " << chatid;
    delete chatroom;

    // Close the chatroom, even if we've been removed from it
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);

    // --> Invite to chat
    bool *flagInviteToChatRoom = &requestFlagsChat[a1][MegaChatRequest::TYPE_INVITE_TO_CHATROOM]; *flagInviteToChatRoom = false;
    bool *chatItemJoined0 = &chatItemUpdated[a1]; *chatItemJoined0 = false;
    bool *chatItemJoined1 = &chatItemUpdated[a2]; *chatItemJoined1 = false;
    bool *chatJoined0 = &chatroomListener->chatUpdated[a1]; *chatJoined0 = false;
    bool *chatJoined1 = &chatroomListener->chatUpdated[a2]; *chatJoined1 = false;
    *flagChatsUpdated1 = &mChatsUpdated[a2]; *flagChatsUpdated1 = false;
    mChatListUpdated[a2].clear();
    mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
    uhAction = &chatroomListener->uhAction[a1]; *uhAction = MEGACHAT_INVALID_HANDLE;
    priv = &chatroomListener->priv[a1]; *priv = MegaChatRoom::PRIV_UNKNOWN;
    megaChatApi[a1]->inviteToChat(chatid, uh, MegaChatPeerList::PRIV_STANDARD);
    ASSERT_TRUE(waitForResponse(flagInviteToChatRoom)) << "Failed to invite a new peer after " << maxTimeout << " seconds";
    ASSERT_TRUE(!lastErrorChat[a1]) << "Failed to invite a new peer. Error: " << lastErrorMsgChat[a1] << " (" << lastErrorChat[a1] << ")";
    ASSERT_TRUE(waitForResponse(chatItemJoined0)) << "Chat list item update for main account not received after " << maxTimeout << " seconds";
    ASSERT_TRUE(waitForResponse(chatItemJoined1)) << "Chat list item update for auxiliar account not received after " << maxTimeout << " seconds";
    ASSERT_TRUE(waitForResponse(chatJoined0)) << "Chatroom update for main account not received after " << maxTimeout << " seconds";
//    ASSERT_TRUE(waitForResponse(chatJoined1)); --> account 1 haven't opened chat, won't receive callback
    ASSERT_TRUE(waitForResponse(mngMsgRecv)) << "Management message not received after " << maxTimeout << " seconds";
    ASSERT_EQ(*uhAction, uh) << "User handle from message doesn't match";
    ASSERT_EQ(*priv, MegaChatRoom::PRIV_UNKNOWN) << "Privilege is incorrect";    // the message doesn't report the new priv
    ASSERT_TRUE(waitForResponse(flagChatsUpdated1)) << "Failed to receive onChatsUpdate " << maxTimeout << " seconds";
    ASSERT_TRUE(isChatroomUpdated(a2, chatid)) << "Chatroom " << chatid << " is not included in onChatsUpdate";
    mChatListUpdated[a2].clear();

    chatroom = megaChatApi[a2]->getChatRoom(chatid);
    ASSERT_TRUE(chatroom) << "Cannot get chatroom for id " << chatid;
    ASSERT_EQ(chatroom->getOwnPrivilege(), MegaChatRoom::PRIV_STANDARD) << "Invalid own privilege";
    delete chatroom;

    chatroom = megaChatApi[a1]->getChatRoom(chatid);
    ASSERT_TRUE(chatroom) << "Cannot get chatroom for id " << chatid;
    ASSERT_EQ(chatroom->getPeerCount(), 1) << "Wrong number of peers in chatroom " << chatid;
    delete chatroom;

    // since we were expulsed from chatroom, we need to open it again
    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a2+1);

    // invite again --> error
    flagInviteToChatRoom = &requestFlagsChat[a1][MegaChatRequest::TYPE_INVITE_TO_CHATROOM]; *flagInviteToChatRoom = false;
    megaChatApi[a1]->inviteToChat(chatid, uh, MegaChatPeerList::PRIV_STANDARD);
    ASSERT_TRUE(waitForResponse(flagInviteToChatRoom)) << "Failed to invite a new peer after " << maxTimeout << " seconds";
    ASSERT_EQ(lastErrorChat[a1], MegaChatError::ERROR_EXIST) << "Invitation should have failed, but it succeed";

    // --> Set title
    string title = "Title " + std::to_string(time(NULL));
    bool *flagChatRoomName = &requestFlagsChat[a1][MegaChatRequest::TYPE_EDIT_CHATROOM_NAME]; *flagChatRoomName = false;
    bool *titleItemChanged0 = &titleUpdated[a1]; *titleItemChanged0 = false;
    bool *titleItemChanged1 = &titleUpdated[a2]; *titleItemChanged1 = false;
    bool *titleChanged0 = &chatroomListener->titleUpdated[a1]; *titleChanged0 = false;
    bool *titleChanged1 = &chatroomListener->titleUpdated[a2]; *titleChanged1 = false;
    mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
    string *msgContent = &chatroomListener->content[a1]; *msgContent = "";
    megaChatApi[a1]->setChatTitle(chatid, title.c_str());
    ASSERT_TRUE(waitForResponse(flagChatRoomName)) << "Timeout expired for set chat title";
    ASSERT_TRUE(!lastErrorChat[a1]) << "Failed to set chat title. Error: " << lastErrorMsgChat[a1] << " (" << lastErrorChat[a1] << ")";
    ASSERT_TRUE(waitForResponse(titleItemChanged0)) << "Timeout expired for receiving chat list item update";
    ASSERT_TRUE(waitForResponse(titleItemChanged1)) << "Timeout expired for receiving chat list item update";
    ASSERT_TRUE(waitForResponse(titleChanged0)) << "Timeout expired for receiving chatroom update";
    ASSERT_TRUE(waitForResponse(titleChanged1)) << "Timeout expired for receiving chatroom update";
    ASSERT_TRUE(waitForResponse(mngMsgRecv)) << "Timeout expired for receiving management message";
    ASSERT_EQ(title, *msgContent) << "Title received doesn't match the title set";

    chatroom = megaChatApi[a2]->getChatRoom(chatid);
    ASSERT_TRUE(chatroom) << "Cannot get chatroom for id " << chatid;
    ASSERT_STREQ(chatroom->getTitle(), title.c_str()) << "Titles don't match";
    delete chatroom;    chatroom = NULL;

    // --> Change peer privileges to Moderator
    bool *flagUpdatePeerPermision = &requestFlagsChat[a1][MegaChatRequest::TYPE_UPDATE_PEER_PERMISSIONS]; *flagUpdatePeerPermision = false;
    bool *peerUpdated0 = &peersUpdated[a1]; *peerUpdated0 = false;
    bool *peerUpdated1 = &peersUpdated[a2]; *peerUpdated1 = false;
    mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
    uhAction = &chatroomListener->uhAction[a1]; *uhAction = MEGACHAT_INVALID_HANDLE;
    priv = &chatroomListener->priv[a1]; *priv = MegaChatRoom::PRIV_UNKNOWN;
    megaChatApi[a1]->updateChatPermissions(chatid, uh, MegaChatRoom::PRIV_MODERATOR);
    ASSERT_TRUE(waitForResponse(flagUpdatePeerPermision)) << "Timeout expired for update privilege of peer";
    ASSERT_TRUE(!lastErrorChat[a1]) << "Failed to update privilege of peer Error: " << lastErrorMsgChat[a1] << " (" << lastErrorChat[a1] << ")";
    ASSERT_TRUE(waitForResponse(peerUpdated0)) << "Timeout expired for receiving peer update";
    ASSERT_TRUE(waitForResponse(peerUpdated1)) << "Timeout expired for receiving peer update";
    ASSERT_TRUE(waitForResponse(mngMsgRecv)) << "Timeout expired for receiving management message";
    ASSERT_EQ(*uhAction, uh) << "User handle from message doesn't match";
    ASSERT_EQ(*priv, MegaChatRoom::PRIV_MODERATOR) << "Privilege is incorrect";

    // --> Change peer privileges to Read-only
    flagUpdatePeerPermision = &requestFlagsChat[a1][MegaChatRequest::TYPE_UPDATE_PEER_PERMISSIONS]; *flagUpdatePeerPermision = false;
    peerUpdated0 = &peersUpdated[a1]; *peerUpdated0 = false;
    peerUpdated1 = &peersUpdated[a2]; *peerUpdated1 = false;
    mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
    uhAction = &chatroomListener->uhAction[a1]; *uhAction = MEGACHAT_INVALID_HANDLE;
    priv = &chatroomListener->priv[a1]; *priv = MegaChatRoom::PRIV_UNKNOWN;
    megaChatApi[a1]->updateChatPermissions(chatid, uh, MegaChatRoom::PRIV_RO);
    ASSERT_TRUE(waitForResponse(flagUpdatePeerPermision)) << "Timeout expired for update privilege of peer";
    ASSERT_TRUE(!lastErrorChat[a1]) << "Failed to update privilege of peer Error: " << lastErrorMsgChat[a1] << " (" << lastErrorChat[a1] << ")";
    ASSERT_TRUE(waitForResponse(peerUpdated0)) << "Timeout expired for receiving peer update";
    ASSERT_TRUE(waitForResponse(peerUpdated1)) << "Timeout expired for receiving peer update";
    ASSERT_TRUE(waitForResponse(mngMsgRecv)) << "Timeout expired for receiving management message";
    ASSERT_EQ(*uhAction, uh) << "User handle from message doesn't match";
    ASSERT_EQ(*priv, MegaChatRoom::PRIV_RO) << "Privilege is incorrect";

    // --> Try to send a message without the right privilege
    string msg1 = "HI " + account(a1).getEmail()+ " - This message can't be send because I'm read-only";
    bool *flagRejected = &chatroomListener->msgRejected[a2]; *flagRejected = false;
    chatroomListener->clearMessages(a2);   // will be set at reception
    MegaChatMessage *msgSent = megaChatApi[a2]->sendMessage(chatid, msg1.c_str());
    ASSERT_TRUE(msgSent) << "Succeed to send message, when it should fail";
    delete msgSent; msgSent = NULL;
    ASSERT_TRUE(waitForResponse(flagRejected)) << "Timeout expired for rejection of message";    // for confirmation, sendMessage() is synchronous
    ASSERT_EQ(chatroomListener->mConfirmedMessageHandle[a2], MEGACHAT_INVALID_HANDLE) << "Message confirmed, when it should fail";

    // --> Load some message to feed history
    loadHistory(a1, chatid, chatroomListener);
    loadHistory(a2, chatid, chatroomListener);

    // --> Send typing notification
    bool *flagTyping1 = &chatroomListener->userTyping[a2]; *flagTyping1 = false;
    uhAction = &chatroomListener->uhAction[a2]; *uhAction = MEGACHAT_INVALID_HANDLE;
    megaChatApi[a1]->sendTypingNotification(chatid);
    ASSERT_TRUE(!lastErrorChat[a1]) << "Failed to send user typing: " << lastErrorMsgChat[a1] << " (" << lastErrorChat[a1] << ")";
    ASSERT_TRUE(waitForResponse(flagTyping1)) << "Timeout expired for sending typing notification";
    ASSERT_EQ(*uhAction, megaChatApi[a1]->getMyUserHandle()) << "My user handle is wrong at typing";

    // --> Send stop typing notification
    flagTyping1 = &chatroomListener->userTyping[a2]; *flagTyping1 = false;
    uhAction = &chatroomListener->uhAction[a2]; *uhAction = MEGACHAT_INVALID_HANDLE;
    megaChatApi[a1]->sendStopTypingNotification(chatid);
    ASSERT_TRUE(!lastErrorChat[a1]) << "Failed to send user has stopped typing: " << lastErrorMsgChat[a1] << " (" << lastErrorChat[a1] << ")";
    ASSERT_TRUE(waitForResponse(flagTyping1)) << "Timeout expired for sending stop typing notification";
    ASSERT_EQ(*uhAction, megaChatApi[a1]->getMyUserHandle()) << "My user handle is wrong at stop typing";

    // --> Archive the chatroom
    chatroom = megaChatApi[a1]->getChatRoom(chatid);
    delete chatroom; chatroom = NULL;
    bool *flagChatArchived = &requestFlagsChat[a1][MegaChatRequest::TYPE_ARCHIVE_CHATROOM]; *flagChatArchived = false;
    bool *chatArchiveChanged = &chatArchived[a1]; *chatArchiveChanged = false;
    bool *chatroomArchiveChanged = &chatroomListener->archiveUpdated[a1]; *chatroomArchiveChanged = false;
    megaChatApi[a1]->archiveChat(chatid, true);
    ASSERT_TRUE(waitForResponse(flagChatArchived)) << "Timeout expired for archiving chat";
    ASSERT_TRUE(!lastErrorChat[a1]) << "Failed to archive chat. Error: " << lastErrorMsgChat[a1] << " (" << lastErrorChat[a1] << ")";
    ASSERT_TRUE(waitForResponse(chatArchiveChanged)) << "Timeout expired for receiving chat list item update about archive";
    ASSERT_TRUE(waitForResponse(chatroomArchiveChanged)) << "Timeout expired for receiving chatroom update about archive (This time out is usually produced by missing api notification)";
    chatroom = megaChatApi[a1]->getChatRoom(chatid);
    ASSERT_TRUE(chatroom->isArchived()) << "Chatroom is not archived when it should";
    delete chatroom; chatroom = NULL;

    // TODO: Redmine ticket: #10596
    {
        // give some margin to API-chatd synchronization, so chatd knows the room is archived and needs
        // to be unarchived upon new message
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    // --> Send a message and wait for reception by target user
    string msg0 = "HI " + account(a1).getEmail() + " - Testing groupchats";
    bool *msgConfirmed = &chatroomListener->msgConfirmed[a1]; *msgConfirmed = false;
    bool *msgReceived = &chatroomListener->msgReceived[a2]; *msgReceived = false;
    bool *msgDelivered = &chatroomListener->msgDelivered[a1]; *msgDelivered = false;
    chatArchiveChanged = &chatArchived[a1]; *chatArchiveChanged = false;
    chatroomArchiveChanged = &chatroomListener->archiveUpdated[a1]; *chatroomArchiveChanged = false;
    chatroomListener->clearMessages(a1);
    chatroomListener->clearMessages(a2);
    MegaChatMessage *messageSent = megaChatApi[a1]->sendMessage(chatid, msg0.c_str());
    ASSERT_TRUE(waitForResponse(msgConfirmed)) << "Timeout expired for receiving confirmation by server";    // for confirmation, sendMessage() is synchronous
    MegaChatHandle msgId = chatroomListener->mConfirmedMessageHandle[a1];
    ASSERT_TRUE(chatroomListener->hasArrivedMessage(a1, msgId)) << "Message not received";
    ASSERT_NE(msgId, MEGACHAT_INVALID_HANDLE) << "Wrong message id at origin";
    ASSERT_TRUE(waitForResponse(msgReceived)) << "Timeout expired for receiving message by target user";    // for reception
    ASSERT_TRUE(chatroomListener->hasArrivedMessage(a2, msgId)) << "Wrong message id at destination";
    MegaChatMessage *messageReceived = megaChatApi[a2]->getMessage(chatid, msgId);   // message should be already received, so in RAM
    ASSERT_TRUE(messageReceived && !strcmp(msg0.c_str(), messageReceived->getContent())) << "Content of message doesn't match";
    // now wait for automatic unarchive, due to new message
    ASSERT_TRUE(waitForResponse(chatArchiveChanged)) << "Timeout expired for receiving chat list item update after new message";
    ASSERT_TRUE(waitForResponse(chatroomArchiveChanged)) << "Timeout expired for receiving chatroom update after new message";
    chatroom = megaChatApi[a1]->getChatRoom(chatid);
    ASSERT_FALSE(chatroom->isArchived()) << "Chatroom is not unarchived automatically upon new message";
    delete chatroom; chatroom = NULL;


    // --> Archive the chatroom
    flagChatArchived = &requestFlagsChat[a1][MegaChatRequest::TYPE_ARCHIVE_CHATROOM]; *flagChatArchived = false;
    chatArchiveChanged = &chatArchived[a1]; *chatArchiveChanged = false;
    chatroomArchiveChanged = &chatroomListener->archiveUpdated[a1]; *chatroomArchiveChanged = false;
    megaChatApi[a1]->archiveChat(chatid, true);
    ASSERT_TRUE(waitForResponse(flagChatArchived)) << "Timeout expired for archiving chat";
    ASSERT_TRUE(!lastErrorChat[a1]) << "Failed to archive chat. Error: " << lastErrorMsgChat[a1] << " (" << lastErrorChat[a1] << ")";
    ASSERT_TRUE(waitForResponse(chatArchiveChanged)) << "Timeout expired for receiving chat list item update about archive";
    ASSERT_TRUE(waitForResponse(chatroomArchiveChanged)) << "Timeout expired for receiving chatroom update about archive";
    chatroom = megaChatApi[a1]->getChatRoom(chatid);
    ASSERT_TRUE(chatroom->isArchived()) << "Chatroom is not archived when it should";
    delete chatroom; chatroom = NULL;

    // --> Unarchive the chatroom
    delete chatroom; chatroom = NULL;
    flagChatArchived = &requestFlagsChat[a1][MegaChatRequest::TYPE_ARCHIVE_CHATROOM]; *flagChatArchived = false;
    chatArchiveChanged = &chatArchived[a1]; *chatArchiveChanged = false;
    chatroomArchiveChanged = &chatroomListener->archiveUpdated[a1]; *chatroomArchiveChanged = false;
    megaChatApi[a1]->archiveChat(chatid, false);
    ASSERT_TRUE(waitForResponse(flagChatArchived)) << "Timeout expired for archiving chat";
    ASSERT_TRUE(!lastErrorChat[a1]) << "Failed to archive chat. Error: " << lastErrorMsgChat[a1] << " (" << lastErrorChat[a1] << ")";
    ASSERT_TRUE(waitForResponse(chatArchiveChanged)) << "Timeout expired for receiving chat list item update about archive";
    ASSERT_TRUE(waitForResponse(chatroomArchiveChanged)) << "Timeout expired for receiving chatroom update about archive";
    chatroom = megaChatApi[a1]->getChatRoom(chatid);
    ASSERT_FALSE(chatroom->isArchived()) << "Chatroom is archived when it shouldn't";
    delete chatroom; chatroom = NULL;

    delete messageSent;
    messageSent = NULL;

    delete messageReceived;
    messageReceived = NULL;

    // --> Close the chatroom
    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);

    // --> Remove peer from groupchat
    bool *flagRemoveFromChatRoom = &requestFlagsChat[a1][MegaChatRequest::TYPE_REMOVE_FROM_CHATROOM]; *flagRemoveFromChatRoom = false;
    bool *chatClosed = &chatItemClosed[a2]; *chatClosed = false;
    megaChatApi[a1]->removeFromChat(chatid, uh);
    ASSERT_TRUE(waitForResponse(flagRemoveFromChatRoom)) << "Timeout expired";
    ASSERT_TRUE(!lastErrorChat[a1]) << "Error remove peer from group chat. Error: " << lastErrorMsgChat[a1] << " (" << lastErrorChat[a1] << ")";
    ASSERT_TRUE(waitForResponse(chatClosed)) << "Timeout expired";
    chatroom = megaChatApi[a2]->getChatRoom(chatid);
    ASSERT_TRUE(chatroom) << "Cannot get chatroom for id " << chatid;
    ASSERT_FALSE(chatroom->isActive()) << "Chatroom should be inactive, but it's still active";
    delete chatroom;    chatroom = NULL;

    // --> Invite to chat
    flagInviteToChatRoom = &requestFlagsChat[a1][MegaChatRequest::TYPE_INVITE_TO_CHATROOM]; *flagInviteToChatRoom = false;
    megaChatApi[a1]->inviteToChat(chatid, uh, MegaChatPeerList::PRIV_STANDARD);
    ASSERT_TRUE(waitForResponse(flagInviteToChatRoom)) << "Failed to invite a new peer after " << maxTimeout << " seconds";
    ASSERT_TRUE(!lastErrorChat[a1]) << "Failed to invite a new peer. Error: " << lastErrorMsgChat[a1] << " (" << lastErrorChat[a1] << ")";

    delete chatroomListener;
    delete [] sessionPrimary;
    sessionPrimary = NULL;
    delete [] sessionSecondary;
    sessionSecondary = NULL;
}


/**
 * @brief MegaChatApiTest.PublicChatManagement
 *
 * Requirements:
 * - Both accounts should be conctacts
 * (if not accomplished, the test automatically solves the above)
 *
 * This test does the following:
 * [ Anonymous mode test ]
 * - Login in primary account
 * + Init anonymous in secondary account
 * - Create a public chat with no peers nor title
 * - Open chatroom
 * - Create chat link (ERR)
 * - Set title
 * - Create chat link
 * + Load chat link
 * + Open chatroom
 * + Send a message (ERR)
 * - Send a message
 * + Close preview
 * - Remove chat link
 * + Load chat link (ERR)
 * + Logout
 *
 * [ Public chat test ]
 * + Login in secondary account
 * - Create chat link
 * + Load chat link
 * + Open chatroom
 * + Send a message (ERR)
 * + Autojoin chat link
 * + Send a message
 * + Set chat to private mode
 * + Remove peer from groupchat (OK)
 * + Preview chat link (ERR)
 * - Invite other account
 * - Leave chat room
*/
TEST_F(MegaChatApiTest, PublicChatManagement)
{
    unsigned a1 = 0;
    unsigned a2 = 1;

    /// Anonymous mode test
    // Login in primary account
    char *sessionPrimary = login(a1);
    ASSERT_TRUE(sessionPrimary);

    // Init anonymous in secondary account and connect
    initState[a2] = megaChatApi[a2]->initAnonymous();
    ASSERT_EQ(initState[a2], MegaChatApi::INIT_ANONYMOUS) << "Init sesion in anonymous mode failed";
    char *sessionAnonymous = megaApi[a2]->dumpSession();

    // Create a public chat with no peers nor title, this chat will be reused by the rest of the tests
    MegaChatHandle chatid = MEGACHAT_INVALID_HANDLE;
    bool *flagCreateChatRoom = &requestFlagsChat[a1][MegaChatRequest::TYPE_CREATE_CHATROOM]; *flagCreateChatRoom = false;
    MegaChatPeerList *peers = MegaChatPeerList::createInstance();
    megaChatApi[a1]->createPublicChat(peers);
    ASSERT_TRUE(waitForResponse(flagCreateChatRoom)) << "Expired timeout for creating public groupchat";
    ASSERT_TRUE(!lastErrorChat[a1]) << "Failed to create public groupchat. Error: " << lastErrorMsgChat[a1] << " (" << lastErrorChat[a1] << ")";
    chatid = this->chatid[a1];
    ASSERT_NE(chatid, MEGACHAT_INVALID_HANDLE) << "Wrong chat id";
    delete peers;
    peers = NULL;

    // Open chatroom
    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a1+1);
    bool *flagChatdOnline = &mChatConnectionOnline[a1]; *flagChatdOnline = false;
    while (megaChatApi[a1]->getChatConnectionState(chatid) != MegaChatApi::CHAT_CONNECTION_ONLINE)
    {
        postLog("Waiting for connection to chatd...");
        ASSERT_TRUE(waitForResponse(flagChatdOnline)) << "Timeout expired for connecting to chatd";
        *flagChatdOnline = false;
    }

    // Create chat link (ERR No title)
    bool *flagCreateChatLink = &requestFlagsChat[a1][MegaChatRequest::TYPE_CHAT_LINK_HANDLE]; *flagCreateChatLink = false;
    megaChatApi[a1]->createChatLink(chatid, this);
    ASSERT_TRUE(waitForResponse(flagCreateChatLink)) << "Timeout expired for create chat link";

    // Set title
    string title = "TestPublicChatWithTitle_" + dateToString().substr(dateToString().length() - 5, 5);
    bool *flagChatRoomName = &requestFlagsChat[a1][MegaChatRequest::TYPE_EDIT_CHATROOM_NAME]; *flagChatRoomName = false;
    bool *titleItemChanged0 = &titleUpdated[a1]; *titleItemChanged0 = false;
    bool *titleChanged0 = &chatroomListener->titleUpdated[a1]; *titleChanged0 = false;
    bool *mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
    string *msgContent = &chatroomListener->content[a1]; *msgContent = "";
    megaChatApi[a1]->setChatTitle(chatid, title.c_str());
    ASSERT_TRUE(waitForResponse(flagChatRoomName)) << "Timeout expired for set chat title";
    ASSERT_TRUE(!lastErrorChat[a1]) << "Failed to set chat title. Error: " << lastErrorMsgChat[a1] << " (" << lastErrorChat[a1] << ")";
    ASSERT_TRUE(waitForResponse(titleItemChanged0)) << "Timeout expired for receiving chat list item update";
    ASSERT_TRUE(waitForResponse(titleChanged0)) << "Timeout expired for receiving chatroom update";
    ASSERT_TRUE(waitForResponse(mngMsgRecv)) << "Timeout expired for receiving management message";
    ASSERT_EQ(title, *msgContent) << "Title received doesn't match the title set";

    // Create chat link
    flagCreateChatLink = &requestFlagsChat[a1][MegaChatRequest::TYPE_CHAT_LINK_HANDLE]; *flagCreateChatLink = false;
    megaChatApi[a1]->createChatLink(chatid, this);
    ASSERT_TRUE(waitForResponse(flagCreateChatLink)) << "Timeout expired for create chat link";
    ASSERT_TRUE(!lastErrorChat[a1]) << "Error creating chat link. Error: " << lastErrorMsgChat[a1] << " (" << lastErrorChat[a1] << ")";
    std::string chatLink = this->chatLinks[a1];
    assert(!chatLink.empty());

    // Load chat link
    bool *previewsUpdated = &chatroomListener->previewsUpdated[a1]; *previewsUpdated = false;
    bool *flagPreviewChat = &requestFlagsChat[a2][MegaChatRequest::TYPE_LOAD_PREVIEW]; *flagPreviewChat = false;
    megaChatApi[a2]->openChatPreview(chatLink.c_str(), this);
    ASSERT_TRUE(waitForResponse(flagPreviewChat)) << "Timeout expired for load chat link";
    ASSERT_TRUE(!lastErrorChat[a2]) << "Failed to open chat preview. Error: " << lastErrorMsgChat[a2] << " (" << lastErrorChat[a2] << ")";

    // Open chatroom
    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a2+1);
    ASSERT_TRUE(waitForResponse(previewsUpdated)) << "Timeout expired for update previewers";

    // Send a message (ERR)
    string msg = "HI " + account(a1).getEmail()+ " - This message will be rejected because now I'm a previewer";
    bool *flagRejected = &chatroomListener->msgRejected[a2]; *flagRejected = false;
    chatroomListener->clearMessages(a2);   // will be set at reception
    MegaChatMessage *msgSent = megaChatApi[a2]->sendMessage(chatid, msg.c_str());
    delete msgSent; msgSent = NULL;
    ASSERT_TRUE(waitForResponse(flagRejected)) << "Timeout expired for rejection of message";    // for confirmation, sendMessage() is synchronous
    ASSERT_EQ(chatroomListener->mConfirmedMessageHandle[a2], MEGACHAT_INVALID_HANDLE) << "Message confirmed, when it should fail";

    // Send a message
    msg = "HI Anonymous user, This message will be sent";
    flagRejected = &chatroomListener->msgRejected[a1]; *flagRejected = false;
    chatroomListener->clearMessages(a1);   // will be set at reception
    msgSent = megaChatApi[a1]->sendMessage(chatid, msg.c_str());
    ASSERT_TRUE(msgSent) << "Succeed to send message";
    delete msgSent; msgSent = NULL;
    ASSERT_EQ(chatroomListener->mConfirmedMessageHandle[a1], MEGACHAT_INVALID_HANDLE) << "Message confirmed, when it should fail";

    // Close preview
    previewsUpdated = &chatroomListener->previewsUpdated[a1]; *previewsUpdated = false;
    megaChatApi[a2]->closeChatPreview(chatid);
    ASSERT_TRUE(waitForResponse(previewsUpdated)) << "Timeout expired for close preview";

    // Remove chat link
    bool *setRemoveChatLink = &requestFlagsChat[a1][MegaChatRequest::TYPE_CHAT_LINK_HANDLE]; *setRemoveChatLink = false;
    megaChatApi[a1]->removeChatLink(chatid, this);
    ASSERT_TRUE(waitForResponse(setRemoveChatLink)) << "Timeout expired for close chat link";
    ASSERT_TRUE(!lastErrorChat[a1]) << "Failed to remove chat link. Error: " << lastErrorMsgChat[a1] << " (" << lastErrorChat[a1] << ")";

    // Preview chat link (ERR)
    flagPreviewChat = &requestFlagsChat[a2][MegaChatRequest::TYPE_LOAD_PREVIEW]; *flagPreviewChat = false;
    megaChatApi[a2]->openChatPreview(chatLink.c_str(), this);
    ASSERT_TRUE(waitForResponse(flagPreviewChat)) << "Timeout expired for load chat link";
    ASSERT_EQ(lastErrorChat[a2], API_ENOENT) << "Unexpected error loading an invalid chat-link: " << lastErrorMsgChat[a2];

    // Logout in anonymous mode
    ASSERT_NO_FATAL_FAILURE({ logout(a2); });
    delete [] sessionAnonymous;
    sessionAnonymous = NULL;

    /// Public chats test
    // Login in secondary account
    char *sessionSecondary = login(a2);
    ASSERT_TRUE(sessionSecondary);

    // Make a1 and a2 contacts
    { // scope for 'user' local variable
    MegaUser *user = megaApi[a1]->getContact(account(a2).getEmail().c_str());
    if (!user || (user->getVisibility() != MegaUser::VISIBILITY_VISIBLE))
    {
        ASSERT_NO_FATAL_FAILURE({ makeContact(a1, a2); });
        delete user;
    }
    }

    // Create chat link
    flagCreateChatLink = &requestFlagsChat[a1][MegaChatRequest::TYPE_CHAT_LINK_HANDLE]; *flagCreateChatLink = false;
    megaChatApi[a1]->createChatLink(chatid, this);
    ASSERT_TRUE(waitForResponse(flagCreateChatLink)) << "Timeout expired for create chat link";
    ASSERT_TRUE(!lastErrorChat[a1]) << "Error creating chat link. Error: " << lastErrorMsgChat[a1] << " (" << lastErrorChat[a1] << ")";
    chatLink = this->chatLinks[a1];
    assert(!chatLink.empty());

    // Load chat link (OK)
    previewsUpdated = &chatroomListener->previewsUpdated[a1]; *previewsUpdated = false;
    flagPreviewChat = &requestFlagsChat[a2][MegaChatRequest::TYPE_LOAD_PREVIEW]; *flagPreviewChat = false;
    megaChatApi[a2]->openChatPreview(chatLink.c_str(), this);
    ASSERT_TRUE(waitForResponse(flagPreviewChat)) << "Timeout expired for load chat link";
    ASSERT_TRUE(!lastErrorChat[a2]) << "Failed to open chat preview. Error: " << lastErrorMsgChat[a2] << " (" << lastErrorChat[a2] << ")";

    // Open chatroom
    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a2+1);
    ASSERT_TRUE(waitForResponse(previewsUpdated)) << "Timeout expired for update previewers";

    // Try to send a message (ERR)
    string msgaux = "HI " + account(a1).getEmail()+ " - This message can't be send because I'm in preview mode (read-only)";
    flagRejected = &chatroomListener->msgRejected[a2]; *flagRejected = false;
    chatroomListener->clearMessages(a2);   // will be set at reception
    msgSent = megaChatApi[a2]->sendMessage(chatid, msgaux.c_str());
    ASSERT_TRUE(msgSent) << "Fail to send message, when it should succeed";
    delete msgSent; msgSent = NULL;
    ASSERT_TRUE(waitForResponse(flagRejected)) << "Timeout expired for rejection of message";
    ASSERT_EQ(chatroomListener->mConfirmedMessageHandle[a2], MEGACHAT_INVALID_HANDLE) << "Message confirmed, when it should fail";

    // Autojoin chat link
    bool *flagJoinChatLink = &requestFlagsChat[a2][MegaChatRequest::TYPE_AUTOJOIN_PUBLIC_CHAT]; *flagJoinChatLink = false;
    previewsUpdated = &chatroomListener->previewsUpdated[a1]; *previewsUpdated = false;
    megaChatApi[a2]->autojoinPublicChat(chatid, this);
    ASSERT_TRUE(waitForResponse(flagJoinChatLink)) << "Timeout expired for autojoin chat-link";
    ASSERT_TRUE(!lastErrorChat[a2]) << "Failed to autojoin chat-link. Error: " << lastErrorMsgChat[a2] << " (" << lastErrorChat[a2] << ")";
    ASSERT_TRUE(waitForResponse(previewsUpdated)) << "Timeout expired for update previewers";
    MegaChatListItem *item = megaChatApi[a2]->getChatListItem(chatid);
    ASSERT_EQ(item->getNumPreviewers(), 0) << "Wrong number of previewers.";
    delete item;
    item = NULL;

    // Send a message
    msgaux = "HI " + account(a1).getEmail()+ " - I have autojoined to this chat";
    flagRejected = &chatroomListener->msgRejected[a2]; *flagRejected = false;
    chatroomListener->clearMessages(a2);   // will be set at reception
    msgSent = megaChatApi[a2]->sendMessage(chatid, msgaux.c_str());
    ASSERT_TRUE(msgSent) << "Succeed to send message, when it should fail";
    delete msgSent; msgSent = NULL;
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);

    // Set chat to private mode
    bool *setPublicChatToPrivate = &requestFlagsChat[a1][MegaChatRequest::TYPE_SET_PRIVATE_MODE]; *setPublicChatToPrivate = false;
    megaChatApi[a1]->setPublicChatToPrivate(chatid, this);
    ASSERT_TRUE(waitForResponse(setPublicChatToPrivate)) << "Timeout expired for close chat link";

    // Remove peer from groupchat
    auto uh =  megaChatApi[a2]->getMyUserHandle();
    bool *flagRemoveFromChatRoom = &requestFlagsChat[a1][MegaChatRequest::TYPE_REMOVE_FROM_CHATROOM]; *flagRemoveFromChatRoom = false;
    bool *chatClosed = &chatItemClosed[a2]; *chatClosed = false;
    megaChatApi[a1]->removeFromChat(chatid, uh);
    ASSERT_TRUE(waitForResponse(flagRemoveFromChatRoom)) << "Timeout expired for remove peer from chat";
    ASSERT_TRUE(!lastErrorChat[a1]) << "Error remove peer from group chat. Error: " << lastErrorMsgChat[a1] << " (" << lastErrorChat[a1] << ")";
    ASSERT_TRUE(waitForResponse(chatClosed)) << "Timeout expired for remove peer from chat";

    MegaChatRoom * auxchatroom = megaChatApi[a2]->getChatRoom(chatid);
    ASSERT_TRUE(auxchatroom) << "Cannot get chatroom for id " << chatid;
    ASSERT_FALSE(auxchatroom->isActive()) << "Chatroom should be inactive, but it's still active";
    delete auxchatroom;    auxchatroom = NULL;

    // Preview chat link (ERR)
    flagPreviewChat = &requestFlagsChat[a2][MegaChatRequest::TYPE_LOAD_PREVIEW]; *flagPreviewChat = false;
    megaChatApi[a2]->openChatPreview(chatLink.c_str(), this);
    ASSERT_TRUE(waitForResponse(flagPreviewChat)) << "Timeout expired for load chat link";
    ASSERT_EQ(lastErrorChat[a2], API_ENOENT) << "Unexpected error loading an invalid chat-link: " << lastErrorMsgChat[a2];

    // --> Invite to chat
    bool *flagInviteToChatRoom = &requestFlagsChat[a1][MegaChatRequest::TYPE_INVITE_TO_CHATROOM]; *flagInviteToChatRoom = false;
    megaChatApi[a1]->inviteToChat(chatid, uh, MegaChatPeerList::PRIV_STANDARD);
    ASSERT_TRUE(waitForResponse(flagInviteToChatRoom)) << "Failed to invite a new peer after " << maxTimeout << " seconds";
    ASSERT_TRUE(!lastErrorChat[a1]) << "Failed to invite a new peer. Error: " << lastErrorMsgChat[a1] << " (" << lastErrorChat[a1] << ")";

    // Close chatroom
    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);

    delete chatroomListener;
    delete [] sessionPrimary;
    sessionPrimary = NULL;
    delete [] sessionSecondary;
    sessionSecondary = NULL;
}

/**
 * @brief MegaChatApiTest.Reactions
 *
 * Requirements:
 * - Both accounts should be conctacts
 * (if not accomplished, the test automatically solves the above)
 *
 * This test does the following:
 * - Create a group chat room or select an existing one
 * - Change another account privileges to readonly
 * - Send message
 * - Check reactions in message (error)
 * - Add reaction with NULL reaction (error)
 * - Add reaction with invalid chat (error)
 * - Add reaction with invalid message (error)
 * + Add reaction without enough permissions (error)
 * - Add reaction
 * - Add duplicate reaction (error)
 * - Check reactions in message
 * - Remove reaction with NULL reaction (error)
 * - Remove reaction with invalid chat (error)
 * - Remove reaction with invalid message (error)
 * + Remove reaction without enough permissions (error)
 * - Remove reaction
 * - Remove non-existent reaction (error)
 */
TEST_F(MegaChatApiTest, Reactions)
{
    unsigned a1 = 0;
    unsigned a2 = 1;

    // Login both accounts
    char *sessionPrimary = login(a1);
    ASSERT_TRUE(sessionPrimary);
    char *sessionSecondary = login(a2);
    ASSERT_TRUE(sessionSecondary);

    // Prepare peers, privileges...
    MegaUser *user = megaApi[a1]->getContact(account(a2).getEmail().c_str());
    if (!user || (user->getVisibility() != MegaUser::VISIBILITY_VISIBLE))
    {
        ASSERT_NO_FATAL_FAILURE({ makeContact(a1, a2); });
        delete user;
        user = megaApi[a1]->getContact(account(a2).getEmail().c_str());
    }

    // Get a group chatroom with both users
    MegaChatHandle uh = user->getHandle();
    delete user;
    user = NULL;
    MegaChatPeerList *peers = MegaChatPeerList::createInstance();
    peers->addPeer(uh, MegaChatPeerList::PRIV_STANDARD);
    MegaChatHandle chatid = getGroupChatRoom(a1, a2, peers);
    delete peers;
    ASSERT_NE(chatid, MEGACHAT_INVALID_HANDLE);

    // Open chatroom
    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a1+1);
    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a2+1);
    ::mega::unique_ptr <MegaChatRoom> chatroom (megaChatApi[a1]->getChatRoom(chatid));
    ::mega::unique_ptr<char[]> chatidB64(megaApi[a1]->handleToBase64(chatid));
    ASSERT_TRUE(chatroom) << "Cannot get chatroom for id " << chatidB64.get();

    if (chatroom->getPeerPrivilegeByHandle(uh) != PRIV_RO)
    {
        // Change peer privileges to Read-only
        bool *flagUpdatePeerPermision = &requestFlagsChat[a1][MegaChatRequest::TYPE_UPDATE_PEER_PERMISSIONS]; *flagUpdatePeerPermision = false;
        bool *peerUpdated0 = &peersUpdated[a1]; *peerUpdated0 = false;
        bool *peerUpdated1 = &peersUpdated[a2]; *peerUpdated1 = false;
        bool *mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
        MegaChatHandle *uhAction = &chatroomListener->uhAction[a1]; *uhAction = MEGACHAT_INVALID_HANDLE;
        int *priv = &chatroomListener->priv[a1]; *priv = MegaChatRoom::PRIV_UNKNOWN;
        TestMegaChatRequestListener auxrequestListener(nullptr, megaChatApi[a1]);
        megaChatApi[a1]->updateChatPermissions(chatid, uh, MegaChatRoom::PRIV_RO, &auxrequestListener);
        ASSERT_TRUE(auxrequestListener.waitForResponse()) << "Timeout expired for update privilege of peer";
        ASSERT_TRUE(!auxrequestListener.getErrorCode()) << "Failed to update privilege of peer Error: " << auxrequestListener.getErrorCode();
        ASSERT_TRUE(waitForResponse(peerUpdated0)) << "Timeout expired for receiving peer update";
        ASSERT_TRUE(waitForResponse(peerUpdated1)) << "Timeout expired for receiving peer update";
        ASSERT_TRUE(waitForResponse(mngMsgRecv)) << "Timeout expired for receiving management message";
        ASSERT_EQ(*uhAction, uh) << "User handle from message doesn't match";
        ASSERT_EQ(*priv, MegaChatRoom::PRIV_RO) << "Privilege is incorrect";
    }

    // Send a message and wait for reception by target user
    string msg0 = "HI " + account(a1).getEmail() + " - Testing reactions";
    bool *msgConfirmed = &chatroomListener->msgConfirmed[a1]; *msgConfirmed = false;
    bool *msgReceived = &chatroomListener->msgReceived[a2]; *msgReceived = false;
    bool *msgDelivered = &chatroomListener->msgDelivered[a1]; *msgDelivered = false;
    chatroomListener->clearMessages(a1);
    chatroomListener->clearMessages(a2);
    std::unique_ptr<MegaChatMessage> messageSent(megaChatApi[a1]->sendMessage(chatid, msg0.c_str()));
    ASSERT_TRUE(waitForResponse(msgConfirmed)) << "Timeout expired for receiving confirmation by server";    // for confirmation, sendMessage() is synchronous
    MegaChatHandle msgId = chatroomListener->mConfirmedMessageHandle[a1];
    ASSERT_TRUE(chatroomListener->hasArrivedMessage(a1, msgId)) << "Message not received";
    ASSERT_NE(msgId, MEGACHAT_INVALID_HANDLE) << "Wrong message id at origin";
    ASSERT_TRUE(waitForResponse(msgReceived)) << "Timeout expired for receiving message by target user";    // for reception
    ASSERT_TRUE(chatroomListener->hasArrivedMessage(a2, msgId)) << "Wrong message id at destination";
    MegaChatMessage *messageReceived = megaChatApi[a2]->getMessage(chatid, msgId);   // message should be already received, so in RAM
    ASSERT_TRUE(messageReceived && !strcmp(msg0.c_str(), messageReceived->getContent())) << "Content of message doesn't match";

    // Check reactions for the message sent above (It shouldn't exist any reaction)
    ::mega::unique_ptr <MegaStringList> reactionsList;
    reactionsList.reset(megaChatApi[a1]->getMessageReactions(chatid, msgId));
    ASSERT_FALSE(reactionsList->size()) << "getMessageReactions Error: The message shouldn't have reactions";
    int userCount = megaChatApi[a1]->getMessageReactionCount(chatid, msgId, "😰");
    ASSERT_FALSE(userCount) << "getReactionUsers Error: The reaction shouldn't exist";

    // Add invalid reaction (error)
    TestMegaChatRequestListener requestListener(nullptr, megaChatApi[a1]);
    megaChatApi[a1]->addReaction(chatid, msgId, nullptr, &requestListener);
    ASSERT_TRUE(requestListener.waitForResponse()) << "Timeout expired for add reaction";
    ASSERT_EQ(requestListener.getErrorCode(), MegaChatError::ERROR_ARGS) << "addReaction: Unexpected error for NULL reaction param.";

    // Add reaction for invalid chat (error)
    requestListener = TestMegaChatRequestListener (nullptr, megaChatApi[a1]);
    megaChatApi[a1]->addReaction(MEGACHAT_INVALID_HANDLE, msgId, "😰", &requestListener);
    ASSERT_TRUE(requestListener.waitForResponse()) << "Timeout expired for add reaction";
    ASSERT_EQ(requestListener.getErrorCode(), MegaChatError::ERROR_NOENT) << "addReaction: Unexpected error for invalid chat.";

    // Add reaction for invalid message (error)
    requestListener = TestMegaChatRequestListener (nullptr, megaChatApi[a1]);
    megaChatApi[a1]->addReaction(chatid, MEGACHAT_INVALID_HANDLE, "😰", &requestListener);
    ASSERT_TRUE(requestListener.waitForResponse()) << "Timeout expired for add reaction";
    ASSERT_EQ(requestListener.getErrorCode(), MegaChatError::ERROR_NOENT) << "addReaction: Unexpected error for invalid message.";

    // Add reaction without enough permissions (error)
    requestListener = TestMegaChatRequestListener (nullptr, megaChatApi[a2]);
    megaChatApi[a2]->addReaction(chatid, msgId, "😰", &requestListener);
    ASSERT_TRUE(requestListener.waitForResponse()) << "Timeout expired for add reaction";
    ASSERT_EQ(requestListener.getErrorCode(), MegaChatError::ERROR_ACCESS) << "addReaction: Unexpected error adding reaction without enough permissions.";

    // Add reaction (ok)
    requestListener = TestMegaChatRequestListener (nullptr, megaChatApi[a1]);
    megaChatApi[a1]->addReaction(chatid, msgId, "😰", &requestListener);
    ASSERT_TRUE(requestListener.waitForResponse()) << "Timeout expired for add reaction";
    ASSERT_EQ(requestListener.getErrorCode(), MegaChatError::ERROR_OK) << "addReaction: Unexpected error adding reaction.";

    // Add existing reaction (error)
    requestListener = TestMegaChatRequestListener (nullptr, megaChatApi[a1]);
    megaChatApi[a1]->addReaction(chatid, msgId, "😰", &requestListener);
    ASSERT_TRUE(requestListener.waitForResponse()) << "Timeout expired for add reaction";
    ASSERT_EQ(requestListener.getErrorCode(), MegaChatError::ERROR_EXIST) << "addReaction: Unexpected error for adding an existing reaction.";

    // Check reactions
    reactionsList.reset(megaChatApi[a1]->getMessageReactions(chatid, msgId));
    ASSERT_TRUE(reactionsList->size()) << "getMessageReactions Error: The message doesn't have reactions";
    userCount = megaChatApi[a1]->getMessageReactionCount(chatid, msgId, "😰");
    ASSERT_TRUE(userCount) << "getReactionUsers Error: The reaction doesn't exists";

    // Remove reaction (ok)
    requestListener = TestMegaChatRequestListener (nullptr, megaChatApi[a1]);
    megaChatApi[a1]->delReaction(chatid, msgId, "😰", &requestListener);
    ASSERT_TRUE(requestListener.waitForResponse()) << "Timeout expired for del reaction";
    ASSERT_EQ(requestListener.getErrorCode(), MegaChatError::ERROR_OK) << "delReaction: Unexpected error removing reaction.";

    // Remove unexisting reaction (error)
    requestListener = TestMegaChatRequestListener (nullptr, megaChatApi[a1]);
    megaChatApi[a1]->delReaction(chatid, msgId, "😰", &requestListener);
    ASSERT_TRUE(requestListener.waitForResponse()) << "Timeout expired for del reaction";
    ASSERT_EQ(requestListener.getErrorCode(), MegaChatError::ERROR_EXIST) << "delReaction: Unexpected error for removing a non-existent reaction.";

    // Close chatroom
    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);

    delete chatroomListener;
    delete [] sessionPrimary;
    sessionPrimary = NULL;
    delete [] sessionSecondary;
    sessionSecondary = NULL;
}

/**
 * @brief MegaChatApiTest.OfflineMode
 *
 * Requirements:
 * - Both accounts should be conctacts
 * - The 1on1 chatroom between them should exist
 * (if not accomplished, the test automatically solves the above)
 *
 * This test does the following:
 *
 * - Send message without internet connection
 * - Logout
 * - Init offline sesion
 * - Check messages unsend
 * - Connect to internet
 * - Check message has been received by the server
 *
 */
TEST_F(MegaChatApiTest, DISABLED_OfflineMode)
{
    unsigned a1 = 0;
    unsigned a2 = 1;

    char *sessionPrimary = login(a1);
    ASSERT_TRUE(sessionPrimary);
    char *sessionSecondary = login(a2);
    ASSERT_TRUE(sessionSecondary);

    // Prepare peers, privileges...
    MegaUser *user = megaApi[a1]->getContact(account(a2).getEmail().c_str());
    if (!user || (user->getVisibility() != MegaUser::VISIBILITY_VISIBLE))
    {
        ASSERT_NO_FATAL_FAILURE({ makeContact(a1, a2); });
        delete user;
        user = megaApi[a1]->getContact(account(a2).getEmail().c_str());
    }

    MegaChatHandle uh = user->getHandle();
    delete user;
    user = NULL;

    MegaChatPeerList *peers = MegaChatPeerList::createInstance();
    peers->addPeer(uh, MegaChatPeerList::PRIV_STANDARD);

    MegaChatHandle chatid = getGroupChatRoom(a1, a2, peers);
    ASSERT_NE(chatid, MEGACHAT_INVALID_HANDLE);
    ASSERT_EQ(megaChatApi[a1]->getChatConnectionState(chatid), MegaChatApi::CHAT_CONNECTION_ONLINE) <<
                             "Not connected to chatd for account " << (a1+1) << ": " << account(a1).getEmail();
    ASSERT_EQ(megaChatApi[a2]->getChatConnectionState(chatid), MegaChatApi::CHAT_CONNECTION_ONLINE) <<
                             "Not connected to chatd for account " << (a2+1) << ": " << account(a2).getEmail();

    MegaChatRoom *chatRoom = megaChatApi[a1]->getChatRoom(chatid);
    ASSERT_TRUE(chatRoom && (chatid != MEGACHAT_INVALID_HANDLE)) << "Can't get a chatroom";
    delete peers;
    peers = NULL;

    const char *info = MegaChatApiTest::printChatRoomInfo(chatRoom);
    postLog(info);
    delete [] info; info = NULL;
    delete chatRoom;

    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a1+1);

    // Load some message to feed history
    bool *flagHistoryLoaded = &chatroomListener->historyLoaded[a1]; *flagHistoryLoaded = false;
    megaChatApi[a1]->loadMessages(chatid, 16);
    ASSERT_TRUE(waitForResponse(flagHistoryLoaded)) << "Expired timeout for loading history";
    ASSERT_TRUE(!lastErrorChat[a1]) << "Failed to load history. Error: " << lastErrorMsgChat[a1] << " (" << lastErrorChat[a1] << ")";

    std::stringstream buffer;
    buffer << endl << endl << "Disconnect from the Internet now" << endl << endl;
    postLog(buffer.str());

//        system("pause");

    string msg0 = "This is a test message sent without Internet connection";
    chatroomListener->clearMessages(a1);
    MegaChatMessage *msgSent = megaChatApi[a1]->sendMessage(chatid, msg0.c_str());
    ASSERT_TRUE(msgSent) << "Failed to send message";
    ASSERT_EQ(msgSent->getStatus(), MegaChatMessage::STATUS_SENDING) << "Wrong message status.";

    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);

    // close session and resume it while offline
    ASSERT_NO_FATAL_FAILURE({ logout(a1, false); });
    bool *flagInit = &initStateChanged[a1]; *flagInit = false;
    megaChatApi[a1]->init(sessionPrimary);
    MegaApi::removeLoggerObject(logger());
    ASSERT_TRUE(waitForResponse(flagInit)) << "Expired timeout for initialization";
    int initStateValue = initState[a1];
    ASSERT_EQ(initStateValue, MegaChatApi::INIT_OFFLINE_SESSION) <<
                     "Wrong chat initialization state.";

    // check the unsent message is properly loaded
    flagHistoryLoaded = &chatroomListener->historyLoaded[a1]; *flagHistoryLoaded = false;
    bool *msgUnsentLoaded = &chatroomListener->msgLoaded[a1]; *msgUnsentLoaded = false;
    chatroomListener->clearMessages(a1);
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a1+1);
    bool msgUnsentFound = false;
    do
    {
        ASSERT_TRUE(waitForResponse(msgUnsentLoaded)) << "Expired timeout to load unsent message";
        if (chatroomListener->hasArrivedMessage(a1, msgSent->getMsgId()))
        {
            msgUnsentFound = true;
            break;
        }
        *msgUnsentLoaded = false;
    } while (*flagHistoryLoaded);
    ASSERT_TRUE(msgUnsentFound) << "Failed to load unsent message";
    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);

    buffer.str("");
    buffer << endl << endl << "Connect from the Internet now" << endl << endl;
    postLog(buffer.str());

//        system("pause");

    bool *flagRetry = &requestFlagsChat[a1][MegaChatRequest::TYPE_RETRY_PENDING_CONNECTIONS]; *flagRetry = false;
    megaChatApi[a1]->retryPendingConnections();
    ASSERT_TRUE(waitForResponse(flagRetry)) << "Timeout expired for retry pending connections";
    ASSERT_TRUE(!lastErrorChat[a1]) << "Failed to retry pending connections";

    flagHistoryLoaded = &chatroomListener->historyLoaded[a1]; *flagHistoryLoaded = false;
    bool *msgSentLoaded = &chatroomListener->msgLoaded[a1]; *msgSentLoaded = false;
    chatroomListener->clearMessages(a1);
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a1+1);
    bool msgSentFound = false;
    do
    {
        ASSERT_TRUE(waitForResponse(msgSentLoaded)) << "Expired timeout to load sent message";
        if (chatroomListener->hasArrivedMessage(a1, msgSent->getMsgId()))
        {
            msgSentFound = true;
            break;
        }
        *msgSentLoaded = false;
    } while (*flagHistoryLoaded);
    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);

    delete [] sessionPrimary;
    // We need to ensure we finish the test being logged in for the tear down
    ASSERT_NO_FATAL_FAILURE({ logout(a1); });
    sessionPrimary = login(a1);
    ASSERT_TRUE(sessionPrimary);

    ASSERT_TRUE(msgSentFound) << "Failed to load sent message";
    delete msgSent; msgSent = NULL;
    delete chatroomListener;
    chatroomListener = NULL;

    delete [] sessionPrimary;
    delete [] sessionSecondary;
}

/**
 * @brief MegaChatApiTest.ClearHistory
 *
 * Requirements:
 * - Both accounts should be conctacts
 * - The 1on1 chatroom between them should exist
 * (if not accomplished, the test automatically solves the above)
 *
 * This test does the following:
 *
 * - Send five mesages to chatroom
 * Check history has five messages
 * - Clear history
 * Check history has zero messages
 *
 */
TEST_F(MegaChatApiTest, ClearHistory)
{
    unsigned a1 = 0;
    unsigned a2 = 1;

    char *sessionPrimary = login(a1);
    ASSERT_TRUE(sessionPrimary);
    char *sessionSecondary = login(a2);
    ASSERT_TRUE(sessionSecondary);

    MegaUser *user = megaApi[a1]->getContact(account(a2).getEmail().c_str());
    if (!user || (user->getVisibility() != MegaUser::VISIBILITY_VISIBLE))
    {
        ASSERT_NO_FATAL_FAILURE({ makeContact(a1, a2); });
    }
    delete user;
    user = NULL;

    MegaChatHandle chatid = getPeerToPeerChatRoom(a1, a2);
    ASSERT_NE(chatid, MEGACHAT_INVALID_HANDLE);

    // Open chatrooms
    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a1+1);
    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a2+1);

    // Load some message to feed history
    loadHistory(a1, chatid, chatroomListener);
    loadHistory(a2, chatid, chatroomListener);

    // Send 5 messages to have some history
    for (int i = 0; i < 5; i++)
    {
        string msg0 = "HI " + account(a2).getEmail() + " - Testing clearhistory. This messages is the number " + std::to_string(i);

        MegaChatMessage *message = sendTextMessageOrUpdate(a1, a2, chatid, msg0, chatroomListener);
        ASSERT_TRUE(message);

        delete message;
        message = NULL;
    }

    // Close the chatrooms
    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);
    delete chatroomListener;

    // Open chatrooms
    chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a1+1);
    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a2+1);

    // --> Load some message to feed history
    int count = loadHistory(a1, chatid, chatroomListener);
    // we sent 5 messages, but if the chat already existed, there was a "Clear history" message already
    ASSERT_TRUE(count == 5 || count == 6) << "Wrong count of messages: " << count;
    count = loadHistory(a2, chatid, chatroomListener);
    ASSERT_TRUE(count == 5 || count == 6) << "Wrong count of messages: " << count;

    // Clear history
    ASSERT_NO_FATAL_FAILURE({ clearHistory(a1, a2, chatid, chatroomListener); });
    // TODO: in this case, it's not just to clear the history, but
    // to also check the other user received the corresponding message.

    // Close and re-open chatrooms
    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);
    delete chatroomListener;
    chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a1+1);
    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a2+1);

    // --> Check history is been truncated
    count = loadHistory(a1, chatid, chatroomListener);
    ASSERT_EQ(count, 1) << "Wrong count of messages";
    count = loadHistory(a2, chatid, chatroomListener);
    ASSERT_EQ(count, 1) << "Wrong count of messages";

    // Close the chatrooms
    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);
    delete chatroomListener;

    delete [] sessionPrimary;
    sessionPrimary = NULL;
    delete [] sessionSecondary;
    sessionSecondary = NULL;
}

/**
 * @brief MegaChatApiTest.SwitchAccounts
 *
 * This test does the following:
 * - Login with accoun1 email and pasword
 * - Logout
 * - With the same megaApi and megaChatApi, login with account2
 */
TEST_F(MegaChatApiTest, SwitchAccounts)
{
    unsigned a1 = 0;
    unsigned a2 = 1;

    char *session = login(a1);
    ASSERT_TRUE(session);

    MegaChatListItemList *items = megaChatApi[a1]->getChatListItems();
    for (unsigned i = 0; i < items->size(); i++)
    {
        const MegaChatListItem *item = items->get(i);
        if (item->isPublic())
        {
            continue;
        }

        const char *info = MegaChatApiTest::printChatListItemInfo(item);
        postLog(info);
        delete [] info; info = NULL;

        std::this_thread::sleep_for(std::chrono::seconds(3));

        MegaChatHandle chatid = item->getChatId();
        MegaChatListItem *itemUpdated = megaChatApi[a1]->getChatListItem(chatid);

        info = MegaChatApiTest::printChatListItemInfo(itemUpdated);
        postLog(info);
        delete [] info; info = NULL;

        delete itemUpdated;
        itemUpdated = NULL;
    }

    delete items;
    items = NULL;

    ASSERT_NO_FATAL_FAILURE({ logout(a1, true); });    // terminate() and destroy Client

    delete [] session;
    session = NULL;

    // Login over same index account but with other user
    session = login(a1, NULL, account(a2).getEmail().c_str(), account(a2).getPassword().c_str());
    ASSERT_TRUE(session);

    delete [] session;
    session = NULL;
}

/**
 * @brief MegaChatApiTest.Attachment
 *
 * Requirements:
 * - Both accounts should be conctacts
 * - The 1on1 chatroom between them should exist
 * (if not accomplished, the test automatically solves the above)
 * - Image <PATH_IMAGE>/<FILE_IMAGE_NAME> should exist
 *
 * This test does the following:
 *
 * - Upload new file
 * - Send file as attachment to chatroom
 * + Download received file
 * + Import received file into the cloud
 * - Revoke access to file
 * + Download received file again --> no access
 *
 * - Upload an image
 * - Download the thumbnail
 * + Download the thumbnail
 *
 */
TEST_F(MegaChatApiTest, Attachment)
{
    unsigned a1 = 0;
    unsigned a2 = 1;

    char *primarySession = login(a1);
    ASSERT_TRUE(primarySession);
    char *secondarySession = login(a2);
    ASSERT_TRUE(secondarySession);

    // 0. Ensure both accounts are contacts and there's a 1on1 chatroom
    MegaUser *user = megaApi[a1]->getContact(account(a2).getEmail().c_str());
    if (!user || (user->getVisibility() != MegaUser::VISIBILITY_VISIBLE))
    {
        ASSERT_NO_FATAL_FAILURE({ makeContact(a1, a2); });
    }
    delete user;
    user = NULL;

    MegaChatHandle chatid = getPeerToPeerChatRoom(a1, a2);
    ASSERT_NE(chatid, MEGACHAT_INVALID_HANDLE);

    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, chatroomListener)) << "Cannot open chatroom";
    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(chatid, chatroomListener)) << "Cannot open chatroom";

    // Load some messages to feed history
    loadHistory(a1, chatid, chatroomListener);
    loadHistory(a2, chatid, chatroomListener);

    chatroomListener->clearMessages(a1);   // will be set at confirmation
    chatroomListener->clearMessages(a2);   // will be set at reception

    std::string formatDate = dateToString();

    // A uploads a new file
    createFile(formatDate, LOCAL_PATH, formatDate);
    MegaNode* nodeSent = uploadFile(a1, formatDate, LOCAL_PATH, REMOTE_PATH);
    ASSERT_TRUE(nodeSent);

    // A sends the file as attachment to the chatroom
    MegaChatMessage *msgSent = attachNode(a1, a2, chatid, nodeSent, chatroomListener);
    ASSERT_TRUE(msgSent);
    MegaNode *nodeReceived = msgSent->getMegaNodeList()->get(0)->copy();

    // B downloads the node
    ASSERT_TRUE(downloadNode(a2, nodeReceived)) << "Cannot download node attached to message";

    // B imports the node
    ASSERT_TRUE(importNode(a2, nodeReceived, FILE_IMAGE_NAME)) << "Cannot import node attached to message";

    // A revokes access to node
    bool *flagRequest = &requestFlagsChat[a1][MegaChatRequest::TYPE_REVOKE_NODE_MESSAGE]; *flagRequest = false;
    bool *flagConfirmed = &chatroomListener->msgConfirmed[a1]; *flagConfirmed = false;
    bool *flagReceived = &chatroomListener->msgReceived[a2]; *flagReceived = false;
    chatroomListener->mConfirmedMessageHandle[a1] = MEGACHAT_INVALID_HANDLE;
    chatroomListener->clearMessages(a1);   // will be set at confirmation
    chatroomListener->clearMessages(a2);   // will be set at reception
    megachat::MegaChatHandle revokeAttachmentNode = nodeSent->getHandle();
    megaChatApi[a1]->revokeAttachment(chatid, revokeAttachmentNode, this);
    ASSERT_TRUE(waitForResponse(flagRequest)) << "Failed to revoke access to node after " << maxTimeout << " seconds";
    ASSERT_TRUE(!lastErrorChat[a1]) << "Failed to revoke access: " << lastErrorChat[a1];
    ASSERT_TRUE(waitForResponse(flagConfirmed)) << "Timeout expired for receiving confirmation by server";
    MegaChatHandle msgId0 = chatroomListener->mConfirmedMessageHandle[a1];
    ASSERT_NE(msgId0, MEGACHAT_INVALID_HANDLE) << "Wrong message id";

    // Wait for message recived has same id that message sent. It can fail if we receive a message
    ASSERT_TRUE(waitForResponse(flagReceived)) << "Timeout expired for receiving message by target user";

    ASSERT_TRUE(chatroomListener->hasArrivedMessage(a2, msgId0)) << "Message ids don't match";
    MegaChatMessage *msgReceived = megaChatApi[a2]->getMessage(chatid, msgId0);   // message should be already received, so in RAM
    ASSERT_TRUE(msgReceived) << "Message not found";
    ASSERT_EQ(msgReceived->getType(), MegaChatMessage::TYPE_REVOKE_NODE_ATTACHMENT) << "Unexpected type of message";
    ASSERT_EQ(msgReceived->getHandleOfAction(), nodeSent->getHandle()) << "Handle of attached nodes don't match";

    // Remove the downloaded file to try to download it again after revoke
    std::string filePath = DOWNLOAD_PATH + std::string(formatDate);
    std::string secondaryFilePath = DOWNLOAD_PATH + std::string("remove");
    rename(filePath.c_str(), secondaryFilePath.c_str());

    // B attempt to download the file after access revocation
    ASSERT_FALSE(downloadNode(1, nodeReceived)) << "Download succeed, when it should fail";

    delete nodeReceived;
    nodeReceived = NULL;

    delete msgSent;
    msgSent = NULL;

    delete nodeSent;
    nodeSent = NULL;

    // A uploads an image to check previews / thumbnails
    std::string path = DEFAULT_PATH;
    if (getenv(PATH_IMAGE.c_str()) != NULL)
    {
        path = getenv(PATH_IMAGE.c_str());
    }
    nodeSent = uploadFile(a1, FILE_IMAGE_NAME, path, REMOTE_PATH);
    ASSERT_TRUE(nodeSent);
    msgSent = attachNode(a1, a2, chatid, nodeSent, chatroomListener);
    ASSERT_TRUE(msgSent);
    nodeReceived = msgSent->getMegaNodeList()->get(0)->copy();

    // A gets the thumbnail of the uploaded image
    std::string thumbnailPath = LOCAL_PATH + "/thumbnail0.jpg";
    RequestTracker getThumbnailTracker;
    megaApi[a1]->getThumbnail(nodeSent, thumbnailPath.c_str(), &getThumbnailTracker);
    ASSERT_EQ(getThumbnailTracker.waitForResult(), API_OK) << "Failed to get thumbnail. Error: " << getThumbnailTracker.getErrorString();

    // B gets the thumbnail of the attached image
    thumbnailPath = LOCAL_PATH + "/thumbnail1.jpg";
    RequestTracker getThumbnailTracker2;
    megaApi[a2]->getThumbnail(nodeReceived, thumbnailPath.c_str(), &getThumbnailTracker2);
    ASSERT_EQ(getThumbnailTracker2.waitForResult(), API_OK) << "Failed to get thumbnail (2). Error: " << getThumbnailTracker2.getErrorString();

    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);

    delete msgReceived;
    msgReceived = NULL;

    delete nodeReceived;
    nodeReceived = NULL;

    delete msgSent;
    msgSent = NULL;

    delete nodeSent;
    nodeSent = NULL;

    delete [] primarySession;
    primarySession = NULL;
    delete [] secondarySession;
    secondarySession = NULL;
}

/**
 * @brief MegaChatApiTest.LastMessage
 *
 * Requirements:
 *      - Both accounts should be conctacts
 *      - The 1on1 chatroom between them should exist
 * (if not accomplished, the test automatically solves them)
 *
 * This test does the following:
 *
 * - Send a message to chatroom
 * + Receive message
 * Check if the last message received is equal to the message sent
 *
 * - Upload new file
 * - Send file as attachment to chatroom
 * + Receive message with attach node
 * Check if the last message content is equal to the node's name sent
 */
TEST_F(MegaChatApiTest, LastMessage)
{
    unsigned a1 = 0;
    unsigned a2 = 1;

    char *sessionPrimary = login(a1);
    ASSERT_TRUE(sessionPrimary);
    char *sessionSecondary = login(a2);
    ASSERT_TRUE(sessionSecondary);

    MegaUser *user = megaApi[a1]->getContact(account(a2).getEmail().c_str());
    if (!user || (user->getVisibility() != MegaUser::VISIBILITY_VISIBLE))
    {
        ASSERT_NO_FATAL_FAILURE({ makeContact(a1, a2); });
    }
    delete user;
    user = NULL;

    MegaChatHandle chatid = getPeerToPeerChatRoom(a1, a2);
    ASSERT_NE(chatid, MEGACHAT_INVALID_HANDLE);

    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a1+1);
    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a2+1);

    // Load some message to feed history
    loadHistory(a1, chatid, chatroomListener);
    loadHistory(a2, chatid, chatroomListener);

    chatroomListener->clearMessages(a1);
    chatroomListener->clearMessages(a2);
    std::string formatDate = dateToString();

    MegaChatMessage *msgSent = sendTextMessageOrUpdate(a1, a2, chatid, formatDate, chatroomListener);
    ASSERT_TRUE(msgSent);
    MegaChatHandle msgId = msgSent->getMsgId();
    bool hasArrived = chatroomListener->hasArrivedMessage(a1, msgId);
    ASSERT_TRUE(hasArrived) << "Id of sent message has not been received yet";
    MegaChatListItem *itemAccount1 = megaChatApi[a1]->getChatListItem(chatid);
    MegaChatListItem *itemAccount2 = megaChatApi[a2]->getChatListItem(chatid);
    ASSERT_STREQ(formatDate.c_str(), itemAccount1->getLastMessage()) <<
                     "Content of last-message doesn't match.\n Sent vs Received.";
    ASSERT_EQ(itemAccount1->getLastMessageId(), msgId) << "Last message id is different from message sent id";
    ASSERT_EQ(itemAccount2->getLastMessageId(), msgId) << "Last message id is different from message received id";
    MegaChatMessage *messageConfirm = megaChatApi[a1]->getMessage(chatid, msgId);
    ASSERT_STREQ(messageConfirm->getContent(), itemAccount1->getLastMessage()) <<
                     "Content of last-message reported id is different than last-message reported content";

    delete itemAccount1;
    itemAccount1 = NULL;
    delete itemAccount2;
    itemAccount2 = NULL;

    delete msgSent;
    msgSent = NULL;
    delete messageConfirm;
    messageConfirm = NULL;

    ASSERT_NO_FATAL_FAILURE({ clearHistory(a1, a2, chatid, chatroomListener); });
    chatroomListener->clearMessages(a1);
    chatroomListener->clearMessages(a2);

    formatDate = dateToString();
    createFile(formatDate, LOCAL_PATH, formatDate);
    MegaNode* nodeSent = uploadFile(a1, formatDate, LOCAL_PATH, REMOTE_PATH);
    ASSERT_TRUE(nodeSent);
    msgSent = attachNode(a1, a2, chatid, nodeSent, chatroomListener);
    ASSERT_TRUE(msgSent);
    MegaNode *nodeReceived = msgSent->getMegaNodeList()->get(0)->copy();
    msgId = msgSent->getMsgId();
    hasArrived = chatroomListener->hasArrivedMessage(a1, msgId);
    ASSERT_TRUE(hasArrived) << "Id of sent message has not been received yet";
    itemAccount1 = megaChatApi[a1]->getChatListItem(chatid);
    itemAccount2 = megaChatApi[a2]->getChatListItem(chatid);
    ASSERT_STREQ(formatDate.c_str(), itemAccount1->getLastMessage()) <<
                     "Last message content differs from content of message sent.\n Sent vs Received.";
    ASSERT_EQ(itemAccount1->getLastMessageId(), msgId) << "Last message id is different from message sent id";
    ASSERT_EQ(itemAccount2->getLastMessageId(), msgId) << "Last message id is different from message received id";
    delete itemAccount1;
    itemAccount1 = NULL;
    delete itemAccount2;
    itemAccount2 = NULL;

    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);

    delete nodeReceived;
    nodeReceived = NULL;

    delete nodeSent;
    nodeSent = NULL;

    delete msgSent;
    msgSent = NULL;

    delete [] sessionPrimary;
    sessionPrimary = NULL;
    delete [] sessionSecondary;
    sessionSecondary = NULL;
}

/**
 * @brief MegaChatApiTest.SendContact
 *
 * Requirements:
 *      - Both accounts should be conctacts
 *      - The 1on1 chatroom between them should exist
 * (if not accomplished, the test automatically solves them)
 *
 * This test does the following:
 * - Send a message with an attach contact to chatroom
 * + Receive message
 *
 * + Forward the contact message
 * - Receive message
 * Check if message type is TYPE_CONTACT_ATTACHMENT and contact email received is equal to account2 email
 */
TEST_F(MegaChatApiTest, SendContact)
{
    unsigned a1 = 0;
    unsigned a2 = 1;

    char *primarySession = login(a1);
    ASSERT_TRUE(primarySession);
    char *secondarySession = login(a2);
    ASSERT_TRUE(secondarySession);

    MegaUser *user = megaApi[a1]->getContact(account(a2).getEmail().c_str());
    if (!user || (user->getVisibility() != MegaUser::VISIBILITY_VISIBLE))
    {
        ASSERT_NO_FATAL_FAILURE({ makeContact(a1, a2); });
    }
    delete user;
    user = NULL;

    MegaChatHandle chatid = getPeerToPeerChatRoom(a1, a2);
    ASSERT_NE(chatid, MEGACHAT_INVALID_HANDLE);

    // 1. A sends a message to B while B has the chat opened.
    // --> check the confirmed in A, the received message in B, the delivered in A

    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account 1";
    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account 2";

    loadHistory(a1, chatid, chatroomListener);
    loadHistory(a2, chatid, chatroomListener);

    bool *flagConfirmed = &chatroomListener->msgConfirmed[a1]; *flagConfirmed = false;
    bool *flagReceived = &chatroomListener->msgContactReceived[a2]; *flagReceived = false;
    bool *flagDelivered = &chatroomListener->msgDelivered[a1]; *flagDelivered = false;
    chatroomListener->clearMessages(a1);
    chatroomListener->clearMessages(a2);

    user = megaApi[a1]->getContact(account(a2).getEmail().c_str());
    ASSERT_TRUE(user) << "Failed to get contact with email" << account(a2).getEmail();
    MegaChatHandle uh1 = user->getHandle();
    delete user;
    user = NULL;

    MegaHandleList* contactList = MegaHandleList::createInstance();
    contactList->addMegaHandle(uh1);
    MegaChatMessage *messageSent = megaChatApi[a1]->attachContacts(chatid, contactList);

    ASSERT_TRUE(waitForResponse(flagConfirmed)) << "Timeout expired for receiving confirmation by server";
    MegaChatHandle msgId0 = chatroomListener->mConfirmedMessageHandle[a1];
    ASSERT_NE(msgId0, MEGACHAT_INVALID_HANDLE) << "Wrong message id at origin";

    ASSERT_TRUE(waitForResponse(flagReceived)) << "Timeout expired for receiving message by target user";    // for reception
    ASSERT_TRUE(chatroomListener->hasArrivedMessage(a2, msgId0)) << "Wrong message id at destination";
    MegaChatMessage *msgReceived = megaChatApi[a2]->getMessage(chatid, msgId0);   // message should be already received, so in RAM
    ASSERT_TRUE(msgReceived) << "Failed to get message by id";

    ASSERT_EQ(msgReceived->getType(), MegaChatMessage::TYPE_CONTACT_ATTACHMENT) << "Wrong type of message.";
    ASSERT_EQ(msgReceived->getUsersCount(), 1) << "Wrong number of users in message.";
    ASSERT_STREQ(msgReceived->getUserEmail(0), account(a2).getEmail().c_str()) << "Wrong email address in message.";

    // Check if reception confirmation is active and, in this case, only 1on1 rooms have acknowledgement of receipt
    if (megaChatApi[a1]->isMessageReceptionConfirmationActive()
            && !megaChatApi[a1]->getChatRoom(chatid)->isGroup())
    {
        ASSERT_TRUE(waitForResponse(flagDelivered)) << "Timeout expired for receiving delivery notification";    // for delivery
    }

    flagConfirmed = &chatroomListener->msgConfirmed[a2]; *flagConfirmed = false;
    flagReceived = &chatroomListener->msgContactReceived[a1]; *flagReceived = false;
    flagDelivered = &chatroomListener->msgDelivered[a2]; *flagDelivered = false;
    chatroomListener->clearMessages(a1);
    chatroomListener->clearMessages(a2);

    MegaChatMessage *messageForwared = megaChatApi[a2]->forwardContact(chatid, msgId0, chatid);
    ASSERT_TRUE(messageForwared) << "Failed to forward a contact message";
    ASSERT_TRUE(waitForResponse(flagConfirmed)) << "Timeout expired for receiving confirmation by server";
    MegaChatHandle msgId1 = chatroomListener->mConfirmedMessageHandle[a2];
    ASSERT_NE(msgId1, MEGACHAT_INVALID_HANDLE) << "Wrong message id at origin";

    ASSERT_TRUE(waitForResponse(flagReceived)) << "Timeout expired for receiving message by target user";    // for reception
    ASSERT_TRUE(chatroomListener->hasArrivedMessage(a1, msgId1)) << "Wrong message id at destination";
    MegaChatMessage *msgReceived1 = megaChatApi[a2]->getMessage(chatid, msgId1);   // message should be already received, so in RAM
    ASSERT_TRUE(msgReceived1) << "Failed to get message by id";

    ASSERT_EQ(msgReceived1->getType(), MegaChatMessage::TYPE_CONTACT_ATTACHMENT) << "Wrong type of message.";
    ASSERT_EQ(msgReceived1->getUsersCount(), 1) << "Wrong number of users in message.";
    ASSERT_STREQ(msgReceived1->getUserEmail(0), account(a2).getEmail().c_str()) << "Wrong email address in message.";

    // Check if reception confirmation is active and, in this case, only 1on1 rooms have acknowledgement of receipt
    if (megaChatApi[a2]->isMessageReceptionConfirmationActive()
            && !megaChatApi[a2]->getChatRoom(chatid)->isGroup())
    {
        ASSERT_TRUE(waitForResponse(flagDelivered)) << "Timeout expired for receiving delivery notification";    // for delivery
    }

    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);

    delete contactList;
    contactList = NULL;

    delete messageSent;
    messageSent = NULL;

    delete msgReceived;
    msgReceived = NULL;

    delete [] primarySession;
    primarySession = NULL;
    delete [] secondarySession;
    secondarySession = NULL;
}

/**
 * @brief MegaChatApiTest.GroupLastMessage
 *
 * Requirements:
 *      - Both accounts should be conctacts
 * (if not accomplished, the test automatically solves them)
 *
 * This test does the following:
 * - Create a group chat room
 * - Send a message to chatroom
 * + Receive message
 * - Change chatroom title
 * + Check chatroom titles has changed
 *
 * Check if the last message content is equal to the last message sent excluding
 * management messages, which are not to be shown as last message.
 */
TEST_F(MegaChatApiTest, GroupLastMessage)
{
    unsigned a1 = 0;
    unsigned a2 = 1;

    char *session0 = login(a1);
    ASSERT_TRUE(session0);
    char *session1 = login(a2);
    ASSERT_TRUE(session1);

    // Prepare peers, privileges...
    MegaUser *user = megaApi[a1]->getContact(account(a2).getEmail().c_str());
    if (!user || (user->getVisibility() != MegaUser::VISIBILITY_VISIBLE))
    {
        ASSERT_NO_FATAL_FAILURE({ makeContact(a1, a2); });
        delete user;
        user = megaApi[a1]->getContact(account(a2).getEmail().c_str());
    }

    MegaChatHandle uh = user->getHandle();
    delete user;
    user = NULL;

    MegaChatPeerList *peers = MegaChatPeerList::createInstance();
    peers->addPeer(uh, MegaChatPeerList::PRIV_STANDARD);

    MegaChatHandle chatid = getGroupChatRoom(a1, a2, peers);
    delete peers;
    ASSERT_NE(chatid, MEGACHAT_INVALID_HANDLE);

    // --> Open chatroom
    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account 1";
    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account 2";

    // Load some message to feed history
    loadHistory(a1, chatid, chatroomListener);
    loadHistory(a2, chatid, chatroomListener);

    chatroomListener->clearMessages(a1);
    chatroomListener->clearMessages(a2);

    std::string textToSend = "Last Message";
    MegaChatMessage *msgSent = sendTextMessageOrUpdate(a1, a2, chatid, textToSend, chatroomListener);
    ASSERT_TRUE(msgSent);
    MegaChatHandle msgId = msgSent->getMsgId();
    bool hasArrived = chatroomListener->hasArrivedMessage(a1, msgId);
    ASSERT_TRUE(hasArrived) << "Id of sent message has not been received yet";
    MegaChatListItem *itemAccount1 = megaChatApi[a1]->getChatListItem(chatid);
    MegaChatListItem *itemAccount2 = megaChatApi[a2]->getChatListItem(chatid);
    ASSERT_EQ(itemAccount1->getLastMessageId(), msgId) << "Last message id is different from message sent id";
    ASSERT_EQ(itemAccount2->getLastMessageId(), msgId) << "Last message id is different from message received id";
    delete itemAccount1;
    itemAccount1 = NULL;
    delete itemAccount2;
    itemAccount2 = NULL;


    // --> Set title
    std::string title = "Title " + std::to_string(time(NULL));
    bool *flagChatRoomName = &requestFlagsChat[a1][MegaChatRequest::TYPE_EDIT_CHATROOM_NAME]; *flagChatRoomName = false;
    bool *titleItemChanged0 = &titleUpdated[a1]; *titleItemChanged0 = false;
    bool *titleItemChanged1 = &titleUpdated[a2]; *titleItemChanged1 = false;
    bool *titleChanged0 = &chatroomListener->titleUpdated[a1]; *titleChanged0 = false;
    bool *titleChanged1 = &chatroomListener->titleUpdated[a2]; *titleChanged1 = false;
    bool *mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
    std::string *msgContent = &chatroomListener->content[a1]; *msgContent = "";
    megaChatApi[a1]->setChatTitle(chatid, title.c_str());
    ASSERT_TRUE(waitForResponse(flagChatRoomName)) << "Timeout expired for changing name";
    ASSERT_TRUE(!lastErrorChat[a1]) << "Failed to change name. Error: " << lastErrorMsgChat[a1] << " (" << lastErrorChat[a1] << ")";
    ASSERT_TRUE(waitForResponse(titleItemChanged0)) << "Timeout expired for receiving chat list title update for main account";
    ASSERT_TRUE(waitForResponse(titleItemChanged1)) << "Timeout expired for receiving chat list title update for auxiliar account";
    ASSERT_TRUE(waitForResponse(titleChanged0)) << "Timeout expired for receiving chatroom title update for main account";
    ASSERT_TRUE(waitForResponse(titleChanged1)) << "Timeout expired for receiving chatroom title update for auxiliar account";
    ASSERT_TRUE(waitForResponse(mngMsgRecv)) << "Timeout expired for receiving management";
    ASSERT_EQ(title, *msgContent) <<
                     "Title name has not changed correctly. Name established by a1 VS name received in a2";
    MegaChatHandle managementMsg1 = chatroomListener->msgId[a1].back();
    MegaChatHandle managementMsg2 = chatroomListener->msgId[a2].back();

    itemAccount1 = megaChatApi[a1]->getChatListItem(chatid);
    itemAccount2 = megaChatApi[a2]->getChatListItem(chatid);
    ASSERT_STREQ(title.c_str(), itemAccount1->getLastMessage()) << "Last message content has not the tittle at account 1.";

    ASSERT_STREQ(title.c_str(), itemAccount2->getLastMessage()) << "Last message content has not the tittle at account 2.";

    ASSERT_EQ(itemAccount1->getLastMessageId(), managementMsg1) << "Last message id is different from management message id at account1";
    ASSERT_EQ(itemAccount2->getLastMessageId(), managementMsg2) << "Last message id is different from management message id at account2";
    ASSERT_EQ(itemAccount2->getLastMessageId(), itemAccount1->getLastMessageId()) << "Last message id is different from account1 and account2";

    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);

    delete itemAccount1;
    itemAccount1 = NULL;
    delete itemAccount2;
    itemAccount2 = NULL;

    delete msgSent;
    msgSent = NULL;

    delete [] session0;
    session0 = NULL;
    delete [] session1;
    session1 = NULL;
}

/**
 * @brief MegaChatApiTest.RetentionHistory
 *
 * Requirements:
 *      - Both accounts should be contacts
 * (if not accomplished, the test automatically solves them)
 *
 * This test does the following:
 * - Select or create a group chat room
 * - Set secondary account chat room privilege to READ ONLY
 * + Set retention time for an invalid handle (error)
 * + Set retention time for an invalid chatroom (error)
 * + Set retention time without enough permissions (error)
 * - Set retention time to zero (disabled)
 * - Send a couple of messages
 * - Set retention time to 5 seconds
 * - Sleep 30 seconds
 * - Check history has been cleared
 * + Check history has been cleared
 * - Set retention time to zero (disabled)
 * - Send 5 messages
 * - Close and re-open chatrooms
 * - Check history contains messages
 * + Check history contains messages
 * - Close the chatrooms
 **/
TEST_F(MegaChatApiTest, RetentionHistory)
{
    unsigned a1 = 0;
    unsigned a2 = 1;

    // Login both accounts
    ::mega::unique_ptr<char[]>sessionPrimary(login(a1));
    ASSERT_TRUE(sessionPrimary);
    ::mega::unique_ptr<char[]>sessionSecondary(login(a2));
    ASSERT_TRUE(sessionSecondary);

    // Prepare peers, privileges...
    ::mega::unique_ptr<MegaUser>user(megaApi[a1]->getContact(account(a2).getEmail().c_str()));
    if (!user || (user->getVisibility() != MegaUser::VISIBILITY_VISIBLE))
    {
        ASSERT_NO_FATAL_FAILURE({ makeContact(a1, a2); });
        user.reset(megaApi[a1]->getContact(account(a2).getEmail().c_str()));
    }

    // Get a group chatroom with both users
    MegaChatHandle uh = user->getHandle();
    ::mega::unique_ptr<MegaChatPeerList> peers(MegaChatPeerList::createInstance());
    peers->addPeer(uh, MegaChatPeerList::PRIV_STANDARD);
    MegaChatHandle chatid = getGroupChatRoom(a1, a2, peers.get());
    ASSERT_NE(chatid, MEGACHAT_INVALID_HANDLE);

    // Open chatroom
    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a1+1);
    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a2+1);
    ::mega::unique_ptr <MegaChatRoom> chatroom (megaChatApi[a1]->getChatRoom(chatid));
    ::mega::unique_ptr<char[]> chatidB64(megaApi[a1]->handleToBase64(chatid));
    ASSERT_TRUE(chatroom) << "Cannot get chatroom for id " << chatidB64.get();

    // Set secondary account priv to READ ONLY
    if (chatroom->getPeerPrivilegeByHandle(uh) != PRIV_RO)
    {
        // Change peer privileges to Read-only
        bool *flagUpdatePeerPermision = &requestFlagsChat[a1][MegaChatRequest::TYPE_UPDATE_PEER_PERMISSIONS]; *flagUpdatePeerPermision = false;
        bool *peerUpdated0 = &peersUpdated[a1]; *peerUpdated0 = false;
        bool *peerUpdated1 = &peersUpdated[a2]; *peerUpdated1 = false;
        bool *mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
        MegaChatHandle *uhAction = &chatroomListener->uhAction[a1]; *uhAction = MEGACHAT_INVALID_HANDLE;
        int *priv = &chatroomListener->priv[a1]; *priv = MegaChatRoom::PRIV_UNKNOWN;
        megaChatApi[a1]->updateChatPermissions(chatid, uh, MegaChatRoom::PRIV_RO);
        ASSERT_TRUE(waitForResponse(flagUpdatePeerPermision)) << "Timeout expired for update privilege of peer";
        ASSERT_TRUE(!lastErrorChat[a1]) << "Failed to update privilege of peer Error: " << lastErrorMsgChat[a1] << " (" << lastErrorChat[a1] << ")";
        ASSERT_TRUE(waitForResponse(peerUpdated0)) << "Timeout expired for receiving peer update";
        ASSERT_TRUE(waitForResponse(peerUpdated1)) << "Timeout expired for receiving peer update";
        ASSERT_TRUE(waitForResponse(mngMsgRecv)) << "Timeout expired for receiving management message";
        ASSERT_EQ(*uhAction, uh) << "User handle from message doesn't match";
        ASSERT_EQ(*priv, MegaChatRoom::PRIV_RO) << "Privilege is incorrect";
    }

    // Set retention time for an invalid handle
    bool *flagChatRetentionTime = &requestFlagsChat[a2][MegaChatRequest::TYPE_SET_RETENTION_TIME]; *flagChatRetentionTime = false;
    megaChatApi[a2]->setChatRetentionTime(MEGACHAT_INVALID_HANDLE, 1);
    ASSERT_TRUE(waitForResponse(flagChatRetentionTime)) << "Timeout expired set chat retention time";
    ASSERT_EQ(lastErrorChat[a2], MegaChatError::ERROR_ARGS) << "Set retention time: Unexpected error for Invalid handle. Error: " << lastErrorMsgChat[a2];

    // Set retention time for a not found chatroom
    flagChatRetentionTime = &requestFlagsChat[a2][MegaChatRequest::TYPE_SET_RETENTION_TIME]; *flagChatRetentionTime = false;
    megaChatApi[a2]->setChatRetentionTime(123456, 1);
    ASSERT_TRUE(waitForResponse(flagChatRetentionTime)) << "Timeout expired set chat retention time";
    ASSERT_EQ(lastErrorChat[a2], MegaChatError::ERROR_NOENT) << "Set retention time: Unexpected error for a not found chatroom. Error: " << lastErrorMsgChat[a2];

    // Set retention time without enough permissions
    flagChatRetentionTime = &requestFlagsChat[a2][MegaChatRequest::TYPE_SET_RETENTION_TIME]; *flagChatRetentionTime = false;
    megaChatApi[a2]->setChatRetentionTime(chatid, 1);
    ASSERT_TRUE(waitForResponse(flagChatRetentionTime)) << "Timeout expired set chat retention time";
    ASSERT_EQ(lastErrorChat[a2], MegaChatError::ERROR_ACCESS) << "Set retention time: Unexpected error for not enough permissions. Error: " << lastErrorMsgChat[a2];

    // Disable retention time
    if (chatroom->getRetentionTime() != 0)
    {
        // Disable retention time if any
        bool *retentionTimeChanged0 = &chatroomListener->retentionTimeUpdated[a1]; *retentionTimeChanged0 = false;
        bool *retentionTimeChanged1 = &chatroomListener->retentionTimeUpdated[a2]; *retentionTimeChanged1 = false;
        bool *mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
        flagChatRetentionTime = &requestFlagsChat[a1][MegaChatRequest::TYPE_SET_RETENTION_TIME]; *flagChatRetentionTime = false;
        megaChatApi[a1]->setChatRetentionTime(chatid, 0);
        ASSERT_TRUE(waitForResponse(flagChatRetentionTime)) << "Timeout expired set chat retention time";
        ASSERT_EQ(lastErrorChat[a1], MegaChatError::ERROR_OK) << "Set retention time: Unexpected error. Error: " << lastErrorMsgChat[a1];
        ASSERT_TRUE(waitForResponse(retentionTimeChanged0)) << "Timeout expired for receiving chatroom update";
        ASSERT_TRUE(waitForResponse(retentionTimeChanged1)) << "Timeout expired for receiving chatroom update";
        ASSERT_TRUE(waitForResponse(mngMsgRecv)) << "Timeout expired for receiving management message";
    }

    // Send 5 messages
    std::string messageToSend = "Msg from " +account(a1).getEmail();
    for (int i = 0; i < 5; i++)
    {
        unique_ptr<MegaChatMessage> msg(sendTextMessageOrUpdate(a1, a2, chatid, messageToSend, chatroomListener));
        ASSERT_TRUE(msg);
    }

    // Set retention time to 5 seconds
    bool *retentionTimeChanged0 = &chatroomListener->retentionTimeUpdated[a1]; *retentionTimeChanged0 = false;
    bool *retentionTimeChanged1 = &chatroomListener->retentionTimeUpdated[a2]; *retentionTimeChanged1 = false;
    bool *mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
    bool *flagConfirmed0 = &chatroomListener->retentionHistoryTruncated[a1]; *flagConfirmed0 = false;
    MegaChatHandle *msgId0 = &chatroomListener->mRetentionMessageHandle[a1]; *msgId0 = MEGACHAT_INVALID_HANDLE;
    bool *flagConfirmed1 = &chatroomListener->retentionHistoryTruncated[a2]; *flagConfirmed1 = false;
    MegaChatHandle *msgId1 = &chatroomListener->mRetentionMessageHandle[a2]; *msgId1 = MEGACHAT_INVALID_HANDLE;
    flagChatRetentionTime = &requestFlagsChat[a1][MegaChatRequest::TYPE_SET_RETENTION_TIME]; *flagChatRetentionTime = false;
    megaChatApi[a1]->setChatRetentionTime(chatid, 5);
    ASSERT_TRUE(waitForResponse(flagChatRetentionTime)) << "Timeout expired set chat retention time";
    ASSERT_EQ(lastErrorChat[a1], MegaChatError::ERROR_OK) << "Set retention time: Unexpected error. Error: " << lastErrorMsgChat[a1];
    ASSERT_TRUE(waitForResponse(retentionTimeChanged0)) << "Timeout expired for receiving chatroom update";
    ASSERT_TRUE(waitForResponse(retentionTimeChanged1)) << "Timeout expired for receiving chatroom update";
    ASSERT_TRUE(waitForResponse(mngMsgRecv)) << "Timeout expired for receiving management message";

    // Wait a considerable time period to ensure that retentionTime has been processed successfully
    std::this_thread::sleep_for(std::chrono::seconds(chatd::Client::kMinRetentionTimeout + 10));
    ASSERT_TRUE(waitForResponse(flagConfirmed0)) << "Retention history autotruncate hasn't been received for account " << (a1+1) << " after timeout: " << maxTimeout << " seconds";
    ASSERT_NE(*msgId0, MEGACHAT_INVALID_HANDLE) << "Wrong message id";
    ASSERT_TRUE(waitForResponse(flagConfirmed1)) << "Retention history autotruncate hasn't been received for account " << (a2+1) << " after timeout: " << maxTimeout << " seconds";
    ASSERT_NE(*msgId1, MEGACHAT_INVALID_HANDLE) << "Wrong message id";
    ASSERT_FALSE(loadHistory(a1, chatid, chatroomListener)) << "History should be empty after retention history autotruncate";

    // Disable retention time
    retentionTimeChanged0 = &chatroomListener->retentionTimeUpdated[a1]; *retentionTimeChanged0 = false;
    retentionTimeChanged1 = &chatroomListener->retentionTimeUpdated[a2]; *retentionTimeChanged1 = false;
    mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
    flagChatRetentionTime = &requestFlagsChat[a1][MegaChatRequest::TYPE_SET_RETENTION_TIME]; *flagChatRetentionTime = false;
    megaChatApi[a1]->setChatRetentionTime(chatid, 0);
    ASSERT_TRUE(waitForResponse(flagChatRetentionTime)) << "Timeout expired set chat retention time";
    ASSERT_EQ(lastErrorChat[a1], MegaChatError::ERROR_OK) << "Set retention time: Unexpected error. Error: " << lastErrorMsgChat[a1];
    ASSERT_TRUE(waitForResponse(retentionTimeChanged0)) << "Timeout expired for receiving chatroom update";
    ASSERT_TRUE(waitForResponse(retentionTimeChanged1)) << "Timeout expired for receiving chatroom update";
    ASSERT_TRUE(waitForResponse(mngMsgRecv)) << "Timeout expired for receiving management message";

    // Send 5 messages
    messageToSend = "Msg from " +account(a1).getEmail();
    for (int i = 0; i < 5; i++)
    {
        unique_ptr<MegaChatMessage> msg(sendTextMessageOrUpdate(a1, a2, chatid, messageToSend, chatroomListener));
        ASSERT_TRUE(msg);
    }

    // Close chatrooms
    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);
    delete chatroomListener;

    // Logout and login
    ASSERT_NO_FATAL_FAILURE({ logout(a1, true); });
    sessionPrimary.reset(login(a1));
    ASSERT_TRUE(sessionPrimary);
    ASSERT_NO_FATAL_FAILURE({ logout(a2, true); });
    sessionSecondary.reset(login(a2));
    ASSERT_TRUE(sessionSecondary);

    // Open chatroom
    chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a1+1);
    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a2+1);

    // Check history has 5 messages + setRetentionTime management message
    int count = loadHistory(a1, chatid, chatroomListener);
    ASSERT_TRUE(count == 6 || count == 7) << "Wrong count of messages: " << count;
    count = loadHistory(a2, chatid, chatroomListener);
    ASSERT_TRUE(count == 6 || count == 7) << "Wrong count of messages: " << count;

    // Close the chatrooms
    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);
    delete chatroomListener;
}

/**
 * @brief MegaChatApiTest.ChangeMyOwnName
 *
 * This test does the following:
 * - Get current name
 * - Change last name - it has been updated in memory and db.
 * - Get current name - value from memory
 * - Logout
 * - Login
 * - Get current name - value from db
 * - Change last name - set initial value for next tests execution
 *
 * Check if last name changed is the same at memory and at db
 */
TEST_F(MegaChatApiTest, ChangeMyOwnName)
{
    unsigned a1 = 0;

    char *sessionPrimary = login(a1);
    ASSERT_TRUE(sessionPrimary);
    std::string appendToLastName = "Test";

    std::string myAccountLastName;
    char *nameFromApi = megaChatApi[a1]->getMyLastname();
    if (nameFromApi)
    {
        myAccountLastName = nameFromApi;
        delete [] nameFromApi;
        nameFromApi = NULL;
    }

    std::string newLastName = myAccountLastName + appendToLastName;
    ASSERT_NO_FATAL_FAILURE({ changeLastName(a1, newLastName); });

    nameFromApi = megaChatApi[a1]->getMyLastname();
    std::string finalLastName;
    if (nameFromApi)
    {
        finalLastName = nameFromApi;
        delete [] nameFromApi;
        nameFromApi = NULL;
    }

    ASSERT_NO_FATAL_FAILURE({ logout(a1, false); });

    char *newSession = login(a1, sessionPrimary);
    ASSERT_TRUE(newSession);

    nameFromApi = megaChatApi[a1]->getMyLastname();
    std::string lastNameAfterLogout;
    if (nameFromApi)
    {
        lastNameAfterLogout = nameFromApi;
        delete [] nameFromApi;
        nameFromApi = NULL;
    }

    //Name comes back to old value.
    ASSERT_NO_FATAL_FAILURE({ changeLastName(a1, myAccountLastName); });

    ASSERT_EQ(newLastName, finalLastName) <<
                     "Failed to change fullname (checked from memory). Name established VS Name in memory";
    ASSERT_EQ(lastNameAfterLogout, finalLastName) <<
                     "Failed to change fullname (checked from DB) Name established VS Name in DB";

    delete [] sessionPrimary;
    sessionPrimary = NULL;

    delete [] newSession;
    newSession = NULL;
}

/**
 * @brief MegaChatApiTest.GetChatFilters
 *
 * This test does the following:
 *
 * - Test getChatListItems filters and masks results with previous interface (deprecated)
 * - Compares the completion of the results by complementary options (e.g. total non-archived
 * chats must be equal to non-archived reads + non-archived unread)
 *
 * Note: masks and filters values can be found at megachatapi.h documentation/comments
 */
TEST_F(MegaChatApiTest, GetChatFilters)
{
    static constexpr unsigned int accountIndex = 0;
    std::unique_ptr<char[]> session(login(accountIndex));

    const auto getLogTrace = [](const std::string& name, const auto& l) -> std::string
    {
        return std::string{name + ": " + std::to_string(l.size()) + " chats received\n"};
    };
    const auto equals = [](const auto& lhs, const auto& rhs) -> bool
    {
        if (!lhs && !rhs)               return true;
        if (!lhs || !rhs)               return false;
        if (lhs->size() != rhs->size()) return false;

        const auto s = lhs->size();
        for (unsigned int i = 0; i < s; ++i)
        {
            if (lhs->get(i)->getChatId() != rhs->get(i)->getChatId()) return false;
        }

        return true;
    };

    std::unique_ptr<MegaChatRoomList> chats(megaChatApi[accountIndex]->getChatRooms());
    postLog(getLogTrace("getChatRooms()", *chats));
    std::unique_ptr<MegaChatListItemList> allChats(megaChatApi[accountIndex]->getChatListItems(0, 0));
    postLog(getLogTrace("getChatListItems(0, 0)", *allChats));
    ASSERT_TRUE(equals(allChats, chats)) << "Filterless chat retrieval doesn't match";

    const auto getErrMsg = [](const std::string& name) -> std::string
    {
        return std::string {"Error " + name + " [deprecated] chats retrieval"};
    };

    std::unique_ptr<MegaChatListItemList> nonArchivedChatsDep(megaChatApi[accountIndex]->getChatListItems());
    postLog(getLogTrace("[deprecated] getChatListItems()", *nonArchivedChatsDep));
    std::unique_ptr<MegaChatListItemList> byTypeAllNADep(megaChatApi[accountIndex]->getChatListItemsByType(MegaChatApi::CHAT_TYPE_ALL));
    postLog(getLogTrace("[deprecated] getChatListItemsByType(CHAT_TYPE_ALL)", *byTypeAllNADep));
    std::unique_ptr<MegaChatListItemList> nonArchivedChats(
        megaChatApi[accountIndex]->getChatListItems(MegaChatApi::CHAT_FILTER_BY_ARCHIVED_OR_NON_ARCHIVED,
                                                    MegaChatApi::CHAT_GET_NON_ARCHIVED));
    static const std::string pref = "getChatListItems(";
    postLog(getLogTrace(pref + std::to_string(MegaChatApi::CHAT_FILTER_BY_ARCHIVED_OR_NON_ARCHIVED)
                        + ", "+ std::to_string(MegaChatApi::CHAT_GET_NON_ARCHIVED)
                        + ")", *nonArchivedChats));
    ASSERT_TRUE(equals(nonArchivedChatsDep, byTypeAllNADep)) << getErrMsg("all non-archived");
    ASSERT_TRUE(equals(nonArchivedChats, byTypeAllNADep)) << getErrMsg("byType(CHAT_TYPE_ALL)");

    std::unique_ptr<MegaChatListItemList> nonArchivedActiveChatsDep(megaChatApi[accountIndex]->getActiveChatListItems());
    postLog(getLogTrace("getActiveChatListItems()", *nonArchivedActiveChatsDep));
    std::unique_ptr<MegaChatListItemList> nonArchivedActiveChats(
        megaChatApi[accountIndex]->getChatListItems(MegaChatApi::CHAT_FILTER_BY_ARCHIVED_OR_NON_ARCHIVED
                                                    + MegaChatApi::CHAT_FILTER_BY_ACTIVE_OR_NON_ACTIVE
                                                    , MegaChatApi::CHAT_GET_NON_ARCHIVED
                                                    + MegaChatApi::CHAT_GET_ACTIVE));

    postLog(getLogTrace(pref + std::to_string(MegaChatApi::CHAT_FILTER_BY_ARCHIVED_OR_NON_ARCHIVED)
                        + "+"+ std::to_string(MegaChatApi::CHAT_FILTER_BY_ACTIVE_OR_NON_ACTIVE)
                        + ", " + std::to_string(MegaChatApi::CHAT_GET_NON_ARCHIVED)
                        + "+" + std::to_string(MegaChatApi::CHAT_GET_ACTIVE)
                        + ")", *nonArchivedActiveChats));
    ASSERT_TRUE(equals(nonArchivedActiveChats, nonArchivedActiveChatsDep)) << getErrMsg("non-archived active");

    std::unique_ptr<MegaChatListItemList> nonArchivedInactiveChatsDep(megaChatApi[accountIndex]-> getInactiveChatListItems());
    postLog(getLogTrace("getInactiveChatListItems()",*nonArchivedInactiveChatsDep));
    std::unique_ptr<MegaChatListItemList> nonArchivedInactiveChats(
        megaChatApi[accountIndex]->getChatListItems(MegaChatApi::CHAT_FILTER_BY_ARCHIVED_OR_NON_ARCHIVED
                                                    + MegaChatApi::CHAT_FILTER_BY_ACTIVE_OR_NON_ACTIVE
                                                    , MegaChatApi::CHAT_GET_NON_ARCHIVED
                                                    + MegaChatApi::CHAT_GET_NON_ACTIVE));

    postLog(getLogTrace(pref + std::to_string(MegaChatApi::CHAT_FILTER_BY_ARCHIVED_OR_NON_ARCHIVED)
                        + "+" + std::to_string(MegaChatApi::CHAT_FILTER_BY_ACTIVE_OR_NON_ACTIVE)
                        + ", " + std::to_string(MegaChatApi::CHAT_GET_NON_ARCHIVED)
                        + "+" + std::to_string(MegaChatApi::CHAT_GET_NON_ACTIVE)
                        + ")", *nonArchivedInactiveChats));
    ASSERT_TRUE(equals(nonArchivedInactiveChats, nonArchivedInactiveChatsDep)) << getErrMsg("non-archived inactive");
    ASSERT_EQ(nonArchivedInactiveChats->size() + nonArchivedActiveChats->size(), nonArchivedChats->size())
                     << "Incomplete set non-archived active/non-active";

    std::unique_ptr<MegaChatListItemList> archivedChatsDep(megaChatApi[accountIndex]->getArchivedChatListItems());
    postLog(getLogTrace("getArchivedChatListItems()", *archivedChatsDep));
    std::unique_ptr<MegaChatListItemList> archivedChats(
        megaChatApi[accountIndex]->getChatListItems(MegaChatApi::CHAT_FILTER_BY_ARCHIVED_OR_NON_ARCHIVED
                                                    , MegaChatApi::CHAT_GET_ARCHIVED));
    postLog(getLogTrace(pref + std::to_string(MegaChatApi::CHAT_FILTER_BY_ARCHIVED_OR_NON_ARCHIVED)
                        + ", " + std::to_string(MegaChatApi::CHAT_GET_ARCHIVED)
                        + ")", *archivedChats));
    ASSERT_TRUE(equals(archivedChatsDep, archivedChats)) << getErrMsg("archived");

    std::unique_ptr<MegaChatListItemList> nonArchivedUnreadChatsDep(megaChatApi[accountIndex]->getUnreadChatListItems());
    postLog(getLogTrace("getUnreadChatListItems()", *nonArchivedUnreadChatsDep));
    std::unique_ptr<MegaChatListItemList> nonArchivedUnreadChats(
        megaChatApi[accountIndex]->getChatListItems(MegaChatApi::CHAT_FILTER_BY_ARCHIVED_OR_NON_ARCHIVED
                                                    + MegaChatApi::CHAT_FILTER_BY_READ_OR_UNREAD
                                                    , MegaChatApi::CHAT_GET_NON_ARCHIVED
                                                    + MegaChatApi::CHAT_GET_UNREAD));
    postLog(getLogTrace(pref + std::to_string(MegaChatApi::CHAT_FILTER_BY_ARCHIVED_OR_NON_ARCHIVED)
                        + "+" + std::to_string(MegaChatApi::CHAT_FILTER_BY_READ_OR_UNREAD)
                        + ", " + std::to_string(MegaChatApi::CHAT_GET_NON_ARCHIVED)
                        + "+" + std::to_string(MegaChatApi::CHAT_GET_UNREAD)
                        + ")", *nonArchivedUnreadChats));
    ASSERT_TRUE(equals(nonArchivedUnreadChatsDep, nonArchivedUnreadChats)) << getErrMsg("non-archived unread");

    std::unique_ptr<MegaChatListItemList> nonArchivedReadChats(
        megaChatApi[accountIndex]->getChatListItems(MegaChatApi::CHAT_FILTER_BY_ARCHIVED_OR_NON_ARCHIVED
                                                    + MegaChatApi::CHAT_FILTER_BY_READ_OR_UNREAD
                                                    , MegaChatApi::CHAT_GET_NON_ARCHIVED
                                                    + MegaChatApi::CHAT_GET_READ));
    postLog(getLogTrace(pref + std::to_string(MegaChatApi::CHAT_FILTER_BY_ARCHIVED_OR_NON_ARCHIVED)
                        + "+" + std::to_string(MegaChatApi::CHAT_FILTER_BY_READ_OR_UNREAD)
                        + ", " + std::to_string(MegaChatApi::CHAT_GET_NON_ARCHIVED)
                        + "+" + std::to_string(MegaChatApi::CHAT_GET_READ)
                        + ")", *nonArchivedReadChats));
    ASSERT_EQ(nonArchivedReadChats->size() + nonArchivedUnreadChats->size(), nonArchivedChats->size())
        << "Error nonArchivedRead chats added to nonArchivedUnread don't equal nonArchived chats";

    std::unique_ptr<MegaChatListItemList> byTypeIndividualNADep(megaChatApi[accountIndex]->getChatListItemsByType(MegaChatApi::CHAT_TYPE_INDIVIDUAL));
    std::unique_ptr<MegaChatListItemList> nonArchivedIndividual(
        megaChatApi[accountIndex]->getChatListItems(MegaChatApi::CHAT_FILTER_BY_ARCHIVED_OR_NON_ARCHIVED
                                                    + MegaChatApi::CHAT_FILTER_BY_INDIVIDUAL_OR_GROUP
                                                    , MegaChatApi::CHAT_GET_NON_ARCHIVED
                                                    + MegaChatApi::CHAT_GET_INDIVIDUAL));
    postLog(getLogTrace(pref + std::to_string(MegaChatApi::CHAT_FILTER_BY_ARCHIVED_OR_NON_ARCHIVED)
                        + "+" + std::to_string(MegaChatApi::CHAT_FILTER_BY_INDIVIDUAL_OR_GROUP)
                        + ", " + std::to_string(MegaChatApi::CHAT_GET_NON_ARCHIVED)
                        + "+" + std::to_string(MegaChatApi::CHAT_GET_INDIVIDUAL)
                        + ")", *nonArchivedIndividual));
    ASSERT_TRUE(equals(byTypeIndividualNADep, nonArchivedIndividual)) << getErrMsg("byType(CHAT_TYPE_INDIVIDUAL)");

    std::unique_ptr<MegaChatListItemList> byTypeGroupNADep(megaChatApi[accountIndex]->getChatListItemsByType(MegaChatApi::CHAT_TYPE_GROUP));
    std::unique_ptr<MegaChatListItemList> nonArchivedGroups(
        megaChatApi[accountIndex]->getChatListItems(MegaChatApi::CHAT_FILTER_BY_ARCHIVED_OR_NON_ARCHIVED
                                                    + MegaChatApi::CHAT_FILTER_BY_INDIVIDUAL_OR_GROUP
                                                    , MegaChatApi::CHAT_GET_NON_ARCHIVED
                                                    + MegaChatApi::CHAT_GET_GROUP));
    postLog(getLogTrace(pref + std::to_string(MegaChatApi::CHAT_FILTER_BY_ARCHIVED_OR_NON_ARCHIVED)
                        + "+" + std::to_string(MegaChatApi::CHAT_FILTER_BY_INDIVIDUAL_OR_GROUP)
                        + ", " + std::to_string(MegaChatApi::CHAT_GET_NON_ARCHIVED)
                        + "+" + std::to_string(MegaChatApi::CHAT_GET_GROUP)
                        + ")",  *nonArchivedGroups));
    ASSERT_TRUE(equals(byTypeGroupNADep, nonArchivedGroups)) << getErrMsg("byType(CHAT_TYPE_GROUP)");

    std::unique_ptr<MegaChatListItemList> byTypePrivateNADep(megaChatApi[accountIndex]->getChatListItemsByType(MegaChatApi::CHAT_TYPE_GROUP_PRIVATE));
    std::unique_ptr<MegaChatListItemList> nonArchivedPrivate(
        megaChatApi[accountIndex]->getChatListItems(MegaChatApi::CHAT_FILTER_BY_ARCHIVED_OR_NON_ARCHIVED
                                                    + MegaChatApi::CHAT_FILTER_BY_PUBLIC_OR_PRIVATE
                                                    , MegaChatApi::CHAT_GET_NON_ARCHIVED
                                                    + MegaChatApi::CHAT_GET_PRIVATE));
    postLog(getLogTrace(pref + std::to_string(MegaChatApi::CHAT_FILTER_BY_ARCHIVED_OR_NON_ARCHIVED)
                        + "+" + std::to_string(MegaChatApi::CHAT_FILTER_BY_PUBLIC_OR_PRIVATE)
                        + ", " + std::to_string(MegaChatApi::CHAT_GET_NON_ARCHIVED)
                        + "+" + std::to_string(MegaChatApi::CHAT_GET_PRIVATE)
                        + ")",  *nonArchivedPrivate));
    ASSERT_TRUE(equals(byTypePrivateNADep, nonArchivedPrivate)) << getErrMsg("byType(CHAT_TYPE_PRIVATE)");

    std::unique_ptr<MegaChatListItemList> byTypePublicNADep(megaChatApi[accountIndex]->getChatListItemsByType(MegaChatApi::CHAT_TYPE_GROUP_PUBLIC));
    std::unique_ptr<MegaChatListItemList> nonArchivedPublic(
        megaChatApi[accountIndex]->getChatListItems(MegaChatApi::CHAT_FILTER_BY_ARCHIVED_OR_NON_ARCHIVED
                                                    + MegaChatApi::CHAT_FILTER_BY_PUBLIC_OR_PRIVATE
                                                    , MegaChatApi::CHAT_GET_NON_ARCHIVED
                                                    + MegaChatApi::CHAT_GET_PUBLIC));
    postLog(getLogTrace(pref + std::to_string(MegaChatApi::CHAT_FILTER_BY_ARCHIVED_OR_NON_ARCHIVED)
                        + "+" + std::to_string(MegaChatApi::CHAT_FILTER_BY_PUBLIC_OR_PRIVATE)
                        + ", " + std::to_string(MegaChatApi::CHAT_GET_NON_ARCHIVED)
                        + "+" + std::to_string(MegaChatApi::CHAT_GET_PUBLIC)
                        + ")",  *nonArchivedPublic));
    ASSERT_TRUE(equals(byTypePublicNADep, nonArchivedPublic)) << getErrMsg("byType(CHAT_TYPE_PUBLIC)");

    std::unique_ptr<MegaChatListItemList> byTypeMeetingNADep(megaChatApi[accountIndex]->getChatListItemsByType(MegaChatApi::CHAT_TYPE_MEETING_ROOM));
    std::unique_ptr<MegaChatListItemList> nonArchivedMeeting(
        megaChatApi[accountIndex]->getChatListItems(MegaChatApi::CHAT_FILTER_BY_ARCHIVED_OR_NON_ARCHIVED
                                                    + MegaChatApi::CHAT_FILTER_BY_MEETING_OR_NON_MEETING
                                                    , MegaChatApi::CHAT_GET_NON_ARCHIVED
                                                    + MegaChatApi::CHAT_GET_MEETING));
    postLog(getLogTrace(pref + std::to_string(MegaChatApi::CHAT_FILTER_BY_ARCHIVED_OR_NON_ARCHIVED)
                        + "+" + std::to_string(MegaChatApi::CHAT_FILTER_BY_MEETING_OR_NON_MEETING)
                        + ", " + std::to_string(MegaChatApi::CHAT_GET_NON_ARCHIVED)
                        + "+" + std::to_string(MegaChatApi::CHAT_GET_MEETING)
                        + ")", *nonArchivedMeeting));
    ASSERT_TRUE(equals(byTypeMeetingNADep, nonArchivedMeeting)) << getErrMsg("byType(CHAT_TYPE_MEETING_ROOM)");

    std::unique_ptr<MegaChatListItemList> byTypeNonMeetingNADep(megaChatApi[accountIndex]->getChatListItemsByType(MegaChatApi::CHAT_TYPE_NON_MEETING));
    std::unique_ptr<MegaChatListItemList> nonArchivedNonMeeting(
        megaChatApi[accountIndex]->getChatListItems(MegaChatApi::CHAT_FILTER_BY_ARCHIVED_OR_NON_ARCHIVED
                                                    + MegaChatApi::CHAT_FILTER_BY_MEETING_OR_NON_MEETING
                                                    , MegaChatApi::CHAT_GET_NON_ARCHIVED
                                                    + MegaChatApi::CHAT_GET_NON_MEETING));
    postLog(getLogTrace(pref + std::to_string(MegaChatApi::CHAT_FILTER_BY_ARCHIVED_OR_NON_ARCHIVED)
                        + "+" + std::to_string(MegaChatApi::CHAT_FILTER_BY_MEETING_OR_NON_MEETING)
                        + ", " + std::to_string(MegaChatApi::CHAT_GET_NON_ARCHIVED)
                        + "+" + std::to_string(MegaChatApi::CHAT_GET_NON_MEETING)
                        + ")", *nonArchivedNonMeeting));
    ASSERT_TRUE(equals(byTypeNonMeetingNADep, nonArchivedNonMeeting)) << getErrMsg("byType(CHAT_TYPE_NON_MEETING)");
}

#ifndef KARERE_DISABLE_WEBRTC
/**
 * @brief MegaChatApiTest.Calls
 *
 * Requirements:
 *      - Both accounts should be conctacts
 * (if not accomplished, the test automatically solves them)
 *
 * This test does the following:
 * - A calls B
 * - B rejects the call
 *
 * - A calls B
 * - A cancels the call before B answers
 *
 * - B logouts
 * - A calls B
 * - B logins
 * - B rejects the call
 *
 * - A calls B
 * - B doesn't answer the call
 *
 */
TEST_F(MegaChatApiTest, Calls)
{
    unsigned a1 = 0;
    unsigned a2 = 1;

    char *primarySession = login(a1);
    ASSERT_TRUE(primarySession);
    char *secondarySession = login(a2);
    ASSERT_TRUE(secondarySession);

    MegaUser *user = megaApi[a1]->getContact(account(a2).getEmail().c_str());
    if (!user || user->getVisibility() != MegaUser::VISIBILITY_VISIBLE)
    {
        ASSERT_NO_FATAL_FAILURE({ makeContact(a1, a2); });
    }
    delete user;

    MegaChatHandle chatid = getPeerToPeerChatRoom(a1, a2);
    ASSERT_NE(chatid, MEGACHAT_INVALID_HANDLE);

    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);

    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account 1";
    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account 2";

    loadHistory(a1, chatid, chatroomListener);
    loadHistory(a2, chatid, chatroomListener);

    mLocalVideoListener[a1] = new TestChatVideoListener();
    mLocalVideoListener[a2] = new TestChatVideoListener();
    megaChatApi[a1]->addChatLocalVideoListener(chatid, mLocalVideoListener[a1]);
    megaChatApi[a2]->addChatLocalVideoListener(chatid, mLocalVideoListener[a2]);
    // Remote video listener aren't necessary because call is never ging to be answered at tests

    // A calls B and B hangs up the call
    bool *callInProgress = &mCallInProgress[a1]; *callInProgress = false;
    bool *flagStartCall = &requestFlagsChat[a1][MegaChatRequest::TYPE_START_CHAT_CALL]; *flagStartCall = false;
    bool *callReceivedRinging = &mCallReceivedRinging[a2]; *callReceivedRinging = false;
    mCallIdExpectedReceived[a2] = MEGACHAT_INVALID_HANDLE;
    mChatIdRingInCall[a2] = MEGACHAT_INVALID_HANDLE;
    bool *callDestroyed0 = &mCallDestroyed[a1]; *callDestroyed0 = false;
    bool *callDestroyed1 = &mCallDestroyed[a2]; *callDestroyed1 = false;
    int *termCode0 = &mTerminationCode[a1]; *termCode0 = 0;
    int *termCode1 = &mTerminationCode[a2]; *termCode1 = 0;
    bool *flagHangUpCall = &requestFlagsChat[a2][MegaChatRequest::TYPE_HANG_CHAT_CALL]; *flagHangUpCall = false;
    mCallIdRingIn[a2] = MEGACHAT_INVALID_HANDLE;
    mCallIdJoining[a1] = MEGACHAT_INVALID_HANDLE;
    megaChatApi[a1]->startChatCall(chatid, false, false);
    ASSERT_TRUE(waitForResponse(flagStartCall)) << "Timeout after start chat call " << maxTimeout << " seconds";
    ASSERT_TRUE(!lastErrorChat[a1]) << "Failed to start chat call: " << lastErrorChat[a1];
    ASSERT_TRUE(waitForResponse(callInProgress)) << "Timeout expired for receiving a call";
    unique_ptr<MegaChatCall> auxCall(megaChatApi[a1]->getChatCall(mChatIdInProgressCall[a1]));
    if (auxCall)
    {
        // set the callid that we expect to for account B in onChatCallUpdate
        mCallIdExpectedReceived[a2] = auxCall->getCallId();
    }

    ASSERT_TRUE(waitForResponse(callReceivedRinging)) << "Timeout expired for receiving a call";
    ASSERT_EQ(mChatIdRingInCall[a2], chatid) << "Incorrect chat id at call receptor";
    ASSERT_EQ(mCallIdJoining[a1], mCallIdRingIn[a2]) << "Differents call id between caller and answer";
    MegaChatCall *call = megaChatApi[a2]->getChatCall(chatid);
    delete call;

    sleep(5);

    megaChatApi[a2]->hangChatCall(mCallIdRingIn[a2]);
    ASSERT_TRUE(waitForResponse(flagHangUpCall)) << "Timeout after hang up chat call " << maxTimeout << " seconds";
    ASSERT_TRUE(!lastErrorChat[a2]) << "Failed to hang up chat call: " << lastErrorChat[a2];
    ASSERT_TRUE(waitForResponse(callDestroyed0)) << "The call has to be finished account 1";
    ASSERT_TRUE(waitForResponse(callDestroyed1)) << "The call has to be finished account 2";


    // A calls B and A hangs up the call before B answers
    flagStartCall = &requestFlagsChat[a1][MegaChatRequest::TYPE_START_CHAT_CALL]; *flagStartCall = false;
    callInProgress = &mCallInProgress[a1]; *callInProgress = false;
    callReceivedRinging = &mCallReceivedRinging[a2]; *callReceivedRinging = false;
    mCallIdExpectedReceived[a2] = MEGACHAT_INVALID_HANDLE;
    mChatIdRingInCall[a2] = MEGACHAT_INVALID_HANDLE;
    callDestroyed0 = &mCallDestroyed[a1]; *callDestroyed0 = false;
    callDestroyed1 = &mCallDestroyed[a2]; *callDestroyed1 = false;
    termCode0 = &mTerminationCode[a1]; *termCode0 = 0;
    termCode1 = &mTerminationCode[a2]; *termCode1 = 0;
    flagHangUpCall = &requestFlagsChat[a1][MegaChatRequest::TYPE_HANG_CHAT_CALL]; *flagHangUpCall = false;
    mCallIdRingIn[a2] = MEGACHAT_INVALID_HANDLE;
    mCallIdJoining[a1] = MEGACHAT_INVALID_HANDLE;
    megaChatApi[a1]->startChatCall(chatid, false, false);
    ASSERT_TRUE(waitForResponse(flagStartCall)) << "Timeout after start chat call " << maxTimeout << " seconds";
    ASSERT_TRUE(!lastErrorChat[a1]) << "Failed to start chat call: " << lastErrorChat[a1];
    ASSERT_TRUE(waitForResponse(callInProgress)) << "Timeout expired for receiving a call";
    auxCall.reset(megaChatApi[a1]->getChatCall(mChatIdInProgressCall[a1]));
    if (auxCall)
    {
        // set the callid that we expect to for account B in onChatCallUpdate
        mCallIdExpectedReceived[a2] = auxCall->getCallId();
    }

    ASSERT_TRUE(waitForResponse(callReceivedRinging)) << "Timeout expired for receiving a call";
    ASSERT_EQ(mChatIdRingInCall[a2], chatid) << "Incorrect chat id at call receptor";
    ASSERT_EQ(mCallIdJoining[a1], mCallIdRingIn[a2]) << "Differents call id between caller and answer";
    call = megaChatApi[a2]->getChatCall(chatid);
    delete call;

    sleep(5);

    megaChatApi[a1]->hangChatCall(mCallIdJoining[a1]);
    ASSERT_TRUE(waitForResponse(flagHangUpCall)) << "Timeout after hang up chat call " << maxTimeout << " seconds";
    ASSERT_TRUE(!lastErrorChat[a1]) << "Failed to hang up chat call: " << lastErrorChat[a1];
    ASSERT_TRUE(waitForResponse(callDestroyed0)) << "The call has to be finished account 1";
    ASSERT_TRUE(waitForResponse(callDestroyed1)) << "The call has to be finished account 2";

    // A calls B(B is logged out), B logins, B receives the call and B hangs up the call
    ASSERT_NO_FATAL_FAILURE({ logout(a2); });
    callInProgress = &mCallInProgress[a1]; *callInProgress = false;
    flagStartCall = &requestFlagsChat[a1][MegaChatRequest::TYPE_START_CHAT_CALL]; *flagStartCall = false;
    bool *callReceived = &mCallReceived[a2]; *callReceived = false;
    callReceivedRinging = &mCallReceivedRinging[a2]; *callReceivedRinging = false;
    mChatIdRingInCall[a2] = MEGACHAT_INVALID_HANDLE;
    mCallIdExpectedReceived[a2] = MEGACHAT_INVALID_HANDLE;
    callDestroyed0 = &mCallDestroyed[a1]; *callDestroyed0 = false;
    callDestroyed1 = &mCallDestroyed[a2]; *callDestroyed1 = false;
    termCode0 = &mTerminationCode[a1]; *termCode0 = 0;
    termCode1 = &mTerminationCode[a2]; *termCode1 = 0;
    flagHangUpCall = &requestFlagsChat[a2][MegaChatRequest::TYPE_HANG_CHAT_CALL]; *flagHangUpCall = false;
    mCallIdRingIn[a2] = MEGACHAT_INVALID_HANDLE;
    mCallIdJoining[a1] = MEGACHAT_INVALID_HANDLE;

    megaChatApi[a1]->startChatCall(chatid, false, false);
    ASSERT_TRUE(waitForResponse(flagStartCall)) << "Timeout after start chat call " << maxTimeout << " seconds";
    ASSERT_TRUE(!lastErrorChat[a1]) << "Failed to start chat call: " << lastErrorChat[a1];
    ASSERT_TRUE(waitForResponse(callInProgress)) << "Timeout expired for receiving a call";
    auxCall.reset(megaChatApi[a1]->getChatCall(mChatIdInProgressCall[a1]));
    if (auxCall)
    {
        // set the callid that we expect to for account B in onChatCallUpdate
        mCallIdExpectedReceived[a2] = auxCall->getCallId();
    }

    char* secondarySession2 = login(a2, secondarySession);
    ASSERT_TRUE(secondarySession2);
    waitForResponse(callReceived);
    if (!(*callReceived))
    {
        std::unique_ptr<MegaChatListItem> itemSecondary(megaChatApi[a2]->getChatListItem(chatid));
        ASSERT_TRUE(itemSecondary) << "Can't retrieve chat list item";

        if (itemSecondary->getLastMessageType() == MegaChatMessage::TYPE_CALL_ENDED)
        {
            // login process for secondary account has taken too much time, so Call has been destroyed with reason kNoAnswer
            std::unique_ptr<MegaChatMessage> m(megaChatApi[a2]->getMessage(chatid, itemSecondary->getLastMessageId()));
            ASSERT_TRUE(m && m->getHandleOfAction() == mCallIdExpectedReceived[a2]) << "Management message of type TYPE_CALL_ENDED not received";
        }
        else
        {
            // the test must fail, as we haven't received the call for secondary account, but neither a call ended management message
            ASSERT_TRUE(callReceived) << "Timeout expired for receiving a call";
        }
    }

    auxCall.reset(megaChatApi[a2]->getChatCall(auxCall->getChatid()));
    ASSERT_TRUE(auxCall) << "Can't retrieve call by callid";

    MegaChatHandle ringingCallId = mCallIdRingIn[a2];
    // This scenario B (loging in and connect to SFU) could take enough time to receive a CALLSTATE with Ringing 0
    if (auxCall->getCallId() == ringingCallId)
    {
        // just perform following actions in case that call is still ringing
        ASSERT_EQ(mChatIdRingInCall[a2], chatid) << "Incorrect chat id at call receptor";
        ASSERT_EQ(mCallIdJoining[a1], ringingCallId) << "Differents call id between caller and answer";
    }

    sleep(5);
    if (auxCall->isRinging())
    {
        /* call hangChatCall just in case it's still ringing, otherwise mcme command won't be sent, and
         * a1 won't receive onChatCallUpdate (MegaChatCall::CALL_STATUS_DESTROYED), so callDestroyed0 won't bet set true
         *
         * Note: when TYPE_HANG_CHAT_CALL request is processed, call could stop ringing, so in that case, test will fail.
         * this is a very corner case, as call must stop ringing in the period between the ringing checkup above and the process
         * of CALL_STATUS_DESTROYED request.
        */
        flagHangUpCall = &requestFlagsChat[a2][MegaChatRequest::TYPE_HANG_CHAT_CALL]; *flagHangUpCall = false;
        megaChatApi[a2]->hangChatCall(ringingCallId);
        ASSERT_TRUE(waitForResponse(flagHangUpCall)) << "Timeout after hang up chat call " << maxTimeout << " seconds";
        ASSERT_TRUE(!lastErrorChat[a2]) << "Failed to hang up chat call: " << lastErrorChat[a2];

        // in case of any of these asserts fails, logs can be checked to find a CALLSTATE with a ringing state change
        ASSERT_TRUE(waitForResponse(callDestroyed0)) << "call not finished for account 1 (possible corner case where call stops ringing before request is processed)";
        ASSERT_TRUE(waitForResponse(callDestroyed1)) << "call not finished for account 2 (possible corner case where call stops ringing before request is processed)";
    }

    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);

    megaChatApi[a1]->removeChatLocalVideoListener(chatid, mLocalVideoListener[a1]);
    megaChatApi[a2]->removeChatLocalVideoListener(chatid, mLocalVideoListener[a2]);

    delete chatroomListener;
    chatroomListener = NULL;

    delete [] primarySession;
    primarySession = NULL;
    delete [] secondarySession;
    secondarySession = NULL;

    delete mLocalVideoListener[a1];
    mLocalVideoListener[a1] = NULL;

    delete mLocalVideoListener[a2];
    mLocalVideoListener[a2] = NULL;

    delete [] primarySession;
    primarySession = NULL;
    delete [] secondarySession;
    secondarySession = NULL;
    delete [] secondarySession2;
    secondarySession2 = NULL;
}

/**
 * @brief MegaChatApiTest.ManualCalls
 *
 * Requirements:
 *      - Both accounts should be conctacts
 * (if not accomplished, the test automatically solves them)
 *
 * This test does the following:
 * - A calls B
 * - B in other client has to answer the call (manual)
 * - A mutes call
 * - A disables video
 * - A unmutes call
 * - A enables video
 * - A finishes the call
 *
 * - A waits for B call
 * - B calls A from other client (manual)
 * - A mutes call
 * - A disables video
 * - A unmutes call
 * - A enables video
 * - A finishes the call
 *
 */
TEST_F(MegaChatApiTest, DISABLED_ManualCalls)
{
    unsigned a1 = 0;
    unsigned a2 = 1;

    char *primarySession = login(a1);
    ASSERT_TRUE(primarySession);
    char *secondarySession = login(a2);
    ASSERT_TRUE(secondarySession);

    MegaUser *user = megaApi[a1]->getContact(account(a2).getEmail().c_str());
    if (!user || user->getVisibility() != MegaUser::VISIBILITY_VISIBLE)
    {
        ASSERT_NO_FATAL_FAILURE({ makeContact(a1, a2); });
    }
    delete user;

    MegaChatHandle chatid = getPeerToPeerChatRoom(a1, a2);
    ASSERT_NE(chatid, MEGACHAT_INVALID_HANDLE);
    ASSERT_EQ(megaChatApi[a1]->getChatConnectionState(chatid), MegaChatApi::CHAT_CONNECTION_ONLINE) <<
                     "Not connected to chatd for account " << (a1+1) << ": " << account(a1).getEmail();

    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);

    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account 1";
    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account 2";

    loadHistory(a1, chatid, chatroomListener);
    loadHistory(a2, chatid, chatroomListener);
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);
    ASSERT_NO_FATAL_FAILURE({ logout(a2); });

    TestChatVideoListener localVideoListener;

    megaChatApi[a1]->addChatLocalVideoListener(chatid, &localVideoListener);

    // Manual Test
    // Emit call
    bool *flagRequest = &requestFlagsChat[a1][MegaChatRequest::TYPE_START_CHAT_CALL]; *flagRequest = false;
    std::cerr << "Start Call" << std::endl;
    megaChatApi[a1]->startChatCall(chatid, true);
    ASSERT_TRUE(waitForResponse(flagRequest)) << "Timeout after start chat call " << maxTimeout << " seconds";
    ASSERT_TRUE(!lastErrorChat[a1]) << "Failed to start chat call: " << lastErrorChat[a1];
    bool *callInProgress = &mCallInProgress[a1]; *callInProgress = false;
    ASSERT_TRUE(waitForResponse(callInProgress)) << "Timeout expired for receiving a call";
    sleep(5);
    std::cerr << "Mute Call" << std::endl;
    megaChatApi[a1]->disableAudio(mChatIdInProgressCall[a1]);
    sleep(5);
    std::cerr << "Disable Video" << std::endl;
    megaChatApi[a1]->disableVideo(mChatIdInProgressCall[a1]);
    sleep(5);
    std::cerr << "Unmute Call" << std::endl;
    megaChatApi[a1]->enableAudio(mChatIdInProgressCall[a1]);
    sleep(5);
    std::cerr << "Enable Video" << std::endl;
    megaChatApi[a1]->enableVideo(mChatIdInProgressCall[a1]);

    MegaChatCall *chatCall = megaChatApi[a1]->getChatCall(mChatIdInProgressCall[a1]);
    ASSERT_TRUE(chatCall) << "Invalid chat call at getChatCallByChatId";

    MegaChatCall *chatCall2 = megaChatApi[a1]->getChatCallByCallId(chatCall->getCallId());
    ASSERT_TRUE(chatCall2) << "Invalid chat call at getChatCall";


    bool *callDestroyed= &mCallDestroyed[a1]; *callDestroyed = false;
    sleep(5);
    std::cerr << "Finish Call" << std::endl;
    sleep(2);
    megaChatApi[a1]->hangChatCall(chatCall->getCallId());
    std::cout << "Call finished." << std::endl;

    ASSERT_TRUE(waitForResponse(callDestroyed)) << "The call has to be finished";
    megaChatApi[a1]->removeChatLocalVideoListener(chatid, &localVideoListener);

    // Receive call
    std::cout << "Ready to receive calls..." << std::endl;
    bool *callReceivedRinging = &mCallReceivedRinging[a1]; *callReceivedRinging = false;
    mChatIdRingInCall[a1] = MEGACHAT_INVALID_HANDLE;
    ASSERT_TRUE(waitForResponse(callReceivedRinging)) << "Timeout expired for receiving a call";
    ASSERT_NE(mChatIdRingInCall[a1], MEGACHAT_INVALID_HANDLE) << "Invalid Chatid from call emisor";
    megaChatApi[a1]->answerChatCall(mChatIdRingInCall[a1], true);
    megaChatApi[a1]->addChatLocalVideoListener(chatid, &localVideoListener);

    sleep(5);
    std::cerr << "Mute Call" << std::endl;
    megaChatApi[a1]->disableAudio(chatCall->getCallId());
    sleep(5);
    std::cerr << "Disable Video" << std::endl;
    megaChatApi[a1]->disableVideo(chatCall->getCallId());
    sleep(5);
    std::cerr << "Unmute Call" << std::endl;
    megaChatApi[a1]->enableAudio(chatCall->getCallId());
    sleep(5);
    std::cerr << "Enable Video" << std::endl;
    megaChatApi[a1]->enableVideo(chatCall->getCallId());

    sleep(10);
    std::cerr << "Finish Call" << std::endl;
    sleep(2);
    megaChatApi[a1]->hangChatCall(mChatIdInProgressCall[a1]);
    std::cout << "Call finished." << std::endl;
    sleep(5);

    megaChatApi[a1]->removeChatLocalVideoListener(chatid, &localVideoListener);

    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);

    delete chatroomListener;
    chatroomListener = NULL;

    delete [] primarySession;
    primarySession = NULL;
    delete [] secondarySession;
    secondarySession = NULL;
}

/**
 * @brief MegaChatApiTest.ManualGroupCalls
 *
 * Requirements:
 *      - Both accounts should be conctacts
 * (if not accomplished, the test automatically solves them)
 *
 * This test does the following:
 * - A looks for the group chat
 * - A starts a call in that group chat
 * - A waits for call was established correctly
 * - A hangs up the call
 *
 * + A waits to receive a incoming call
 * - A answers it
 * - A hangs the call
 */
TEST_F(MegaChatApiTest, DISABLED_ManualGroupCalls)
{
    unsigned a1 = 0;
    std::string chatRoomName("name_of_groupchat");

    char *primarySession = login(a1);
    ASSERT_TRUE(primarySession);
    megachat::MegaChatRoomList *chatRoomList = megaChatApi[a1]->getChatRooms();

    MegaChatHandle chatid = MEGACHAT_INVALID_HANDLE;
    for (unsigned int i = 0; i < chatRoomList->size(); i++)
    {
        const MegaChatRoom *chatRoom = chatRoomList->get(i);
        if (chatRoomName == std::string(chatRoom->getTitle()))
        {
            chatid = chatRoom->getChatId();
            break;
        }
    }

    delete chatRoomList;

    ASSERT_NE(chatid, MEGACHAT_INVALID_HANDLE) << "Chat with title: " << chatRoomName << " not found.";
    ASSERT_EQ(megaChatApi[a1]->getChatConnectionState(chatid), MegaChatApi::CHAT_CONNECTION_ONLINE) <<
                     "Not connected to chatd for account " << (a1+1) << ": " << account(a1).getEmail();

    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);

    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account 1";

    loadHistory(a1, chatid, chatroomListener);

    TestChatVideoListener localVideoListener;
    megaChatApi[a1]->addChatLocalVideoListener(chatid, &localVideoListener);

    // ---- MANUAL TEST ----

    // Start call

    bool *flagRequest = &requestFlagsChat[a1][MegaChatRequest::TYPE_START_CHAT_CALL]; *flagRequest = false;
    std::cerr << "Start Call" << std::endl;
    megaChatApi[a1]->startChatCall(chatid, true);
    ASSERT_TRUE(waitForResponse(flagRequest)) << "Timeout after start chat call " << maxTimeout << " seconds";
    ASSERT_TRUE(!lastErrorChat[a1]) << "Failed to start chat call: " << lastErrorChat[a1];
    bool *callInProgress = &mCallInProgress[a1]; *callInProgress = false;
    ASSERT_TRUE(waitForResponse(callInProgress)) << "Timeout expired for receiving a call";

    std::cout << "Waiting for the other peer to answer the call..." << std::endl;
    sleep(60);

    MegaChatCall *chatCall = megaChatApi[a1]->getChatCall(mChatIdInProgressCall[a1]);
    ASSERT_TRUE(chatCall) << "Invalid chat call at getChatCall (by chatid)";

    MegaChatCall *chatCall2 = megaChatApi[a1]->getChatCallByCallId(chatCall->getCallId());
    ASSERT_TRUE(chatCall2) << "Invalid chat call at getChatCall (by callid)";

    delete chatCall;    chatCall = NULL;
    delete chatCall2;   chatCall2 = NULL;

    bool *callDestroyed= &mCallDestroyed[a1]; *callDestroyed = false;
    std::cerr << "Finish Call" << std::endl;
    megaChatApi[a1]->hangChatCall(mChatIdInProgressCall[a1]);
    std::cout << "Call finished." << std::endl;
    ASSERT_TRUE(waitForResponse(callDestroyed)) << "The call must be already finished and it is not";
    megaChatApi[a1]->removeChatLocalVideoListener(chatid, &localVideoListener);

    // Receive call

    std::cout << "Waiting for the other peer to start a call..." << std::endl;
    sleep(20);

    std::cout << "Ready to receive calls..." << std::endl;
    bool *callReceived = &mCallReceivedRinging[a1]; *callReceived = false;
    mChatIdRingInCall[a1] = MEGACHAT_INVALID_HANDLE;
    ASSERT_TRUE(waitForResponse(callReceived)) << "Timeout expired for receiving a call";
    ASSERT_NE(mChatIdRingInCall[a1], MEGACHAT_INVALID_HANDLE) << "Invalid Chatid from call emisor";
    megaChatApi[a1]->answerChatCall(mChatIdRingInCall[a1], true);
    megaChatApi[a1]->addChatLocalVideoListener(chatid, &localVideoListener);

    sleep(40);  // wait to receive some traffic
    megaChatApi[a1]->hangChatCall(mChatIdInProgressCall[a1]);
    std::cout << "Call finished." << std::endl;
    megaChatApi[a1]->removeChatLocalVideoListener(chatid, &localVideoListener);

    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);

    delete chatroomListener;
    chatroomListener = NULL;

    delete [] primarySession;
    primarySession = NULL;
}

/**
 * @brief MegaChatApiTest.EstablishedCalls
 *
 * Requirements:
 *      - Both accounts should be conctacts
 * (if not accomplished, the test automatically solves them)
 *
 * This test does the following:
 * + A starts a groupal Meeting in chat1 (without audio nor video)
 * - B answers call (without audio nor video)
 * - B puts call in hold on
 * + A puts call in hold on
 * + A releases hold on
 * - B releases hold on
 * - B enables audio monitor
 * - B disables audio monitor
 * + A force reconnect => retryPendingConnections(true)
 * - B hangs up call
 * + A hangs up call
 */
TEST_F(MegaChatApiTest, EstablishedCalls)
{
    unsigned a1 = 0;
    unsigned a2 = 1;

    /* lambda functions to simplify some recurrent operations */
    // gets a pointer to the local flag that indicates if we have reached an specific callstate
    std::function<bool*(unsigned int, int)> getChatCallStateFlag =
    [this](unsigned int index, int state) -> bool*
    {
        switch (state)
        {
            case MegaChatCall::CALL_STATUS_INITIAL:     return &mCallReceived[index];
            case MegaChatCall::CALL_STATUS_CONNECTING:  return &mCallConnecting[index];
            case MegaChatCall::CALL_STATUS_IN_PROGRESS: return &mCallInProgress[index];
            default:                                    break;
        }

        ADD_FAILURE() << "Invalid account index";
        return nullptr;
    };

    // resets the local flag that indicates if we have reached an specific call state
    std::function<void(unsigned int, int)> resetTestChatCallState =
    [getChatCallStateFlag](unsigned int index, int state)
    {
        bool* statusReceived = getChatCallStateFlag(index, state);
        if (statusReceived)    { *statusReceived = false; }
    };

    // waits for a specific callstate
    std::function<void(unsigned int, int)> waitForChatCallState =
    [this, getChatCallStateFlag](unsigned int index, int state)
    {
        bool* statusReceived = getChatCallStateFlag(index, state);
        if (statusReceived)
        {
            ASSERT_TRUE(waitForResponse(statusReceived)) <<
                             "Timeout expired for receiving call state: " << state <<
                             " for account index [" << index << "]";
        }
    };

    // ensures that <action> is executed successfully before maxAttempts and before timeout expires
    // if call gets disconnected before action is executed, command queue will be cleared, so we need to wait
    // until performer account is connected (CALL_STATUS_IN_PROGRESS) to SFU for that call and re-try <action>
    std::function<void(unsigned int, int, bool*, const char *, unsigned int, std::function<void()>)> waitForCallAction =
    [this, &resetTestChatCallState, &getChatCallStateFlag, &waitForChatCallState]
    (unsigned int pIdx, int maxAttempts, bool* exitFlag,  const char* errMsg, unsigned int timeout, std::function<void()>action)
    {
        int retries = 0;
        std::string errStr = errMsg ? errMsg : "executing provided action";
        bool* callConnecting = getChatCallStateFlag(pIdx, MegaChatCall::CALL_STATUS_CONNECTING);
        while (!*exitFlag)
        {
            ASSERT_TRUE(action) << "waitForCallAction: no valid action provided";

            // reset call state flags to false before executing the required action
            resetTestChatCallState(pIdx, MegaChatCall::CALL_STATUS_CONNECTING);
            resetTestChatCallState(pIdx, MegaChatCall::CALL_STATUS_IN_PROGRESS);

            // execute custom user action and wait until exitFlag is set true, OR performer account gets disconnected from SFU for the target call
            action();
            ASSERT_TRUE(waitForMultiResponse(std::vector<bool *> { exitFlag, callConnecting }, false /*waitForAll*/, timeout)) << "Timeout expired for " << errStr;

            // if performer account gets disconnected from SFU for the target call, wait until reconnect and retry <action>
            if (*callConnecting)
            {
               ASSERT_LT(++retries, maxAttempts) << "Max attempts exceeded for " << errStr;
               waitForChatCallState(pIdx, MegaChatCall::CALL_STATUS_IN_PROGRESS);
            }
        }
    };

    std::function<void()> action = nullptr;
    bool* exitFlag = nullptr;

    // Prepare users, and chat room
    std::unique_ptr<char[]> primarySession(login(a1));   // user A
    ASSERT_TRUE(primarySession);
    std::unique_ptr<char[]> secondarySession(login(a2)); // user B
    ASSERT_TRUE(secondarySession);

    std::unique_ptr<MegaUser> user(megaApi[a1]->getContact(account(a2).getEmail().c_str()));
    if (!user || user->getVisibility() != MegaUser::VISIBILITY_VISIBLE)
    {
        ASSERT_NO_FATAL_FAILURE({ makeContact(a1, a2); });
    }
    // Get a group chatroom with both users
    MegaChatHandle uh = user->getHandle();
    std::unique_ptr<MegaChatPeerList> peers(MegaChatPeerList::createInstance());
    peers->addPeer(uh, MegaChatPeerList::PRIV_STANDARD);
    MegaChatHandle chatid = getGroupChatRoom(a1, a2, peers.get());
    ASSERT_NE(chatid, MEGACHAT_INVALID_HANDLE) <<
                     "Common chat for both users not found.";
    ASSERT_EQ(megaChatApi[a1]->getChatConnectionState(chatid), MegaChatApi::CHAT_CONNECTION_ONLINE) <<
                     "Not connected to chatd for account " << (a1+1) << ": " <<
                     account(a1).getEmail();

    std::unique_ptr<TestChatRoomListener>chatroomListener(new TestChatRoomListener(this,
                                                                                   megaChatApi,
                                                                                   chatid));
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, chatroomListener.get())) <<
                     "Can't open chatRoom user A";
    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(chatid, chatroomListener.get())) <<
                     "Can't open chatRoom user B";

    loadHistory(a1, chatid, chatroomListener.get());
    loadHistory(a2, chatid, chatroomListener.get());

    TestChatVideoListener localVideoListenerA;
    megaChatApi[a1]->addChatLocalVideoListener(chatid, &localVideoListenerA);
    TestChatVideoListener localVideoListenerB;
    megaChatApi[a2]->addChatLocalVideoListener(chatid, &localVideoListenerB);

    // A starts a groupal meeting without audio, nor video
    LOG_debug << "Start Call";
    mCallIdJoining[a1] = MEGACHAT_INVALID_HANDLE;
    mChatIdInProgressCall[a1] = MEGACHAT_INVALID_HANDLE;
    mCallIdRingIn[a2] = MEGACHAT_INVALID_HANDLE;
    mChatIdRingInCall[a2] = MEGACHAT_INVALID_HANDLE;

    ASSERT_NO_FATAL_FAILURE({
    waitForAction (1, // just one attempt as mCallReceivedRinging for B account could fail but call could have been created from A account
                   std::vector<bool *> { &requestFlagsChat[a1][MegaChatRequest::TYPE_START_CHAT_CALL], &mCallInProgress[a1], &mCallReceivedRinging[a2]},
                   std::vector<string> { "TYPE_START_CHAT_CALL[a1]", "mCallInProgress[a1]", "mCallReceivedRinging[a2]"},
                   "starting chat call from A",
                   true /* wait for all exit flags*/,
                   true /*reset flags*/,
                   maxTimeout,
                   [this, a1, chatid](){ megaChatApi[a1]->startChatCall(chatid, /*enableVideo*/ false, /*enableAudio*/ false); });
    });

    ASSERT_TRUE(!lastErrorChat[a1]) << "Failed to start chat call: " << lastErrorChat[a1];

    // B picks up the call
    LOG_debug << "B picking up the call";
    mCallIdExpectedReceived[a2] = MEGACHAT_INVALID_HANDLE;
    unique_ptr<MegaChatCall> auxCall(megaChatApi[a1]->getChatCall(mChatIdInProgressCall[a1]));
    if (auxCall)
    {
        mCallIdExpectedReceived[a2] = auxCall->getCallId();
    }

    ASSERT_NE(mChatIdRingInCall[a2], MEGACHAT_INVALID_HANDLE) <<
                     "Invalid Chatid from call emisor";
    ASSERT_TRUE((mCallIdJoining[a1] == mCallIdRingIn[a2])
                      && (mCallIdRingIn[a2] != MEGACHAT_INVALID_HANDLE))
                     << "A and B are in different call";
    ASSERT_NE(mChatIdRingInCall[a2], MEGACHAT_INVALID_HANDLE) <<
                     "Invalid Chatid for B from A (call emisor)";
    LOG_debug << "B received the call";


    ASSERT_NO_FATAL_FAILURE({
    waitForAction (1, // just one attempt as call could be answered properly at B account but any of the other flags not received
                   std::vector<bool *> { &requestFlagsChat[a2][MegaChatRequest::TYPE_ANSWER_CHAT_CALL],
                                         &mChatCallSessionStatusInProgress[a1],
                                         &mChatCallSessionStatusInProgress[a2]
                                       },
                   std::vector<string> { "TYPE_ANSWER_CHAT_CALL[a2]",
                                         "mChatCallSessionStatusInProgress[a1]",
                                         "mChatCallSessionStatusInProgress[a2]"
                                         },
                   "answering chat call from B",
                   true /* wait for all exit flags*/,
                   true /*reset flags*/,
                   maxTimeout,
                   [this, a2, chatid](){ megaChatApi[a2]->answerChatCall(chatid, /*enableVideo*/ false, /*enableAudio*/ false); });
    });

    // B puts the call on hold
    LOG_debug << "B setting the call on hold";
    exitFlag = &mChatCallOnHold[a1]; *exitFlag = false;  // from receiver account
    action = [this, a2, chatid](){ megaChatApi[a2]->setCallOnHold(chatid, /*setOnHold*/ true); };
    waitForCallAction(a2 /*performer*/, MAX_ATTEMPTS, exitFlag, "receiving call on hold at account A", maxTimeout, action);

    // A puts the call on hold
    LOG_debug << "A setting the call on hold";
    exitFlag = &mChatCallOnHold[a2]; *exitFlag = false; // from receiver account
    action = [this, a1, chatid](){ megaChatApi[a1]->setCallOnHold(chatid, /*setOnHold*/ true); };
    waitForCallAction(a2 /*performer*/, MAX_ATTEMPTS, exitFlag, "receiving call on hold at account B", maxTimeout, action);

    // A releases on hold
    LOG_debug << "A releasing on hold";
    exitFlag = &mChatCallOnHoldResumed[a2]; *exitFlag = false; // from receiver account
    action = [this, a1, chatid](){ megaChatApi[a1]->setCallOnHold(chatid, /*setOnHold*/ false); };
    waitForCallAction(a2 /*performer*/, MAX_ATTEMPTS, exitFlag, "receiving call resume from on hold at account B", maxTimeout, action);

    // B releases on hold
    LOG_debug << "B releasing on hold";
    exitFlag = &mChatCallOnHoldResumed[a1]; *exitFlag = false; // from receiver account
    action = [this, a2, chatid](){ megaChatApi[a2]->setCallOnHold(chatid, /*setOnHold*/ false); };
    waitForCallAction(a2 /*performer*/, MAX_ATTEMPTS, exitFlag, "receiving call resume from on hold at account A", maxTimeout, action);

    // B enables audio monitor
    LOG_debug << "B enabling audio in the call";
    exitFlag = &mChatCallAudioEnabled[a1]; *exitFlag = false; // from receiver account
    action = [this, a2, chatid](){ megaChatApi[a2]->enableAudio(chatid); };
    waitForCallAction(a2 /*performer*/, MAX_ATTEMPTS, exitFlag, "receiving audio enabled at account A", maxTimeout, action);

    // A enables audio monitor
    LOG_debug << "A enabling audio in the call";
    exitFlag = &mChatCallAudioEnabled[a2]; *exitFlag = false; // from receiver account
    action = [this, a1, chatid](){ megaChatApi[a1]->enableAudio(chatid); };
    waitForCallAction(a1 /*performer*/, MAX_ATTEMPTS, exitFlag, "receiving audio enabled at account B", maxTimeout, action);

    // B disables audio monitor
    LOG_debug << "B disabling audio in the call";
    exitFlag = &mChatCallAudioDisabled[a1]; *exitFlag = false; // from receiver account
    action = [this, a2, chatid](){ megaChatApi[a2]->disableAudio(chatid); };
    waitForCallAction(a2 /*performer*/, MAX_ATTEMPTS, exitFlag, "receiving audio disabled at account A", maxTimeout, action);

    // A disables audio monitor
    LOG_debug << "A disabling audio in the call";
    exitFlag = &mChatCallAudioDisabled[a2]; *exitFlag = false; // from receiver account
    action = [this, a1, chatid](){ megaChatApi[a1]->disableAudio(chatid); };
    waitForCallAction(a1 /*performer*/, MAX_ATTEMPTS, exitFlag, "receiving audio disabled at account B", maxTimeout, action);

    // A forces reconnect
    LOG_debug << "A forcing a reconnect";
    bool* chatCallReconnectA = &mChatCallReconnection[a1]; *chatCallReconnectA = false;
    bool* sessionWasDestroyedA = &mChatSessionWasDestroyed[a1]; *sessionWasDestroyedA = false;
    bool* sessionWasDestroyedB = &mChatSessionWasDestroyed[a2]; *sessionWasDestroyedB = false;

    bool* chatCallSessionStatusInProgressA = &mChatCallSessionStatusInProgress[a1];
    *chatCallSessionStatusInProgressA = false;
    bool* chatCallSessionStatusInProgressB = &mChatCallSessionStatusInProgress[a2];
    *chatCallSessionStatusInProgressB = false;

    std::function<void()> waitForChatCallReadyA =
      [this, &chatCallSessionStatusInProgressA]()
      {
          ASSERT_TRUE(waitForResponse(chatCallSessionStatusInProgressA)) <<
                           "Timeout expired for A receiving chat call in progress";
      };

    std::function<void()> waitForChatCallReadyB =
      [this, &chatCallSessionStatusInProgressB] ()
      {
          ASSERT_TRUE(waitForResponse(chatCallSessionStatusInProgressB)) <<
                           "Timeout expired for B receiving chat call in progress";
      };

    megaChatApi[a1]->retryPendingConnections(true);
    // wait for session destruction checks
    std::function<void()> waitForChatCallSessionDestroyedB =
        [this, &sessionWasDestroyedB]()
        {
            ASSERT_TRUE(waitForResponse(sessionWasDestroyedB))
                             << "Timeout expired for B receiving session destroyed notification";
        };
    waitForChatCallSessionDestroyedB();
    std::function<void()> waitForChatCallSessionDestroyedA =
        [this, &sessionWasDestroyedA]()
        {
            ASSERT_TRUE(waitForResponse(sessionWasDestroyedA))
                             << "Timeout expired for A receiving session destroyed notification";
        };
    waitForChatCallSessionDestroyedA();
    // Wait for request finish (i.e. disconnection confirmation)
    ASSERT_TRUE(waitForResponse(chatCallReconnectA)) <<
                     "Timeout expired for A to received request completion for reconnection";
    // B confirms new mega chat session is ready
    waitForChatCallReadyB();
    // A confirms new mega chat session is ready
    waitForChatCallReadyA();

    // B hangs up
    bool* callDestroyedB = &mCallDestroyed[a2]; *callDestroyedB = false;
    *sessionWasDestroyedB = false; *sessionWasDestroyedA = false; // reset flags of session destruction

    LOG_debug << "B hangs up the call";
    exitFlag = &requestFlagsChat[a2][MegaChatRequest::TYPE_HANG_CHAT_CALL]; *exitFlag = false; // from receiver account
    action = [this, a2](){ megaChatApi[a2]->hangChatCall(mCallIdRingIn[a2]); };
    waitForCallAction(a2 /*performer*/, MAX_ATTEMPTS, exitFlag, "hanging up chat call at account B", maxTimeout, action);

    // wait for session destruction checks
    waitForChatCallSessionDestroyedB();
    waitForChatCallSessionDestroyedA();
    ASSERT_TRUE(!lastErrorChat[a2]) << "Failed to hang up chat call: " << lastErrorChat[a2];
    LOG_debug << "Call finished for B";

    // A hangs up
    bool* callDestroyedA = &mCallDestroyed[a1]; *callDestroyedA = false;
    LOG_debug << "A hangs up the call";
    exitFlag = &requestFlagsChat[a1][MegaChatRequest::TYPE_HANG_CHAT_CALL]; *exitFlag = false; // from receiver account
    action = [this, a1](){ megaChatApi[a1]->hangChatCall(mCallIdJoining[a1]); };
    waitForCallAction(a1 /*performer*/, MAX_ATTEMPTS, exitFlag, "hanging up chat call at account B", maxTimeout, action);
    ASSERT_TRUE(!lastErrorChat[a1]) << "Failed to hang up A's chat call: " << lastErrorChat[a1];
    LOG_debug << "Call finished for A";

    // Check the call was destroyed at both ends
    LOG_debug << "Now that A and B hung up, we can check if the call is destroyed";
    ASSERT_TRUE(waitForResponse(callDestroyedA)) <<
                     "The call for A should be already finished and it is not";
    LOG_debug << "Destroyed for A is OK, checking for B";
    ASSERT_TRUE(waitForResponse(callDestroyedB)) <<
                     "The call for B should be already finished and it is not";
    LOG_debug << "Destroyed for B is OK.";


    // close & cleanup
    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener.get());
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener.get());
    megaChatApi[a1]->removeChatLocalVideoListener(chatid, &localVideoListenerA);
    megaChatApi[a2]->removeChatLocalVideoListener(chatid, &localVideoListenerB);
}


/**
 * @brief MegaChatApiTest.ScheduledMeetings
 *
 * Requirements:
 *      - Both accounts should be conctacts
 * (if not accomplished, the test automatically solves them)
 *
 * This test does the following:
 * + TEST 1.  A Creates a Meeting room and a recurrent scheduled meeting in one step
 * + TEST 2.  A Updates a recurrent scheduled meeting with invalid TimeZone (Error)
 * + TEST 3.  A Updates previous recurrent scheduled meeting with valid data
 * + TEST 4.  A Updates a scheduled meeting occurrence with invalid schedId (Error)
 * + TEST 5.  A Updates a scheduled meeting occurrence (new child sched meeting created)
 * + TEST 6.  A Fetch scheduled meetings occurrences chatroom
 * + TEST 7.  A Cancels previous scheduled meeting occurrence
 * + TEST 8.  A Cancel entire series
 * + TEST 9.  A Deletes scheduled meeting with invalid schedId (Error)
 * + TEST 10. A Deletes scheduled meeting
 */
TEST_F(MegaChatApiTest, ScheduledMeetings)
{
    unsigned a1 = 0;
    unsigned a2 = 1;

    // aux data structure to handle lambdas' arguments
    struct SchedMeetingData
    {
        MegaChatHandle chatId = MEGACHAT_INVALID_HANDLE;
        MegaChatHandle schedId = MEGACHAT_INVALID_HANDLE;
        std::string timeZone, title, description;
        MegaChatTimeStamp startDate = 0, endDate = 0, overrides = 0, newStartDate = 0, newEndDate = 0;
        bool cancelled = false, newCancelled = false, publicChat = false, speakRequest = false,
                waitingRoom = false, openInvite = false, isMeeting = false;
        std::shared_ptr<MegaChatScheduledFlags> flags;
        std::shared_ptr<MegaChatScheduledRules> rules;
        std::shared_ptr<MegaChatPeerList> peerList;
    } smDataTests127, smDataTests456;

    // remove scheduled meeting
    const auto deleteSchedMeeting = [this, &a1, &a2](const unsigned int index, const int expectedError, const SchedMeetingData& smData) -> void
    {
        lastErrorChat[index] = MegaChatError::ERROR_OK;                      // reset last MegaChatRequest error
        mSchedMeetingUpdated[a1] = mSchedMeetingUpdated[a2] = false;         // reset sched meetings updated flags
        mSchedIdRemoved[a1] = mSchedIdRemoved[a2] = MEGACHAT_INVALID_HANDLE; // reset sched meetings id's (do after assign vars above)

        // wait for onRequestFinish
        ASSERT_NO_FATAL_FAILURE({
        waitForAction (1,
                       std::vector<bool *> { &requestFlagsChat[a1][MegaChatRequest::TYPE_DELETE_SCHEDULED_MEETING]},
                       std::vector<string> { "TYPE_DELETE_SCHEDULED_MEETING[a1]"},
                       "Removing scheduled meeting from A",
                       true /* wait for all exit flags*/,
                       true /*reset flags*/,
                       maxTimeout,
                       [this, &index, &d = smData]()
                       {
                            megaChatApi[index]->removeScheduledMeeting(d.chatId, d.schedId);
                       });
        });
        ASSERT_EQ(lastErrorChat[a1], expectedError) << "Unexpected TYPE_DELETE_SCHEDULED_MEETING request error";
        if (expectedError != MegaChatError::ERROR_OK) { return; }

        // wait for onChatSchedMeetingUpdate (just in case expectedError is ERROR_OK)
        waitForMultiResponse(std::vector<bool *> {&mSchedMeetingUpdated[a1], &mSchedMeetingUpdated[a2]}, true, maxTimeout);
        ASSERT_NE(mSchedIdRemoved[a1], MEGACHAT_INVALID_HANDLE) << "Scheduled meeting for primary account could not be removed";
        ASSERT_NE(mSchedIdRemoved[a2], MEGACHAT_INVALID_HANDLE) << "Scheduled meeting for secondary account could not be removed";
    };

    // update scheduled meeting
    const auto updateSchedMeeting = [this, &a1, &a2](const unsigned int index, const int expectedError, const SchedMeetingData& smData) -> void
    {
        lastErrorChat[index] = MegaChatError::ERROR_OK;                      // reset last MegaChatRequest error
        mSchedMeetingUpdated[a1] = mSchedMeetingUpdated[a2] = false;         // reset sched meetings updated flags
        mSchedIdUpdated[a1] = mSchedIdUpdated[a2] = MEGACHAT_INVALID_HANDLE; // reset sched meetings id's (do after assign vars above)

        // wait for onRequestFinish
        ASSERT_NO_FATAL_FAILURE({
        waitForAction (1,
                       std::vector<bool *> { &requestFlagsChat[a1][MegaChatRequest::TYPE_UPDATE_SCHEDULED_MEETING]},
                       std::vector<string> { "TYPE_UPDATE_SCHEDULED_MEETING[a1]"},
                       "Updating meeting room and scheduled meeting from A",
                       true /* wait for all exit flags*/,
                       true /*reset flags*/,
                       maxTimeout,
                       [this, &index, &d = smData]()
                       {
                            megaChatApi[index]->updateScheduledMeeting(d.chatId, d.schedId, d.timeZone.c_str(), d.startDate, d.endDate, d.title.c_str(),
                                                                       d.description.c_str(), d.cancelled, d.flags.get(), d.rules.get());
                       });
        });
        ASSERT_EQ(lastErrorChat[a1], expectedError) << "Unexpected TYPE_UPDATE_SCHEDULED_MEETING request error.";
        if (expectedError != MegaChatError::ERROR_OK) { return; }

        // wait for onChatSchedMeetingUpdate (just in case expectedError is ERROR_OK)
        waitForMultiResponse(std::vector<bool *> {&mSchedMeetingUpdated[a1], &mSchedMeetingUpdated[a2]}, true, maxTimeout);
        ASSERT_NE(mSchedIdUpdated[a1], MEGACHAT_INVALID_HANDLE) << "Scheduled meeting for primary account could not be updated";
        ASSERT_NE(mSchedIdUpdated[a2], MEGACHAT_INVALID_HANDLE) << "Scheduled meeting for secondary account could not be updated";
    };

    // update scheduled meeting occurrence
    const auto updateOccurrence = [this, &a1, &a2](const unsigned int index, int expectedError, const SchedMeetingData& smData) -> void
    {
        lastErrorChat[index] = MegaChatError::ERROR_OK;                      // reset last MegaChatRequest error
        mSchedMeetingUpdated[a1] = mSchedMeetingUpdated[a2] = false;         // reset sched meetings updated flags
        mSchedIdUpdated[a1] = mSchedIdUpdated[a2] = MEGACHAT_INVALID_HANDLE; // reset sched meetings id's (do after assign vars above)

        // wait for onRequestFinish
        ASSERT_NO_FATAL_FAILURE({
        waitForAction (1,
                       std::vector<bool *> { &requestFlagsChat[a1][MegaChatRequest::TYPE_UPDATE_SCHEDULED_MEETING_OCCURRENCE]},
                       std::vector<string> { "TYPE_UPDATE_SCHEDULED_MEETING_OCCURRENCE[a1]"},
                       "Updating scheduled meeting occurrence",
                       true /* wait for all exit flags */,
                       true /* reset flags */,
                       maxTimeout,
                       [this, &index, &d = smData]()
                       {
                            megaChatApi[index]->updateScheduledMeetingOccurrence(d.chatId, d.schedId, d.overrides, d.newStartDate, d.newEndDate, d.newCancelled);
                       });
        });

        ASSERT_EQ(lastErrorChat[a1], expectedError) << "Unexpected TYPE_UPDATE_SCHEDULED_MEETING_OCCURRENCE request error.";
        if (expectedError != MegaChatError::ERROR_OK) { return; }

        // wait for onChatSchedMeetingUpdate (just in case expectedError is ERROR_OK)
        waitForMultiResponse(std::vector<bool *> {&mSchedMeetingUpdated[a1], &mSchedMeetingUpdated[a2]}, true, maxTimeout);
        ASSERT_NE(mSchedIdUpdated[a1], MEGACHAT_INVALID_HANDLE) << "Scheduled meeting occurrence for primary account could not be updated";
        ASSERT_NE(mSchedIdUpdated[a2], MEGACHAT_INVALID_HANDLE) << "Scheduled meeting occurrence for secondary account could not be updated";
    };

    // get scheduled meeting
    const auto getSchedMeeting = [this](const unsigned int index, const SchedMeetingData& smData) -> std::unique_ptr<MegaChatScheduledMeeting*>
    {
        const auto smList = std::make_unique<megachat::MegaChatScheduledMeetingList*>(megaChatApi[index]->getScheduledMeetingsByChat(smData.chatId));
        const bool validSchedId = smData.schedId != MEGACHAT_INVALID_HANDLE;
        for (size_t i = 0, sz = (*smList)->size(); i < sz; ++i)
        {
            if (!validSchedId && (*smList)->at(i)->parentSchedId() == MEGACHAT_INVALID_HANDLE)
            {
                // if no schedId provided return the parent sched meeting for this chat
                return std::make_unique<MegaChatScheduledMeeting*>((*smList)->at(i)->copy());
            }

            if (validSchedId && (*smList)->at(i)->schedId() == smData.schedId)
            {
                // if schedId provided return the sched meeting that matches with provided schedId, if any
                return std::make_unique<MegaChatScheduledMeeting*>((*smList)->at(i)->copy());
            }
        }
        return nullptr;
    };

    // create chatroom and scheduled meeting
    const auto createChatroomAndSchedMeeting = [this, &a1, &a2] (const unsigned int index, const SchedMeetingData& smData) -> void
    {

        // reset sched meetings id and chatid to invalid handle
        chatid[a1] = chatid[a2] = MEGACHAT_INVALID_HANDLE;
        mSchedIdUpdated[a1] = mSchedIdUpdated[a2] = MEGACHAT_INVALID_HANDLE;

        // create Meeting room and scheduled meeting
        ASSERT_NO_FATAL_FAILURE({
        waitForAction (1,
                       std::vector<bool *> { &requestFlagsChat[a1][MegaChatRequest::TYPE_CREATE_CHATROOM], &mSchedMeetingUpdated[a1], &mSchedMeetingUpdated[a2], &chatItemUpdated[a2]},
                       std::vector<string> { "TYPE_CREATE_CHATROOM[a1]", "mChatSchedMeeting[a1]", "mChatSchedMeeting[a2]", "chatItemUpdated[a2]"},
                       "Creating meeting room and scheduled meeting from A",
                       true /* wait for all exit flags*/,
                       true /*reset flags*/,
                       maxTimeout,
                       [this, &index, &d = smData]()
                       {
                            megaChatApi[index]->createChatroomAndSchedMeeting(d.peerList.get(), d.isMeeting, d.publicChat,
                                                                           d.title.c_str(), d.speakRequest, d.waitingRoom,
                                                                           d.openInvite, d.timeZone.c_str(), d.startDate, d.endDate,
                                                                           d.description.c_str(), d.flags.get(), d.rules.get(), nullptr /*attributes*/);
                       });
        });

        ASSERT_NE(chatid[a1], MEGACHAT_INVALID_HANDLE) << "Chatroom could not be created";
        ASSERT_NE(mSchedIdUpdated[a1], MEGACHAT_INVALID_HANDLE) << "Scheduled meeting for primary account could not be created";
        ASSERT_NE(mSchedIdUpdated[a2], MEGACHAT_INVALID_HANDLE) << "Scheduled meeting for secondary account could not be created";
    };

    // fetch scheduled meeting occurrences
    const auto fetchOccurrences = [this, a1](const unsigned int index, int expectedError, const SchedMeetingData& smData) -> void
    {
        // check if occurrence is inside requested range
        const MegaChatTimeStamp sinceTs = smData.startDate;
        const auto isValidOccurr = [&sinceTs](const MegaChatTimeStamp& ts)
        {
            return sinceTs <= ts; // check until limit in this method when apps can filter ocurrences by that field
        };

        lastErrorChat[index] = MegaChatError::ERROR_OK; // reset last MegaChatRequest error
        mOccurrList[index].reset();                     // clear occurrences list

        // wait for onRequestFinish
        ASSERT_NO_FATAL_FAILURE({
        waitForAction (1,
                       std::vector<bool *> { &requestFlagsChat[a1][MegaChatRequest::TYPE_FETCH_SCHEDULED_MEETING_OCCURRENCES] },
                       std::vector<string> { "TYPE_FETCH_SCHEDULED_MEETING_OCCURRENCES[a1]" },
                       "Fetching scheduled meeting occurrences",
                       true /* wait for all exit flags */,
                       true /* reset flags */,
                       maxTimeout,
                       [this, &index, &d = smData]()
                       {
                            megaChatApi[index]->fetchScheduledMeetingOccurrencesByChat(d.chatId, d.startDate);
                       });
        });

        ASSERT_EQ(lastErrorChat[a1], expectedError) << "Unexpected TYPE_FETCH_SCHEDULED_MEETING_OCCURRENCES request error.";
        for (size_t i =  0; i < mOccurrList[index]->size(); ++i)
        {
             const auto occurr = mOccurrList[index]->at(i);
             ASSERT_TRUE(isValidOccurr(occurr->startDateTime()) && isValidOccurr(occurr->endDateTime())) << "Some of received occurrences are out of specified range";
        }
    };

    //================================================================================//
    // TEST preparation
    //================================================================================//
    const std::unique_ptr <char[]> primarySession(login(a1));
    ASSERT_TRUE(primarySession);
    const std::unique_ptr <char[]> secondarySession(login(a2));
    ASSERT_TRUE(secondarySession);
    std::unique_ptr<MegaUser> user(megaApi[a1]->getContact(account(a2).getEmail().c_str()));
    if (!user || user->getVisibility() != MegaUser::VISIBILITY_VISIBLE)
    {
        ASSERT_NO_FATAL_FAILURE({ makeContact(a1, a2); });
        user.reset(megaApi[a1]->getContact(account(a2).getEmail().c_str()));
        ASSERT_TRUE(user) << "Secondary account is not a contact of primary account yet";
    }

    //================================================================================//
    // TEST 1. Create a meeting room and a recurrent scheduled meeting
    //================================================================================//
    LOG_debug << "TEST_ScheduledMeetings 1: Create meeting room and scheduled meeting";
    const std::shared_ptr<MegaChatPeerList> peerList(MegaChatPeerList::createInstance());
    const time_t now = time(nullptr);
    peerList->addPeer(user->getHandle(), MegaChatPeerList::PRIV_STANDARD);
    std::string title = "SMChat_" + std::to_string(now);
    std::string description = "SMChat_Description";
    std::string timeZone = "Europe/Madrid";
    const MegaChatTimeStamp startDate = now + 60;
    const MegaChatTimeStamp endDate =  startDate + 60;

    // create MegaChatScheduledFlags
    std::shared_ptr<MegaChatScheduledFlags> flags(MegaChatScheduledFlags::createInstance());
    flags->setSendEmails(true);

    // create MegaChatScheduledRules
    std::shared_ptr<::mega::MegaIntegerList> byWeekDay(::mega::MegaIntegerList::createInstance());
    byWeekDay->add(1); byWeekDay->add(3); byWeekDay->add(5);
    std::shared_ptr<MegaChatScheduledRules> rules(MegaChatScheduledRules::createInstance(MegaChatScheduledRules::FREQ_WEEKLY,
                                                                                         MegaChatScheduledRules::INTERVAL_INVALID,
                                                                                         MEGACHAT_INVALID_TIMESTAMP,
                                                                                         byWeekDay.get(), nullptr, nullptr));
    smDataTests127.peerList = peerList;
    smDataTests127.isMeeting = true;
    smDataTests127.publicChat = true;
    smDataTests127.title = title;
    smDataTests127.speakRequest = false;
    smDataTests127.waitingRoom = false;
    smDataTests127.openInvite = false;
    smDataTests127.timeZone = timeZone;
    smDataTests127.startDate = startDate;
    smDataTests127.endDate = endDate;
    smDataTests127.description = ""; // description is not a mandatory field
    smDataTests127.flags = nullptr;  // flags is not a mandatory field
    smDataTests127.rules = rules;
    createChatroomAndSchedMeeting (a1, smDataTests127);

    const MegaChatHandle chatId = chatid[a1];
    const MegaChatHandle schedId = mSchedIdUpdated[a1];
    const std::unique_ptr<char[]> chatIdB64(MegaApi::userHandleToBase64(chatId));
    const std::unique_ptr<char[]> schedIdB64(MegaApi::userHandleToBase64(schedId));
    SchedMeetingData smData; // Designated initializers generate too many warnings (gcc)
    smData.chatId = chatid[a1];
    smData.schedId = MEGACHAT_INVALID_HANDLE;

    const auto  schedMeet = getSchedMeeting(a1, smData);
    ASSERT_TRUE(schedMeet) << "Can't retrieve scheduled meeting for new chat " << (chatIdB64 ? chatIdB64.get() : "INVALID chatId");
    ASSERT_TRUE(!(*schedMeet)->flags() && !(*schedMeet)->description()) << "Scheduled meeting flags must be unset and description must be an empty string" ;
    ASSERT_TRUE(flags->sendEmails()) << "Scheduled meeting created doesn't have send emails flag enabled but it was set on creation";

    //================================================================================//
    // TEST 2. Update a recurrent scheduled meeting with invalid TimeZone (Error)
    //================================================================================//
    LOG_debug << "TEST_ScheduledMeetings 2: Update a recurrent scheduled meeting with invalid TimeZone (Error)";
    timeZone = "Europe/Borlin"; // invalid timezone
    title.append("(updated)");
    description.append("(updated)");
    smDataTests127.chatId = chatId;
    smDataTests127.schedId = schedId;
    smDataTests127.timeZone = timeZone;
    smDataTests127.title = title;
    smDataTests127.cancelled = false;
    updateSchedMeeting(a1, MegaChatError::ERROR_ARGS, smDataTests127);

    //================================================================================//
    // TEST 3. Update previous recurrent scheduled meeting with valid data
    //================================================================================//
    LOG_debug << "TEST_ScheduledMeetings 3: Update a recurrent scheduled meeting";
    timeZone = "Europe/Dublin";
    smDataTests127.timeZone = timeZone;
    updateSchedMeeting(a1, MegaChatError::ERROR_OK, smDataTests127);

    //================================================================================//
    // TEST 4. Update a scheduled meeting occurrence with invalid schedId (Error)
    //================================================================================//
    LOG_debug << "TEST_ScheduledMeetings 4: Update a scheduled meeting occurrence with invalid schedId (Error)";
    smDataTests456.chatId = chatId;
    smDataTests456.overrides = startDate;
    smDataTests456.newStartDate = startDate;
    smDataTests456.newEndDate = endDate;
    smDataTests456.newCancelled = false;
    updateOccurrence(a1, MegaChatError::ERROR_NOENT, smDataTests456);

    //================================================================================//
    // TEST 5. Update a scheduled meeting occurrence (new child sched meeting created)
    //================================================================================//
    LOG_debug << "TEST_ScheduledMeetings 5: Update a scheduled meeting occurrence (child sched meeting created)";
    MegaChatTimeStamp overrides =  startDate;
    const MegaChatTimeStamp auxStartDate =  startDate + 50;
    const MegaChatTimeStamp auxEndDate = endDate + 50;
    // update occurrence and ensure that we have received a new child scheduled meeting whose parent is the original sched meeting and contains the updated occurrence
    smDataTests456.schedId = schedId;
    smDataTests456.overrides = overrides;
    smDataTests456.newStartDate = auxStartDate;
    smDataTests456.newEndDate = auxEndDate;
    updateOccurrence(a1, MegaChatError::ERROR_OK, smDataTests456);
    auto sched = std::make_unique<MegaChatScheduledMeeting*>(megaChatApi[a1]->getScheduledMeeting(chatId, mSchedIdUpdated[a1]));
    ASSERT_TRUE(sched && (*sched)->parentSchedId() == schedId) << "Child scheduled meeting for primary account has not been received";

    const MegaChatHandle childSchedId = (*sched)->schedId();
    smData = SchedMeetingData(); // Designated initializers generate too many warnings (gcc)
    smData.chatId = chatid[a1];
    smData.schedId = childSchedId;
    ASSERT_TRUE(getSchedMeeting(a1, smData)) << "Can't retrieve child scheduled meeting for chat "
                     << (chatIdB64 ? chatIdB64.get() : "INVALID chatId");

    //================================================================================//
    // TEST 6. Fetch scheduled meetings occurrences chatroom
    //================================================================================//
    LOG_debug << "TEST_ScheduledMeetings 6: fetch scheduled meetings occurrences";
    smData = SchedMeetingData(); // Designated initializers generate too many warnings (gcc)
    smData.chatId = chatId;
    smData.startDate = MEGACHAT_INVALID_TIMESTAMP;
    fetchOccurrences(a1, MegaChatError::ERROR_OK, smData);
    ASSERT_TRUE(mOccurrList[a1] && mOccurrList[a1]->size() == MegaChatScheduledMeeting::NUM_OCURRENCES_REQ) <<
                     "Scheduled meeting occurrences for primary account could not be fetched";

    const MegaChatScheduledMeetingOccurr* lastestOcurr = mOccurrList[a1]->at(mOccurrList[a1]->size() -1);
    if (lastestOcurr && lastestOcurr->startDateTime() != MEGACHAT_INVALID_TIMESTAMP)
    {
        smData = SchedMeetingData(); // Designated initializers generate too many warnings (gcc)
        smData.chatId = chatId;
        smData.startDate = lastestOcurr->startDateTime();
        fetchOccurrences(a1, MegaChatError::ERROR_OK, smData);
        ASSERT_TRUE(mOccurrList[a1] && mOccurrList[a1]->size() == MegaChatScheduledMeeting::NUM_OCURRENCES_REQ) <<
                         "More scheduled meeting occurrences for primary account could not be fetched";
    }

    //================================================================================//
    // TEST 7. Cancel previous scheduled meeting occurrence
    //================================================================================//
    LOG_debug << "TEST_ScheduledMeetings 7: Cancel a scheduled meeting occurrence";
    overrides = auxStartDate;
    smDataTests456.schedId = childSchedId;
    smDataTests456.overrides = overrides;
    smDataTests456.newCancelled = true;
    updateOccurrence(a1, MegaChatError::ERROR_OK, smDataTests456);
    sched = std::make_unique<MegaChatScheduledMeeting*>(megaChatApi[a1]->getScheduledMeeting(chatId, mSchedIdUpdated[a1]));
    ASSERT_TRUE(sched && (*sched)->schedId() == childSchedId && (*sched)->cancelled()) << "Scheduled meeting occurrence could not be cancelled";

    //================================================================================//
    // TEST 8. Cancel entire series
    //================================================================================//
    LOG_debug << "TEST_ScheduledMeetings 8: Update a recurrent scheduled meeting";
    smDataTests127.cancelled = true;
    updateSchedMeeting(a1, MegaChatError::ERROR_OK, smDataTests127);
    smData = SchedMeetingData(); // Designated initializers generate too many warnings (gcc)
    smData.chatId = chatId;
    smData.startDate = MEGACHAT_INVALID_TIMESTAMP;
    fetchOccurrences(a1, MegaChatError::ERROR_OK, smData);
    ASSERT_TRUE(mOccurrList[a1] && !mOccurrList[a1]->size()) << "No scheduled meeting occurrences for primary account should be received";

    //================================================================================//
    // TEST 9. Delete scheduled meeting with invalid schedId (Error)
    //================================================================================//
    LOG_debug << "TEST_ScheduledMeetings 9: remove a scheduled meeting occurrence with invalid schedId (Error)";
    smData = SchedMeetingData(); // Designated initializers generate too many warnings (gcc)
    smData.chatId = chatId;
    smData.schedId = MEGACHAT_INVALID_HANDLE;
    deleteSchedMeeting(a1, MegaChatError::ERROR_ARGS, smData);

    //================================================================================//
    // TEST 10. Delete scheduled meeting
    //================================================================================//
    LOG_debug << "TEST_ScheduledMeetings 10: remove a scheduled meeting occurrence";
    smData = SchedMeetingData(); // Designated initializers generate too many warnings (gcc)
    smData.chatId = chatId;
    smData.schedId = schedId;
    deleteSchedMeeting(a1, MegaChatError::ERROR_OK, smData);
}
#endif

/**
 * @brief MegaChatApiTest.RichLinkUserAttribute
 *
 * This test does the following:
 *
 * - Get state for rich link user attribute
 * - Enable/disable rich link generation
 * - Check if value has been established correctly
 * - Change value for rich link counter
 * - Check if value has been established correctly
 *
 */
TEST_F(MegaChatApiTest, RichLinkUserAttribute)
{
    unsigned a1 = 0;

   char *primarySession = login(a1);
   ASSERT_TRUE(primarySession);

   // Get rich link state
   TestMegaRequestListener requestListener(megaApi[a1], nullptr);
   megaApi[a1]->shouldShowRichLinkWarning(&requestListener);
   ASSERT_TRUE(requestListener.waitForResponse()) << "Expired timeout for rich Link";
   int error = requestListener.getErrorCode();
   ASSERT_TRUE(!error || error == ::mega::API_ENOENT) << "Should show richLink warning. Error: " << error;
   ASSERT_EQ(requestListener.getMegaRequest()->getNumDetails(), 1) << "Active at shouldShowRichLink";

   // Enable/disable rich link generation
   bool enableRichLink = !(requestListener.getMegaRequest()->getFlag());
   requestListener = TestMegaRequestListener(megaApi[a1], nullptr);
   megaApi[a1]->enableRichPreviews(enableRichLink, &requestListener);
   ASSERT_TRUE(requestListener.waitForResponse()) << "User attribute retrieval not finished after timeout";
   ASSERT_TRUE(!requestListener.getErrorCode()) << "Failed to enable rich preview. Error: " << requestListener.getErrorCode();

   // Get rich link state
   requestListener = TestMegaRequestListener(megaApi[a1], nullptr);
   megaApi[a1]->shouldShowRichLinkWarning(&requestListener);
   ASSERT_TRUE(requestListener.waitForResponse()) << "Expired timeout for rich Link";
   error = requestListener.getErrorCode();
   ASSERT_TRUE(!error || error == ::mega::API_ENOENT) << "Should show richLink warning. Error: " << error;
   ASSERT_FALSE(requestListener.getMegaRequest()->getFlag()) << "Rich link enable/disable has not worked, (Rich link warning hasn't to be shown)";

   // Change value for rich link counter
   int counter = 1;
   requestListener = TestMegaRequestListener(megaApi[a1], nullptr);
   megaApi[a1]->setRichLinkWarningCounterValue(counter, &requestListener);
   ASSERT_TRUE(requestListener.waitForResponse()) << "User attribute retrieval not finished after timeout";
   ASSERT_FALSE(requestListener.getErrorCode()) << "Failed to set rich preview count. Error: " << requestListener.getErrorCode();

   requestListener = TestMegaRequestListener(megaApi[a1], nullptr);
   megaApi[a1]->shouldShowRichLinkWarning(&requestListener);
   ASSERT_TRUE(requestListener.waitForResponse()) << "Expired timeout for rich Link";
   error = requestListener.getErrorCode();
   ASSERT_TRUE(!error || error == ::mega::API_ENOENT) << "Should show richLink warning. Error: " << error;
   ASSERT_EQ(requestListener.getMegaRequest()->getNumDetails(), 1) << "Active at shouldShowRichLink";
   ASSERT_EQ(counter, requestListener.getMegaRequest()->getNumber()) << "Rich link count has not taken the correct value.";
   ASSERT_TRUE(requestListener.getMegaRequest()->getFlag()) << "Rich link enable/disable has not worked, (Rich link warning has to be shown)";

   delete [] primarySession;
   primarySession = NULL;
}

/**
 * @brief MegaChatApiTest.SendRichLink
 *
 * This test does the following:
 *
 * - Enable rich links
 * - Send a message with a url
 * - Wait for rich link update
 * - Check if message has been updated with a rich link
 *
 */
TEST_F(MegaChatApiTest, SendRichLink)
{
    unsigned a1 = 0;
    unsigned a2 = 1;

    constexpr unsigned int timeoutUsec = maxTimeout * 1000000;
    char *primarySession = login(a1);
    ASSERT_TRUE(primarySession);
    char *secondarySession = login(a2);
    ASSERT_TRUE(secondarySession);

    // Enable rich link
    bool enableRichLink = true;
    bool* richPreviewEnabled = &mUsersChanged[a1][MegaUser::CHANGE_TYPE_RICH_PREVIEWS]; *richPreviewEnabled = false;
    TestMegaRequestListener requestListener(megaApi[a1], nullptr);
    megaApi[a1]->enableRichPreviews(enableRichLink, &requestListener);
    ASSERT_TRUE(requestListener.waitForResponse()) << "User attribute retrieval not finished after timeout";
    int error = requestListener.getErrorCode();
    ASSERT_FALSE(error) << "Failed to enable rich preview. Error: " << error;
    ASSERT_TRUE(waitForResponse(richPreviewEnabled)) << "Richlink previews attr change not received, account" << (a1+1) << ", after timeout: " << maxTimeout << " seconds";

    MegaUser *user = megaApi[a1]->getContact(account(a2).getEmail().c_str());
    if (!user || (user->getVisibility() != MegaUser::VISIBILITY_VISIBLE))
    {
        ASSERT_NO_FATAL_FAILURE({ makeContact(a1, a2); });
    }
    delete user;
    user = NULL;

    MegaChatHandle chatid = getPeerToPeerChatRoom(a1, a2);
    ASSERT_NE(chatid, MEGACHAT_INVALID_HANDLE);

    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a1+1);
    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a2+1);

    // Load some message to feed history
    loadHistory(a1, chatid, chatroomListener);
    loadHistory(a2, chatid, chatroomListener);

    // Define lambda for future messages check
    auto checkMessages = [&](MegaChatMessage* msgSent, const std::string& msgToSend, bool isRichLink, bool senderOnly = false)
    {

        std::array<const unsigned int, 2> an = { a1, a2 };
        // Wait for update (richLink needs a "double" edition)
        if (!chatroomListener->msgEdited[a1])
        {
            for (auto& ai : an)
            {
                ASSERT_TRUE(waitForResponse(&chatroomListener->msgEdited[ai])) << "Message HAS NOT been fully updated after richlink edition, account" << (ai+1) << ", after timeout: " << maxTimeout << " seconds";
            }
        }
        // Check if messages have been updated correctly
        for (auto& ai : an)
        {
            MegaChatMessage* msgUpdated = senderOnly ? msgSent : megaChatApi[ai]->getMessage(chatid, msgSent->getMsgId());
            if (!senderOnly)
            {
                unsigned int tWaited = 0;
                while (((isRichLink && msgUpdated->getType() != MegaChatMessage::TYPE_CONTAINS_META) ||
                            (!isRichLink && msgUpdated->getType() == MegaChatMessage::TYPE_CONTAINS_META)) &&
                        (tWaited < timeoutUsec))
                {
                    std::this_thread::sleep_for(std::chrono::microseconds(pollingT));
                    tWaited += pollingT;
                    MegaChatMessage* newMsgUpdated = megaChatApi[ai]->getMessage(chatid, msgSent->getMsgId());
                    if (msgUpdated != newMsgUpdated)
                    {
                        delete msgUpdated;
                        msgUpdated = newMsgUpdated;
                    }
                }
            }
            ASSERT_TRUE((isRichLink && msgUpdated->getType() == MegaChatMessage::TYPE_CONTAINS_META) ||
                        (!isRichLink && msgUpdated->getType() != MegaChatMessage::TYPE_CONTAINS_META)) << "Invalid Message Type: " << msgUpdated->getType() << ", it should " << (isRichLink ? "" : "NOT ") << "be " << MegaChatMessage::TYPE_CONTAINS_META << " (account " << (ai+1) << ")";
            ASSERT_TRUE((isRichLink && msgUpdated->getContainsMeta() &&
                         msgUpdated->getContainsMeta()->getRichPreview()) ||
                        (!isRichLink && !msgUpdated->getContainsMeta())) << "Rich link information HAS" << (isRichLink ? " NOT" : "") << " been established (account " << (ai+1) << ")";
            ASSERT_TRUE(!isRichLink || msgUpdated->getContainsMeta()->getType() == MegaChatContainsMeta::CONTAINS_META_RICH_PREVIEW) << "Invalid ContainsMeta Type: " << (isRichLink ? to_string(msgUpdated->getContainsMeta()->getType()) : "NONE") << ", due to containsPreview = " << (isRichLink ? "true" : "false") << ", it should " << (isRichLink ? "" : "NOT ") << "be " << MegaChatContainsMeta::CONTAINS_META_RICH_PREVIEW << " (account " << (ai+1) << ")";

            const char* updatedText = isRichLink ? msgUpdated->getContainsMeta()->getRichPreview()->getText() :
                                                   msgUpdated->getContent();
            ASSERT_EQ(updatedText, msgToSend) << "Two strings have to have the same value (account " << (ai+1) << "): UpdatedText -> " << updatedText << " Message sent: " << msgToSend;

            if (senderOnly)
            {
                return;
            }

            delete msgUpdated;
        }
    };

    //=================================//
    // TEST 1. Send rich link message
    //=================================//

    LOG_debug << "TEST 1. Send rich link message";
    std::string messageToSend = "Hello friend, http://mega.nz";
    // Need to do this for the first message as it's send and edited
    chatroomListener->msgEdited[a1] = false;
    chatroomListener->msgEdited[a2] = false;
    MegaChatMessage* msgSent = sendTextMessageOrUpdate(a1, a2, chatid, messageToSend, chatroomListener);
    ASSERT_TRUE(msgSent);
    checkMessages(msgSent, messageToSend, true);

    //===============================================================================================//
    // TEST 2. Remove richlink (used to remove preview) from previous message with removeRichLink()
    //===============================================================================================//

    LOG_debug << "TEST 2. Remove richlink";
    // No call to sendTextMessageOrUpdate, so we must manually waitForUpdate for msgEdited to be set to 'true'
    chatroomListener->msgEdited[a1] = false;
    chatroomListener->msgEdited[a2] = false;
    MegaChatMessage* msgUpdated1 = megaChatApi[a1]->removeRichLink(chatid, msgSent->getMsgId());
    checkMessages(msgUpdated1, messageToSend, false, true);

    //===================================================================//
    // TEST 3. Edit previous non-richlinked message by removing the URL.
    //===================================================================//

    LOG_debug << "TEST 3. Edit previous non-richlinked message by removing the URL";
    std::string messageToUpdate2 = "Hello friend";
    MegaChatMessage* msgUpdated2 = sendTextMessageOrUpdate(a1, a2, chatid, messageToUpdate2, chatroomListener, msgUpdated1->getMsgId());
    ASSERT_TRUE(msgUpdated2);
    checkMessages(msgUpdated2, messageToUpdate2, false);


    //======================================================//
    // TEST 4. Edit previous message by adding a new URL.
    //======================================================//

    LOG_debug << "TEST 4. Edit previous message by adding a new URL";
    std::string messageToUpdate3 = "Hello friend, sorry, the URL is https://mega.nz";
    MegaChatMessage* msgUpdated3 = sendTextMessageOrUpdate(a1, a2, chatid, messageToUpdate3, chatroomListener, msgUpdated2->getMsgId());
    ASSERT_TRUE(msgUpdated3);
    checkMessages(msgUpdated3, messageToUpdate3, true);

    //===============================================================//
    // TEST 5. Edit previous message by modifying the previous URL.
    //===============================================================//

    LOG_debug << "TEST 5. Edit previous message by modifying the previous URL";
    std::string messageToUpdate4 = "Argghhh!!! Sorry again!! I meant https://mega.io that's the good one!!!";
    MegaChatMessage* msgUpdated4 = sendTextMessageOrUpdate(a1, a2, chatid, messageToUpdate4, chatroomListener, msgUpdated3->getMsgId());
    ASSERT_TRUE(msgUpdated4);
    checkMessages(msgUpdated4, messageToUpdate4, true);

    //===============================================================//
    // TEST 6. Edit previous richlinked message by deleting the URL.
    //===============================================================//

    LOG_debug << "TEST 6. Edit previous richlinked message by deleting the URL";
    std::string messageToUpdate5 = "No more richlinks please!!!!";
    MegaChatMessage* msgUpdated5 = sendTextMessageOrUpdate(a1, a2, chatid, messageToUpdate5, chatroomListener, msgUpdated4->getMsgId());
    ASSERT_TRUE(msgUpdated5);
    checkMessages(msgUpdated5, messageToUpdate5, false);


    // Close chat rooms and free up memory
    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);

    delete msgSent;
    delete msgUpdated1;
    delete msgUpdated2;
    delete msgUpdated3;
    delete msgUpdated4;
    delete msgUpdated5;

    delete chatroomListener;

    delete [] primarySession;
    delete [] secondarySession;
}

/**
 * @brief MegaChatApiTest.SendGiphy
 *
 * This test does the following:
 *
 * - Send a message with a giphy
 * - Check if the receiver can get get the message correctly
 * - Check if the json can be parsed correctly
 */
TEST_F(MegaChatApiTest, SendGiphy)
{
    unsigned a1 = 0;
    unsigned a2 = 1;

    TestChatRoomListener* chatroomListener = nullptr;
    MegaUser* user = nullptr;
    MegaChatHandle chatid = 0;
    char* primarySession = nullptr;
    char* secondarySession = nullptr;

    ASSERT_NO_FATAL_FAILURE({
    initChat(a1, a2, user, chatid, primarySession, secondarySession, chatroomListener);
    });

    bool* flagConfirmed = &chatroomListener->msgConfirmed[a1]; *flagConfirmed = false;
    bool* flagReceived = &chatroomListener->msgContactReceived[a2]; *flagReceived = false;
    bool* flagDelivered = &chatroomListener->msgDelivered[a1]; *flagDelivered = false;
    chatroomListener->clearMessages(a1);
    chatroomListener->clearMessages(a2);

    //giphy data
    const char* srcMp4 = "giphy://media/Wm9XlKG2xIMiVcH4CP/200.mp4?cid=a2a900dl&rid=200.mp4&dom=bWVkaWEyLmdpcGh5LmNvbQ%3D%3D";
    const char* srcWebp = "giphy://media/Wm9XlKG2xIMiVcH4CP/200.webp?cid=a2a900dl&rid=200.webp&dom=bWVkaWEyLmdpcGh5LmNvbQ%3D%3D";
    long long sizeMp4 = 59970;
    long long sizeWebp = 159970;
    int giphyWidth = 200;
    int giphyHeight = 200;
    const char* giphyTitle = "MegaChatApiTest.SendGiphy";

    MegaChatMessage* msgSent = megaChatApi[a1]->sendGiphy(chatid, srcMp4, srcWebp, sizeMp4, sizeWebp, giphyWidth, giphyHeight, giphyTitle);

    // Wait for update
    ASSERT_TRUE(waitForResponse(flagConfirmed)) << "Timeout expired for receiving confirmation by server. Timeout: " << maxTimeout << " seconds";

    // Check if message has been received correctly
    MegaChatHandle msgId0 = chatroomListener->mConfirmedMessageHandle[a1];
    MegaChatMessage* msgReceived = megaChatApi[a2]->getMessage(chatid, msgId0);
    ASSERT_EQ(msgReceived->getType(), MegaChatMessage::TYPE_CONTAINS_META) << "Invalid Message Type (account " << (a1+1) << ")";
    auto meta = msgReceived->getContainsMeta();
    ASSERT_TRUE(meta && meta->getGiphy()) << "Giphy information has not been established (account " << (a1+1) << ")";
    auto giphy = meta->getGiphy();
    ASSERT_STREQ(giphy->getMp4Src(), srcMp4) << "giphy mp4 src of message received doesn't match that of the message sent";
    ASSERT_STREQ(giphy->getWebpSrc(), srcWebp) << "giphy webp src of message received doesn't match that of the message sent";
    ASSERT_EQ(giphy->getMp4Size(), sizeMp4) << "giphy mp4 size of message received doesn't match that of the message sent";
    ASSERT_EQ(giphy->getWebpSize(), sizeWebp) << "giphy webp size of message received doesn't match that of the message sent";
    ASSERT_EQ(giphy->getWidth(), giphyWidth) << "giphy width of message received doesn't match that of the message sent";
    ASSERT_EQ(giphy->getHeight(), giphyHeight) << "giphy height size of message received doesn't match that of the message sent";
    ASSERT_STREQ(giphy->getTitle(), giphyTitle) << "giphy title of message received doesn't match that of the message sent";

    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);

    delete user;
    delete msgReceived;
    delete msgSent;
    delete[] primarySession;
    delete[] secondarySession;
}

void MegaChatApiTest::initChat(unsigned int a1, unsigned int a2, MegaUser*& user, megachat::MegaChatHandle& chatid, char*& primarySession, char*& secondarySession, TestChatRoomListener*& chatroomListener)
{
    primarySession = login(a1);
    ASSERT_TRUE(primarySession);
    secondarySession = login(a2);
    ASSERT_TRUE(secondarySession);

    user = megaApi[a1]->getContact(account(a2).getEmail().c_str());
    if (!user || (user->getVisibility() != MegaUser::VISIBILITY_VISIBLE))
    {
        ASSERT_NO_FATAL_FAILURE({ makeContact(a1, a2); });
    }

    chatid = getPeerToPeerChatRoom(a1, a2);
    ASSERT_NE(chatid, MEGACHAT_INVALID_HANDLE);

    // 1. A sends a message to B while B has the chat opened.
    // --> check the confirmed in A, the received message in B, the delivered in A

    chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account 1";
    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account 2";

    loadHistory(a1, chatid, chatroomListener);
    loadHistory(a2, chatid, chatroomListener);
}

int MegaChatApiTest::loadHistory(unsigned int accountIndex, MegaChatHandle chatid, TestChatRoomListener *chatroomListener)
{
    // first of all, ensure the chatd connection is ready
    bool *flagChatdOnline = &mChatConnectionOnline[accountIndex]; *flagChatdOnline = false;
    while (megaChatApi[accountIndex]->getChatConnectionState(chatid) != MegaChatApi::CHAT_CONNECTION_ONLINE)
    {
        postLog("Attempt to load history when still offline. Waiting for connection...");
        bool responseOk = waitForResponse(flagChatdOnline);
        EXPECT_TRUE(responseOk) << "Timeout expired for connecting to chatd";
        *flagChatdOnline = false;
        if (!responseOk) return 0;
    }

    chatroomListener->msgCount[accountIndex] = 0;
    while (1)
    {
        bool *flagHistoryLoaded = &chatroomListener->historyLoaded[accountIndex];
        *flagHistoryLoaded = false;
        int source = megaChatApi[accountIndex]->loadMessages(chatid, 16);
        if (source == MegaChatApi::SOURCE_NONE || source == MegaChatApi::SOURCE_ERROR)
        {
            break;  // no more history or cannot retrieve it
        }

        const char *hstr = MegaApi::userHandleToBase64(chatid);
        bool responseOk = waitForResponse(flagHistoryLoaded);
        EXPECT_TRUE(responseOk) << "Timeout expired for loading history from chat: " << hstr;
        delete [] hstr;
        if (!responseOk) return 0;
    }

    return chatroomListener->msgCount[accountIndex];
}

void MegaChatApiTest::makeContact(unsigned int a1, unsigned int a2)
{
    bool *flagRequestInviteContact = &requestFlags[a1][MegaRequest::TYPE_INVITE_CONTACT];
    *flagRequestInviteContact = false;
    bool *flagContactRequestUpdatedSecondary = &mContactRequestUpdated[a2];
    *flagContactRequestUpdatedSecondary = false;
    std::string contactRequestMessage = "Contact Request Message";
    RequestTracker inviteContactTracker;
    megaApi[a1]->inviteContact(account(a2).getEmail().c_str(),
                               contactRequestMessage.c_str(),
                               MegaContactRequest::INVITE_ACTION_ADD,
                               &inviteContactTracker);

    ASSERT_TRUE(waitForResponse(flagRequestInviteContact)) << "Expired timeout for invite contact request";
    ASSERT_EQ(inviteContactTracker.waitForResult(), API_OK) << "Error invite contact. Error: " << inviteContactTracker.getErrorString();
    ASSERT_TRUE(waitForResponse(flagContactRequestUpdatedSecondary)) << "Expired timeout for receive contact request";

    ASSERT_NO_FATAL_FAILURE({ getContactRequest(a2, false); });

    bool *flagReplyContactRequest = &requestFlags[a2][MegaRequest::TYPE_REPLY_CONTACT_REQUEST];
    *flagReplyContactRequest = false;
    bool *flagContactRequestUpdatedPrimary = &mContactRequestUpdated[a1];
    *flagContactRequestUpdatedPrimary = false;
    RequestTracker replyContactRequestTracker;
    megaApi[a2]->replyContactRequest(mContactRequest[a2], MegaContactRequest::REPLY_ACTION_ACCEPT,
                                     &replyContactRequestTracker);
    ASSERT_TRUE(waitForResponse(flagReplyContactRequest)) << "Expired timeout for reply contact request";
    ASSERT_EQ(replyContactRequestTracker.waitForResult(), API_OK) << "Error reply contact request. Error: " << replyContactRequestTracker.getErrorString();
    ASSERT_TRUE(waitForResponse(flagContactRequestUpdatedPrimary)) << "Expired timeout for receive contact request reply";

    delete mContactRequest[a2];
    mContactRequest[a2] = NULL;
}

bool MegaChatApiTest::isChatroomUpdated(unsigned int index, MegaChatHandle chatid)
{
    for (auto &auxchatid: mChatListUpdated[index])
    {
       if (auxchatid == chatid)
       {
           return true;
       }
    }
    return false;
}

MegaChatHandle MegaChatApiTest::getGroupChatRoom(unsigned int a1, unsigned int a2,
                                                 MegaChatPeerList *peers, int a1Priv, bool create, bool publicChat, const char*)
{
    std::string logMsg;
    MegaChatRoomList *chats = megaChatApi[a1]->getChatRooms();
    bool chatroomExist = false;
    MegaChatHandle targetChatid = MEGACHAT_INVALID_HANDLE;
    for (unsigned i = 0; i < chats->size() && !chatroomExist; ++i)
    {
        const MegaChatRoom *chat = chats->get(i);
        if (!chat->isGroup() || !chat->isActive()
                || (chat->isPublic() != publicChat)
                || ((int)chat->getPeerCount() != peers->size())
                || (a1Priv != megachat::MegaChatPeerList::PRIV_UNKNOWN && a1Priv != chat->getOwnPrivilege()))
        {
            continue;
        }

        for (unsigned userIndex = 0; userIndex < chat->getPeerCount(); userIndex++)
        {
            if (chat->getPeerHandle(userIndex) == peers->getPeerHandle(0))
            {
                bool a2LoggedIn = (megaChatApi[a2] &&
                                   (megaChatApi[a2]->getInitState() == MegaChatApi::INIT_ONLINE_SESSION ||
                                    megaChatApi[a2]->getInitState() == MegaChatApi::INIT_OFFLINE_SESSION));

                MegaChatRoom *chatToCheck = a2LoggedIn ? megaChatApi[a2]->getChatRoom(chat->getChatId()) : NULL;
                if (!a2LoggedIn || (chatToCheck))
                {
                    delete chatToCheck;
                    chatroomExist = true;
                    targetChatid = chat->getChatId();
                    unique_ptr<char[]> base64(::MegaApi::handleToBase64(targetChatid));
                    logMsg.append("getGroupChatRoom: existing chat found, chatid: ").append(base64.get());

                    // --> Ensure we are connected to chatd for the chatroom
                    int connState = megaChatApi[a1]->getChatConnectionState(targetChatid);
                    EXPECT_EQ(connState, MegaChatApi::CHAT_CONNECTION_ONLINE) <<
                                     "Not connected to chatd for account " << (a1+1) << ": " << account(a1).getEmail();
                    if (connState != MegaChatApi::CHAT_CONNECTION_ONLINE) return MEGACHAT_INVALID_HANDLE;
                    if (a2LoggedIn)
                    {
                        connState = megaChatApi[a2]->getChatConnectionState(targetChatid);
                        EXPECT_EQ(megaChatApi[a2]->getChatConnectionState(targetChatid), MegaChatApi::CHAT_CONNECTION_ONLINE) <<
                                     "Not connected to chatd for account " << (a2+1) << ": " << account(a2).getEmail();
                        if (connState != MegaChatApi::CHAT_CONNECTION_ONLINE) return MEGACHAT_INVALID_HANDLE;
                    }
                    break;
                }
            }
        }
    }

    delete chats;
    chats = NULL;

    if (!chatroomExist && create)
    {
        bool *flagCreateChatRoom = &requestFlagsChat[a1][MegaChatRequest::TYPE_CREATE_CHATROOM]; *flagCreateChatRoom = false;
        bool *chatItemPrimaryReceived = &chatItemUpdated[a1]; *chatItemPrimaryReceived = false;
        bool *chatItemSecondaryReceived = &chatItemUpdated[a2]; *chatItemSecondaryReceived = false;
        chatid[a1] = MEGACHAT_INVALID_HANDLE;
        bool *flagChatdOnline1 = &mChatConnectionOnline[a1]; *flagChatdOnline1 = false;
        bool *flagChatdOnline2 = &mChatConnectionOnline[a2]; *flagChatdOnline2 = false;

        megaChatApi[a1]->createChat(true, peers, this);
        bool responseOk = waitForResponse(flagCreateChatRoom);
        EXPECT_TRUE(responseOk) << "Expired timeout for creating groupchat";
        if (!responseOk) return MEGACHAT_INVALID_HANDLE;
        EXPECT_FALSE(lastErrorChat[a1]) << "Failed to create groupchat. Error: " << lastErrorMsgChat[a1] << " (" << lastErrorChat[a1] << ")";
        if (lastErrorChat[a1]) return MEGACHAT_INVALID_HANDLE;
        targetChatid = chatid[a1];
        EXPECT_NE(targetChatid, MEGACHAT_INVALID_HANDLE) << "Wrong chat id";
        if (targetChatid == MEGACHAT_INVALID_HANDLE) return MEGACHAT_INVALID_HANDLE;

        responseOk = waitForResponse(chatItemPrimaryReceived);
        EXPECT_TRUE(responseOk) << "Expired timeout for receiving the new chat list item";
        if (!responseOk) return MEGACHAT_INVALID_HANDLE;

        unique_ptr<char[]> base64(::MegaApi::handleToBase64(targetChatid));
        logMsg.append("getGroupChatRoom: new chat created, chatid: ").append(base64.get());
        // wait for login into chatd for the new groupchat
        while (megaChatApi[a1]->getChatConnectionState(targetChatid) != MegaChatApi::CHAT_CONNECTION_ONLINE)
        {
            postLog("Waiting for connection to chatd for new chat before proceeding with test...");
            responseOk = waitForResponse(flagChatdOnline1);
            EXPECT_TRUE(responseOk) << "Timeout expired for connecting to chatd after creation";
            if (!responseOk) return MEGACHAT_INVALID_HANDLE;
            *flagChatdOnline1 = false;
        }

        // since we may have multiple notifications for other chats, check we received the right one
        MegaChatListItem *chatItemSecondaryCreated = NULL;
        do
        {
            responseOk = waitForResponse(chatItemSecondaryReceived);
            EXPECT_TRUE(responseOk) << "Expired timeout for receiving the new chat list item";
            if (!responseOk) return MEGACHAT_INVALID_HANDLE;
            *chatItemSecondaryReceived = false;

            chatItemSecondaryCreated = megaChatApi[a2]->getChatListItem(targetChatid);
            if (!chatItemSecondaryCreated)
            {
                continue;
            }
            else
            {
                if (chatItemSecondaryCreated->getChatId() != targetChatid)
                {
                    delete chatItemSecondaryCreated; chatItemSecondaryCreated = NULL;
                }
            }
        } while (!chatItemSecondaryCreated);

        delete chatItemSecondaryCreated;    chatItemSecondaryCreated = NULL;

        // wait for login into chatd for the new groupchat
        while (megaChatApi[a2]->getChatConnectionState(targetChatid) != MegaChatApi::CHAT_CONNECTION_ONLINE)
        {
            postLog("Waiting for connection to chatd for new chat before proceeding with test...");
            responseOk = waitForResponse(flagChatdOnline2);
            EXPECT_TRUE(responseOk) << "Timeout expired for connecting to chatd after creation";
            if (!responseOk) return MEGACHAT_INVALID_HANDLE;
            *flagChatdOnline2 = false;
        }
    }

    postLog(logMsg);
    return targetChatid;
}

MegaChatHandle MegaChatApiTest::getPeerToPeerChatRoom(unsigned int a1, unsigned int a2)
{
    MegaUser *peerPrimary = megaApi[a1]->getContact(account(a2).getEmail().c_str());
    MegaUser *peerSecondary = megaApi[a2]->getContact(account(a1).getEmail().c_str());
    EXPECT_TRUE(peerPrimary && peerSecondary) << "Fail to get Peers";
    if (!peerPrimary || !peerSecondary) return MEGACHAT_INVALID_HANDLE;

    MegaChatHandle chatid0 = MEGACHAT_INVALID_HANDLE;
    MegaChatRoom *chatroom0 = megaChatApi[a1]->getChatRoomByUser(peerPrimary->getHandle());
    if (!chatroom0) // chat 1on1 doesn't exist yet --> create it
    {
        MegaChatPeerList *peers = MegaChatPeerList::createInstance();
        peers->addPeer(peerPrimary->getHandle(), MegaChatPeerList::PRIV_STANDARD);

        bool *flag = &requestFlagsChat[a1][MegaChatRequest::TYPE_CREATE_CHATROOM]; *flag = false;
        bool *chatCreated = &chatItemUpdated[a1]; *chatCreated = false;
        bool *chatReceived = &chatItemUpdated[a2]; *chatReceived = false;
        bool *flagChatdOnline1 = &mChatConnectionOnline[a1]; *flagChatdOnline1 = false;
        bool *flagChatdOnline2 = &mChatConnectionOnline[a2]; *flagChatdOnline2 = false;
        megaChatApi[a1]->createChat(false, peers, this);
        bool responseOk = waitForResponse(flag);
        EXPECT_TRUE(responseOk) << "Expired timeout for create new chatroom request";
        if (!responseOk) return MEGACHAT_INVALID_HANDLE;
        EXPECT_FALSE(lastErrorChat[a1]) << "Error create new chatroom request. Error: " << lastErrorMsgChat[a1] << " (" << lastErrorChat[a1] << ")";
        if (lastErrorChat[a1]) return MEGACHAT_INVALID_HANDLE;
        responseOk = waitForResponse(chatCreated);
        EXPECT_TRUE(responseOk) << "Expired timeout for  create new chatroom";
        if (!responseOk) return MEGACHAT_INVALID_HANDLE;
        responseOk = waitForResponse(chatReceived);
        EXPECT_TRUE(responseOk) << "Expired timeout for create new chatroom";
        if (!responseOk) return MEGACHAT_INVALID_HANDLE;
        chatroom0 = megaChatApi[a1]->getChatRoomByUser(peerPrimary->getHandle());
        chatid0 = chatroom0->getChatId();
        EXPECT_NE(chatid0, MEGACHAT_INVALID_HANDLE) << "Invalid chatid";
        if (chatid0 == MEGACHAT_INVALID_HANDLE) return MEGACHAT_INVALID_HANDLE;

        // Wait until both accounts are connected to chatd
        while (megaChatApi[a1]->getChatConnectionState(chatid0) != MegaChatApi::CHAT_CONNECTION_ONLINE)
        {
            postLog("Waiting for connection to chatd...");
            responseOk = waitForResponse(flagChatdOnline1);
            EXPECT_TRUE(responseOk) << "Timeout expired for connecting to chatd, account " << (a1+1);
            if (!responseOk) return MEGACHAT_INVALID_HANDLE;
            *flagChatdOnline1 = false;
        }
        while (megaChatApi[a2]->getChatConnectionState(chatid0) != MegaChatApi::CHAT_CONNECTION_ONLINE)
        {
            postLog("Waiting for connection to chatd...");
            responseOk = waitForResponse(flagChatdOnline2);
            EXPECT_TRUE(waitForResponse(flagChatdOnline2)) << "Timeout expired for connecting to chatd, account " << (a2+1);
            if (!responseOk) return MEGACHAT_INVALID_HANDLE;
            *flagChatdOnline2 = false;
        }
    }
    else
    {
        // --> Ensure we are connected to chatd for the chatroom
        chatid0 = chatroom0->getChatId();
        EXPECT_NE(chatid0, MEGACHAT_INVALID_HANDLE) << "Invalid chatid";
        if (chatid0 == MEGACHAT_INVALID_HANDLE) return MEGACHAT_INVALID_HANDLE;
        EXPECT_EQ(megaChatApi[a1]->getChatConnectionState(chatid0), MegaChatApi::CHAT_CONNECTION_ONLINE) <<
                         "Not connected to chatd for account " << (a1+1) << ": " << account(a1).getEmail();
        if (megaChatApi[a1]->getChatConnectionState(chatid0) != MegaChatApi::CHAT_CONNECTION_ONLINE) return MEGACHAT_INVALID_HANDLE;
        EXPECT_EQ(megaChatApi[a2]->getChatConnectionState(chatid0), MegaChatApi::CHAT_CONNECTION_ONLINE) <<
                         "Not connected to chatd for account " << (a2+1) << ": " << account(a2).getEmail();
        if (megaChatApi[a2]->getChatConnectionState(chatid0) != MegaChatApi::CHAT_CONNECTION_ONLINE) return MEGACHAT_INVALID_HANDLE;
    }

    delete chatroom0;
    chatroom0 = NULL;

    MegaChatRoom *chatroom1 = megaChatApi[a2]->getChatRoomByUser(peerSecondary->getHandle());
    MegaChatHandle chatid1 = chatroom1->getChatId();
    delete chatroom1;
    chatroom1 = NULL;
    EXPECT_EQ(chatid0, chatid1) << "Chat identificator is different for account0 and account1.";
    if (chatid0 != chatid1) return MEGACHAT_INVALID_HANDLE;

    delete peerPrimary;
    peerPrimary = NULL;
    delete peerSecondary;
    peerSecondary = NULL;

    return chatid0;
}

MegaChatMessage * MegaChatApiTest::sendTextMessageOrUpdate(unsigned int senderAccountIndex, unsigned int receiverAccountIndex,
                                                MegaChatHandle chatid, const string &textToSend,
                                                TestChatRoomListener *chatroomListener, MegaChatHandle messageId)
{
    bool *flagConfirmed = NULL;
    bool *flagReceived = NULL;
    bool *flagDelivered = &chatroomListener->msgDelivered[senderAccountIndex]; *flagDelivered = false;
    chatroomListener->clearMessages(senderAccountIndex);
    chatroomListener->clearMessages(receiverAccountIndex);

    MegaChatMessage *messageSendEdit = NULL;
    MegaChatHandle *msgidSendEdit = NULL;
    if (messageId == MEGACHAT_INVALID_HANDLE)
    {
        flagConfirmed = &chatroomListener->msgConfirmed[senderAccountIndex]; *flagConfirmed = false;
        flagReceived = &chatroomListener->msgReceived[receiverAccountIndex]; *flagReceived = false;

        messageSendEdit = megaChatApi[senderAccountIndex]->sendMessage(chatid, textToSend.c_str());
        msgidSendEdit = &chatroomListener->mConfirmedMessageHandle[senderAccountIndex];
    }
    else  // Update Message
    {
        flagConfirmed = &chatroomListener->msgEdited[senderAccountIndex]; *flagConfirmed = false;
        flagReceived = &chatroomListener->msgEdited[receiverAccountIndex]; *flagReceived = false;
        messageSendEdit = megaChatApi[senderAccountIndex]->editMessage(chatid, messageId, textToSend.c_str());
        msgidSendEdit = &chatroomListener->mEditedMessageHandle[senderAccountIndex];
    }

    EXPECT_TRUE(messageSendEdit) << "Failed to edit message";
    if (!messageSendEdit) return nullptr;
    delete messageSendEdit;
    bool responseOk = waitForResponse(flagConfirmed);
    EXPECT_TRUE(responseOk) << "Timeout expired for receiving confirmation by server";    // for confirmation, sendMessage() is synchronous
    if (!responseOk) return nullptr;
    MegaChatHandle msgPrimaryId = *msgidSendEdit;
    EXPECT_NE(msgPrimaryId, MEGACHAT_INVALID_HANDLE) << "Wrong message id for sent message";
    if (msgPrimaryId == MEGACHAT_INVALID_HANDLE) return nullptr;
    MegaChatMessage *messageSent = megaChatApi[senderAccountIndex]->getMessage(chatid, msgPrimaryId);   // message should be already confirmed, so in RAM
    EXPECT_TRUE(messageSent) << "Failed to find the confirmed message by msgid";
    if (!messageSent) return nullptr;
    EXPECT_EQ(messageSent->getMsgId(), msgPrimaryId) << "Failed to retrieve the message id";
    if (messageSent->getMsgId() != msgPrimaryId) return nullptr;

    responseOk = waitForResponse(flagReceived);
    EXPECT_TRUE(responseOk) << "Timeout expired for receiving message by target user";    // for reception
    if (!responseOk) return nullptr;
    responseOk = chatroomListener->hasArrivedMessage(receiverAccountIndex, msgPrimaryId);
    EXPECT_TRUE(responseOk) << "Message id of sent message and received message don't match";
    if (!responseOk) return nullptr;
    MegaChatHandle msgSecondaryId = msgPrimaryId;
    MegaChatMessage *messageReceived = megaChatApi[receiverAccountIndex]->getMessage(chatid, msgSecondaryId);   // message should be already received, so in RAM
    EXPECT_TRUE(messageReceived) << "Failed to retrieve the message at the receiver account";
    if (!messageReceived) return nullptr;
    EXPECT_STREQ(textToSend.c_str(), messageReceived->getContent()) << "Content of message received doesn't match the content of sent message";
    if (strcmp(textToSend.c_str(), messageReceived->getContent())) return nullptr;

    // Check if reception confirmation is active and, in this case, only 1on1 rooms have acknowledgement of receipt
    if (megaChatApi[senderAccountIndex]->isMessageReceptionConfirmationActive()
            && !megaChatApi[senderAccountIndex]->getChatRoom(chatid)->isGroup())
    {
        responseOk = waitForResponse(flagDelivered);
        EXPECT_TRUE(responseOk) << "Timeout expired for receiving delivery notification";    // for delivery
        if (!responseOk) return nullptr;
    }

    // Update Message
    if (messageId != MEGACHAT_INVALID_HANDLE)
    {
        EXPECT_TRUE(messageReceived->isEdited()) << "Edited messages is not reported as edition";
        if (!messageReceived->isEdited()) return nullptr;
    }

    delete messageReceived;
    messageReceived = NULL;

    return messageSent;
}

void MegaChatApiTest::checkEmail(unsigned int indexAccount)
{
    char *myEmail = megaChatApi[indexAccount]->getMyEmail();
    ASSERT_TRUE(myEmail) << "Incorrect email";
    ASSERT_EQ(string(myEmail), account(indexAccount).getEmail());

    std::stringstream buffer;
    buffer << "My email is: " << myEmail << endl;
    postLog(buffer.str());

    delete [] myEmail;
    myEmail = NULL;
}

string MegaChatApiTest::dateToString()
{
    time_t rawTime;
    struct tm * timeInfo;
    char formatDate[80];
    time(&rawTime);
    timeInfo = localtime(&rawTime);
    strftime(formatDate, 80, "%Y%m%d_%H%M%S", timeInfo);

    return formatDate;
}

MegaChatMessage *MegaChatApiTest::attachNode(unsigned int a1, unsigned int a2, MegaChatHandle chatid,
                                        MegaNode* nodeToSend, TestChatRoomListener* chatroomListener)
{
    MegaNodeList *megaNodeList = MegaNodeList::createInstance();
    megaNodeList->addNode(nodeToSend);

    bool *flagRequest = &requestFlagsChat[a1][MegaChatRequest::TYPE_ATTACH_NODE_MESSAGE]; *flagRequest = false;
    bool *flagConfirmed = &chatroomListener->msgConfirmed[a1]; *flagConfirmed = false;
    bool *flagReceived = &chatroomListener->msgReceived[a2]; *flagReceived = false;

    megaChatApi[a1]->attachNodes(chatid, megaNodeList, this);
    bool responseOk = waitForResponse(flagRequest);
    EXPECT_TRUE(responseOk) << "Expired timeout for attaching node";
    if (!responseOk) return nullptr;
    EXPECT_FALSE(lastErrorChat[a1]) << "Failed to attach node. Error: " << lastErrorMsgChat[a1] << " (" << lastErrorChat[a1] << ")";
    delete megaNodeList;
    if (lastErrorChat[a1]) return nullptr;

    responseOk = waitForResponse(flagConfirmed);
    EXPECT_TRUE(responseOk) << "Timeout expired for receiving confirmation by server";
    if (!responseOk) return nullptr;
    MegaChatHandle msgId0 = chatroomListener->mConfirmedMessageHandle[a1];
    EXPECT_NE(msgId0, MEGACHAT_INVALID_HANDLE) << "Wrong message id for message sent";
    if (msgId0 == MEGACHAT_INVALID_HANDLE) return nullptr;
    MegaChatMessage *msgSent = megaChatApi[a1]->getMessage(chatid, msgId0);   // message should be already confirmed, so in RAM

    responseOk = waitForResponse(flagReceived);
    EXPECT_TRUE(waitForResponse(flagReceived)) << "Timeout expired for receiving message by target user";    // for reception
    if (!responseOk) return nullptr;
    EXPECT_TRUE(chatroomListener->hasArrivedMessage(a2, msgId0)) << "Wrong message id at destination";
    if (!chatroomListener->hasArrivedMessage(a2, msgId0)) return nullptr;
    MegaChatMessage *msgReceived = megaChatApi[a2]->getMessage(chatid, msgId0);   // message should be already received, so in RAM
    EXPECT_TRUE(msgReceived) << "Failed to get messagbe by id";
    if (!msgReceived) return nullptr;
    EXPECT_EQ(msgReceived->getType(), MegaChatMessage::TYPE_NODE_ATTACHMENT) << "Wrong type of message. Type: " << msgReceived->getType();
    if (msgReceived->getType() != MegaChatMessage::TYPE_NODE_ATTACHMENT) return nullptr;
    megaNodeList = msgReceived->getMegaNodeList();
    EXPECT_TRUE(megaNodeList) << "Failed to get list of nodes attached";
    if (!megaNodeList) return nullptr;
    EXPECT_EQ(megaNodeList->size(), 1) << "Wrong size of list of nodes attached";
    if (megaNodeList->size() != 1) return nullptr;
    EXPECT_TRUE(nodeToSend && megaNodeList->get(0)->getHandle() == nodeToSend->getHandle()) << "Handle of node from received message doesn't match the nodehandle attached";
    if (!nodeToSend || megaNodeList->get(0)->getHandle() != nodeToSend->getHandle()) return nullptr;

    delete msgReceived;
    msgReceived = NULL;

    return msgSent;
}

void MegaChatApiTest::clearHistory(unsigned int a1, unsigned int a2, MegaChatHandle chatid, TestChatRoomListener *chatroomListener)
{
    bool *flagTruncateHistory = &requestFlagsChat[a1][MegaChatRequest::TYPE_TRUNCATE_HISTORY]; *flagTruncateHistory = false;
    bool *flagTruncatedPrimary = &chatroomListener->historyTruncated[a1]; *flagTruncatedPrimary = false;
    bool *flagTruncatedSecondary = &chatroomListener->historyTruncated[a2]; *flagTruncatedSecondary = false;
    bool *chatItemUpdated0 = &chatItemUpdated[a1]; *chatItemUpdated0 = false;
    bool *chatItemUpdated1 = &chatItemUpdated[a2]; *chatItemUpdated1 = false;
    megaChatApi[a1]->clearChatHistory(chatid);
    ASSERT_TRUE(waitForResponse(flagTruncateHistory)) << "Expired timeout for truncating history";
    ASSERT_TRUE(!lastErrorChat[a1]) << "Failed to truncate history. Error: " << lastErrorMsgChat[a1] << " (" << lastErrorChat[a1] << ")";
    ASSERT_TRUE(waitForResponse(flagTruncatedPrimary)) << "Expired timeout for truncating history for primary account";
    ASSERT_TRUE(waitForResponse(flagTruncatedSecondary)) << "Expired timeout for truncating history for secondary account";
    ASSERT_TRUE(waitForResponse(chatItemUpdated0)) << "Expired timeout for receiving chat list item update for primary account";
    ASSERT_TRUE(waitForResponse(chatItemUpdated1)) << "Expired timeout for receiving chat list item update for secondary account";

    MegaChatListItem *itemPrimary = megaChatApi[a1]->getChatListItem(chatid);
    ASSERT_EQ(itemPrimary->getUnreadCount(), 0) << "Wrong unread count for chat list item after clear history.";
    ASSERT_STREQ(itemPrimary->getLastMessage(), "") << "Wrong content of last message for chat list item after clear history.";
    ASSERT_EQ(itemPrimary->getLastMessageType(), MegaChatMessage::TYPE_TRUNCATE) << "Wrong type of last message after clear history.";
    ASSERT_NE(itemPrimary->getLastTimestamp(), 0) << "Wrong last timestamp after clear history";
    delete itemPrimary; itemPrimary = NULL;
    MegaChatListItem *itemSecondary = megaChatApi[a2]->getChatListItem(chatid);
    ASSERT_EQ(itemSecondary->getUnreadCount(), 0) << "Wrong unread count for chat list item after clear history.";
    ASSERT_STREQ(itemSecondary->getLastMessage(), "") << "Wrong content of last message for chat list item after clear history.";
    ASSERT_EQ(itemSecondary->getLastMessageType(), MegaChatMessage::TYPE_TRUNCATE) << "Wrong type of last message after clear history.";
    ASSERT_NE(itemSecondary->getLastTimestamp(), 0) << "Wrong last timestamp after clear history";
    delete itemSecondary; itemSecondary = NULL;
}

void MegaChatApiTest::leaveChat(unsigned int accountIndex, MegaChatHandle chatid)
{
    bool *flagRemoveFromchatRoom = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_REMOVE_FROM_CHATROOM]; *flagRemoveFromchatRoom = false;
    bool *chatClosed = &chatItemClosed[accountIndex]; *chatClosed = false;
    megaChatApi[accountIndex]->leaveChat(chatid);
    TEST_LOG_ERROR(waitForResponse(flagRemoveFromchatRoom), "Expired timeout for MegaChatApi remove from chatroom");
    TEST_LOG_ERROR(!lastErrorChat[accountIndex], "Failed to leave chatroom. Error: " + lastErrorMsgChat[accountIndex] + " (" + std::to_string(lastErrorChat[accountIndex]) + ")");
    TEST_LOG_ERROR(waitForResponse(chatClosed), "Chatroom closed error");
    MegaChatRoom *chatroom = megaChatApi[accountIndex]->getChatRoom(chatid);
    if (chatroom->isGroup())
    {
        TEST_LOG_ERROR(!chatroom->isActive(), "Chatroom active error");
    }
    delete chatroom;    chatroom = NULL;
}

unsigned int MegaChatApiTest::getMegaChatApiIndex(MegaChatApi *api)
{
    int apiIndex = -1;
    for (int i = 0; i < static_cast<int>(NUM_ACCOUNTS); i++)
    {
        if (api == megaChatApi[i])
        {
            apiIndex = i;
            break;
        }
    }

    if (apiIndex == -1)
    {
        ADD_FAILURE() << "Instance of MegaChatApi not recognized";
    }

    return apiIndex;
}

unsigned int MegaChatApiTest::getMegaApiIndex(MegaApi *api)
{
    int apiIndex = -1;
    for (int i = 0; i < static_cast<int>(NUM_ACCOUNTS); i++)
    {
        if (api == megaApi[i])
        {
            apiIndex = i;
            break;
        }
    }

    if (apiIndex == -1)
    {
        ADD_FAILURE() << "Instance of MegaApi not recognized";
    }

    return apiIndex;
}

void MegaChatApiTest::createFile(const string &fileName, const string &sourcePath, const string &contain)
{
    std::string filePath = sourcePath + "/" + fileName;
    FILE* fileDescriptor = fopen(filePath.c_str(), "w");
    fprintf(fileDescriptor, "%s", contain.c_str());
    fclose(fileDescriptor);
}

MegaNode *MegaChatApiTest::uploadFile(int accountIndex, const std::string& fileName, const std::string& sourcePath, const std::string& targetPath)
{
    addTransfer(accountIndex);
    std::string filePath = sourcePath + "/" + fileName;
    mNodeUploadHandle[accountIndex] = INVALID_HANDLE;
    megaApi[accountIndex]->startUpload(filePath.c_str()
                                       , megaApi[accountIndex]->getNodeByPath(targetPath.c_str())
                                       , nullptr    /*fileName*/
                                       , 0          /*mtime*/
                                       , nullptr    /*appdata*/
                                       , false      /*isSourceTemporary*/
                                       , false      /*startFirst*/
                                       , nullptr    /*cancelToken*/
                                       , this);     /*listener*/
    bool responseOk = waitForResponse(&isNotTransferRunning(accountIndex));
    EXPECT_TRUE(responseOk) << "Expired timeout for upload file";
    if (!responseOk) return nullptr;
    EXPECT_FALSE(lastErrorTransfer[accountIndex]) <<
                     "Error upload file. Error: " << (lastErrorTransfer[accountIndex]) << ". Source: " << filePath << "  target: " << targetPath;
    if (lastErrorTransfer[accountIndex]) return nullptr;

    EXPECT_NE(mNodeUploadHandle[accountIndex], INVALID_HANDLE) << "Upload node handle is invalid";
    if (mNodeUploadHandle[accountIndex] == INVALID_HANDLE) return nullptr;

    MegaNode *node = megaApi[accountIndex]->getNodeByHandle(mNodeUploadHandle[accountIndex]);
    EXPECT_TRUE(node) << "It is not possible recover upload node";

    return node;
}

void MegaChatApiTest::addTransfer(int accountIndex)
{
    mNotTransferRunning[accountIndex] = false;
}

bool &MegaChatApiTest::isNotTransferRunning(int accountIndex)
{
    return mNotTransferRunning[accountIndex];
}

bool MegaChatApiTest::downloadNode(int accountIndex, MegaNode *nodeToDownload)
{
    struct stat st = {}; // init all members to default values (0)
    if (stat(DOWNLOAD_PATH.c_str(), &st) == -1)
    {
#ifdef _WIN32
        _mkdir(DOWNLOAD_PATH.c_str());
#else
        mkdir(DOWNLOAD_PATH.c_str(), 0700);
#endif
    }

    addTransfer(accountIndex);
    megaApi[accountIndex]->startDownload(nodeToDownload,
                                         DOWNLOAD_PATH.c_str(),
                                         nullptr,   /*customName*/
                                         nullptr,   /*appData*/
                                         false,     /*startFirst*/
                                         nullptr,   /*cancelToken*/
                                         this);
    EXPECT_TRUE(waitForResponse(&isNotTransferRunning(accountIndex))) << "Expired timeout for download file";
    return lastErrorTransfer[accountIndex] == API_OK;
}

bool MegaChatApiTest::importNode(int accountIndex, MegaNode *node, const string &targetName)
{
    mNodeCopiedHandle[accountIndex] = INVALID_HANDLE;
    megaApi[accountIndex]->authorizeNode(node);
    unique_ptr<MegaNode> parentNode(megaApi[accountIndex]->getRootNode());
    RequestTracker copyNodeTracker;
    megaApi[accountIndex]->copyNode(node, parentNode.get(), targetName.c_str(), &copyNodeTracker);

    return copyNodeTracker.waitForResult() == API_OK;
}

void MegaChatApiTest::getContactRequest(unsigned int accountIndex, bool outgoing, int expectedSize)
{
    MegaContactRequestList *crl;

    if (outgoing)
    {
        crl = megaApi[accountIndex]->getOutgoingContactRequests();
        ASSERT_EQ(expectedSize, crl->size());

        if (expectedSize)
        {
            mContactRequest[accountIndex] = crl->get(0)->copy();
        }
    }
    else
    {
        crl = megaApi[accountIndex]->getIncomingContactRequests();
        ASSERT_EQ(expectedSize, crl->size());

        if (expectedSize)
        {
            mContactRequest[accountIndex] = crl->get(0)->copy();
        }
    }

    delete crl;
}

int MegaChatApiTest::purgeLocalTree(const std::string &path)
{
#ifdef _WIN32
    // should be reimplemented, maybe using std::filesystem
    std::cout << "Manually purge local tree: " << path << std::endl;
    return 0;

#else
    DIR *directory = opendir(path.c_str());
    size_t path_len = path.length();
    int r = -1;

    if (directory)
    {
        struct dirent *p;
        r = 0;
        while (!r && (p=readdir(directory)))
        {
            int r2 = -1;
            char *buf;
            size_t len;
            /* Skip the names "." and ".." as we don't want to recurse on them. */
            if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
            {
                continue;
            }

            len = path_len + strlen(p->d_name) + 2;
            buf = (char *)malloc(len);

            if (buf)
            {
                struct stat statbuf;
                snprintf(buf, len, "%s/%s", path.c_str(), p->d_name);
                if (!stat(buf, &statbuf))
                {
                    if (S_ISDIR(statbuf.st_mode))
                    {
                        r2 = purgeLocalTree(buf);
                    }
                    else
                    {
                        r2 = unlink(buf);
                    }
                }

                free(buf);
            }

            r = r2;
        }

        closedir(directory);
    }

    if (!r)
    {
        r = rmdir(path.c_str());
    }

    return r;
#endif
}

void MegaChatApiTest::purgeCloudTree(unsigned int accountIndex, MegaNode *node)
{
    MegaNodeList *children;
    children = megaApi[accountIndex]->getChildren(node);

    for (int i = 0; i < children->size(); i++)
    {
        MegaNode *childrenNode = children->get(i);
        if (childrenNode->isFolder())
        {
            purgeCloudTree(accountIndex, childrenNode);
        }

        RequestTracker removeTracker;
        megaApi[accountIndex]->remove(childrenNode, &removeTracker);
        int removeResult = removeTracker.waitForResult();
        TEST_LOG_ERROR((removeResult == API_OK), "Failed to remove node. Error: "
                       + std::to_string(removeResult) + ' ' + removeTracker.getErrorString());
    }

    delete children;
}

void MegaChatApiTest::clearAndLeaveChats(unsigned int accountIndex, MegaChatHandle skipChatId)
{
    MegaChatRoomList *chatRooms = megaChatApi[accountIndex]->getChatRooms();

    for (unsigned int i = 0; i < chatRooms->size(); ++i)
    {
        const MegaChatRoom *chatroom = chatRooms->get(i);

        if (chatroom->isActive() && chatroom->getOwnPrivilege() == MegaChatRoom::PRIV_MODERATOR)
        {
            bool *flagTruncateHistory = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_TRUNCATE_HISTORY]; *flagTruncateHistory = false;
            megaChatApi[accountIndex]->clearChatHistory(chatroom->getChatId());
            TEST_LOG_ERROR(waitForResponse(flagTruncateHistory), "Expired timeout for truncate history");
            TEST_LOG_ERROR(!lastErrorChat[accountIndex], "Failed to truncate history. Error: " + lastErrorMsgChat[accountIndex] + " (" + std::to_string(lastErrorChat[accountIndex]) + ")");
        }

        if (chatroom->isGroup() && chatroom->isActive() && chatroom->getChatId() != skipChatId)
        {
            leaveChat(accountIndex, chatroom->getChatId());
        }
    }

    delete chatRooms;
    chatRooms = NULL;
}

void MegaChatApiTest::removePendingContactRequest(unsigned int accountIndex)
{
    MegaContactRequestList *contactRequests = megaApi[accountIndex]->getOutgoingContactRequests();

    for (int i = 0; i < contactRequests->size(); i++)
    {
        MegaContactRequest *contactRequest = contactRequests->get(i);
        RequestTracker inviteContactTracker;
        megaApi[accountIndex]->inviteContact(contactRequest->getTargetEmail(),
                                             "Removing you",
                                             MegaContactRequest::INVITE_ACTION_DELETE,
                                             &inviteContactTracker);
        int inviteContactResult = inviteContactTracker.waitForResult();
        TEST_LOG_ERROR((inviteContactResult == API_OK), "Failed to remove peer. Error: "
                       + std::to_string(inviteContactResult) + ' ' + inviteContactTracker.getErrorString());
    }

    delete contactRequests;
    contactRequests = NULL;
}

void MegaChatApiTest::changeLastName(unsigned int accountIndex, std::string lastName)
{
    RequestTracker setUserAttributeTracker;
    megaApi[accountIndex]->setUserAttribute(MegaApi::USER_ATTR_LASTNAME, lastName.c_str(),
                                            &setUserAttributeTracker);
    ASSERT_EQ(setUserAttributeTracker.waitForResult(), API_OK)
            << "Failed SDK request to change lastname. Error: " << setUserAttributeTracker.getErrorString();

    RequestTracker getUserAttributeTracker;
    megaApi[accountIndex]->getUserAttribute(MegaApi::USER_ATTR_LASTNAME,
                                            &getUserAttributeTracker);
    ASSERT_EQ(getUserAttributeTracker.waitForResult(), API_OK)
            << "Failed SDK to get lastname. Error: " << getUserAttributeTracker.getErrorString();
    ASSERT_EQ(getUserAttributeTracker.getText(), lastName) << "Failed SDK last name update.";


    // This sleep is necessary to allow execute the two listeners (MegaChatApi and MegaChatApiTest) for
    // MegaRequest::TYPE_GET_ATTR_USER before exit from this function.
    // In other case, we could ask for the name to MegaChatApi before this will be established
    // because MegachatApiTest listener is called before than MegaChatApi listener
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

void MegaChatApiTest::onRequestFinish(MegaApi *api, MegaRequest *request, MegaError *e)
{
    unsigned int apiIndex = getMegaApiIndex(api);
    ASSERT_NE(apiIndex, -1u) << "MegaChatApiTest::onRequestFinish(MegaApi *api, ...)";

    if (e->getErrorCode() == API_OK)
    {
        switch(request->getType())
        {
            case MegaRequest::TYPE_GET_ATTR_USER:
                if (request->getParamType() ==  MegaApi::USER_ATTR_FIRSTNAME)
                {
                    mFirstname = request->getText() ? request->getText() : "";
                    nameReceived[apiIndex] = true;
                }
                else if (request->getParamType() == MegaApi::USER_ATTR_LASTNAME)
                {
                    mLastname = request->getText() ? request->getText() : "";
                    nameReceived[apiIndex] = true;
                }

                break;

            case MegaRequest::TYPE_COPY:
                mNodeCopiedHandle[apiIndex] = request->getNodeHandle();
                break;
        }
    }

    requestFlags[apiIndex][request->getType()] = true;
}

void MegaChatApiTest::onChatsUpdate(MegaApi* api, MegaTextChatList *chats)
{
    if (!chats)
    {
        return;
    }

    unsigned int apiIndex = getMegaApiIndex(api);
    ASSERT_NE(apiIndex, -1u) << "MegaChatApiTest::onChatsUpdate()";
    mChatsUpdated[apiIndex] = true;
    for (int i = 0; i < chats->size(); i++)
    {
         mChatListUpdated[apiIndex].emplace_back(chats->get(i)->getHandle());
    }
}

void MegaChatApiTest::onContactRequestsUpdate(MegaApi* api, MegaContactRequestList* /*requests*/)
{
    unsigned int apiIndex = getMegaApiIndex(api);
    ASSERT_NE(apiIndex, -1u) << "MegaChatApiTest::onContactRequestsUpdate()";

    mContactRequestUpdated[apiIndex] = true;
}

void MegaChatApiTest::onUsersUpdate(::mega::MegaApi* api, ::mega::MegaUserList* userList)
{
    if (!userList) return;

    unsigned int accountIndex = getMegaApiIndex(api);
    ASSERT_NE(accountIndex, -1u) << "MegaChatApiTest::onUsersUpdate()";
    for (int i = 0; i < userList->size(); i++)
    {
        ::mega::MegaUser* user = userList->get(i);
        if (user->getHandle() != megaApi[accountIndex]->getMyUserHandleBinary())
        {
            // add here code to manage other users changes
            continue;
        }

        // own user changes
        if (user->hasChanged(MegaUser::CHANGE_TYPE_RICH_PREVIEWS))
        {
            mUsersChanged[accountIndex][MegaUser::CHANGE_TYPE_RICH_PREVIEWS] = true;
        }
    }
}

void MegaChatApiTest::onRequestFinish(MegaChatApi *api, MegaChatRequest *request, MegaChatError *e)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);
    ASSERT_NE(apiIndex, -1u) << "MegaChatApiTest::onRequestFinish(MegaChatApi *api, ...)";

    lastErrorChat[apiIndex] = e->getErrorCode();
    lastErrorMsgChat[apiIndex] = e->getErrorString();
    if (!lastErrorChat[apiIndex])
    {
        switch(request->getType())
        {
            case MegaChatRequest::TYPE_CREATE_CHATROOM:
                chatid[apiIndex] = request->getChatHandle();
                break;

            case MegaChatRequest::TYPE_GET_FIRSTNAME:
                mChatFirstname = request->getText() ? request->getText() : "";
                break;

            case MegaChatRequest::TYPE_GET_LASTNAME:
                mChatLastname = request->getText() ? request->getText() : "";
                break;

            case MegaChatRequest::TYPE_GET_EMAIL:
                mChatEmail = request->getText() ? request->getText() : "";
                break;

            case MegaChatRequest::TYPE_CHAT_LINK_HANDLE:
                if (!request->getFlag())
                {
                    chatLinks[apiIndex] = request->getText();
                }
                break;
            case MegaChatRequest::TYPE_RETRY_PENDING_CONNECTIONS:
#ifndef KARERE_DISABLE_WEBRTC
                mChatCallReconnection[apiIndex] = request->getFlag() &&
                    !static_cast<bool>(request->getParamType());
#endif
                break;

             case MegaChatRequest::TYPE_FETCH_SCHEDULED_MEETING_OCCURRENCES:
#ifndef KARERE_DISABLE_WEBRTC
                (mOccurrList[apiIndex]).reset(request->getMegaChatScheduledMeetingOccurrList()
                                                ? request->getMegaChatScheduledMeetingOccurrList()->copy()
                                                : nullptr);
#endif
                break;
        }
    }

    requestFlagsChat[apiIndex][request->getType()] = true;
}

void MegaChatApiTest::onChatInitStateUpdate(MegaChatApi *api, int newState)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);
    ASSERT_NE(apiIndex, -1u) << "MegaChatApiTest::onChatInitStateUpdate()";

    initState[apiIndex] = newState;
    initStateChanged[apiIndex] = true;
}

void MegaChatApiTest::onChatListItemUpdate(MegaChatApi *api, MegaChatListItem *item)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);
    ASSERT_NE(apiIndex, -1u) << "MegaChatApiTest::onChatListItemUpdate()";

    if (item)
    {
        std::stringstream buffer;
        buffer << "[api: " << apiIndex << "] Chat list item added or updated - ";

        const char *info = MegaChatApiTest::printChatListItemInfo(item);
        buffer << info;
        postLog(buffer.str());
        delete [] info; info = NULL;

        if (item->hasChanged(MegaChatListItem::CHANGE_TYPE_CLOSED))
        {
            chatItemClosed[apiIndex] = true;
        }
        if (item->hasChanged(MegaChatListItem::CHANGE_TYPE_PARTICIPANTS) ||
                item->hasChanged(MegaChatListItem::CHANGE_TYPE_OWN_PRIV))
        {
            peersUpdated[apiIndex] = true;
        }
        if (item->hasChanged(MegaChatListItem::CHANGE_TYPE_TITLE))
        {
            titleUpdated[apiIndex] = true;
        }
        if (item->hasChanged(MegaChatListItem::CHANGE_TYPE_ARCHIVE))
        {
            chatArchived[apiIndex] = true;
        }

        chatItemUpdated[apiIndex] = true;
    }
}

void MegaChatApiTest::onChatOnlineStatusUpdate(MegaChatApi* api, MegaChatHandle userhandle, int status, bool)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);
    ASSERT_NE(apiIndex, -1u) << "MegaChatApiTest::onChatOnlineStatusUpdate()";
    if (userhandle == megaChatApi[apiIndex]->getMyUserHandle())
    {
        mOnlineStatusUpdated[apiIndex] = true;
        mOnlineStatus[apiIndex] = status;
    }
}

void MegaChatApiTest::onChatPresenceConfigUpdate(MegaChatApi *api, MegaChatPresenceConfig */*config*/)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);
    ASSERT_NE(apiIndex, -1u) << "MegaChatApiTest::onChatPresenceConfigUpdate()";
    mPresenceConfigUpdated[apiIndex] = true;
}

void MegaChatApiTest::onChatConnectionStateUpdate(MegaChatApi *api, MegaChatHandle chatid, int state)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);
    ASSERT_NE(apiIndex, -1u) << "MegaChatApiTest::onChatConnectionStateUpdate()";
    mChatConnectionOnline[apiIndex] = (state == MegaChatApi::CHAT_CONNECTION_ONLINE);
    mLoggedInAllChats[apiIndex] = (state == MegaChatApi::CHAT_CONNECTION_ONLINE) && (chatid == MEGACHAT_INVALID_HANDLE);
}

void MegaChatApiTest::onTransferStart(MegaApi */*api*/, MegaTransfer */*transfer*/)
{

}

void MegaChatApiTest::onTransferFinish(MegaApi *api, MegaTransfer *transfer, MegaError *error)
{
    unsigned int apiIndex = getMegaApiIndex(api);
    ASSERT_NE(apiIndex, -1u) << "MegaChatApiTest::onTransferFinish()";

    mNodeUploadHandle[apiIndex] = transfer->getNodeHandle();
    lastErrorTransfer[apiIndex] = error->getErrorCode();
    mNotTransferRunning[apiIndex] = true;
}

void MegaChatApiTest::onTransferUpdate(MegaApi */*api*/, MegaTransfer */*transfer*/)
{
}

void MegaChatApiTest::onTransferTemporaryError(MegaApi */*api*/, MegaTransfer */*transfer*/, MegaError */*error*/)
{
}

bool MegaChatApiTest::onTransferData(MegaApi */*api*/, MegaTransfer */*transfer*/, char */*buffer*/, size_t /*size*/)
{
    return false;
}

#ifndef KARERE_DISABLE_WEBRTC

void MegaChatApiTest::onChatCallUpdate(MegaChatApi *api, MegaChatCall *call)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);
    ASSERT_NE(apiIndex, -1u) << "MegaChatApiTest::onChatCallUpdate()";

    if (call->hasChanged(MegaChatCall::CHANGE_TYPE_RINGING_STATUS) && call->isRinging())
    {
        if (api->getNumCalls() > 1)
        {
            // Hangup in progress call and answer the new call
//                api->hangChatCall(mCallId[apiIndex]);
//                api->answerChatCall(call->getChatid());

            // Hangup in coming call
            api->hangChatCall(call->getCallId());
        }

        if (mCallIdExpectedReceived[apiIndex] == MEGACHAT_INVALID_HANDLE
                || mCallIdExpectedReceived[apiIndex] == call->getCallId())
        {
            /* we are waiting to receive a ringing call for a specific callid, this could be util
             * for those scenarios where we receive multiple onChatCallUpdate like a login */
            mCallReceivedRinging[apiIndex] = true;
            mChatIdRingInCall[apiIndex] = call->getChatid();
            mCallIdRingIn[apiIndex] = call->getCallId();
        }
    }

    if (call->hasChanged(MegaChatCall::CHANGE_TYPE_STATUS))
    {
        unsigned int apiIndex = getMegaChatApiIndex(api); // why is this needed again?
        ASSERT_NE(apiIndex, -1u) << "MegaChatApiTest::onChatCallUpdate() (2)";
        switch (call->getStatus())
        {
        case MegaChatCall::CALL_STATUS_INITIAL:
            if (mCallIdExpectedReceived[apiIndex] != MEGACHAT_INVALID_HANDLE
                    && mCallIdExpectedReceived[apiIndex] == call->getCallId())
            {
                /* we are waiting to receive a call status change (CALL_STATUS_INITIAL) generated in
                 * Call ctor, for a specific callid, this could be util for those scenarios where
                 * we receive multiple onChatCallUpdate like a login */
                mCallReceived[apiIndex] = true;
            }
            break;

        case MegaChatCall::CALL_STATUS_IN_PROGRESS:
            mCallInProgress[apiIndex] = true;
            mChatIdInProgressCall[apiIndex] = call->getChatid();
            break;

        case MegaChatCall::CALL_STATUS_JOINING:
            mCallIdJoining[apiIndex] = call->getCallId();
            break;

        case MegaChatCall::CALL_STATUS_TERMINATING_USER_PARTICIPATION:
            mTerminationCode[apiIndex] = call->getTermCode();
            break;

        case MegaChatCall::CALL_STATUS_DESTROYED:
            mCallDestroyed[apiIndex] = true;
            break;

        case MegaChatCall::CALL_STATUS_CONNECTING:
            mCallConnecting[apiIndex] = true;
            break;

        default:
            break;
        }
    }

    LOG_debug << "On chat call change state ";
}

void MegaChatApiTest::onChatSessionUpdate(MegaChatApi* api, MegaChatHandle,
                                          MegaChatHandle, MegaChatSession *session)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);
    ASSERT_NE(apiIndex, -1u) << "MegaChatApiTest::onChatSessionUpdate()";
    LOG_debug << "On chat session update START with apiIndex|" << apiIndex << "|";

    if(session->getChanges())
    {
        switch (session->getChanges())
        {
        case MegaChatSession::CHANGE_TYPE_STATUS:
            mChatCallSessionStatusInProgress[apiIndex] =
                session->getStatus() == MegaChatSession::SESSION_STATUS_IN_PROGRESS;
            mChatSessionWasDestroyed[apiIndex] = mChatSessionWasDestroyed[apiIndex]
                || !mChatCallSessionStatusInProgress[apiIndex];
            break;
        case MegaChatSession::CHANGE_TYPE_SESSION_SPEAK_REQUESTED:
            mChatCallSilenceReq[apiIndex] = !session->hasRequestSpeak();
            break;
        case MegaChatSession::CHANGE_TYPE_SESSION_ON_HOLD:
            mChatCallOnHold[apiIndex] = session->isOnHold();
            mChatCallOnHoldResumed[apiIndex] = !session->isOnHold();
            break;
        case MegaChatSession::CHANGE_TYPE_REMOTE_AVFLAGS:
            mChatCallAudioEnabled[apiIndex] = session->hasAudio();
            mChatCallAudioDisabled[apiIndex] = !session->hasAudio();
            break;
        default:
            LOG_debug << "Chat session update |" << session->getChanges() << "| not processed";
            break;
        }
    }

    LOG_debug << "On chat session update END with apiIndex|" << apiIndex << "|";
}

void MegaChatApiTest::onChatSchedMeetingUpdate(megachat::MegaChatApi* api, megachat::MegaChatScheduledMeeting* sm)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);
    ASSERT_NE(apiIndex, -1u) << "MegaChatApiTest::onChatSchedMeetingUpdate()";
    if (sm)
    {
       mSchedMeetingUpdated[apiIndex] = true;
       mSchedIdUpdated[apiIndex] = sm->schedId();

       if (sm->isDeleted())
       {
           mSchedIdRemoved[apiIndex] = sm->schedId();
       }
    }
}

void MegaChatApiTest::onSchedMeetingOccurrencesUpdate(megachat::MegaChatApi* api, MegaChatHandle /*chatid*/, bool /*append*/)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);
    ASSERT_NE(apiIndex, -1u) << "MegaChatApiTest::onSchedMeetingOccurrencesUpdate()";
    mSchedOccurrUpdated[apiIndex] = true;
}

TestChatVideoListener::TestChatVideoListener()
{
}

TestChatVideoListener::~TestChatVideoListener()
{
}

void TestChatVideoListener::onChatVideoData(MegaChatApi*, MegaChatHandle, int, int, char*, size_t)
{
}

#endif

TestChatRoomListener::TestChatRoomListener(MegaChatApiTest *t, MegaChatApi **apis, MegaChatHandle chatid)
{
    this->t = t;
    this->megaChatApi = apis;
    this->chatid = chatid;
    this->message = NULL;

    for (unsigned i = 0u; i < NUM_ACCOUNTS; i++)
    {
        this->historyLoaded[i] = false;
        this->historyTruncated[i] = false;
        this->msgLoaded[i] = false;
        this->msgCount[i] = 0;
        this->msgConfirmed[i] = false;
        this->msgDelivered[i] = false;
        this->msgReceived[i] = false;
        this->msgEdited[i] = false;
        this->msgRejected[i] = false;
        this->msgId[i].clear();
        this->chatUpdated[i] = false;
        this->userTyping[i] = false;
        this->titleUpdated[i] = false;
        this->archiveUpdated[i] = false;
        this->msgAttachmentReceived[i] = false;
        this->msgContactReceived[i] = false;
        this->msgRevokeAttachmentReceived[i] = false;
        this->reactionReceived[i] = false;
        this->retentionTimeUpdated[i] = false;
        this->mConfirmedMessageHandle[i] = MEGACHAT_INVALID_HANDLE;
        this->mEditedMessageHandle[i] = MEGACHAT_INVALID_HANDLE;
    }
}

void TestChatRoomListener::clearMessages(unsigned int apiIndex)
{
    msgId[apiIndex].clear();
    mConfirmedMessageHandle[apiIndex] = MEGACHAT_INVALID_HANDLE;
    mEditedMessageHandle[apiIndex] = MEGACHAT_INVALID_HANDLE;
}

bool TestChatRoomListener::hasValidMessages(unsigned int apiIndex)
{
    return !msgId[apiIndex].empty();
}

bool TestChatRoomListener::hasArrivedMessage(unsigned int apiIndex, MegaChatHandle messageHandle)
{
    for (unsigned i = 0u; i < msgId[apiIndex].size(); ++i)
    {
        if (msgId[apiIndex][i] == messageHandle)
        {
            return true;
        }
    }

    return false;
}

void TestChatRoomListener::onChatRoomUpdate(MegaChatApi *api, MegaChatRoom *chat)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);
    ASSERT_NE(apiIndex, -1u) << "TestChatRoomListener::onChatRoomUpdate()";

    if (!chat)
    {
        std::stringstream buffer;
        buffer << "[api: " << apiIndex << "] Initialization completed!" << endl;
        t->postLog(buffer.str());
        return;
    }
    if (chat)
    {
        if (chat->hasChanged(MegaChatRoom::CHANGE_TYPE_USER_TYPING))
        {
            uhAction[apiIndex] = chat->getUserTyping();
            userTyping[apiIndex] = true;
        }
        else if (chat->hasChanged(MegaChatRoom::CHANGE_TYPE_USER_STOP_TYPING))
        {
            uhAction[apiIndex] = chat->getUserTyping();
            userTyping[apiIndex] = true;
        }
        else if (chat->hasChanged(MegaChatRoom::CHANGE_TYPE_TITLE))
        {
            titleUpdated[apiIndex] = true;
        }
        else if (chat->hasChanged(MegaChatRoom::CHANGE_TYPE_ARCHIVE))
        {
            archiveUpdated[apiIndex] = true;
        }
        else if (chat->hasChanged(MegaChatListItem::CHANGE_TYPE_UPDATE_PREVIEWERS))
        {
            previewsUpdated[apiIndex] = true;
        }
        else if (chat->hasChanged(MegaChatRoom::CHANGE_TYPE_RETENTION_TIME))
        {
            retentionTimeUpdated[apiIndex] = true;
        }
    }

    std::stringstream buffer;
    buffer << "[api: " << apiIndex << "] Chat updated - ";
    const char *info = MegaChatApiTest::printChatRoomInfo(chat);
    buffer << info;
    t->postLog(buffer.str());
    delete [] info; info = NULL;

    chatUpdated[apiIndex] = chat->getChatId();
}

void TestChatRoomListener::onMessageLoaded(MegaChatApi *api, MegaChatMessage *msg)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);
    ASSERT_NE(apiIndex, -1u) << "TestChatRoomListener::onMessageLoaded()";

    if (msg)
    {
        std::stringstream buffer;
        buffer << endl << "[api: " << apiIndex << "] Message loaded - ";
        const char *info = MegaChatApiTest::printMessageInfo(msg);
        buffer << info;
        t->postLog(buffer.str());
        delete [] info; info = NULL;

        msgCount[apiIndex]++;
        msgId[apiIndex].push_back(msg->getMsgId());

        if (msg->getStatus() == MegaChatMessage::STATUS_SENDING_MANUAL)
        {
            if (msg->getCode() == MegaChatMessage::REASON_NO_WRITE_ACCESS)
            {
                msgRejected[apiIndex] = true;
            }
        }

        if (msg->getType() == MegaChatMessage::TYPE_NODE_ATTACHMENT)
        {
            msgAttachmentReceived[apiIndex] = true;
        }
        else if (msg->getType() == MegaChatMessage::TYPE_CONTACT_ATTACHMENT)
        {
            msgContactReceived[apiIndex] = true;
        }

        msgLoaded[apiIndex] = true;
    }
    else
    {
        historyLoaded[apiIndex] = true;
        std::stringstream buffer;
        buffer << "[api: " << apiIndex << "] Loading of messages completed" << endl;
        t->postLog(buffer.str());
    }
}

void TestChatRoomListener::onMessageReceived(MegaChatApi *api, MegaChatMessage *msg)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);
    ASSERT_NE(apiIndex, -1u) << "TestChatRoomListener::onMessageReceived()";

    std::stringstream buffer;
    buffer << "[api: " << apiIndex << "] Message received - ";
    const char *info = MegaChatApiTest::printMessageInfo(msg);
    buffer << info;
    t->postLog(buffer.str());
    delete [] info; info = NULL;

    if (msg->getType() == MegaChatMessage::TYPE_ALTER_PARTICIPANTS ||
            msg->getType() == MegaChatMessage::TYPE_PRIV_CHANGE)
    {
        uhAction[apiIndex] = msg->getHandleOfAction();
        priv[apiIndex] = msg->getPrivilege();
    }
    if (msg->getType() == MegaChatMessage::TYPE_CHAT_TITLE)
    {
        content[apiIndex] = msg->getContent() ? msg->getContent() : "<empty>";
        titleUpdated[apiIndex] = true;
    }

    msgId[apiIndex].push_back(msg->getMsgId());

    if (msg->getType() == MegaChatMessage::TYPE_NODE_ATTACHMENT)
    {
        msgAttachmentReceived[apiIndex] = true;
    }
    else if (msg->getType() == MegaChatMessage::TYPE_CONTACT_ATTACHMENT)
    {
        msgContactReceived[apiIndex] = true;

    }
    else if(msg->getType() == MegaChatMessage::TYPE_REVOKE_NODE_ATTACHMENT)
    {
        msgRevokeAttachmentReceived[apiIndex] = true;
    }

    msgReceived[apiIndex] = true;
}

void TestChatRoomListener::onReactionUpdate(MegaChatApi *api, MegaChatHandle, const char*, int)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);
    ASSERT_NE(apiIndex, -1u) << "TestChatRoomListener::onReactionUpdate()";
    reactionReceived[apiIndex] = true;
}

void TestChatRoomListener::onHistoryTruncatedByRetentionTime(MegaChatApi *api, MegaChatMessage *msg)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);
    ASSERT_NE(apiIndex, -1u) << "TestChatRoomListener::onHistoryTruncatedByRetentionTime()";
    mRetentionMessageHandle[apiIndex] = msg->getMsgId();
    retentionHistoryTruncated[apiIndex] = true;
}

void TestChatRoomListener::onMessageUpdate(MegaChatApi *api, MegaChatMessage *msg)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);
    ASSERT_NE(apiIndex, -1u) << "TestChatRoomListener::onMessageUpdate()";

    std::stringstream buffer;
    buffer << "[api: " << apiIndex << "] Message updated - ";
    const char *info = MegaChatApiTest::printMessageInfo(msg);
    buffer << info;
    t->postLog(buffer.str());
    delete [] info; info = NULL;

    msgId[apiIndex].push_back(msg->getMsgId());

    if (msg->hasChanged(MegaChatMessage::CHANGE_TYPE_STATUS))
    {
        if (msg->getStatus() == MegaChatMessage::STATUS_SERVER_RECEIVED)
        {
            mConfirmedMessageHandle[apiIndex] = msg->getMsgId();
            msgConfirmed[apiIndex] = true;
        }
        else if (msg->getStatus() == MegaChatMessage::STATUS_DELIVERED)
        {
            msgDelivered[apiIndex] = true;
        }
    }

    if (msg->hasChanged(MegaChatMessage::CHANGE_TYPE_CONTENT) && msg->isEdited())
    {
        mEditedMessageHandle[apiIndex] = msg->getMsgId();
        msgEdited[apiIndex] = true;
    }

    if (msg->getType() == MegaChatMessage::TYPE_TRUNCATE)
    {
        historyTruncated[apiIndex] = true;
    }
}

unsigned int TestChatRoomListener::getMegaChatApiIndex(MegaChatApi *api)
{
    int apiIndex = -1;
    for (unsigned int i = 0; i < NUM_ACCOUNTS; i++)
    {
        if (api == this->megaChatApi[i])
        {
            apiIndex = i;
            break;
        }
    }

    assert(apiIndex != -1); // Instance of MegaChatApi not recognized
    return apiIndex;
}

MegaLoggerTest::MegaLoggerTest(const char *filename)
{
    testlog.open(filename, ios::out | ios::app);
}

MegaLoggerTest::~MegaLoggerTest()
{
    testlog.close();
}

void MegaLoggerTest::log(const char *time, int loglevel, const char *source, const char *message)
{
    testlog << "[" << time << "] " << SimpleLogger::toStr((LogLevel)loglevel) << ": ";
    testlog << message << " (" << source << ")" << endl;
}

void MegaLoggerTest::postLog(const char *message)
{
    testlog << message << endl;
}

void MegaLoggerTest::log(int loglevel, const char *message)
{
    string levelStr;

    switch (loglevel)
    {
        case MegaChatApi::LOG_LEVEL_ERROR: levelStr = "err"; break;
        case MegaChatApi::LOG_LEVEL_WARNING: levelStr = "warn"; break;
        case MegaChatApi::LOG_LEVEL_INFO: levelStr = "info"; break;
        case MegaChatApi::LOG_LEVEL_VERBOSE: levelStr = "verb"; break;
        case MegaChatApi::LOG_LEVEL_DEBUG: levelStr = "debug"; break;
        case MegaChatApi::LOG_LEVEL_MAX: levelStr = "debug-verbose"; break;
        default: levelStr = ""; break;
    }

    // message comes with a line-break at the end
    testlog  << message;
}

bool MegaChatApiUnitaryTest::UNITARYTEST_ParseUrl()
{
    // Test cases
    mOKTests ++;
    std::map<std::string, int> checkUrls;
    checkUrls["googl."] = 0;
    checkUrls["googl.com\"fsdafasdf"] = 1;
    checkUrls["googl.com<fsdafasdf"] = 1;
    checkUrls["http://googl.com"] = 1;
    checkUrls["http://www.googl.com"] = 1;
    checkUrls["www.googl.com"] = 1;
    checkUrls["esto   es un prueba   www.mega.nz dsfasdfa"] = 1;
    checkUrls["esto   es un prueba \twww.mega.nz\tdsfasdfa"] = 1;
    checkUrls["esto   es un prueba \nwww.mega.nz\ndsfasdfa"] = 1;
    checkUrls["esto es un prueba www.mega. nz"] = 1;
    checkUrls["ftp://www.googl.com"] = 0;
    checkUrls["www.googl .com"] = 1;
    checkUrls[" www.sfdsadfasfdsfsdf "] = 0;
    checkUrls["example.com/products?id=1&page=2"] = 1;
    checkUrls["www.example.com/products?iddfdsfdsfsfsdfa=1&page=2"] = 1;
    checkUrls["https://mega.co.nz/#!p2QnF89I!Kf-m03Lwmyut-eF7RnJjSv1PRYYtYHg7oodFrW1waEQ"] = 0;
    checkUrls["https://mega.co.nz/file/p2Qn984I#Kf-m03Lwmyut-eF7RnJjSv1PRYYtYHg7oodFrW1waEQ"] = 0;
    checkUrls["https://mega.co.nz/folder/p2Qn984I#Kf-m03Lwmyut-eF7RnJjSv1PRYYtYHg7oodFrW1waEQ"] = 0;
    checkUrls["https://mega.co.nz/file/p2Qn984I#"] = 0;
    checkUrls["https://mega.co.nz/folder/p2Qn984I#"] = 0;
    checkUrls["https://mega.co.nz/foder/p2Qn984I#"] = 1;
    checkUrls["https://mega.nz/#F!l6h3985J!j8QVi46YEyzaISaqGVRsOA"] = 0;
    checkUrls["https://mega.nz/?fbclid=IwAR260bchewVmPrlijdF8-TbbvCnnKqkWcr3vrCx6VKChvI8NgLNK1oOSaAk#F!xP4E98AB!FH_5HjrWyFsUMjjEHCFIHw"] = 0;
    checkUrls["mega.nz/?fbclid=IwAR260bchewVmPrlijdF8-TbbvCnnKqkWcr3vrCx6VKChvI8NgLNK1oOSaAk#F!xP498AAB!FH_5HjrWyFsUjjnEHCFIHw"] = 0;
    checkUrls["www.mega.nz/?fbclid=IwAR260bchewVmPrlijdF8-TbbvCnnKqkWcr3vrCx6VKChvI8NgLNK1oOSaAk#F!xP4EAYYB!FH_5HjrWyFsUMKnEHCFIHw"] = 0;
    checkUrls["https://mega.nz/?fbclid=IwAR260bchewVmPrlijdF8-TbbvCnnKqkWcrNK1oOSaAk#!xP4EYYAB!FH_5HjrWyFsUMKjjHCFIHw"] = 0;
    checkUrls["https://mega.nz/?fbclid=IwAR260bchewVmPrlijdF8-TbbvCnnKqkWcrNK1oOSaAkC!xP4EAYYB!FH_5HjrWyFsUMKnjjCFIHw"] = 0;
    checkUrls["https://mega.nz/C!xP4E45AB!FH_5HjrWyTTUMKnEHCFIHw"] = 0;
    checkUrls["https://mega.nz/?fbclid=IwAR260bchewVmPrlijdF8-TbbvCnnKqkWcr3vrCx6VKChvI8NgLNK1oOSaAk/chat/xP4EA55B!FH_5HjrWyFsU45nEHCFIHw"] = 0;
    checkUrls["mega.nz/?fbclid=IwAR260bchewVmPrlijdF8-TbbvCnnKqkWcr3vrCx6VKChvI8NgLNK1oOSaAk"] = 1;
    checkUrls["ELPAIS.com"] = 1;
    checkUrls["ELPAIS.COM"] = 1;
    checkUrls["https://www.ELPAIS.CoM"] = 1;
    checkUrls["sdfsadfsad://dsfasdfasd.dsd"] = 0;
    checkUrls["sshf://www.ELPAIS.CoM"] = 0;
    checkUrls["Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. http://www.microsiervos.com/archivo/curiosidades/montana-mas-alta-sistema-solar-en-vesta.html Ut enim ad minim veniam,"] = 1;
    checkUrls["googel.com:"] = 1;
    checkUrls["5/16/18 10:43 AM] platano asdf: Os Mandi Otto link estusbeidj😒😙😓😙😙😙😙😙 www.facebook.com"] = 1;
    checkUrls["http://15.08.02.jpg\",\"s\":3936106,\"hash\":\"GA2oPPAFx4gKx231I3odD1rTHVwOQQyAClb\",\"fa\":\"827:0*00661Rw6wWo/823:1*0Nb4JK5Gd-0\",\"ts\":1529413682}]"] = 1;
    checkUrls["www.ta_ta.com"] = 1;
    checkUrls["http://foo.com/blah_blah"] = 1;
    checkUrls["http://foo.com/blah_blah/"] = 1;
    checkUrls["http://foo.com/blah_blah_(wikipedia)"] = 1;
    checkUrls["http://foo.com/blah_blah_(wikipedia)_(again)"] = 1;
    checkUrls["http://www.example.com/wpstyle/?p=364"] = 1;
    checkUrls["https://www.example.com/foo/?bar=baz&inga=42&quux"] = 1;
    checkUrls["http://odf.ws/123"] = 1;
    checkUrls["http://userid:password@example.com:8080)"] = 0;
    checkUrls["http://userid:password@example.com:8080/"] = 0;
    checkUrls["http://userid@example.com"] = 0;
    checkUrls["http://userid@example.com/"] = 0;
    checkUrls["http://userid@example.com:8080"] = 0;
    checkUrls["http://userid@example.com:8080/"] = 0;
    checkUrls["http://userid:password@example.com"] = 0;
    checkUrls["http://userid:password@example.com/"] = 0;
    checkUrls["http://foo.com/blah_(wikipedia)#cite-1"] = 1;
    checkUrls["http://foo.com/unicode_(✪)_in_parens"] = 1;
    checkUrls["http://code.google.com/events/#&product=browser"] = 1;
    checkUrls["www.code.google.com/events/#&product=browser"] = 1;
    checkUrls["http://1337.net"] = 1;
    checkUrls["http://a.b-c.de"] = 1;
    checkUrls["https://foo_bar.example.com/"] = 1;
    checkUrls["http://"] = 0;
    checkUrls["http://."] = 0;
    checkUrls["http://.."] = 0;
    checkUrls["http://../"] = 0;
    checkUrls["http://?"] = 0;
    checkUrls["http://??"] = 0;
    checkUrls["http://\?\?/"] = 0; // escape '?' to avoid confusion with trigraph (clang)
    checkUrls["http://#"] = 0;
    checkUrls["http://foo.bar?q=Spaces should be encoded"] = 0;
    checkUrls["///a"] = 0;
    checkUrls["http:// shouldfail.com"] = 1;
    checkUrls["http://foo.bar/foo(bar)baz quux"] = 1;
    checkUrls["http://10.1.1.0"] = 1;
    checkUrls["http://3628126748"] = 0;
    checkUrls["http://123.123.123"] = 0;
    checkUrls["http://123.123..123"] = 0;
    checkUrls["http://.www.foo.bar./"] = 0;
    checkUrls["Test ..www.google.es..."] = 1;
    checkUrls["Test ..test..."] = 0;
    checkUrls[":// should fail"] = 0;
    checkUrls["prueba,,,"] = 0;
    checkUrls["prueba!!"] = 0;
    checkUrls["prueba.com!!"] = 1;
    checkUrls["pepitoPerez@gmail.com"] = 0;
    checkUrls["hi..dsdd"] = 0;
    checkUrls["hidsfdf..ddsfsdsdd"] = 0;
    checkUrls["hidsfdf..com"] = 0;
    checkUrls["hidsfdf.d.ddsfsdsdd"] = 0;
    checkUrls["122.123.122.123/jjkkk"] = 1;

    std::cout << "          TEST - Message::parseUrl()" << std::endl;
    bool succesful = true;
    int executedTests = 0;
    int failureTests = 0;
    std::string url;
    for (const auto& testCase : checkUrls)
    {
        executedTests ++;
        if (chatd::Message::hasUrl(testCase.first, url) != !!testCase.second)
        {
            failureTests ++;
            std::cout << "         [" << " FAILED Parse" << "] " << testCase.first << std::endl;
            LOG_debug << "Failed to parse: " << testCase.first;
            succesful = false;
        }
    }

    if (failureTests > 0)
    {
        mFailedTests ++;
    }

    std::cout << "          TEST - Message::parseUrl() - Executed Tests : " << executedTests << "   Failure Tests : " << failureTests << std::endl;
    return succesful;
}

#ifndef KARERE_DISABLE_WEBRTC
bool MegaChatApiUnitaryTest::UNITARYTEST_SfuDataReception()
{
    int failedTest = 0;
    const auto onTestFailed = [&failedTest](const std::string& cmd, const std::string& msg){
        std::string errStr = "          [FAILED processing SFU command] :";
        errStr.append(cmd).append(". ").append(msg);
        failedTest++;
        std::cout << errStr << std::endl;
        LOG_debug << errStr;
    };

    std::cout << "          TEST - SfuConnection::handleIncomingData()" << std::endl;
    mOKTests++;
    MockupCall call;
    std::map<std::string, std::unique_ptr<sfu::Command>> commands;
    sfu::SfuConnection::setCallbackToCommands(call, commands);
    std::map<std::string, bool> checkCommands;
    checkCommands["{\"warn\":\"warn msg\"}"]                                                    = true;
    checkCommands["{\"deny\":\"audio\",\"msg\":\"deny msg\"}"]                                  = true;
    checkCommands["{\"err\":129,\"msg\":\"Error\"}"]                                            = true;
    checkCommands["{\"a\":\"HIRES_STOP\"}"]                                                     = true;
    checkCommands["{\"a\":\"PEERLEFT\",\"cid\":2,\"rsn\":65}"]                                  = true;
    checkCommands["{\"a\":\"PEERJOIN\",\"cid\":2,\"userId\":\"amECEsVQJQ8\",\"av\":0,\"v\":2}"] = true;
    checkCommands["{\"a\":\"HIRES_START\"}"]                                                    = true;
    checkCommands["{\"a\":\"HELLO\",\"cid\":1,\"na\":20,\"mods\":[\"amECEsVQJQ8\"]}"]           = true;
    checkCommands["{\"a\":\"AV\",\"cid\":3,\"av\":1}"]                                          = true;
    checkCommands["{\"a\":\"VTHUMBS\",\"tracks\":[[2,0]]}"]                                     = true;
    checkCommands["{\"a\":\"HIRES\",\"tracks\":[[2,0,1]]}"]                                     = true;
    checkCommands["{\"a\":\"VTHUMB_START\"}"]                                                   = true;
    checkCommands["{\"a\":\"VTHUMB_STOP\"}"]                                                    = true;
    checkCommands["{\"a\":\"KEY\",\"id\":0,\"from\":2,"
                  "\"key\":\"RE8HjOLZl8ITM7FMIbAcigPWxq7i6DGqLQm-aNLAkEk\"}"]                   = true;

    int executedTests = 0;
    for (const auto& testCase : checkCommands)
    {
        executedTests++;
        rapidjson::Document document;
        sfu::SfuConnection::SfuData outdata;
        if (!sfu::SfuConnection::parseSfuData(testCase.first.c_str(), document, outdata))
        {
            onTestFailed(testCase.first, outdata.msg);
        }

        if (outdata.notificationType == sfu::SfuConnection::SfuData::SFU_COMMAND)
        {
            bool commandProcSuccess = (commands.find(outdata.notification) != commands.end()
                    && commands[outdata.notification]->processCommand(document));
            if (commandProcSuccess != testCase.second)
            {
                onTestFailed(testCase.first, outdata.msg);
            }
        }
        // else => SFU_WARN | SFU_ERROR | SFU_DENY
    }

    if (failedTest > 0)
    {
        mFailedTests++;
    }

    std::cout << "          TEST - SfuConnection::handleIncomingData() - Executed Tests : " << executedTests << "   Failure Tests : " << failedTest << std::endl;
    return !failedTest;
}
#endif

karere::IApp::IChatListHandler* MegaChatApiUnitaryTest::chatListHandler()
{
    return nullptr;
}

void MegaChatApiUnitaryTest::onPresenceConfigChanged(const presenced::Config& /*config*/, bool /*pending*/)
{

}

void MegaChatApiUnitaryTest::onPresenceLastGreenUpdated(karere::Id /*userid*/, uint16_t /*lastGreen*/)
{

}

void MegaChatApiUnitaryTest::onDbError(int /*error*/, const std::string &/*msg*/)
{

}

TestMegaRequestListener::TestMegaRequestListener(MegaApi *megaApi, MegaChatApi *megaChatApi)
    : RequestListener(megaApi, megaChatApi)
{
}

TestMegaRequestListener::~TestMegaRequestListener()
{
    delete mRequest;
    delete mError;
}

void TestMegaRequestListener::onRequestFinish(MegaApi *, MegaRequest *request, MegaError *e)
{
    mFinished = true;
    mRequest = request->copy();
    mError = e->copy();

}

int TestMegaRequestListener::getErrorCode() const
{
    assert(mFinished);
    assert(mError);
    return mError->getErrorCode();
}

MegaRequest *TestMegaRequestListener::getMegaRequest() const
{
    assert(mFinished);
    assert(mRequest);
    return mRequest;
}

TestMegaChatRequestListener::TestMegaChatRequestListener(MegaApi *megaApi, MegaChatApi *megaChatApi)
    : RequestListener(megaApi, megaChatApi)
{
}

TestMegaChatRequestListener::~TestMegaChatRequestListener()
{
    delete mRequest;
    delete mError;
}

void TestMegaChatRequestListener::onRequestFinish(MegaChatApi *, MegaChatRequest *request, MegaChatError *e)
{
    mFinished = true;
    mRequest = request->copy();
    mError = e->copy();
}

int TestMegaChatRequestListener::getErrorCode() const
{
    assert(mFinished);
    assert(mError);
    return mError->getErrorCode();
}

MegaChatRequest *TestMegaChatRequestListener::getMegaChatRequest() const
{
    assert(mFinished);
    assert(mRequest);
    return mRequest;
}

bool RequestListener::waitForResponse(unsigned int timeout)
{
    assert(!mFinished);
    timeout *= 1000000; // convert to micro-seconds
    unsigned int tWaited = 0;    // microseconds
    bool connRetried = false;
    while(!mFinished)
    {
        std::this_thread::sleep_for(std::chrono::microseconds(pollingT));

        if (timeout)
        {
            tWaited += pollingT;
            if (tWaited >= timeout)
            {
                return false;   // timeout is expired
            }
            else if (!connRetried && tWaited > (pollingT * 10))
            {
                for (unsigned int i = 0; i < NUM_ACCOUNTS; i++)
                {
                    if (mMegaApi && mMegaApi->isLoggedIn())
                    {
                        mMegaApi->retryPendingConnections();
                    }

                    if (mMegaChatApi && mMegaChatApi->getInitState() == MegaChatApi::INIT_ONLINE_SESSION)
                    {
                        mMegaChatApi->retryPendingConnections();
                    }
                }
                connRetried = true;
            }
        }
    }

    return true;    // response is received
}

RequestListener::RequestListener(MegaApi *megaApi, MegaChatApi* megaChatApi)
    : mMegaApi(megaApi)
    , mMegaChatApi(megaChatApi)
{

}

#ifndef KARERE_DISABLE_WEBRTC
bool MockupCall::handleAvCommand(Cid_t, unsigned, uint32_t)
{
    return true;
}

bool MockupCall::handleAnswerCommand(Cid_t, std::shared_ptr<sfu::Sdp>, uint64_t, std::vector<sfu::Peer>&, const std::map<Cid_t, std::string>&, const std::map<Cid_t, sfu::TrackDescriptor>&, const std::map<Cid_t, sfu::TrackDescriptor>&, std::set<karere::Id>&, bool)
{
    return true;
}

bool MockupCall::handleKeyCommand(const Keyid_t&, const Cid_t&, const std::string &)
{
    return true;
}

bool MockupCall::handleVThumbsCommand(const std::map<Cid_t, sfu::TrackDescriptor> &)
{
    return true;
}

bool MockupCall::handleVThumbsStartCommand()
{
    return true;
}

bool MockupCall::handleVThumbsStopCommand()
{
    return true;
}

bool MockupCall::handleHiResCommand(const std::map<Cid_t, sfu::TrackDescriptor> &)
{
    return true;
}

bool MockupCall::handleHiResStartCommand()
{
    return true;
}

bool MockupCall::handleHiResStopCommand()
{
    return true;
}

bool MockupCall::handleSpeakReqsCommand(const std::vector<Cid_t> &)
{
    return true;
}

bool MockupCall::handleSpeakReqDelCommand(Cid_t)
{
    return true;
}

bool MockupCall::handleSpeakOnCommand(Cid_t)
{
    return true;
}

bool MockupCall::handleSpeakOffCommand(Cid_t)
{
    return true;
}


bool MockupCall::handlePeerJoin(Cid_t, uint64_t, sfu::SfuProtocol, int, std::string&, std::vector<std::string>&)
{
    return true;
}

bool MockupCall::handlePeerLeft(Cid_t, unsigned)
{
    return true;
}

bool MockupCall::handleBye(unsigned)
{
    return true;
}

bool MockupCall::handleModAdd(uint64_t)
{
    return true;
}

bool MockupCall::handleModDel(uint64_t)
{
    return true;
}

void MockupCall::onSendByeCommand()
{

}

void MockupCall::onSfuDisconnected()
{

}
bool MockupCall::error(unsigned int, const string &)
{
    return true;
}

bool MockupCall::processDeny(const std::string&, const std::string&)
{
    return true;
}

void MockupCall::logError(const char *)
{

}

bool MockupCall::handleHello(const Cid_t /*userid*/, const unsigned int /*nAudioTracks*/, const unsigned int /*nVideoTracks*/,
                             const std::set<karere::Id>& /*mods*/, const bool /*wr*/, const bool /*allowed*/,
                             const std::map<karere::Id, bool>& /*wrUsers*/)
{
    return true;
}
#endif
