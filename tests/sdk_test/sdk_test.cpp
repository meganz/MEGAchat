#include "gtestcommon.h"
#include "sdk_test.h"

#include <mega.h>
#include <megaapi.h>
#include <mega/process.h>

#ifdef _WIN32
#include <direct.h>
#endif

using namespace mega;
using namespace megachat;
using namespace std;
using CleanupFunction = MegaChatApiTest::MegaMrProper::CleanupFunction;

const std::string MegaChatApiTest::DEFAULT_PATH = "../../tests/sdk_test/";
const std::string MegaChatApiTest::FILE_IMAGE_NAME = "logo.png";
const std::string MegaChatApiTest::PATH_IMAGE = "PATH_IMAGE";

const std::string MegaChatApiTest::LOCAL_PATH = "./tmp"; // no ending slash
const std::string MegaChatApiTest::REMOTE_PATH = "/";
const std::string MegaChatApiTest::DOWNLOAD_PATH = LOCAL_PATH + "/download/";
std::string USER_AGENT_DESCRIPTION = "MEGAChatTest";

const string& getDefaultLogName()
{
    static const string LOG_NAME{"test.log"};
    return LOG_NAME;
}


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
    vector<pair<string, string>> accEnvVars{{"MEGA_EMAIL0", "MEGA_PWD0"},
                                            {"MEGA_EMAIL1", "MEGA_PWD1"},
                                            {"MEGA_EMAIL2", "MEGA_PWD2"}};

    RuntimeArgValues argVals(vector<string>(argv, argv + argc), std::move(accEnvVars));
    if (argVals.isHelp())
    {
        return 0;
    }

    if (!argVals.isValid())
    {
        std::cout << "No tests executed (invalid arguments)." << std::endl;
        return -1;
    }

    if (argVals.isListOnly())
    {
        testing::InitGoogleTest(&argc, argv);
        return RUN_ALL_TESTS(); // returns 0 (success) or 1 (failed tests)
    }

    remove(argVals.getLog().c_str());

    if (argVals.isMainProcWithWorkers())
    {
        // Don't run tests, only manage subprocesses.
        // To get here run with --INSTANCES:2 [--EMAIL-POOL:foo+bar-{1-28}@mega.nz]
        // If --EMAIL-POOL runtime arg is missing, email template will be taken from MEGA_PWD0 env var.
        // Password for all emails built from template will be taken from MEGA_PWD0 env var.
        // If it did not get an email template, it'll use 1 single subprocess with the existing env vars.
        GTestParallelRunner pr(std::move(argVals));
        return pr.run();
    }

    // runt test(s)
    if (!argVals.getCustomApiUrl().empty())
    {
        g_APIURL_default = argVals.getCustomApiUrl();
    }
    if (!argVals.getCustomUserAget().empty())
    {
        USER_AGENT_DESCRIPTION = argVals.getCustomUserAget();
    }

    MegaChatApiTest::init(argVals.getLog()); // logger set here will also be enough for MegaChatApiUnitaryTest
    testing::InitGoogleTest(&argc, argv);
    testing::UnitTest::GetInstance()->listeners().Append(new GTestLogger());

    int rc = RUN_ALL_TESTS(); // returns 0 (success) or 1 (failed tests)

    MegaChatApiTest::terminate();

    return rc;
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

    // Every karere::Client will add another logger -- will instantiate a MyMegaApi member,
    // which will instantiate a new MyMegaLogger member, which (by default) will call
    // MegaApi::addLoggerObject() which will add it to g_externalLogger.
    // That has led to duplicated messages in the log file. Below is an attempt to work around that.
    g_externalLogger.useOnlyFirstLogger();

    // 1. Initialize chat engine
    bool *flagInit = &initStateChanged[accountIndex]; *flagInit = false;
    int initializationState = megaChatApi[accountIndex]->init(session);
    EXPECT_GE(initializationState, 0) << "MegaChatApiImpl::init returned error";
    if (initializationState < 0) return nullptr;
    MegaApi::removeLoggerObject(logger());

    // MegaChatApi::INIT_TERMINATED will not be notified. Do not wait for state-change in that case.
    // Worth asking: why is MegaChatApi::init() returning that undocumented value?!
    if (initializationState != MegaChatApi::INIT_TERMINATED)
    {
        bool responseOk = waitForResponse(flagInit);
        EXPECT_TRUE(responseOk) << "Initialization failed";
        if (!responseOk) return nullptr;
        int initStateValue = initState[accountIndex];
        if (!session)
        {
            EXPECT_EQ(initStateValue, MegaChatApi::INIT_WAITING_NEW_SESSION) << "Wrong chat initialization state (1).";
            if (initStateValue != MegaChatApi::INIT_WAITING_NEW_SESSION) return nullptr;
        }
        else
        {
            EXPECT_EQ(initStateValue, MegaChatApi::INIT_OFFLINE_SESSION) << "Wrong chat initialization state (2).";
            if (initStateValue != MegaChatApi::INIT_OFFLINE_SESSION) return nullptr;
        }
    }

    // 2. login
    flagInit = &initStateChanged[accountIndex]; *flagInit = false;
    RequestTracker loginTracker(megaApi[accountIndex]);
    session && std::strlen(session)
        ? megaApi[accountIndex]->fastLogin(session, &loginTracker)  // session must be not null and non empty
        : megaApi[accountIndex]->login(mail.c_str(), pwd.c_str(), &loginTracker);

    int loginResult = loginTracker.waitForResult();
    EXPECT_EQ(loginResult, API_OK) << "Login failed. Error: " << loginResult << ' ' << loginTracker.getErrorString();
    if (loginResult != API_OK) return nullptr;

    // 3. fetchnodes
    bool *loggedInFlag = &mLoggedInAllChats[accountIndex]; *loggedInFlag = false;
    RequestTracker fetchNodesTracker(megaApi[accountIndex]);
    megaApi[accountIndex]->fetchNodes(&fetchNodesTracker);
    int fetchNodesResult = fetchNodesTracker.waitForResult();
    EXPECT_EQ(fetchNodesResult, API_OK) << "Error fetch nodes. Error: " << fetchNodesResult << ' ' << fetchNodesTracker.getErrorString();
    if (fetchNodesResult != API_OK) return nullptr;
    // after fetchnodes, karere should be ready for offline, at least
    int initStateValue = initState[accountIndex];
    if (initStateValue == MegaChatApi::INIT_WAITING_NEW_SESSION || initStateValue == MegaChatApi::INIT_OFFLINE_SESSION)
    {
        bool responseOk = waitForResponse(flagInit);
        EXPECT_TRUE(responseOk) << "Expired timeout for change init state";
        if (!responseOk) return nullptr;
        initStateValue = initState[accountIndex];
    }
    EXPECT_EQ(initStateValue, MegaChatApi::INIT_ONLINE_SESSION) << "Wrong chat initialization state (3).";
    if (initStateValue != MegaChatApi::INIT_ONLINE_SESSION) return nullptr;

    // if there are chatrooms in this account, wait to be joined to all of them
    std::unique_ptr<MegaChatListItemList> items(megaChatApi[accountIndex]->getChatListItems());
    if (items->size())
    {
        bool responseOk = waitForResponse(loggedInFlag, 120);
        EXPECT_TRUE(responseOk) << "Expired timeout for login to all chats in account '" << mail << "'. (DDOS protection triggered?)";
        if (!responseOk) return nullptr;
    }

    return megaApi[accountIndex]->dumpSession();
}

void MegaChatApiTest::logout(unsigned int accountIndex, bool closeSession)
{
    RequestTracker logoutTracker(megaApi[accountIndex]);
    if (closeSession)
    {
        ChatLogoutTracker chatLogoutTracker;
        megaChatApi[accountIndex]->addChatRequestListener(&chatLogoutTracker);

#ifdef ENABLE_SYNC
        megaApi[accountIndex]->logout(false, &logoutTracker);
#else
        megaApi[accountIndex]->logout(logoutTracker.get());
#endif

        ASSERT_EQ(chatLogoutTracker.waitForResult(), MegaChatError::ERROR_OK);
        megaChatApi[accountIndex]->removeChatRequestListener(&chatLogoutTracker);
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
        ChatRequestTracker crt(megaChatApi[accountIndex]);
        megaChatApi[accountIndex]->localLogout(&crt);
        ASSERT_EQ(crt.waitForResult(), MegaChatError::ERROR_OK) << "Error chat logout. Error: " << crt.getErrorString();
    }

    MegaApi::addLoggerObject(logger());   // need to restore customized logger
}

void MegaChatApiTest::init(const std::string& log)
{
    std::cout << "[========] Global test environment initialization" << endl;

    getEnv().setLogFile(log);
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
    const string name = string(ti->test_suite_name()) + '.' + ti->name();
    struct stat st = {}; // init all members to default values (0)

    LOG_info << "Test " << name << ": SetUp starting.";
    if (stat(LOCAL_PATH.c_str(), &st) == -1)
    {
#ifdef _WIN32
        _mkdir(LOCAL_PATH.c_str());
#else
        mkdir(LOCAL_PATH.c_str(), 0700);
#endif
    }

    for (unsigned i = 0u; i < NUM_ACCOUNTS; ++i)
    {
        char path[1024];
#ifdef _WIN32
        _getcwd(path, sizeof path);
#else
        if (!getcwd(path, sizeof path))
        {
            LOG_err << "Test " << name << ": getcwd() failed.";
        }
#endif
        megaApi[i] = new MegaApi(APPLICATION_KEY.c_str(), path, USER_AGENT_DESCRIPTION.c_str());
        megaApi[i]->setLogLevel(MegaApi::LOG_LEVEL_DEBUG);
        megaApi[i]->setLoggingName(to_string(i).c_str());
        megaApi[i]->addListener(this);

        megaChatApi[i] = new MegaChatApi(megaApi[i]);
        megaChatApi[i]->setLogLevel(MegaChatApi::LOG_LEVEL_DEBUG);
        megaChatApi[i]->addChatListener(this);

#ifndef KARERE_DISABLE_WEBRTC
        megaChatApi[i]->addChatCallListener(this);
        megaChatApi[i]->addSchedMeetingListener(this);
#endif

        RequestTracker loginRt(megaApi[i]);
        megaApi[i]->login(account(i).getEmail().c_str(), account(i).getPassword().c_str(),
                          &loginRt);

        const bool sdkLoginSuccess = loginRt.waitForResult() == API_OK;
        if (!sdkLoginSuccess)
        {
            LOG_err << "Login failed, clearing resources";

            // destroy MegaChatApi instance
            megaChatApi[i]->removeChatCallListener(this);
            delete megaChatApi[i];
            megaChatApi[i] = nullptr;

            // SDK logout and destroy MegaApi instance
            RequestTracker sdkLocalLogoutRt(megaApi[i]);
            megaApi[i]->localLogout(&sdkLocalLogoutRt);
            const int logoutErr = sdkLocalLogoutRt.waitForResult();
            megaApi[i]->removeListener(this);
            delete megaApi[i];
            megaApi[i] = NULL;

            if (logoutErr != API_OK && logoutErr != API_ESID)
            {
                LOG_err << "Error sdk local logout. Error: " << logoutErr
                        << ' ' << sdkLocalLogoutRt.getErrorString();
            }

            ASSERT_TRUE(false) << "Login failed in SetUp(). Error: " << loginRt.getErrorString();
        }

        // SDK login succeeded
        // kill all sessions to ensure no interferences from other tests running in parallel
        RequestTracker killSessionRT(megaApi[i]);
        megaApi[i]->killSession(INVALID_HANDLE, &killSessionRT);
        ASSERT_EQ(killSessionRT.waitForResult(), API_OK) << "Kill sessions failed in SetUp(). Error: "
                                                         << killSessionRT.getErrorString();

        // Logout from SDK
        RequestTracker logoutTracker(megaApi[i]);
#ifdef ENABLE_SYNC
        megaApi[i]->logout(false, &logoutTracker);
#else
        megaApi[i]->logout(&logoutTracker);
#endif
        const int logoutResult = logoutTracker.waitForResult();
        ASSERT_TRUE(logoutResult == API_OK || logoutResult == API_ESID)
            << "Logout failed in SetUp(). Error: " << logoutResult << ' '
            << logoutTracker.getErrorString();

        for (int j = 0; j < ::mega::MegaRequest::TOTAL_OF_REQUEST_TYPES; ++j)
        {
            requestFlags[i][j] = false;
        }

        initStateChanged[i] = false;
        initState[i] = -1;
        mChatConnectionOnline[i] = false;
        mLoggedInAllChats[i] = false;
        mChatsUpdated[i] = false;
        mChatListUpdated[i].clear();
        lastErrorTransfer[i] = -1;
        chatroom[i] = NULL;
        chatUpdated[i] = false;
        chatItemUpdated[i] = false;
        chatItemClosed[i] = false;
        peersUpdated[i] = false;
        titleUpdated[i] = false;
        chatArchived[i] = false;
        chatPreviewClosed[i] = false;
        mNotTransferRunning[i] = true;
        mPresenceConfigUpdated[i] = false;
        mUsersUpdate[i] = false;

#ifndef KARERE_DISABLE_WEBRTC
        mCallWithIdReceived[i] = false;
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
        mOwnCallPermissionsChanged[i] = false;
#endif
    }

    LOG_info << "Test " << name << ": SetUp finished.";
}

void clearMegaChatApiImplLeftovers();

void MegaChatApiTest::TearDown()
{
    // Required order:
    // 1. clear and leave unused chatrooms
    // 2. logout megaApi
    // 3. logout megaChatApi
    // 4. delete megaChatApi
    // 5. delete megaApi
    const ::testing::TestInfo* ti = ::testing::UnitTest::GetInstance()->current_test_info();
    string name = string(ti->test_suite_name()) + '.' + ti->name();
    LOG_info << "Test " << name << ": TearDown starting.";

    for (unsigned int i = 0; i < NUM_ACCOUNTS; ++i)
    {
        // 1. clear and leave unused chatrooms
        if (megaChatApi[i])
        {
            if (megaChatApi[i]->getInitState() == MegaChatApi::INIT_ONLINE_SESSION ||
                    megaChatApi[i]->getInitState() == MegaChatApi::INIT_OFFLINE_SESSION)
            {
                vector<MegaChatHandle> chatsToSkip;
                for (unsigned j = 0; j < NUM_ACCOUNTS; ++j)
                {
                    if (j == i) { continue; }

                    MegaChatHandle uh = megaChatApi[i]->getUserHandleByEmail(account(j).getEmail().c_str());
                    if (uh != MEGACHAT_INVALID_HANDLE)
                    {
                        std::unique_ptr<MegaChatPeerList> peers(MegaChatPeerList::createInstance());
                        peers->addPeer(uh, MegaChatPeerList::PRIV_STANDARD);
                        MegaChatHandle chatToSkip = getGroupChatRoom({i, j}, peers.get(), MegaChatPeerList::PRIV_UNKNOWN, false);
                        chatsToSkip.push_back(chatToSkip);
                    }
                }
                clearAndLeaveChats(i, chatsToSkip);
            }
        }

        if (megaApi[i] && megaApi[i]->isLoggedIn())
        {
            std::unique_ptr <MegaNode> cloudNode(megaApi[i]->getRootNode());
            purgeCloudTree(i, cloudNode.get());
            std::unique_ptr <MegaNode> rubbishNode(megaApi[i]->getRubbishNode());
            purgeCloudTree(i, rubbishNode.get());
            removePendingContactRequest(i);

            // 2. logout megaApi
            ChatLogoutTracker chatLogoutCrt;
            megaChatApi[i]->addChatRequestListener(&chatLogoutCrt);

            RequestTracker logoutRt(megaApi[i]);
#ifdef ENABLE_SYNC
            megaApi[i]->logout(false, &logoutRt);
#else
            megaApi[i]->logout(&logoutTracker);
#endif
            TEST_LOG_ERROR(logoutRt.waitForResult(60) == API_OK, "Failed to logout from SDK. Error: " + logoutRt.getErrorString());
            TEST_LOG_ERROR(chatLogoutCrt.waitForResult() == MegaChatError::ERROR_OK, "Failed to auto-logout from chat. Error: " + chatLogoutCrt.getErrorString());
            megaChatApi[i]->removeChatRequestListener(&chatLogoutCrt);
        }

        if (megaChatApi[i])
        {
            // 3. logout megaChatApi
            ChatRequestTracker crtLogout(megaChatApi[i]);
            megaChatApi[i]->logout(&crtLogout);
            TEST_LOG_ERROR(crtLogout.waitForResult(60) == MegaChatError::ERROR_OK, "Failed to logout from Chat. Error: " + crtLogout.getErrorString());
            MegaApi::addLoggerObject(logger());   // need to restore customized logger

#ifndef KARERE_DISABLE_WEBRTC
            megaChatApi[i]->removeChatCallListener(this);
            megaChatApi[i]->removeSchedMeetingListener(this);
#endif
            megaChatApi[i]->removeChatListener(this);

            // 4. delete megaChatApi
            delete megaChatApi[i];
            megaChatApi[i] = NULL;
        }

        if (megaApi[i])
        {
            // 5. delete megaApi
            delete megaApi[i];
            megaApi[i] = NULL;
        }
    }

    purgeLocalTree(LOCAL_PATH);

    // Clear MegaChatApi leftovers AFTER MegaApi instances have been released
    clearMegaChatApiImplLeftovers();

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

bool MegaChatApiTest::exitWait(ExitBoolFlags& eF, const bool waitForAll) const
{
    return waitForAll
        ? eF.allEqualTo(true)
        : eF.anyEqualTo(true);
};

bool MegaChatApiTest::waitForMultiResponse(ExitBoolFlags& eF, bool waitForAll, unsigned int timeout) const
{
    timeout *= 1000000; // convert to micro-seconds
    unsigned int tWaited = 0;    // microseconds
    bool connRetried = false;
    while (!exitWait(eF, waitForAll))
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

void MegaChatApiTest::waitForAction(int maxAttempts, ExitBoolFlags& eF, const std::string& actionMsg, bool waitForAll, bool resetFlags, unsigned int timeout, std::function<void()>action)
{
    ASSERT_TRUE(action) << "waitForAction: no valid action provided";

    if (resetFlags)
    {
        ASSERT_TRUE(eF.updateAll(false)) << "waitForAction: Cannot reset all ExitBoolFlags";
    }

    int retries = 0;
    while (!exitWait(eF, waitForAll))
    {
        action();
        if (!waitForMultiResponse(eF, waitForAll, timeout))
        {
            std::string msg = "Attempt ["; msg.append(std::to_string(retries)).append("] for ").append(actionMsg).append(":\n ");
            for (size_t i = 0; i < eF.size(); i++)
            {
                msg += eF.printAll();
            }
            LOG_debug << msg;
            ASSERT_LE(++retries, maxAttempts) << "Max attempts exceeded for " << actionMsg;
        }
    }
}

void MegaChatApiTest::waitForAction(int maxAttempts, std::vector<bool*> exitFlags, const std::vector<std::string>& flagsStr, const std::string& actionMsg, bool waitForAll, bool resetFlags, unsigned int timeout, std::function<void()>action)
{
    ASSERT_TRUE(exitFlags.size() == flagsStr.size() || flagsStr.empty()) << "waitForAction: invalid flags provided";
    ASSERT_TRUE(action) << "waitForAction: no valid action provided";

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
 * @brief MegaChatApiTest.BasicTest
 * + This test can be taken as reference for creating new ones.
 */
TEST_F(MegaChatApiTest, BasicTest)
{
    //========================================================================//
    // Auxiliar test functions
    //========================================================================//
    /** add here all auxiliar lambdas that this test can require **/

    CleanupFunction testCleanup = [this]
    {
        LOG_debug << "MegaChatApiTest.BasicTest: Cleanup";
        closeOpenedChatrooms();
        logoutTestAccounts();
    };
    MegaMrProper p (testCleanup);

    // login into all involved accounts for this test, and establish required contact relationships
    // Note: all involved accounts in this test, must be added to mSessions and mAccounts
    const unsigned a1 = 0;
    const unsigned a2 = 1;
    mData.mOpIdx = a1; // set test operator role index
    mData.mSessions.emplace(a1, login(a1));
    mData.mSessions.emplace(a2, login(a2));
    const MegaChatHandle a1Uh = megaChatApi[a1]->getMyUserHandle();
    const MegaChatHandle a2Uh = megaChatApi[a2]->getMyUserHandle();
    mData.mAccounts.emplace(a1, a1Uh);
    mData.mAccounts.emplace(a2, a2Uh);
    ASSERT_NO_FATAL_FAILURE(mData.areSessionsValid(););
    ASSERT_NO_FATAL_FAILURE(makeContact(a1, a2););
    ASSERT_NO_FATAL_FAILURE(mData.checkSessionsAndAccounts(););

    // set chat selection criteria
    mData.mChatOptions.mCreate          = true;
    mData.mChatOptions.mPublicChat      = true;
    mData.mChatOptions.mMeetingRoom     = false;
    mData.mChatOptions.mWaitingRoom     = false;
    mData.mChatOptions.mSpeakRequest    = false;
    mData.mChatOptions.mOpenInvite      = false;

    // set chat operator idx and privileges, and create chat participants list
    // chat operator idx corresponds with idx of account from which we retrieve chatroom
    mData.mChatOptions.mChatOpIdx = a1;
    mData.mChatOptions.mOpPriv = megachat::MegaChatPeerList::PRIV_MODERATOR;
    mData.mChatOptions.mChatPeerList.reset(megachat::MegaChatPeerList::createInstance());
    mData.mChatOptions.mChatPeerList->addPeer(a2Uh, MegaChatPeerList::PRIV_STANDARD);
    mData.mChatOptions.mChatPeerIdx.emplace_back(a2);

    // get a group chatroom
    mData.mChatid = getGroupChatRoom();
    ASSERT_NE(mData.mChatid, MEGACHAT_INVALID_HANDLE) << "Can't get a chatroom with selected criteria";

    // open chatroom (just add chatroom listeners for chatroom participants)
    //std::shared_ptr<TestChatRoomListener> crl(new TestChatRoomListener(this, megaChatApi, mData.mChatid));
    auto crl = std::make_shared<TestChatRoomListener>(this, megaChatApi, mData.mChatid);
    mData.mChatroomListeners.emplace(a1, crl);
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(mData.mChatid, crl.get())) << "Can't open chatRoom a1 account";
    mData.mChatroomListeners.emplace(a2, crl);
    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(mData.mChatid, crl.get())) << "Can't open chatRoom a2 account";

    // load history
    ASSERT_GE(loadHistory(a1, mData.mChatid, crl.get()), 0);
    ASSERT_GE(loadHistory(a2, mData.mChatid, crl.get()), 0);

    LOG_debug << "#### Test1: Change chatroom title from a1 ####";
    setChatTitle(std::string("Title ") + std::to_string(time(NULL)), 120 /*waitSecs*/);
}

#ifndef KARERE_DISABLE_WEBRTC
/**
 * @brief MegaChatApiTest.WaitingRoomsJoiningOrder
 * + Test1: Check Waiting room order from A, when B and C answer call.
 */
TEST_F(MegaChatApiTest, WaitingRoomsJoiningOrder)
{
    /** Checks that callid for account i has been received at onChatCallUpdate(CALL_STATUS_IN_PROGRESS) **/
    auto checkCallIdInProgress = [this](unsigned i) -> void
    {
        std::unique_ptr<MegaChatCall> call(megaChatApi[i]->getChatCall(mData.mChatid));
        ASSERT_TRUE(call) << "Can't get call for a1. Callid: " << getChatIdStrB64(mData.mChatid);

        MegaChatHandle* callId = handleVars().getVar(i, "CallIdInProgress");
        ASSERT_TRUE(callId) << "Can't get CallInProgress var for a1";
        ASSERT_NE(*callId, MEGACHAT_INVALID_HANDLE) << "Invalid callid received at onChatCallUpdate for a1";
        ASSERT_NE(call->getCallId(), MEGACHAT_INVALID_HANDLE) << "Invalid callid in MegaChatCall for a1";
        ASSERT_TRUE(*callId == call->getCallId()) << "Callids doesn't match "
                                                  << getChatIdStrB64(mData.mChatid)
                                                  << " " << getChatIdStrB64(*callId);
    };

    CleanupFunction testCleanup = [this] () -> void
    {
        ExitBoolFlags eF;
        addBoolVarAndExitFlag(mData.mOpIdx, eF, "CallDestroyed", false); // mOpIdx - onChatCallUpdate(CALL_STATUS_DESTROYED)
        endChatCallTestCleanup(mData.mOpIdx, eF, mData.mChatid);
        closeOpenedChatrooms();
        cleanChatVideoListeners();
        logoutTestAccounts();
    };
    MegaMrProper p (testCleanup);

    // login into all involved accounts for this test, and establish required contact relationships
    // Note: all involved accounts in this test, must be added to mSessions and mAccounts
    const unsigned a1 = 0;
    const unsigned a2 = 1;
    const unsigned a3 = 2;
    mData.mOpIdx = a1;  // set test operator role index
    mData.mSessions.emplace(a1, login(a1));
    mData.mSessions.emplace(a2, login(a2));
    mData.mSessions.emplace(a3, login(a3));
    const MegaChatHandle a1Uh = megaChatApi[a1]->getMyUserHandle();
    const MegaChatHandle a2Uh = megaChatApi[a2]->getMyUserHandle();
    const MegaChatHandle a3Uh = megaChatApi[a3]->getMyUserHandle();
    mData.mAccounts.emplace(a1, a1Uh);
    mData.mAccounts.emplace(a2, a2Uh);
    mData.mAccounts.emplace(a3, a3Uh);
    ASSERT_NO_FATAL_FAILURE(mData.areSessionsValid(););
    ASSERT_NO_FATAL_FAILURE(makeContact(a1, a2););
    ASSERT_NO_FATAL_FAILURE(makeContact(a1, a3););
    ASSERT_NO_FATAL_FAILURE(makeContact(a2, a3););
    ASSERT_NO_FATAL_FAILURE(mData.checkSessionsAndAccounts(););

    // set chat selection criteria
    mData.mChatOptions.mCreate          = true;
    mData.mChatOptions.mPublicChat      = true;
    mData.mChatOptions.mMeetingRoom     = true;
    mData.mChatOptions.mWaitingRoom     = true;
    mData.mChatOptions.mSpeakRequest    = false;
    mData.mChatOptions.mOpenInvite      = false;

    // set chat operator idx and privileges, and create chat participants list
    // chat operator idx corresponds with idx of account from which we retrieve chatroom
    mData.mChatOptions.mChatOpIdx = a1;
    mData.mChatOptions.mOpPriv = megachat::MegaChatPeerList::PRIV_MODERATOR;
    mData.mChatOptions.mChatPeerList.reset(megachat::MegaChatPeerList::createInstance());
    mData.mChatOptions.mChatPeerList->addPeer(a2Uh, MegaChatPeerList::PRIV_STANDARD);
    mData.mChatOptions.mChatPeerIdx.emplace_back(a2);
    mData.mChatOptions.mChatPeerList->addPeer(a3Uh, MegaChatPeerList::PRIV_STANDARD);
    mData.mChatOptions.mChatPeerIdx.emplace_back(a3);

    // init scheduled meeting local data
    const time_t now = time(nullptr);
    initLocalSchedMeeting(megachat::MEGACHAT_INVALID_HANDLE /*chatId*/, megachat::MEGACHAT_INVALID_HANDLE /*schedId*/, "Europe/Madrid",
                          "SMChat_" + std::to_string(now), "SMChat_Desc", now + 300 /*startDate*/, now + 600 /*endDate*/,
                          megachat::MEGACHAT_INVALID_TIMESTAMP /*overrides*/, megachat::MEGACHAT_INVALID_TIMESTAMP /*newStartDate*/,
                          megachat::MEGACHAT_INVALID_TIMESTAMP /*newEndDate*/, false /*cancelled*/, false /*newCancelled*/,
                          true /*publicChat*/, false /*speakRequest*/, true /*waitingRoom*/, false /* openInvite*/, true /*isMeeting*/,
                          false /*sendEmails*/, MegaChatScheduledRules::FREQ_DAILY, MegaChatScheduledRules::INTERVAL_INVALID,
                          MEGACHAT_INVALID_TIMESTAMP /*rulesUntil*/, mData.mChatOptions.mChatPeerList.get(),
                          nullptr /*rulesByWeekDay*/, nullptr /*rulesByMonthDay*/, nullptr /*rulesByMonthWeekDay*/);

    // get a group chatroom with waiting room enabled and a scheduled meeting
    std::string err = "Cannot get a chatroom ";
    mData.mChatid = getGroupChatRoom();
    ASSERT_NE(mData.mChatid, MEGACHAT_INVALID_HANDLE) << "Invalid chatid returned by getGroupChatRoom";

    // retrieve chatroom by chatid, and check that waiting room is enabled and has a valid scheduled meeting
    std::unique_ptr<MegaChatRoom> chatroom(megaChatApi[a1]->getChatRoom(mData.mChatid));
    ASSERT_TRUE(chatroom) << err << "with selected criteria";
    ASSERT_TRUE(chatroom->isWaitingRoom()) << err << "with waiting room enabled" << "chatid: " << getChatIdStrB64(mData.mChatid);
    std::unique_ptr<MegaChatScheduledMeetingList> smlist(megaChatApi[a1]->getScheduledMeetingsByChat(mData.mChatid));
    ASSERT_TRUE(smlist && smlist->size() == 1) << err << "with a scheduled meeting" << "chatid: " << getChatIdStrB64(mData.mChatid);
    const MegaChatHandle schedId = smlist->at(0)->schedId();

    // open chatroom (just add chatroom listeners for chatroom participants)
    std::shared_ptr<TestChatRoomListener> crl(new TestChatRoomListener(this, megaChatApi, mData.mChatid));
    mData.mChatroomListeners.emplace(a1, crl);
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(mData.mChatid, crl.get())) << "Can't open chatRoom a1 account";

    LOG_debug << "#### Test1: Check Waiting room order ####";
    // A Starts call with waiting room enabled. B and C answers call,
    // ensure that users in WR are ordered by joining time.
    ExitBoolFlags eF;
    MegaChatHandle invalHandle = MEGACHAT_INVALID_HANDLE;
    addHandleVar(a1, "CallIdInProgress", invalHandle);                          // a1 - callid received at onChatCallUpdate(CALL_STATUS_IN_PROGRESS)
    addBoolVarAndExitFlag(a1, eF, "CallReceived"  , false);                     // a1 - onChatCallUpdate(CALL_STATUS_INITIAL)
    addBoolVarAndExitFlag(a2, eF, "CallReceived"  , false);                     // a2 - onChatCallUpdate(CALL_STATUS_INITIAL)
    addBoolVarAndExitFlag(a3, eF, "CallReceived"  , false);                     // a3 - onChatCallUpdate(CALL_STATUS_INITIAL)
    addBoolVarAndExitFlag(a1, eF, "CallInProgress", false);                     // a1 - onChatCallUpdate(CALL_STATUS_IN_PROGRESS)
    startWaitingRoomCall(a1, eF, mData.mChatid, schedId,
                         false /*audio*/, false /*video*/);
    checkCallIdInProgress(a1);  // check received callid for caller(a1)

    clearTemporalVars();  // important: this prevents conflicts in following test cases of this integration test

    // a2 answers call
    ExitBoolFlags eF1;
    addBoolVarAndExitFlag(a2, eF1, "CallWR"        , false);                     // a2 - onChatCallUpdate(CALL_STATUS_WAITING_ROOM)
    addBoolVarAndExitFlag(a1, eF1, "CallWrChanged" , false);                     // a1 - onChatCallUpdate(CHANGE_TYPE_WR_USERS_ENTERED)
    answerChatCall(a2, eF1, mData.mChatid, false /*video*/,
                   false /*audio*/);

    clearTemporalVars();    // clean-up for next test cases

    // a3 answers call
    ExitBoolFlags eF2;
    addBoolVarAndExitFlag(a3, eF2, "CallWR"        , false);                      // a3 - onChatCallUpdate(CALL_STATUS_WAITING_ROOM)
    addBoolVarAndExitFlag(a1, eF2, "CallWrChanged" , false);                      // a1 - onChatCallUpdate(CHANGE_TYPE_WR_USERS_ENTERED)
    answerChatCall(a3, eF2, mData.mChatid, false /*video*/,
                   false /*audio*/);

    clearTemporalVars();  // clean-up for next test cases

    // a1 checks waiting room participants order
    std::unique_ptr<MegaChatCall>call(megaChatApi[a1]->getChatCall(mData.mChatid));
    ASSERT_TRUE(call) << "Can't get chat call from a1 for chatid: " << getChatIdStrB64(mData.mChatid);
    const MegaChatWaitingRoom* wr = call->getWaitingRoom();
    ASSERT_TRUE(wr) << "Can't get waiting room from a1 for chatid: " << getChatIdStrB64(mData.mChatid);
    std::unique_ptr<::mega::MegaHandleList>wrUsers(wr->getUsers());
    ASSERT_TRUE(wrUsers) << "Can't get waiting room user list from a1 for chatid: " << getChatIdStrB64(mData.mChatid);
    ASSERT_TRUE(wrUsers->size() == 2) << "Unexpected size for Waiting room user list from a1 for chatid: " << getChatIdStrB64(mData.mChatid);
    ASSERT_EQ(wrUsers->get(0), a2Uh) << "First user in waiting room should be a2. chatid: " << getChatIdStrB64(mData.mChatid);
    ASSERT_EQ(wrUsers->get(1), a3Uh) << "Second user in waiting room should be a3. chatid: " << getChatIdStrB64(mData.mChatid);
}

/**
 * @brief MegaChatApiTest.RejectCall
 * + Test1: A starts chat call, B answers it
 * + Test2: B rejects chat call
 */
TEST_F(MegaChatApiTest, RejectCall)
{
    /** Checks that callId for account i has been received at onChatCallUpdate(CALL_STATUS_IN_PROGRESS) **/
    auto checkCallIdInProgress = [this](unsigned i) -> void
    {
        std::unique_ptr<MegaChatCall> call(megaChatApi[i]->getChatCall(mData.mChatid));
        ASSERT_TRUE(call) << "Can't get call for " << std::to_string(i) <<". CallId: " << getChatIdStrB64(mData.mChatid);

        MegaChatHandle* callId = handleVars().getVar(i, "CallIdInProgress");
        ASSERT_TRUE(callId) << "Can't get CallInProgress var for " << std::to_string(i);
        ASSERT_NE(*callId, MEGACHAT_INVALID_HANDLE) << "Invalid callId received at onChatCallUpdate for: " << std::to_string(i);
        ASSERT_NE(call->getCallId(), MEGACHAT_INVALID_HANDLE) << "Invalid callId in MegaChatCall for: " << std::to_string(i);
        ASSERT_TRUE(*callId == call->getCallId()) << "CallId's don't match "
                                                  << getChatIdStrB64(mData.mChatid)
                                                  << " " << getChatIdStrB64(*callId);
    };

    CleanupFunction testCleanup = [this]() -> void
    {
        ExitBoolFlags eF;
        addBoolVarAndExitFlag(mData.mOpIdx, eF, "CallDestroyed", false); // mOpIdx - onChatCallUpdate(CALL_STATUS_DESTROYED)
        endChatCallTestCleanup(mData.mOpIdx, eF, mData.mChatid);
        closeOpenedChatrooms();
        cleanChatVideoListeners();
        logoutTestAccounts();
    };
    MegaMrProper p (testCleanup);

    // login into all involved accounts for this test, and establish required contact relationships
    // Note: all involved accounts in this test, must be added to mSessions and mAccounts
    const unsigned a1 = 0;
    const unsigned a2 = 1;
    mData.mOpIdx = a1;  // set test operator role index
    mData.mSessions.emplace(a1, login(a1));
    mData.mSessions.emplace(a2, login(a2));
    const MegaChatHandle a1Uh = megaChatApi[a1]->getMyUserHandle();
    const MegaChatHandle a2Uh = megaChatApi[a2]->getMyUserHandle();
    mData.mAccounts.emplace(a1, a1Uh);
    mData.mAccounts.emplace(a2, a2Uh);
    ASSERT_NO_FATAL_FAILURE(mData.areSessionsValid(););
    ASSERT_NO_FATAL_FAILURE(makeContact(a1, a2););
    ASSERT_NO_FATAL_FAILURE(mData.checkSessionsAndAccounts(););

    // set chat selection criteria
    mData.mChatOptions.mCreate          = true;
    mData.mChatOptions.mPublicChat      = true;
    mData.mChatOptions.mMeetingRoom     = false;
    mData.mChatOptions.mWaitingRoom     = false;
    mData.mChatOptions.mSpeakRequest    = false;
    mData.mChatOptions.mOpenInvite      = false;

    // set chat operator idx and privileges, and create chat participants list
    // chat operator idx corresponds with idx of account from which we retrieve chatroom
    mData.mChatOptions.mChatOpIdx = a1;
    mData.mChatOptions.mOpPriv = megachat::MegaChatPeerList::PRIV_MODERATOR;
    mData.mChatOptions.mChatPeerList.reset(megachat::MegaChatPeerList::createInstance());
    mData.mChatOptions.mChatPeerList->addPeer(a2Uh, MegaChatPeerList::PRIV_STANDARD);
    mData.mChatOptions.mChatPeerIdx.emplace_back(a2);

    // get a chat room
    std::string err = "Cannot get a chatroom ";
    mData.mChatid = getGroupChatRoom();
    ASSERT_NE(mData.mChatid, MEGACHAT_INVALID_HANDLE) << "Invalid chatid returned by getGroupChatRoom";

    // retrieve chatroom by chatid
    std::unique_ptr<MegaChatRoom> chatroom(megaChatApi[a1]->getChatRoom(mData.mChatid));
    ASSERT_TRUE(chatroom) << err << "with selected criteria";

    // open chatroom (just add chatroom listeners for chatroom participants)
    std::shared_ptr<TestChatRoomListener> crl(new TestChatRoomListener(this, megaChatApi, mData.mChatid));
    mData.mChatroomListeners.emplace(a1, crl);
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(mData.mChatid, crl.get())) << "Can't open chatRoom a1 account";

    LOG_debug << "#### Test1: A starts chat call, B answers it ####";
    // ensure that users in WR are ordered by joining time.
    ExitBoolFlags eF;
    MegaChatHandle invalHandle = MEGACHAT_INVALID_HANDLE;
    addHandleVar(a1, "CallIdInProgress", invalHandle);                           // a1 - callId received at onChatCallUpdate(CALL_STATUS_IN_PROGRESS)
    addBoolVarAndExitFlag(a1, eF, "CallReceived"  , false);                      // a1 - onChatCallUpdate(CALL_STATUS_INITIAL)
    addBoolVarAndExitFlag(a2, eF, "CallReceived"  , false);                      // a2 - onChatCallUpdate(CALL_STATUS_INITIAL)
    addBoolVarAndExitFlag(a1, eF, "CallInProgress", false);                      // a1 - onChatCallUpdate(CALL_STATUS_IN_PROGRESS)
    startCallInChat(a1, eF, mData.mChatid, false /*audio*/,
                    false /*video*/, false /*notRinging*/);
    checkCallIdInProgress(a1); // check received callId for caller(a1)

    clearTemporalVars();  // clean-up for next test cases

    LOG_debug << "#### Test2: B rejects chat call ####";
    ExitBoolFlags eF1;
    addBoolVarAndExitFlag(a2, eF1, "CallStopsRinging"  , false);                  // a2 - onChatCallUpdate(CHANGE_TYPE_RINGING_STATUS)
    rejectCall(a2, eF1, mData.mChatid);
}
#endif

/**
 * @brief MegaChatApiTest.ResumeSession
 *
 * This test does the following:
 *
 * - Test1: Create a new session
 * - Test2: Resume with previous sesion
 * - Test3: Resume an existing session without karere cache
 * - Test4: Re-create Karere cache without login out from SDK
 * - Test5: Close session
 * - Test6: Login with chat enabled, transition to disabled and back to enabled
 * - Test7: Login with chat disabled, transition to enabled
 * - Test8: Go into background, sleep and back to foreground
 *
 */
TEST_F(MegaChatApiTest, ResumeSession)
{
    unsigned accountIndex = 0;

    LOG_debug << "#### Test1: Create a new session ####";
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

    LOG_debug << "#### Test2: Resume with previous sesion ####";
    ASSERT_NO_FATAL_FAILURE({ logout(accountIndex, false); }); // keeps session alive
    char *tmpSession = login(accountIndex, session);
    ASSERT_TRUE(tmpSession);
    ASSERT_STREQ(session, tmpSession) << "Bad session key";
    delete [] tmpSession;   tmpSession = NULL;

    ASSERT_NO_FATAL_FAILURE({ checkEmail(accountIndex); });

    LOG_debug << "#### Test3: Resume an existing session without karere cache ####";
    RequestTracker logoutTracker(megaApi[accountIndex]);  // keeping cache
    megaApi[accountIndex]->localLogout(&logoutTracker);
    ASSERT_EQ(logoutTracker.waitForResult(), API_OK) << "Error local sdk logout. Error: " << logoutTracker.getErrorString();

    // logout from Karere removing cache
    ChatRequestTracker crtLogout(megaChatApi[accountIndex]);
    megaChatApi[accountIndex]->logout(&crtLogout);
    ASSERT_EQ(crtLogout.waitForResult(), MegaChatError::ERROR_OK) << "Error chat logout. Error: " << crtLogout.getErrorString();
    MegaApi::addLoggerObject(logger());   // need to restore customized logger
    // try to initialize chat engine with cache --> should fail
    bool *flagInit = &initStateChanged[accountIndex]; *flagInit = false;
    ASSERT_EQ(megaChatApi[accountIndex]->init(session), MegaChatApi::INIT_NO_CACHE) <<
                     "Wrong chat initialization state (4).";
    ASSERT_TRUE(waitForResponse(flagInit)) << "Expired timeout for change init state";
    MegaApi::removeLoggerObject(logger());
    megaApi[accountIndex]->invalidateCache();

    LOG_debug << "#### Test4: Re-create Karere cache without login out from SDK ####";
    flagInit = &initStateChanged[accountIndex]; *flagInit = false;
    RequestTracker loginTracker(megaApi[accountIndex]);
    session ? megaApi[accountIndex]->fastLogin(session, &loginTracker)
            : megaApi[accountIndex]->login(account(accountIndex).getEmail().c_str(),
                                           account(accountIndex).getPassword().c_str(),
                                           &loginTracker);
    ASSERT_EQ(loginTracker.waitForResult(), API_OK) << "Error sdk fast login. Error: " << loginTracker.getErrorString();

    // fetchnodes in SDK
    RequestTracker fetchNodesTracker(megaApi[accountIndex]);
    megaApi[accountIndex]->fetchNodes(&fetchNodesTracker);
    ASSERT_EQ(fetchNodesTracker.waitForResult(), API_OK) << "Error fetchnodes. Error: " << fetchNodesTracker.getErrorString();
    ASSERT_TRUE(waitForResponse(flagInit)) << "Expired timeout for change init state";
    int initStateValue = initState[accountIndex];
    ASSERT_EQ(initStateValue, MegaChatApi::INIT_ONLINE_SESSION) << "Wrong chat initialization state (5).";

    // check there's a list of chats already available
    MegaChatListItemList *list = megaChatApi[accountIndex]->getChatListItems();
    ASSERT_TRUE(list->size()) << "Chat list item is empty";
    delete list; list = NULL;

    LOG_debug << "#### Test5: Close session ####";
    ASSERT_NO_FATAL_FAILURE({ logout(accountIndex, true); });
    delete [] session; session = NULL;

    LOG_debug << "#### Test6: Login with chat enabled, transition to disabled and back to enabled ####";
    session = login(accountIndex);
    ASSERT_TRUE(session) << "Empty session key";
    // fully disable chat: logout + remove logger + delete MegaChatApi instance
    ChatRequestTracker crtLogout2(megaChatApi[accountIndex]);
    megaChatApi[accountIndex]->logout(&crtLogout2);
    // tolerate -11 (Access denied) which can be returned here
    //
    // debugging this is extremely tricky, because memory corruption can occur when setting breakpoints due to
    // MegaChatApiImpl::sendPendingRequests() -> case MegaChatRequest::TYPE_LOGOUT -> delete mClient;
    // which should happen with a delay (how much?), but often will crash after continuing from the breakpoint
    int logoutResult = crtLogout2.waitForResult();
    TEST_LOG_ERROR(logoutResult == MegaChatError::ERROR_OK, "Error chat logout (2). Error: " +
                   std::to_string(logoutResult) + " (" + crtLogout2.getErrorString() + ')');
    MegaApi::addLoggerObject(logger());   // need to restore customized logger
    delete megaChatApi[accountIndex];
    // create a new MegaChatApi instance
    megaChatApi[accountIndex] = new MegaChatApi(megaApi[accountIndex]);
    megaChatApi[accountIndex]->setLogLevel(MegaChatApi::LOG_LEVEL_DEBUG);
    megaChatApi[accountIndex]->addChatListener(this);
    MegaChatApi::setLoggerObject(logger());
    // back to enabled: init + fetchnodes + connect
    ASSERT_EQ(megaChatApi[accountIndex]->init(session), MegaChatApi::INIT_NO_CACHE) <<
                     "Wrong chat initialization state (6).";

    MegaApi::removeLoggerObject(logger());
    flagInit = &initStateChanged[accountIndex]; *flagInit = false;
    RequestTracker fetchNodesTracker2(megaApi[accountIndex]);
    megaApi[accountIndex]->fetchNodes(&fetchNodesTracker2);
    ASSERT_EQ(fetchNodesTracker2.waitForResult(), API_OK) << "Error fetchnodes. Error: " << fetchNodesTracker2.getErrorString();
    ASSERT_TRUE(waitForResponse(flagInit)) << "Expired timeout for change init state";
    initStateValue = initState[accountIndex];
    ASSERT_EQ(initStateValue, MegaChatApi::INIT_ONLINE_SESSION) <<
                     "Wrong chat initialization state (7).";

    // check there's a list of chats already available
    list = megaChatApi[accountIndex]->getChatListItems();
    ASSERT_TRUE(list->size()) << "Chat list item is empty";
    delete list; list = NULL;
    // close session and remove cache
    ASSERT_NO_FATAL_FAILURE({ logout(accountIndex, true); });
    delete [] session; session = NULL;

    LOG_debug << "#### Test7: Login with chat disabled, transition to enabled ####";
    // fully disable chat: remove logger + delete MegaChatApi instance
    delete megaChatApi[accountIndex];
    // create a new MegaChatApi instance
    megaChatApi[accountIndex] = new MegaChatApi(megaApi[accountIndex]);
    megaChatApi[accountIndex]->setLogLevel(MegaChatApi::LOG_LEVEL_DEBUG);
    megaChatApi[accountIndex]->addChatListener(this);
    MegaChatApi::setLoggerObject(logger());
    // login in SDK
    RequestTracker loginTracker2(megaApi[accountIndex]);
    megaApi[accountIndex]->login(account(accountIndex).getEmail().c_str(),
                                 account(accountIndex).getPassword().c_str(),
                                 &loginTracker2);
    ASSERT_EQ(loginTracker2.waitForResult(), API_OK) << "Error fast login. Error: " << loginTracker2.getErrorString();
    session = megaApi[accountIndex]->dumpSession();
    // fetchnodes in SDK
    RequestTracker fetchNodesTracker3(megaApi[accountIndex]);
    megaApi[accountIndex]->fetchNodes(&fetchNodesTracker3);
    ASSERT_EQ(fetchNodesTracker3.waitForResult(), API_OK) << "Error fetch nodes. Error: " << fetchNodesTracker3.getErrorString();
    // init in Karere
    ASSERT_EQ(megaChatApi[accountIndex]->init(session), MegaChatApi::INIT_NO_CACHE) << "Bad Megachat state.";
    MegaApi::removeLoggerObject(logger());
    // full-fetchndoes in SDK to regenerate cache in Karere
    flagInit = &initStateChanged[accountIndex]; *flagInit = false;
    RequestTracker fetchNodesTracker4(megaApi[accountIndex]);
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

    LOG_debug << "#### Test8: Go into background, sleep and back to foreground ####";
    for(int i = 0; i < 3; i++)
    {
        ChatRequestTracker crtBkgrTrue(megaChatApi[accountIndex]);
        megaChatApi[accountIndex]->setBackgroundStatus(true, &crtBkgrTrue);
        ASSERT_EQ(crtBkgrTrue.waitForResult(), MegaChatError::ERROR_OK) << "Failed to set background status on. Error: " << crtBkgrTrue.getErrorString();

        logger()->postLog("========== Enter background status ================= ");
        std::this_thread::sleep_for(std::chrono::seconds(15));

        ChatRequestTracker crtBkgrFalse(megaChatApi[accountIndex]);
        megaChatApi[accountIndex]->setBackgroundStatus(false, &crtBkgrFalse);
        ASSERT_EQ(crtBkgrFalse.waitForResult(), MegaChatError::ERROR_OK) << "Failed to set background status off. Error: " << crtBkgrFalse.getErrorString();

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
 * - Test1: Login
 * - Test2: Set status busy
 * - Test3: Set status online
 * - Test4: enable auto-away with 5 seconds timeout (to force to send values to the server)
 *
 */
TEST_F(MegaChatApiTest, SetOnlineStatus)
{
    LOG_debug << "#### Test1: Login ####";
    unsigned accountIndex = 0;
    bool *flagPresence = &mPresenceConfigUpdated[accountIndex]; *flagPresence = false;
    char *sesion = login(accountIndex);
    ASSERT_TRUE(sesion);

    ASSERT_TRUE(waitForResponse(flagPresence)) << "Presence config not received after " << maxTimeout << " seconds";

    // Reset status to online before starting the test
    if (megaChatApi[accountIndex]->getPresenceConfig()->getOnlineStatus() != MegaChatApi::STATUS_ONLINE)
    {
        ChatRequestTracker crtOnline(megaChatApi[accountIndex]);
        megaChatApi[accountIndex]->setOnlineStatus(MegaChatApi::STATUS_ONLINE, &crtOnline);
        ASSERT_EQ(crtOnline.waitForResult(), MegaChatError::ERROR_OK) << "Failed to set online status. Error: " << crtOnline.getErrorString();
    }

    LOG_debug << "#### Test2: Set status busy ####";
    flagPresence = &mPresenceConfigUpdated[accountIndex]; *flagPresence = false;
    bool* flagStatus = &mOnlineStatusUpdated[accountIndex]; *flagStatus = false;
    ChatRequestTracker crtBusy(megaChatApi[accountIndex]);
    megaChatApi[accountIndex]->setOnlineStatus(MegaChatApi::STATUS_BUSY, &crtBusy);
    ASSERT_EQ(crtBusy.waitForResult(), MegaChatError::ERROR_OK) << "Failed to set busy status. Error: " << crtBusy.getErrorString();
    ASSERT_TRUE(waitForResponse(flagPresence)) << "Presence config not received after " << maxTimeout << " seconds";
    ASSERT_TRUE(waitForResponse(flagStatus)) << "Online status not received after " << maxTimeout << " seconds";

    LOG_debug << "#### Test3: Set status online ####";
    flagStatus = &mOnlineStatusUpdated[accountIndex]; *flagStatus = false;
    flagPresence = &mPresenceConfigUpdated[accountIndex]; *flagPresence = false;
    ChatRequestTracker crtOnline(megaChatApi[accountIndex]);
    megaChatApi[accountIndex]->setOnlineStatus(MegaChatApi::STATUS_ONLINE, &crtOnline);
    ASSERT_EQ(crtOnline.waitForResult(), MegaChatError::ERROR_OK) << "Failed to set online status (2). Error: " << crtOnline.getErrorString();
    ASSERT_TRUE(waitForResponse(flagPresence)) << "Presence config not received after " << maxTimeout << " seconds";
    ASSERT_TRUE(waitForResponse(flagStatus)) << "Online status not received after " << maxTimeout << " seconds";

    LOG_debug << "#### Test4: enable auto-away with 5 seconds timeout (to force to send values to the server) ####";
    int64_t autowayTimeout = 5;
    if (megaChatApi[accountIndex]->getPresenceConfig()->getAutoawayTimeout() == autowayTimeout)
    {
        autowayTimeout ++;
    }

    flagPresence = &mPresenceConfigUpdated[accountIndex]; *flagPresence = false;
    ChatRequestTracker crtAutoaway(megaChatApi[accountIndex]);
    megaChatApi[accountIndex]->setPresenceAutoaway(true, autowayTimeout, &crtAutoaway);
    ASSERT_EQ(crtAutoaway.waitForResult(), MegaChatError::ERROR_OK) << "Failed to set presence autoaway. Error: " << crtAutoaway.getErrorString();
    ASSERT_TRUE(waitForResponse(flagPresence)) << "Presence config not received after " << maxTimeout << " seconds";

    // disable persist
    if (megaChatApi[accountIndex]->getPresenceConfig()->isPersist())
    {
        flagPresence = &mPresenceConfigUpdated[accountIndex]; *flagPresence = false;
        ChatRequestTracker crtUAutoaway(megaChatApi[accountIndex]);
        megaChatApi[accountIndex]->setPresencePersist(false, &crtUAutoaway);
        ASSERT_EQ(crtUAutoaway.waitForResult(), MegaChatError::ERROR_OK) << "Failed to unset presence autoaway. Error: " << crtUAutoaway.getErrorString();
        ASSERT_TRUE(waitForResponse(flagPresence)) << "Presence config not received after " << maxTimeout << " seconds";
    }

    // Set signal activity true, signal activity to false is sent automatically by presenced client
    ChatRequestTracker crtActivity(megaChatApi[accountIndex]);
    megaChatApi[accountIndex]->signalPresenceActivity(&crtActivity);
    ASSERT_EQ(crtActivity.waitForResult(), MegaChatError::ERROR_OK) << "Failed to signal presence activity. Error: " << crtActivity.getErrorString();

    LOG_debug << "Going to sleep for longer than autoaway timeout";
    MegaChatPresenceConfig *config = megaChatApi[accountIndex]->getPresenceConfig();
    std::this_thread::sleep_for(std::chrono::seconds(static_cast<unsigned int>(config->getAutoawayTimeout() + 12)));   // +12 to ensure at least one heartbeat (every 10s), where the `USERACTIVE 0` is sent for transition to Away

    // check if the status is away
    ASSERT_EQ(mOnlineStatus[accountIndex], MegaChatApi::STATUS_AWAY) <<
                     "Online status didn't changed to away automatically after timeout";
    int onlineStatus = megaChatApi[accountIndex]->getOnlineStatus();
    ASSERT_EQ(onlineStatus, MegaChatApi::STATUS_AWAY) <<
                     "Online status didn't changed to away automatically after timeout. Received: " << MegaChatRoom::statusToString(onlineStatus);

    // now signal user's activity to become online again
    flagStatus = &mOnlineStatusUpdated[accountIndex]; *flagStatus = false;
    ChatRequestTracker crtActivity2(megaChatApi[accountIndex]);
    megaChatApi[accountIndex]->signalPresenceActivity(&crtActivity2);
    ASSERT_EQ(crtActivity2.waitForResult(), MegaChatError::ERROR_OK) << "Failed to signal presence activity (2). Error: " << crtActivity2.getErrorString();
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
 * - Test1: Print chatrooms information
 * - Test2: Load history from one chatroom
 * - Test3: Close chatroom
 * - Test4: Load history from cache
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

    LOG_debug << "#### Test1: Print chatrooms information ####";
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

                ChatRequestTracker fnTracker(megaChatApi[accountIndex]);
                megaChatApi[accountIndex]->getUserFirstname(uh, nullptr, &fnTracker);

                ChatRequestTracker lnTracker(megaChatApi[accountIndex]);
                megaChatApi[accountIndex]->getUserLastname(uh, nullptr, &lnTracker);

                ASSERT_EQ(fnTracker.waitForResult(), MegaChatError::ERROR_OK) << "Failed to retrieve first name. Error: " << fnTracker.getErrorString();
                buffer << "Peer firstname (" << uh << "): '" << fnTracker.getText() << '\'' << endl;
                ASSERT_EQ(lnTracker.waitForResult(), MegaChatError::ERROR_OK) << "Failed to retrieve last name. Error: " << lnTracker.getErrorString();
                buffer << "Peer lastname (" << uh << "): '" << lnTracker.getText() << '\'' << endl;

                char *email = megaChatApi[accountIndex]->getContactEmail(uh);
                if (email)
                {
                    buffer << "Contact email (" << uh << "): " << email << " (len: " << strlen(email) << ")" << endl;
                    delete [] email;
                }
                else
                {
                    ChatRequestTracker emailTracker(megaChatApi[accountIndex]);
                    megaChatApi[accountIndex]->getUserEmail(uh, &emailTracker);
                    ASSERT_EQ(emailTracker.waitForResult(), MegaChatError::ERROR_OK) << "Failed to retrieve email. Error: " << emailTracker.getErrorString();
                    buffer << "Peer email (" << uh << "): '" << emailTracker.getText() << '\'' << endl;
                }
            }
        }

        LOG_debug << "#### Test2: Load history from one chatroom ####";
        buffer << "Loading messages for chat " << chatroom->getTitle() << " (id: " << chatroom->getChatId() << ")" << endl;
        loadHistory(accountIndex, chatid, chatroomListener);

        LOG_debug << "#### Test3: Close chatroom ####";
        megaChatApi[accountIndex]->closeChatRoom(chatid, chatroomListener);
        delete chatroomListener;

        LOG_debug << "#### Test4: Load history from cache (it should be cached now) ####";
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
 * - Both accounts should be contacts
 * - The 1on1 chatroom between them should exist
 * (if not accomplished, the test automatically solves the above)
 *
 * This test does the following:
 *
 * - Test1: A Sends a message B receives it
 * - Test2: Update the message
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

    LOG_debug << "#### Test1: A Sends a message B receives it ####"; // check the confirmed in A, the received message in B, the delivered in A
    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a1+1);
    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a2+1);

    // Load some message to feed history
    loadHistory(a1, chatid, chatroomListener);
    loadHistory(a2, chatid, chatroomListener);

    std::string messageToSend = "HI " + account(a1).getEmail() + " - This is a testing message automatically sent to you";
    MegaChatMessage *msgSent = sendTextMessageOrUpdate(a1, a2, chatid, messageToSend, chatroomListener);
    ASSERT_TRUE(msgSent);

    LOG_debug << "#### Test2: Update the message ####";
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
 * - Test1: Create a group chat room or select an existing one
 * - Test2: Remove member from chat
 * - Test3: Invite a new member
 * - Test4: Invite same account (error)
 * - Test5: Change chatroom title
 * - Test6: Change privileges to Moderator
 * - Test7: Change peer privileges to Read-only
 * + Test8: Try to send a message without the right privilege (error)
 * - Test9: Archive chatroom
 * - Test10: Test10: Send a message and wait for reception by target user (automatically unarchives)
 * - Test11: Archive chatroom
 * - Test12: Unarchive chatroom
 * - Test13: Remove peer from groupchat
 * - Test14: Invite another account
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

    LOG_debug << "#### Test1: Create a group chat room or select an existing one ####";
    MegaChatHandle chatid = getGroupChatRoom({a1, a2}, peers);
    delete peers;
    ASSERT_NE(chatid, MEGACHAT_INVALID_HANDLE);

    // --> Open chatroom
    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a1+1);
    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a2+1);

    LOG_debug << "#### Test2: Remove member from chat ####";
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
    ChatRequestTracker crtRemoveFromChat(megaChatApi[a1]);
    megaChatApi[a1]->removeFromChat(chatid, uh, &crtRemoveFromChat);
    ASSERT_EQ(crtRemoveFromChat.waitForResult(), MegaChatError::ERROR_OK) << "Failed to remove peer from chatroom " << crtRemoveFromChat.getErrorString();
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
    ASSERT_EQ(chatroom->getPeerCount(), 0u) << "Wrong number of peers in chatroom " << chatid;
    delete chatroom;

    ASSERT_TRUE(waitForResponse(chatItemLeft0)) << "Chat list item update not received for main account after " << maxTimeout << " seconds";
    ASSERT_TRUE(waitForResponse(chatItemLeft1)) << "Chat list item update not received for auxiliar account after " << maxTimeout << " seconds";
    ASSERT_TRUE(waitForResponse(chatItemClosed1)) << "Chat list item close notification for auxiliar account not received after " << maxTimeout << " seconds";
    ASSERT_TRUE(waitForResponse(chatLeft0)) << "Chat list item leave notification for main account not received after " << maxTimeout << " seconds";

    ASSERT_TRUE(waitForResponse(chatLeft1)) << "Chat list item leave notification for auxiliar account not received after " << maxTimeout << " seconds";
    chatroom = megaChatApi[a1]->getChatRoom(chatid);
    ASSERT_TRUE(chatroom) << "Cannot get chatroom for id " << chatid;
    ASSERT_EQ(chatroom->getPeerCount(), 0u) << "Wrong number of peers in chatroom " << chatid;
    delete chatroom;

    // Close the chatroom, even if we've been removed from it
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);

    LOG_debug << "#### Test3: Invite a new member ####";
    bool *chatItemJoined0 = &chatItemUpdated[a1]; *chatItemJoined0 = false;
    bool *chatItemJoined1 = &chatItemUpdated[a2]; *chatItemJoined1 = false;
    bool *chatJoined0 = &chatroomListener->chatUpdated[a1]; *chatJoined0 = false;
    bool *chatJoined1 = &chatroomListener->chatUpdated[a2]; *chatJoined1 = false;
    *flagChatsUpdated1 = &mChatsUpdated[a2]; *flagChatsUpdated1 = false;
    mChatListUpdated[a2].clear();
    mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
    uhAction = &chatroomListener->uhAction[a1]; *uhAction = MEGACHAT_INVALID_HANDLE;
    priv = &chatroomListener->priv[a1]; *priv = MegaChatRoom::PRIV_UNKNOWN;
    ChatRequestTracker crtInviteToChat(megaChatApi[a1]);
    megaChatApi[a1]->inviteToChat(chatid, uh, MegaChatPeerList::PRIV_STANDARD, &crtInviteToChat);
    ASSERT_EQ(crtInviteToChat.waitForResult(), MegaChatError::ERROR_OK) << "Failed to invite a new peer. Error: " << crtInviteToChat.getErrorString();
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
    ASSERT_EQ(chatroom->getPeerCount(), 1u) << "Wrong number of peers in chatroom " << chatid;
    delete chatroom;

    // since we were expulsed from chatroom, we need to open it again
    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a2+1);

    LOG_debug << "#### Test4: Invite (again) same account (error) ####";
    ChatRequestTracker crtInviteToChat2(megaChatApi[a1]);
    megaChatApi[a1]->inviteToChat(chatid, uh, MegaChatPeerList::PRIV_STANDARD, &crtInviteToChat2);
    ASSERT_EQ(crtInviteToChat2.waitForResult(), MegaChatError::ERROR_EXIST) << "Invitation should have failed, but it succeed: " << crtInviteToChat2.getErrorString();

    LOG_debug << "#### Test5: Change chatroom title ####";
    string title = "Title " + std::to_string(time(NULL));
    bool *titleItemChanged0 = &titleUpdated[a1]; *titleItemChanged0 = false;
    bool *titleItemChanged1 = &titleUpdated[a2]; *titleItemChanged1 = false;
    bool *titleChanged0 = &chatroomListener->titleUpdated[a1]; *titleChanged0 = false;
    bool *titleChanged1 = &chatroomListener->titleUpdated[a2]; *titleChanged1 = false;
    mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
    string *msgContent = &chatroomListener->content[a1]; *msgContent = "";
    ChatRequestTracker crtSetTitle(megaChatApi[a1]);
    megaChatApi[a1]->setChatTitle(chatid, title.c_str(), &crtSetTitle);
    ASSERT_EQ(crtSetTitle.waitForResult(), MegaChatError::ERROR_OK) << "Failed to set chat title. Error: " << crtSetTitle.getErrorString();
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

    LOG_debug << "#### Test6: Change privileges to Moderator ####";
    bool *peerUpdated0 = &peersUpdated[a1]; *peerUpdated0 = false;
    bool *peerUpdated1 = &peersUpdated[a2]; *peerUpdated1 = false;
    mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
    uhAction = &chatroomListener->uhAction[a1]; *uhAction = MEGACHAT_INVALID_HANDLE;
    priv = &chatroomListener->priv[a1]; *priv = MegaChatRoom::PRIV_UNKNOWN;
    ChatRequestTracker crtMakeMod(megaChatApi[a1]);
    megaChatApi[a1]->updateChatPermissions(chatid, uh, MegaChatRoom::PRIV_MODERATOR, &crtMakeMod);
    ASSERT_EQ(crtMakeMod.waitForResult(), MegaChatError::ERROR_OK) << "Failed to make peer moderator. Error: " << crtMakeMod.getErrorString();
    ASSERT_TRUE(waitForResponse(peerUpdated0)) << "Timeout expired for receiving peer update";
    ASSERT_TRUE(waitForResponse(peerUpdated1)) << "Timeout expired for receiving peer update";
    ASSERT_TRUE(waitForResponse(mngMsgRecv)) << "Timeout expired for receiving management message";
    ASSERT_EQ(*uhAction, uh) << "User handle from message doesn't match";
    ASSERT_EQ(*priv, MegaChatRoom::PRIV_MODERATOR) << "Privilege is incorrect";

    LOG_debug << "#### Test7: Change peer privileges to Read-only ####";
    peerUpdated0 = &peersUpdated[a1]; *peerUpdated0 = false;
    peerUpdated1 = &peersUpdated[a2]; *peerUpdated1 = false;
    mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
    uhAction = &chatroomListener->uhAction[a1]; *uhAction = MEGACHAT_INVALID_HANDLE;
    priv = &chatroomListener->priv[a1]; *priv = MegaChatRoom::PRIV_UNKNOWN;
    ChatRequestTracker crtMakeReadonly(megaChatApi[a1]);
    megaChatApi[a1]->updateChatPermissions(chatid, uh, MegaChatRoom::PRIV_RO, &crtMakeReadonly);
    ASSERT_EQ(crtMakeReadonly.waitForResult(), MegaChatError::ERROR_OK) << "Failed to set peer readonly. Error: " << crtMakeMod.getErrorString();
    ASSERT_TRUE(waitForResponse(peerUpdated0)) << "Timeout expired for receiving peer update";
    ASSERT_TRUE(waitForResponse(peerUpdated1)) << "Timeout expired for receiving peer update";
    ASSERT_TRUE(waitForResponse(mngMsgRecv)) << "Timeout expired for receiving management message";
    ASSERT_EQ(*uhAction, uh) << "User handle from message doesn't match";
    ASSERT_EQ(*priv, MegaChatRoom::PRIV_RO) << "Privilege is incorrect";

    LOG_debug << "#### Test8: Try to send a message without the right privilege (error) ####";
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
    ASSERT_TRUE(waitForResponse(flagTyping1)) << "Timeout expired for sending typing notification";
    ASSERT_EQ(*uhAction, megaChatApi[a1]->getMyUserHandle()) << "My user handle is wrong at typing";

    // --> Send stop typing notification
    flagTyping1 = &chatroomListener->userTyping[a2]; *flagTyping1 = false;
    uhAction = &chatroomListener->uhAction[a2]; *uhAction = MEGACHAT_INVALID_HANDLE;
    megaChatApi[a1]->sendStopTypingNotification(chatid);
    ASSERT_TRUE(waitForResponse(flagTyping1)) << "Timeout expired for sending stop typing notification";
    ASSERT_EQ(*uhAction, megaChatApi[a1]->getMyUserHandle()) << "My user handle is wrong at stop typing";

    LOG_debug << "#### Test9: Archive chatroom ####";
    chatroom = megaChatApi[a1]->getChatRoom(chatid);
    delete chatroom; chatroom = NULL;
    bool *chatArchiveChanged = &chatArchived[a1]; *chatArchiveChanged = false;
    bool *chatroomArchiveChanged = &chatroomListener->archiveUpdated[a1]; *chatroomArchiveChanged = false;
    ChatRequestTracker crtArchive(megaChatApi[a1]);
    megaChatApi[a1]->archiveChat(chatid, true, &crtArchive);
    ASSERT_EQ(crtArchive.waitForResult(), MegaChatError::ERROR_OK) << "Failed to archive chat. Error: " << crtArchive.getErrorString();
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

    LOG_debug << "#### Test10: Send a message and wait for reception by target user (automatically unarchives) ####";
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

    LOG_debug << "#### Test11: Archive chatroom ####";
    chatArchiveChanged = &chatArchived[a1]; *chatArchiveChanged = false;
    chatroomArchiveChanged = &chatroomListener->archiveUpdated[a1]; *chatroomArchiveChanged = false;
    ChatRequestTracker crtArchive2(megaChatApi[a1]);
    megaChatApi[a1]->archiveChat(chatid, true, &crtArchive2);
    ASSERT_EQ(crtArchive2.waitForResult(), MegaChatError::ERROR_OK) << "Failed to archive chat (2). Error: " << crtArchive2.getErrorString();
    ASSERT_TRUE(waitForResponse(chatArchiveChanged)) << "Timeout expired for receiving chat list item update about archive";
    ASSERT_TRUE(waitForResponse(chatroomArchiveChanged)) << "Timeout expired for receiving chatroom update about archive";
    chatroom = megaChatApi[a1]->getChatRoom(chatid);
    ASSERT_TRUE(chatroom->isArchived()) << "Chatroom is not archived when it should";
    delete chatroom; chatroom = NULL;

    LOG_debug << "#### Test12: Unarchive chatroom ####";
    delete chatroom; chatroom = NULL;
    chatArchiveChanged = &chatArchived[a1]; *chatArchiveChanged = false;
    chatroomArchiveChanged = &chatroomListener->archiveUpdated[a1]; *chatroomArchiveChanged = false;
    ChatRequestTracker crtArchive3(megaChatApi[a1]);
    megaChatApi[a1]->archiveChat(chatid, false, &crtArchive3);
    ASSERT_EQ(crtArchive3.waitForResult(), MegaChatError::ERROR_OK) << "Failed to archive chat (3). Error: " << crtArchive3.getErrorString();
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

    LOG_debug << "#### Test13: Remove peer from groupchat ####";
    bool *chatClosed = &chatItemClosed[a2]; *chatClosed = false;
    ChatRequestTracker crtRemoveFromChat2(megaChatApi[a1]);
    megaChatApi[a1]->removeFromChat(chatid, uh, &crtRemoveFromChat2);
    ASSERT_EQ(crtRemoveFromChat2.waitForResult(), MegaChatError::ERROR_OK) << "Failed to remove peer from group chat. Error: " << crtRemoveFromChat2.getErrorString();
    ASSERT_TRUE(waitForResponse(chatClosed)) << "Timeout expired";
    chatroom = megaChatApi[a2]->getChatRoom(chatid);
    ASSERT_TRUE(chatroom) << "Cannot get chatroom for id " << chatid;
    ASSERT_FALSE(chatroom->isActive()) << "Chatroom should be inactive, but it's still active";
    delete chatroom;    chatroom = NULL;

    LOG_debug << "#### Test14: Invite another account ####";
    ChatRequestTracker crtInviteToChat3(megaChatApi[a1]);
    megaChatApi[a1]->inviteToChat(chatid, uh, MegaChatPeerList::PRIV_STANDARD, &crtInviteToChat3);
    ASSERT_EQ(crtInviteToChat3.waitForResult(), MegaChatError::ERROR_OK) << "Failed to invite a new peer (3). Error: " << crtInviteToChat3.getErrorString();

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
 * - Test1: Login in primary account
 * + Test2: Init anonymous in secondary account
 * - Test3: Create a public chat with no peers nor title ####
 * - Test4: Open chatroom
 * - Test5: Create chat link (ERR)
 * - Test6: Set title
 * - Test7: Create chat link
 * + Test8: Load chat link
 * + Test9: Open chatroom
 * + Test10: Send a message (ERR)
 * - Test11: Send a message
 * + Test12: Close preview
 * - Test13: Remove chat link
 * + Test14: Load chat link (ERR)
 * + Test15: Logout
 *
 * [ Public chat test ]
 * + Test16: Login in secondary account
 * - Test17: Create chat link
 * + Test18: Load chat link
 * + Test19: Open chatroom
 * + Test20: Send a message (ERR)
 * + Test21: Autojoin chat link
 * + Test22: Send a message
 * + Test23: Set chat to private mode
 * + Test24: Remove peer from groupchat (OK)
 * + Test25: Preview chat link (ERR)
 * - Test26: Invite other account
 * - Test27: Leave chat room
*/
TEST_F(MegaChatApiTest, PublicChatManagement)
{
    unsigned a1 = 0;
    unsigned a2 = 1;

    /// Anonymous mode test
    LOG_debug << "#### Test1: Login in primary account ####";
    char *sessionPrimary = login(a1);
    ASSERT_TRUE(sessionPrimary);

    LOG_debug << "#### Test2: Init anonymous in secondary account ####";
    initState[a2] = megaChatApi[a2]->initAnonymous();
    ASSERT_EQ(initState[a2], MegaChatApi::INIT_ANONYMOUS) << "Init sesion in anonymous mode failed";
    char *sessionAnonymous = megaApi[a2]->dumpSession();

    LOG_debug << "#### Test3: Create a public chat with no peers nor title ####";
    MegaChatHandle chatid = MEGACHAT_INVALID_HANDLE;
    MegaChatPeerList *peers = MegaChatPeerList::createInstance();
    ChatRequestTracker crtCreate(megaChatApi[a1]);
    megaChatApi[a1]->createPublicChat(peers, nullptr, &crtCreate);
    ASSERT_EQ(crtCreate.waitForResult(), MegaChatError::ERROR_OK) << "Failed to create public groupchat. Error: " << crtCreate.getErrorString();
    chatid = crtCreate.getChatHandle();
    ASSERT_NE(chatid, MEGACHAT_INVALID_HANDLE) << "Wrong chat id";
    delete peers;
    peers = NULL;

    const std::unique_ptr<char[]> chatidB64(MegaApi::userHandleToBase64(chatid));
    LOG_debug << "PublicChatManagement: selected chat: " << chatidB64.get();

    LOG_debug << "#### Test4: Open chatroom ####";
    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a1+1);
    bool *flagChatdOnline = &mChatConnectionOnline[a1]; *flagChatdOnline = false;
    while (megaChatApi[a1]->getChatConnectionState(chatid) != MegaChatApi::CHAT_CONNECTION_ONLINE)
    {
        postLog("Waiting for connection to chatd...");
        ASSERT_TRUE(waitForResponse(flagChatdOnline)) << "Timeout expired for connecting to chatd";
        *flagChatdOnline = false;
    }

    LOG_debug << "#### Test5: Create chat link (ERR) ####";
    ChatRequestTracker crtCreateLink(megaChatApi[a1]);
    megaChatApi[a1]->createChatLink(chatid, &crtCreateLink);
    ASSERT_NE(crtCreateLink.waitForResult(), MegaChatError::ERROR_OK) << "Creating chat link succeeded. Should have failed!";

    LOG_debug << "#### Test6: Set title ####";
    string title = "TestPublicChatWithTitle_" + dateToString().substr(dateToString().length() - 5, 5);
    bool *titleItemChanged0 = &titleUpdated[a1]; *titleItemChanged0 = false;
    bool *titleChanged0 = &chatroomListener->titleUpdated[a1]; *titleChanged0 = false;
    bool *mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
    string *msgContent = &chatroomListener->content[a1]; *msgContent = "";
    ChatRequestTracker crtSetTitle(megaChatApi[a1]);
    megaChatApi[a1]->setChatTitle(chatid, title.c_str(), &crtSetTitle);
    ASSERT_EQ(crtSetTitle.waitForResult(), MegaChatError::ERROR_OK) << "Failed to set chat title. Error: " << crtSetTitle.getErrorString();
    ASSERT_TRUE(waitForResponse(titleItemChanged0)) << "Timeout expired for receiving chat list item update";
    ASSERT_TRUE(waitForResponse(titleChanged0)) << "Timeout expired for receiving chatroom update";
    ASSERT_TRUE(waitForResponse(mngMsgRecv)) << "Timeout expired for receiving management message";
    ASSERT_EQ(title, *msgContent) << "Title received doesn't match the title set";

    LOG_debug << "#### Test7: Create chat link ####";
    ChatRequestTracker crtCreateLink2(megaChatApi[a1]);
    megaChatApi[a1]->createChatLink(chatid, &crtCreateLink2);
    ASSERT_EQ(crtCreateLink2.waitForResult(), MegaChatError::ERROR_OK) << "Error creating chat link (2). Error: " << crtCreateLink2.getErrorString();
    ASSERT_FALSE(crtCreateLink2.getFlag());
    const string& chatLink = crtCreateLink2.getText();
    ASSERT_FALSE(chatLink.empty());

    LOG_debug << "#### Test8: Load chat link ####";
    ChatRequestTracker crtPreviewTracker(megaChatApi[a2]);
    megaChatApi[a2]->openChatPreview(chatLink.c_str(), &crtPreviewTracker);
    ASSERT_EQ(crtPreviewTracker.waitForResult(), MegaChatError::ERROR_OK) << "Failed to open chat preview. Error: " << crtPreviewTracker.getErrorString();

    LOG_debug << "#### Test9: Open chatroom ####";
    bool *previewsUpdated = &chatroomListener->previewsUpdated[a1]; *previewsUpdated = false;
    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a2+1);
    ASSERT_TRUE(waitForResponse(previewsUpdated)) << "Timeout expired for update previewers";

    LOG_debug << "#### Test10: Send a message (ERR) ####";
    string msg = "HI " + account(a1).getEmail()+ " - This message will be rejected because now I'm a previewer";
    bool *flagRejected = &chatroomListener->msgRejected[a2]; *flagRejected = false;
    chatroomListener->clearMessages(a2);   // will be set at reception
    MegaChatMessage *msgSent = megaChatApi[a2]->sendMessage(chatid, msg.c_str());
    delete msgSent; msgSent = NULL;
    ASSERT_TRUE(waitForResponse(flagRejected)) << "Timeout expired for rejection of message";    // for confirmation, sendMessage() is synchronous
    ASSERT_EQ(chatroomListener->mConfirmedMessageHandle[a2], MEGACHAT_INVALID_HANDLE) << "Message confirmed, when it should fail";

    LOG_debug << "#### Test11: Send a message ####";
    msg = "HI Anonymous user, This message will be sent";
    flagRejected = &chatroomListener->msgRejected[a1]; *flagRejected = false;
    chatroomListener->clearMessages(a1);   // will be set at reception
    msgSent = megaChatApi[a1]->sendMessage(chatid, msg.c_str());
    ASSERT_TRUE(msgSent) << "Succeed to send message";
    delete msgSent; msgSent = NULL;
    ASSERT_EQ(chatroomListener->mConfirmedMessageHandle[a1], MEGACHAT_INVALID_HANDLE) << "Message confirmed, when it should fail";

    LOG_debug << "#### Test12: Close preview ####";
    previewsUpdated = &chatroomListener->previewsUpdated[a1]; *previewsUpdated = false;
    megaChatApi[a2]->closeChatPreview(chatid);
    ASSERT_TRUE(waitForResponse(previewsUpdated)) << "Timeout expired for close preview";

    LOG_debug << "#### Test13: Remove chat link ####";
    ChatRequestTracker crtRemoveLink(megaChatApi[a1]);
    megaChatApi[a1]->removeChatLink(chatid, &crtRemoveLink);
    ASSERT_EQ(crtRemoveLink.waitForResult(), MegaChatError::ERROR_OK) << "Failed to remove chat link. Error: " << crtRemoveLink.getErrorString();

    LOG_debug << "#### Test14: Load chat link (ERR) ####";
    ChatRequestTracker crtPreviewTracker2(megaChatApi[a2]);
    megaChatApi[a2]->openChatPreview(chatLink.c_str(), &crtPreviewTracker2);
    ASSERT_NE(crtPreviewTracker2.waitForResult(), MegaChatError::ERROR_OK) << "Opening chat preview succeeded. Should have failed!";

    LOG_debug << "#### Test15: Logout ####";
    ASSERT_NO_FATAL_FAILURE({ logout(a2); });
    delete [] sessionAnonymous;
    sessionAnonymous = NULL;

    /// Public chats test
    LOG_debug << "#### Test16: Login in secondary account ####";
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

    LOG_debug << "#### Test17: Create chat link ####";
    ChatRequestTracker crtCreateLink3(megaChatApi[a1]);
    megaChatApi[a1]->createChatLink(chatid, &crtCreateLink3);
    ASSERT_EQ(crtCreateLink3.waitForResult(), MegaChatError::ERROR_OK) << "Error creating chat link (3). Error: " << crtCreateLink3.getErrorString();
    ASSERT_FALSE(crtCreateLink3.getFlag());
    const string& chatLink3 = crtCreateLink3.getText();
    ASSERT_FALSE(chatLink3.empty());

    LOG_debug << "#### Test18: Load chat link ####";
    ChatRequestTracker crtPreviewTracker3(megaChatApi[a2]);
    megaChatApi[a2]->openChatPreview(chatLink3.c_str(), &crtPreviewTracker3);
    ASSERT_EQ(crtPreviewTracker3.waitForResult(), MegaChatError::ERROR_OK) << "Failed to open chat preview (3). Error: " << crtPreviewTracker3.getErrorString();

    LOG_debug << "#### Test19: Open chatroom ####";
    previewsUpdated = &chatroomListener->previewsUpdated[a1]; *previewsUpdated = false;
    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a2+1);
    ASSERT_TRUE(waitForResponse(previewsUpdated)) << "Timeout expired for update previewers";

    LOG_debug << "#### Test20: Send a message (ERR) ####";
    string msgaux = "HI " + account(a1).getEmail()+ " - This message can't be send because I'm in preview mode (read-only)";
    flagRejected = &chatroomListener->msgRejected[a2]; *flagRejected = false;
    chatroomListener->clearMessages(a2);   // will be set at reception
    msgSent = megaChatApi[a2]->sendMessage(chatid, msgaux.c_str());
    ASSERT_TRUE(msgSent) << "Fail to send message, when it should succeed";
    delete msgSent; msgSent = NULL;
    ASSERT_TRUE(waitForResponse(flagRejected)) << "Timeout expired for rejection of message";
    ASSERT_EQ(chatroomListener->mConfirmedMessageHandle[a2], MEGACHAT_INVALID_HANDLE) << "Message confirmed, when it should fail";

    LOG_debug << "#### Test21: Autojoin chat link ####";
    bool* flagChatsUpdated1 = &mChatsUpdated[a1]; *flagChatsUpdated1 = false;
    mChatListUpdated[a1].clear();
    previewsUpdated = &chatroomListener->previewsUpdated[a1]; *previewsUpdated = false;
    ChatRequestTracker crtAutojoin(megaChatApi[a2]);
    megaChatApi[a2]->autojoinPublicChat(chatid, &crtAutojoin);
    ASSERT_EQ(crtAutojoin.waitForResult(), MegaChatError::ERROR_OK) << "Failed to autojoin chat-link. Error: " << crtAutojoin.getErrorString();
    ASSERT_TRUE(waitForResponse(previewsUpdated)) << "Timeout expired for update previewers";
    ASSERT_TRUE(waitForResponse(flagChatsUpdated1)) << "Failed to receive onChatsUpdate after " << maxTimeout << " seconds";
    ASSERT_TRUE(isChatroomUpdated(a1, chatid)) << "Chatroom " << chatid << " is not included in onChatsUpdate";
    mChatListUpdated[a1].clear();

    MegaChatListItem *item = megaChatApi[a2]->getChatListItem(chatid);
    ASSERT_EQ(item->getNumPreviewers(), 0u) << "Wrong number of previewers.";
    delete item;
    item = NULL;

    LOG_debug << "#### Test22: Send a message ####";
    msgaux = "HI " + account(a1).getEmail()+ " - I have autojoined to this chat";
    flagRejected = &chatroomListener->msgRejected[a2]; *flagRejected = false;
    chatroomListener->clearMessages(a2);   // will be set at reception
    msgSent = megaChatApi[a2]->sendMessage(chatid, msgaux.c_str());
    ASSERT_TRUE(msgSent) << "Succeed to send message, when it should fail";
    delete msgSent; msgSent = NULL;

    LOG_debug << "#### Test23: Set chat to private mode ####";
    ASSERT_NO_FATAL_FAILURE({
        waitForAction (1, // just one attempt
                      std::vector<bool *> { &chatroomListener->chatModeUpdated[a1], &chatroomListener->chatModeUpdated[a2]},
                      std::vector<string> { "chatroomListener->chatModeUpdated[a1]", "chatroomListener->chatModeUpdated[a2]"},
                      "Set chat into private mode(EKR enabled)from A",
                      true /* wait for all exit flags */,
                      true /* reset flags */,
                      maxTimeout,
                      [this, a1, chatid]()
                      {
                          // Convert chat to private mode (EKR enabled)
                          ChatRequestTracker crtSetPrivate(megaChatApi[a1]);
                          megaChatApi[a1]->setPublicChatToPrivate(chatid, &crtSetPrivate);
                          ASSERT_EQ(crtSetPrivate.waitForResult(), MegaChatError::ERROR_OK) << "Failed to set chat to private. Error: " << crtSetPrivate.getErrorString();
                      });
    });


    LOG_debug << "#### Test24: Remove peer from groupchat (OK) ####";
    auto uh =  megaChatApi[a2]->getMyUserHandle();
    bool *chatClosed = &chatItemClosed[a2]; *chatClosed = false;
    ChatRequestTracker crtRemoveFromGroup(megaChatApi[a1]);
    megaChatApi[a1]->removeFromChat(chatid, uh, &crtRemoveFromGroup);
    ASSERT_EQ(crtRemoveFromGroup.waitForResult(), MegaChatError::ERROR_OK) << "Failed to remove peer from group chat. Error: " << crtRemoveFromGroup.getErrorString();
    ASSERT_TRUE(waitForResponse(chatClosed)) << "Timeout expired for remove peer from chat";

    MegaChatRoom * auxchatroom = megaChatApi[a2]->getChatRoom(chatid);
    ASSERT_TRUE(auxchatroom) << "Cannot get chatroom for id " << chatid;
    ASSERT_FALSE(auxchatroom->isActive()) << "Chatroom should be inactive, but it's still active";
    delete auxchatroom;    auxchatroom = NULL;

    LOG_debug << "#### Test25: Preview chat link (ERR) ####";
    ChatRequestTracker crtPreviewTracker4(megaChatApi[a2]);
    megaChatApi[a2]->openChatPreview(chatLink3.c_str(), &crtPreviewTracker4);
    ASSERT_NE(crtPreviewTracker4.waitForResult(), MegaChatError::ERROR_OK) << "Opening chat preview succeeded (4). Should have failed!";

    LOG_debug << "#### Test26: Invite other account ####";
    ChatRequestTracker crtInvite(megaChatApi[a1]);
    megaChatApi[a1]->inviteToChat(chatid, uh, MegaChatPeerList::PRIV_STANDARD, &crtInvite);
    ASSERT_EQ(crtInvite.waitForResult(), MegaChatError::ERROR_OK) << "Failed to invite a new peer. Error: " << crtInvite.getErrorString();

    LOG_debug << "#### Test27: Leave chat room ####";
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
 * - Test1: Create a group chat room or select an existing one
 * - Test2: Change another account privileges to readonly
 * - Test3: Send message and wait for reception by target user
 * - Test4: Check reactions in message (error)
 * - Test5: Add reaction with NULL reaction (error)
 * - Test6: Add reaction with invalid chat (error)
 * - Test7: Add reaction with invalid message (error)
 * + Test8: Add reaction without enough permissions (error)
 * - Test9: Add reaction
 * - Test10: Add duplicate reaction (error)
 * - Test11: Check reactions in message
 * - Test12: Remove reaction
 * - Test13: Remove non-existent reaction (error)
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

    LOG_debug << "#### Test1: Create a group chat room or select an existing one ####";
    MegaChatHandle uh = user->getHandle();
    delete user;
    user = NULL;
    MegaChatPeerList *peers = MegaChatPeerList::createInstance();
    peers->addPeer(uh, MegaChatPeerList::PRIV_STANDARD);
    MegaChatHandle chatid = getGroupChatRoom({a1, a2}, peers);
    delete peers;
    ASSERT_NE(chatid, MEGACHAT_INVALID_HANDLE);

    // Open chatroom
    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a1+1);
    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a2+1);
    std::unique_ptr<MegaChatRoom> chatroom (megaChatApi[a1]->getChatRoom(chatid));
    std::unique_ptr<char[]> chatidB64(megaApi[a1]->handleToBase64(chatid));
    ASSERT_TRUE(chatroom) << "Cannot get chatroom for id " << chatidB64.get();

    LOG_debug << "#### Test2: Change another account privileges to readonly ####";
    if (chatroom->getPeerPrivilegeByHandle(uh) != PRIV_RO)
    {
        // Change peer privileges to Read-only
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

    LOG_debug << "#### Test3: Send message and wait for reception by target user ####";
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

    LOG_debug << "#### Test4: Check reactions in message (error) ####"; // (It shouldn't exist any reaction)
    std::unique_ptr<MegaStringList> reactionsList;
    reactionsList.reset(megaChatApi[a1]->getMessageReactions(chatid, msgId));
    ASSERT_FALSE(reactionsList->size()) << "getMessageReactions Error: The message shouldn't have reactions";
    int userCount = megaChatApi[a1]->getMessageReactionCount(chatid, msgId, "😰");
    ASSERT_FALSE(userCount) << "getReactionUsers Error: The reaction shouldn't exist";

    LOG_debug << "#### Test5: Add reaction with NULL reaction (error) ####";
    TestMegaChatRequestListener requestListener(nullptr, megaChatApi[a1]);
    megaChatApi[a1]->addReaction(chatid, msgId, nullptr, &requestListener);
    ASSERT_TRUE(requestListener.waitForResponse()) << "Timeout expired for add reaction";
    ASSERT_EQ(requestListener.getErrorCode(), MegaChatError::ERROR_ARGS) << "addReaction: Unexpected error for NULL reaction param.";

    LOG_debug << "#### Test6: Add reaction with invalid chat (error) ####";
    requestListener = TestMegaChatRequestListener (nullptr, megaChatApi[a1]);
    megaChatApi[a1]->addReaction(MEGACHAT_INVALID_HANDLE, msgId, "😰", &requestListener);
    ASSERT_TRUE(requestListener.waitForResponse()) << "Timeout expired for add reaction";
    ASSERT_EQ(requestListener.getErrorCode(), MegaChatError::ERROR_NOENT) << "addReaction: Unexpected error for invalid chat.";

    LOG_debug << "#### Test7: Add reaction with invalid message (error) ####";
    requestListener = TestMegaChatRequestListener (nullptr, megaChatApi[a1]);
    megaChatApi[a1]->addReaction(chatid, MEGACHAT_INVALID_HANDLE, "😰", &requestListener);
    ASSERT_TRUE(requestListener.waitForResponse()) << "Timeout expired for add reaction";
    ASSERT_EQ(requestListener.getErrorCode(), MegaChatError::ERROR_NOENT) << "addReaction: Unexpected error for invalid message.";

    LOG_debug << "#### Test8: Add reaction without enough permissions (error) ####";
    requestListener = TestMegaChatRequestListener (nullptr, megaChatApi[a2]);
    megaChatApi[a2]->addReaction(chatid, msgId, "😰", &requestListener);
    ASSERT_TRUE(requestListener.waitForResponse()) << "Timeout expired for add reaction";
    ASSERT_EQ(requestListener.getErrorCode(), MegaChatError::ERROR_ACCESS) << "addReaction: Unexpected error adding reaction without enough permissions.";

    LOG_debug << "#### Test9: Add reaction ####";
    requestListener = TestMegaChatRequestListener (nullptr, megaChatApi[a1]);
    megaChatApi[a1]->addReaction(chatid, msgId, "😰", &requestListener);
    ASSERT_TRUE(requestListener.waitForResponse()) << "Timeout expired for add reaction";
    ASSERT_EQ(requestListener.getErrorCode(), MegaChatError::ERROR_OK) << "addReaction: Unexpected error adding reaction.";

    LOG_debug << "#### Test10: Add duplicate reaction (error) ####";
    requestListener = TestMegaChatRequestListener (nullptr, megaChatApi[a1]);
    megaChatApi[a1]->addReaction(chatid, msgId, "😰", &requestListener);
    ASSERT_TRUE(requestListener.waitForResponse()) << "Timeout expired for add reaction";
    ASSERT_EQ(requestListener.getErrorCode(), MegaChatError::ERROR_EXIST) << "addReaction: Unexpected error for adding an existing reaction.";

    LOG_debug << "#### Test11: Check reactions in message ####";
    reactionsList.reset(megaChatApi[a1]->getMessageReactions(chatid, msgId));
    ASSERT_TRUE(reactionsList->size()) << "getMessageReactions Error: The message doesn't have reactions";
    userCount = megaChatApi[a1]->getMessageReactionCount(chatid, msgId, "😰");
    ASSERT_TRUE(userCount) << "getReactionUsers Error: The reaction doesn't exists";

    LOG_debug << "#### Test12: Remove reaction ####";
    requestListener = TestMegaChatRequestListener (nullptr, megaChatApi[a1]);
    megaChatApi[a1]->delReaction(chatid, msgId, "😰", &requestListener);
    ASSERT_TRUE(requestListener.waitForResponse()) << "Timeout expired for del reaction";
    ASSERT_EQ(requestListener.getErrorCode(), MegaChatError::ERROR_OK) << "delReaction: Unexpected error removing reaction.";

    LOG_debug << "#### Test13: Remove non-existent reaction (error) ####";
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
 * - Test1: Send message without internet connection
 * - Test2: Logout
 * - Test3: Init offline sesion
 * - Test4: Check unsent messages
 * - Test5: Connect to internet
 * - Test6: Check message has been received by the server
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

    MegaChatHandle chatid = getGroupChatRoom({a1, a2}, peers);
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

    LOG_debug << "#### Test1: Send message without internet connection ####";
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

    LOG_debug << "#### Test2: Logout ####";
    // close session and resume it while offline
    ASSERT_NO_FATAL_FAILURE({ logout(a1, false); });

    LOG_debug << "#### Test3: Init offline sesion ####";
    bool *flagInit = &initStateChanged[a1]; *flagInit = false;
    megaChatApi[a1]->init(sessionPrimary);
    MegaApi::removeLoggerObject(logger());
    ASSERT_TRUE(waitForResponse(flagInit)) << "Expired timeout for initialization";
    int initStateValue = initState[a1];
    ASSERT_EQ(initStateValue, MegaChatApi::INIT_OFFLINE_SESSION) <<
                     "Wrong chat initialization state (8).";

    LOG_debug << "#### Test4: Check unsent messages ####";
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

    LOG_debug << "#### Test5: Connect to internet ####";
    buffer.str("");
    buffer << endl << endl << "Connect from the Internet now" << endl << endl;
    postLog(buffer.str());

//        system("pause");

    ChatRequestTracker crtRetryConn(megaChatApi[a1]);
    megaChatApi[a1]->retryPendingConnections(false, &crtRetryConn);
    ASSERT_EQ(crtRetryConn.waitForResult(), MegaChatError::ERROR_OK) << "Failed to retry pending connections. Error: " << crtRetryConn.getErrorString();

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

    LOG_debug << "#### Test6: Check message has been received by the server ####";
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
 * - Test1: Send five mesages to chatroom and check history has five messages
 * - Test2: Clear history and check history has zero messages
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

    LOG_debug << "#### Test1: Send five mesages to chatroom and check history has five messages ####";
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

    LOG_debug << "#### Test2: Clear history and check history has zero messages ####";
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
 * - Test1: Login with account1
 * - Test2: Logout
 * - Test3: With the same megaApi and megaChatApi, login with account2
 */
TEST_F(MegaChatApiTest, SwitchAccounts)
{
    unsigned a1 = 0;
    unsigned a2 = 1;

    LOG_debug << "#### Test1: Login with account1 ####";
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

    LOG_debug << "#### Test2: Logout ####";
    ASSERT_NO_FATAL_FAILURE({ logout(a1, true); });    // terminate() and destroy Client

    delete [] session;
    session = NULL;

    LOG_debug << "#### Test3: With the same megaApi and megaChatApi, login with account2 ####";
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
 * - Test1: Upload new file
 * - Test2: Send file as attachment to chatroom
 * + Test3: Download received file
 * + Test4: Import received file into the cloud
 * - Test5: Revoke access to file
 * + Test6: Download received file again --> no access
 * - Test7: Upload an image to check previews / thumbnails
 * - Test8: Download the thumbnail
 * + Test9: Download the thumbnail
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

    LOG_debug << "#### Test1: Upload new file ####";
    createFile(formatDate, LOCAL_PATH, formatDate);
    MegaNode* nodeSent = uploadFile(a1, formatDate, LOCAL_PATH, REMOTE_PATH);
    ASSERT_TRUE(nodeSent);

    LOG_debug << "#### Test2: Send file as attachment to chatroom ####";
    MegaChatMessage *msgSent = attachNode(a1, a2, chatid, nodeSent, chatroomListener);
    ASSERT_TRUE(msgSent);
    MegaNode *nodeReceived = msgSent->getMegaNodeList()->get(0)->copy();

    LOG_debug << "#### Test3: Download received file ####";
    ASSERT_TRUE(downloadNode(a2, nodeReceived)) << "Cannot download node attached to message";

    LOG_debug << "#### Test4: Import received file into the cloud ####";
    ASSERT_TRUE(importNode(a2, nodeReceived, FILE_IMAGE_NAME)) << "Cannot import node attached to message";

    LOG_debug << "#### Test5: Revoke access to file ####";
    bool *flagConfirmed = &chatroomListener->msgConfirmed[a1]; *flagConfirmed = false;
    bool *flagReceived = &chatroomListener->msgReceived[a2]; *flagReceived = false;
    chatroomListener->mConfirmedMessageHandle[a1] = MEGACHAT_INVALID_HANDLE;
    chatroomListener->clearMessages(a1);   // will be set at confirmation
    chatroomListener->clearMessages(a2);   // will be set at reception
    megachat::MegaChatHandle revokeAttachmentNode = nodeSent->getHandle();
    ChatRequestTracker crtRevokeAttachment(megaChatApi[a1]);
    megaChatApi[a1]->revokeAttachment(chatid, revokeAttachmentNode, &crtRevokeAttachment);
    ASSERT_EQ(crtRevokeAttachment.waitForResult(), MegaChatError::ERROR_OK) << "Failed to revoke access to node. Error: " << crtRevokeAttachment.getErrorString();
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

    LOG_debug << "#### Test6: Download received file again --> no access ####";
    ASSERT_FALSE(downloadNode(1, nodeReceived)) << "Download succeed, when it should fail";

    delete nodeReceived;
    nodeReceived = NULL;

    delete msgSent;
    msgSent = NULL;

    delete nodeSent;
    nodeSent = NULL;

    LOG_debug << "#### Test7: Upload an image to check previews / thumbnails ####";
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

    LOG_debug << "#### Test8: Download the thumbnail ####";
    std::string thumbnailPath = LOCAL_PATH + "/thumbnail0.jpg";
    RequestTracker getThumbnailTracker(megaApi[a1]);
    megaApi[a1]->getThumbnail(nodeSent, thumbnailPath.c_str(), &getThumbnailTracker);
    ASSERT_EQ(getThumbnailTracker.waitForResult(), API_OK) << "Failed to get thumbnail. Error: " << getThumbnailTracker.getErrorString();

    LOG_debug << "#### Test9: Download the thumbnail ####";
    thumbnailPath = LOCAL_PATH + "/thumbnail1.jpg";
    RequestTracker getThumbnailTracker2(megaApi[a2]);
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
 * - Test1: Send a message to chatroom
 * + Test2: Receive message
 * Check if the last message received is equal to the message sent
 *
 * - Test3: Upload new file
 * - Test4: Send file as attachment to chatroom
 * + Test5: Receive message with attach node
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

    LOG_debug << "#### Test1: Send a message to chatroom ####";
    chatroomListener->clearMessages(a1);
    chatroomListener->clearMessages(a2);
    std::string formatDate = dateToString();

    MegaChatMessage *msgSent = sendTextMessageOrUpdate(a1, a2, chatid, formatDate, chatroomListener);
    ASSERT_TRUE(msgSent);
    MegaChatHandle msgId = msgSent->getMsgId();

    LOG_debug << "#### Test2: Receive message ####";
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

    LOG_debug << "#### Test3: Upload new file ####";
    formatDate = dateToString();
    createFile(formatDate, LOCAL_PATH, formatDate);
    MegaNode* nodeSent = uploadFile(a1, formatDate, LOCAL_PATH, REMOTE_PATH);
    ASSERT_TRUE(nodeSent);

    LOG_debug << "#### Test4: Send file as attachment to chatroom ####";
    msgSent = attachNode(a1, a2, chatid, nodeSent, chatroomListener);
    ASSERT_TRUE(msgSent);
    MegaNode *nodeReceived = msgSent->getMegaNodeList()->get(0)->copy();
    msgId = msgSent->getMsgId();

    LOG_debug << "#### Test5: Receive message with attach node ####";
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
 * - Test1: Send a message with an attach contact to chatroom
 * + Test2: Receive message
 * + Test3: Forward the contact message
 * - Test4: Receive message
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

    LOG_debug << "#### Test1: Send a message with an attach contact to chatroom ####";
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

    LOG_debug << "#### Test2: Receive message ####";
    ASSERT_TRUE(waitForResponse(flagReceived)) << "Timeout expired for receiving message by target user";    // for reception
    ASSERT_TRUE(chatroomListener->hasArrivedMessage(a2, msgId0)) << "Wrong message id at destination";
    MegaChatMessage *msgReceived = megaChatApi[a2]->getMessage(chatid, msgId0);   // message should be already received, so in RAM
    ASSERT_TRUE(msgReceived) << "Failed to get message by id";

    ASSERT_EQ(msgReceived->getType(), MegaChatMessage::TYPE_CONTACT_ATTACHMENT) << "Wrong type of message.";
    ASSERT_EQ(msgReceived->getUsersCount(), 1u) << "Wrong number of users in message.";
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

    LOG_debug << "#### Test3: Forward the contact message ####";
    MegaChatMessage *messageForwared = megaChatApi[a2]->forwardContact(chatid, msgId0, chatid);
    ASSERT_TRUE(messageForwared) << "Failed to forward a contact message";
    ASSERT_TRUE(waitForResponse(flagConfirmed)) << "Timeout expired for receiving confirmation by server";
    MegaChatHandle msgId1 = chatroomListener->mConfirmedMessageHandle[a2];
    ASSERT_NE(msgId1, MEGACHAT_INVALID_HANDLE) << "Wrong message id at origin";

    LOG_debug << "#### Test4: Receive message ####";
    ASSERT_TRUE(waitForResponse(flagReceived)) << "Timeout expired for receiving message by target user";    // for reception
    ASSERT_TRUE(chatroomListener->hasArrivedMessage(a1, msgId1)) << "Wrong message id at destination";
    MegaChatMessage *msgReceived1 = megaChatApi[a2]->getMessage(chatid, msgId1);   // message should be already received, so in RAM
    ASSERT_TRUE(msgReceived1) << "Failed to get message by id";

    ASSERT_EQ(msgReceived1->getType(), MegaChatMessage::TYPE_CONTACT_ATTACHMENT) << "Wrong type of message.";
    ASSERT_EQ(msgReceived1->getUsersCount(), 1u) << "Wrong number of users in message.";
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
 * - Test1: Create or retrieve a group chat room
 * - Test2: Send a message to chatroom
 * + Test3: Receive message
 * - Test4: Change chatroom title
 * + Test5: Check chatroom titles has changed
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

    MegaChatHandle chatid = getGroupChatRoom({a1, a2}, peers);
    delete peers;
    ASSERT_NE(chatid, MEGACHAT_INVALID_HANDLE);

    LOG_debug << "#### Test1: Create or retrieve a group chat room ####";
    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account 1";
    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account 2";

    // Load some message to feed history
    loadHistory(a1, chatid, chatroomListener);
    loadHistory(a2, chatid, chatroomListener);

    chatroomListener->clearMessages(a1);
    chatroomListener->clearMessages(a2);

    LOG_debug << "#### Test2: Send a message to chatroom ####";
    std::string textToSend = "Last Message";
    MegaChatMessage *msgSent = sendTextMessageOrUpdate(a1, a2, chatid, textToSend, chatroomListener);
    ASSERT_TRUE(msgSent);
    MegaChatHandle msgId = msgSent->getMsgId();

    LOG_debug << "#### Test3: Receive message ####";
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

    LOG_debug << "#### Test4: Change chatroom title ####";
    std::string title = "Title " + std::to_string(time(NULL));
    bool *titleItemChanged0 = &titleUpdated[a1]; *titleItemChanged0 = false;
    bool *titleItemChanged1 = &titleUpdated[a2]; *titleItemChanged1 = false;
    bool *titleChanged0 = &chatroomListener->titleUpdated[a1]; *titleChanged0 = false;
    bool *titleChanged1 = &chatroomListener->titleUpdated[a2]; *titleChanged1 = false;
    bool *mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
    std::string *msgContent = &chatroomListener->content[a1]; *msgContent = "";
    ChatRequestTracker crtSetTitle(megaChatApi[a1]);
    megaChatApi[a1]->setChatTitle(chatid, title.c_str(),&crtSetTitle);
    ASSERT_EQ(crtSetTitle.waitForResult(), MegaChatError::ERROR_OK) << "Failed to change name. Error: " << crtSetTitle.getErrorString();

    LOG_debug << "#### Test5: Check chatroom titles has changed ####";
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
 * - Test1: Select or create a group chat room
 * - Test2: Set secondary account chat room privilege to READ ONLY
 * + Test3: Set retention time for an invalid handle (error)
 * + Test4: Set retention time for an invalid chatroom (error)
 * + Test5: Set retention time without enough permissions (error)
 * - Test6: Set retention time to zero (disabled)
 * - Test7: Send a couple of messages
 * - Test8: Logout and login and check messages
 **/
TEST_F(MegaChatApiTest, RetentionHistory)
{
    unsigned a1 = 0;
    unsigned a2 = 1;

    // Login both accounts
    std::unique_ptr<char[]>sessionPrimary(login(a1));
    ASSERT_TRUE(sessionPrimary);
    std::unique_ptr<char[]>sessionSecondary(login(a2));
    ASSERT_TRUE(sessionSecondary);

    // Prepare peers, privileges...
    std::unique_ptr<MegaUser>user(megaApi[a1]->getContact(account(a2).getEmail().c_str()));
    if (!user || (user->getVisibility() != MegaUser::VISIBILITY_VISIBLE))
    {
        ASSERT_NO_FATAL_FAILURE({ makeContact(a1, a2); });
        user.reset(megaApi[a1]->getContact(account(a2).getEmail().c_str()));
    }

    LOG_debug << "Select or create a group chat room";
    MegaChatHandle uh = user->getHandle();
    std::unique_ptr<MegaChatPeerList> peers(MegaChatPeerList::createInstance());
    peers->addPeer(uh, MegaChatPeerList::PRIV_STANDARD);
    MegaChatHandle chatid = getGroupChatRoom({a1, a2}, peers.get());
    ASSERT_NE(chatid, MEGACHAT_INVALID_HANDLE);

    // Open chatroom
    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a1+1);
    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(chatid, chatroomListener)) << "Can't open chatRoom account " << (a2+1);
    std::unique_ptr<MegaChatRoom> chatroom (megaChatApi[a1]->getChatRoom(chatid));
    std::unique_ptr<char[]> chatidB64(megaApi[a1]->handleToBase64(chatid));
    ASSERT_TRUE(chatroom) << "Cannot get chatroom for id " << chatidB64.get();

    //=========================================================//
    // Preconditions: Set secondary account priv to READ ONLY.
    //=========================================================//
    if (chatroom->getPeerPrivilegeByHandle(uh) != PRIV_RO)
    {
        // Change peer privileges to Read-only
        bool *peerUpdated0 = &peersUpdated[a1]; *peerUpdated0 = false;
        bool *peerUpdated1 = &peersUpdated[a2]; *peerUpdated1 = false;
        bool *mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
        MegaChatHandle *uhAction = &chatroomListener->uhAction[a1]; *uhAction = MEGACHAT_INVALID_HANDLE;
        int *priv = &chatroomListener->priv[a1]; *priv = MegaChatRoom::PRIV_UNKNOWN;
        ChatRequestTracker crtSetReadonly(megaChatApi[a1]);
        megaChatApi[a1]->updateChatPermissions(chatid, uh, MegaChatRoom::PRIV_RO, &crtSetReadonly);
        ASSERT_EQ(crtSetReadonly.waitForResult(), MegaChatError::ERROR_OK) << "Failed to update privilege of peer. Error: " << crtSetReadonly.getErrorString();
        ASSERT_TRUE(waitForResponse(peerUpdated0)) << "Timeout expired for receiving peer update";
        ASSERT_TRUE(waitForResponse(peerUpdated1)) << "Timeout expired for receiving peer update";
        ASSERT_TRUE(waitForResponse(mngMsgRecv)) << "Timeout expired for receiving management message";
        ASSERT_EQ(*uhAction, uh) << "User handle from message doesn't match";
        ASSERT_EQ(*priv, MegaChatRoom::PRIV_RO) << "Privilege is incorrect";
    }

    LOG_debug << "#### Test1: Set retention time for an invalid handle. ####";
    ChatRequestTracker crtSetRetention(megaChatApi[a2]);
    megaChatApi[a2]->setChatRetentionTime(MEGACHAT_INVALID_HANDLE, 1, &crtSetRetention);
    ASSERT_EQ(crtSetRetention.waitForResult(), MegaChatError::ERROR_ARGS) << "Set retention time: Unexpected error for Invalid handle. Error: " << crtSetRetention.getErrorString();

    LOG_debug << "#### Test2: Set retention time for a not found chatroom ####";
    ChatRequestTracker crtSetRetention2(megaChatApi[a2]);
    megaChatApi[a2]->setChatRetentionTime(123456, 1, &crtSetRetention2);
    ASSERT_EQ(crtSetRetention2.waitForResult(), MegaChatError::ERROR_NOENT) << "Set retention time: Unexpected error for a not found chatroom. Error: " << crtSetRetention2.getErrorString();

    LOG_debug << "#### Test3: Set retention time without enough permissions. ####";
    ChatRequestTracker crtSetRetention3(megaChatApi[a2]);
    megaChatApi[a2]->setChatRetentionTime(chatid, 1, &crtSetRetention3);
    ASSERT_EQ(crtSetRetention3.waitForResult(), MegaChatError::ERROR_ACCESS) << "Set retention time: Unexpected error for not enough permissions. Error: " << crtSetRetention3.getErrorString();

    LOG_debug << "#### Test4: Disable retention time. ####";
    if (chatroom->getRetentionTime() != 0)
    {
        // Disable retention time if any
        bool *retentionTimeChanged0 = &chatroomListener->retentionTimeUpdated[a1]; *retentionTimeChanged0 = false;
        bool *retentionTimeChanged1 = &chatroomListener->retentionTimeUpdated[a2]; *retentionTimeChanged1 = false;
        bool *mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
        ChatRequestTracker crtSetRetention4(megaChatApi[a1]);
        megaChatApi[a1]->setChatRetentionTime(chatid, 0, &crtSetRetention4);
        ASSERT_EQ(crtSetRetention4.waitForResult(), MegaChatError::ERROR_OK) << "Set retention time: Unexpected error. Error: " << crtSetRetention4.getErrorString();
        ASSERT_TRUE(waitForResponse(retentionTimeChanged0)) << "Timeout expired for receiving chatroom update";
        ASSERT_TRUE(waitForResponse(retentionTimeChanged1)) << "Timeout expired for receiving chatroom update";
        ASSERT_TRUE(waitForResponse(mngMsgRecv)) << "Timeout expired for receiving management message";
    }

    LOG_debug << "#### Test5: Send some messages, then enable retention history, and wait for msg's removal ####";
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
    ChatRequestTracker crtSetRetention5(megaChatApi[a1]);
    megaChatApi[a1]->setChatRetentionTime(chatid, 5, &crtSetRetention5);
    ASSERT_EQ(crtSetRetention5.waitForResult(), MegaChatError::ERROR_OK) << "Set retention time: Unexpected error (5). Error: " << crtSetRetention5.getErrorString();
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

    LOG_debug << "#### Test6: Disable retention time ####";
    retentionTimeChanged0 = &chatroomListener->retentionTimeUpdated[a1]; *retentionTimeChanged0 = false;
    retentionTimeChanged1 = &chatroomListener->retentionTimeUpdated[a2]; *retentionTimeChanged1 = false;
    mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;

    LOG_debug << "#### Test7: Send a couple of messages ####";
    ChatRequestTracker crtSetRetention6(megaChatApi[a1]);
    megaChatApi[a1]->setChatRetentionTime(chatid, 0, &crtSetRetention6);
    ASSERT_EQ(crtSetRetention6.waitForResult(), MegaChatError::ERROR_OK) << "Set retention time: Unexpected error (6). Error: " << crtSetRetention6.getErrorString();
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

    LOG_debug << "#### Test8: Logout and login and check messages ####";
    ASSERT_NO_FATAL_FAILURE({ logout(a1, true); });
    ASSERT_NO_FATAL_FAILURE({ logout(a2, true); });
    sessionPrimary.reset(login(a1));
    ASSERT_TRUE(sessionPrimary);
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
 * - Test1: Get current name
 * - Test2: Change last name - it has been updated in memory and db.
 * - Test3: Get current name - value from memory
 * - Test4: Logout
 * - Test5: Login
 * - Test6: Get current name - value from db
 * - Test7: Change last name - set initial value for next tests execution
 *
 * Check if last name changed is the same at memory and at db
 */
TEST_F(MegaChatApiTest, ChangeMyOwnName)
{
    unsigned a1 = 0;

    char *sessionPrimary = login(a1);
    ASSERT_TRUE(sessionPrimary);

    LOG_debug << "#### Test1: Get current name ####";
    std::string appendToLastName = "Test";
    std::string myAccountLastName;
    char *nameFromApi = megaChatApi[a1]->getMyLastname();
    if (nameFromApi)
    {
        myAccountLastName = nameFromApi;
        delete [] nameFromApi;
        nameFromApi = NULL;
    }

    LOG_debug << "#### Test2: Change last name - it has been updated in memory and db. ####";
    std::string newLastName = myAccountLastName + appendToLastName;
    ASSERT_NO_FATAL_FAILURE({ changeLastName(a1, newLastName); });

    LOG_debug << "#### Test3: Get current name - value from memory ####";
    nameFromApi = megaChatApi[a1]->getMyLastname();
    std::string finalLastName;
    if (nameFromApi)
    {
        finalLastName = nameFromApi;
        delete [] nameFromApi;
        nameFromApi = NULL;
    }

    LOG_debug << "#### Test4: Logout ####";
    ASSERT_NO_FATAL_FAILURE({ logout(a1, false); });

    LOG_debug << "#### Test5: Login ####";
    char *newSession = login(a1, sessionPrimary);
    ASSERT_TRUE(newSession);

    LOG_debug << "#### Test6: Get current name - value from db ####";
    nameFromApi = megaChatApi[a1]->getMyLastname();
    std::string lastNameAfterLogout;
    if (nameFromApi)
    {
        lastNameAfterLogout = nameFromApi;
        delete [] nameFromApi;
        nameFromApi = NULL;
    }

    LOG_debug << "#### Test7: Change last name - set initial value for next tests execution ####";
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
 * - Test1: A calls B
 * - Test2: B rejects the call
 * - Test3: A calls B
 * - Test4: A cancels the call before B answers
 * - Test5: B logouts
 * - Test6: A calls B
 * - Test7: B logins
 * - Test8: B rejects the call
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

    LOG_debug << "#### Test1: A calls B ####";
    bool *callInProgress = &mCallInProgress[a1]; *callInProgress = false;
    bool *callReceivedRinging = &mCallReceivedRinging[a2]; *callReceivedRinging = false;
    mCallIdExpectedReceived[a2] = MEGACHAT_INVALID_HANDLE;
    mChatIdRingInCall[a2] = MEGACHAT_INVALID_HANDLE;
    bool *callDestroyed0 = &mCallDestroyed[a1]; *callDestroyed0 = false;
    bool *callDestroyed1 = &mCallDestroyed[a2]; *callDestroyed1 = false;
    int *termCode0 = &mTerminationCode[a1]; *termCode0 = 0;
    int *termCode1 = &mTerminationCode[a2]; *termCode1 = 0;
    mCallIdRingIn[a2] = MEGACHAT_INVALID_HANDLE;
    mCallIdJoining[a1] = MEGACHAT_INVALID_HANDLE;
    ChatRequestTracker crtCall(megaChatApi[a1]);
    megaChatApi[a1]->startChatCall(chatid, false, false, &crtCall);
    ASSERT_EQ(crtCall.waitForResult(), MegaChatError::ERROR_OK) << "Failed to start chat call: " << crtCall.getErrorString();
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

    LOG_debug << "#### Test2: B rejects the call ####";
    ChatRequestTracker crtHangup(megaChatApi[a2]);
    megaChatApi[a2]->hangChatCall(mCallIdRingIn[a2], &crtHangup);
    ASSERT_EQ(crtHangup.waitForResult(), MegaChatError::ERROR_OK) << "Failed to hang up chat call: " << crtHangup.getErrorString();
    ASSERT_TRUE(waitForResponse(callDestroyed0)) << "The call has to be finished account 1";
    ASSERT_TRUE(waitForResponse(callDestroyed1)) << "The call has to be finished account 2";

    LOG_debug << "#### Test3: A calls B ####";
    callInProgress = &mCallInProgress[a1]; *callInProgress = false;
    callReceivedRinging = &mCallReceivedRinging[a2]; *callReceivedRinging = false;
    mCallIdExpectedReceived[a2] = MEGACHAT_INVALID_HANDLE;
    mChatIdRingInCall[a2] = MEGACHAT_INVALID_HANDLE;
    callDestroyed0 = &mCallDestroyed[a1]; *callDestroyed0 = false;
    callDestroyed1 = &mCallDestroyed[a2]; *callDestroyed1 = false;
    termCode0 = &mTerminationCode[a1]; *termCode0 = 0;
    termCode1 = &mTerminationCode[a2]; *termCode1 = 0;
    mCallIdRingIn[a2] = MEGACHAT_INVALID_HANDLE;
    mCallIdJoining[a1] = MEGACHAT_INVALID_HANDLE;
    ChatRequestTracker crtCall2(megaChatApi[a1]);
    megaChatApi[a1]->startChatCall(chatid, false, false, &crtCall2);
    ASSERT_EQ(crtCall2.waitForResult(), MegaChatError::ERROR_OK) << "Failed to start chat call (2): " << crtCall2.getErrorString();
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

    LOG_debug << "#### Test4: A cancels the call before B answers ####";
    ChatRequestTracker crtHangup2(megaChatApi[a1]);
    megaChatApi[a1]->hangChatCall(mCallIdJoining[a1], &crtHangup2);
    ASSERT_EQ(crtHangup2.waitForResult(), MegaChatError::ERROR_OK) << "Failed to hang up chat call (2): " << crtHangup2.getErrorString();
    ASSERT_TRUE(waitForResponse(callDestroyed0)) << "The call has to be finished account 1";
    ASSERT_TRUE(waitForResponse(callDestroyed1)) << "The call has to be finished account 2";

    LOG_debug << "#### Test5: B logouts ####";
    ASSERT_NO_FATAL_FAILURE({ logout(a2); });
    callInProgress = &mCallInProgress[a1]; *callInProgress = false;
    bool* callReceived = &mCallWithIdReceived[a2]; *callReceived = false;
    callReceivedRinging = &mCallReceivedRinging[a2]; *callReceivedRinging = false;
    mChatIdRingInCall[a2] = MEGACHAT_INVALID_HANDLE;
    mCallIdExpectedReceived[a2] = MEGACHAT_INVALID_HANDLE;
    callDestroyed0 = &mCallDestroyed[a1]; *callDestroyed0 = false;
    callDestroyed1 = &mCallDestroyed[a2]; *callDestroyed1 = false;
    termCode0 = &mTerminationCode[a1]; *termCode0 = 0;
    termCode1 = &mTerminationCode[a2]; *termCode1 = 0;
    mCallIdRingIn[a2] = MEGACHAT_INVALID_HANDLE;
    mCallIdJoining[a1] = MEGACHAT_INVALID_HANDLE;

    LOG_debug << "#### Test6: A calls B ####";
    ChatRequestTracker crtCall3(megaChatApi[a1]);
    megaChatApi[a1]->startChatCall(chatid, false, false, &crtCall3);
    ASSERT_EQ(crtCall3.waitForResult(), MegaChatError::ERROR_OK) << "Failed to start chat call (3): " << crtCall3.getErrorString();
    ASSERT_TRUE(waitForResponse(callInProgress)) << "Timeout expired for receiving a call";
    auxCall.reset(megaChatApi[a1]->getChatCall(mChatIdInProgressCall[a1]));
    if (auxCall)
    {
        // set the callid that we expect to for account B in onChatCallUpdate
        mCallIdExpectedReceived[a2] = auxCall->getCallId();
    }

    LOG_debug << "#### Test7: B logins ####";
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

    LOG_debug << "#### Test8: B rejects the call ####";
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
        ChatRequestTracker crtHangup3(megaChatApi[a2]);
        megaChatApi[a2]->hangChatCall(ringingCallId, &crtHangup3);
        ASSERT_EQ(crtHangup3.waitForResult(), MegaChatError::ERROR_OK) << "Failed to hang up chat call (3): " << crtHangup3.getErrorString();

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
 * - Test1: A calls B
 * -        B in other client has to answer the call (manual)
 * - Test2: A mutes call
 * - Test3: A disables video
 * - Test4: A unmutes call
 * - Test5: A enables video
 * - Test6: A finishes the call
 * - Test7: A waits for B call
 * -        B calls A from other client (manual)
 * - Test8: A mutes call
 * - Test9: A disables video
 * - Test10: A unmutes call
 * - Test11: A enables video
 * - Test12: A finishes the call
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
    LOG_debug << "#### Test1: A calls B ####";
    ChatRequestTracker crtCall(megaChatApi[a1]);
    megaChatApi[a1]->startChatCall(chatid, true, true, &crtCall);
    ASSERT_EQ(crtCall.waitForResult(), MegaChatError::ERROR_OK) << "Failed to start chat call: " << crtCall.getErrorString();
    bool *callInProgress = &mCallInProgress[a1]; *callInProgress = false;
    ASSERT_TRUE(waitForResponse(callInProgress)) << "Timeout expired for receiving a call";
    sleep(5);
    LOG_debug << "#### Test2: A mutes call ####";
    megaChatApi[a1]->disableAudio(mChatIdInProgressCall[a1]);
    sleep(5);
    LOG_debug << "#### Test3: A disables video ####";
    megaChatApi[a1]->disableVideo(mChatIdInProgressCall[a1]);
    sleep(5);
    LOG_debug << "#### Test4: A unmutes call ####";
    megaChatApi[a1]->enableAudio(mChatIdInProgressCall[a1]);
    sleep(5);
    LOG_debug << "#### Test5: A enables video ####";
    megaChatApi[a1]->enableVideo(mChatIdInProgressCall[a1]);

    MegaChatCall *chatCall = megaChatApi[a1]->getChatCall(mChatIdInProgressCall[a1]);
    ASSERT_TRUE(chatCall) << "Invalid chat call at getChatCallByChatId";

    MegaChatCall *chatCall2 = megaChatApi[a1]->getChatCallByCallId(chatCall->getCallId());
    ASSERT_TRUE(chatCall2) << "Invalid chat call at getChatCall";


    bool *callDestroyed= &mCallDestroyed[a1]; *callDestroyed = false;
    sleep(5);
    LOG_debug << "#### Test6: A finishes the call ####";
    sleep(2);
    megaChatApi[a1]->hangChatCall(chatCall->getCallId());
    std::cout << "Call finished." << std::endl;

    ASSERT_TRUE(waitForResponse(callDestroyed)) << "The call has to be finished";
    megaChatApi[a1]->removeChatLocalVideoListener(chatid, &localVideoListener);

    LOG_debug << "#### Test7: A waits for B call (manual) ####";
    bool *callReceivedRinging = &mCallReceivedRinging[a1]; *callReceivedRinging = false;
    mChatIdRingInCall[a1] = MEGACHAT_INVALID_HANDLE;
    ASSERT_TRUE(waitForResponse(callReceivedRinging)) << "Timeout expired for receiving a call";
    ASSERT_NE(mChatIdRingInCall[a1], MEGACHAT_INVALID_HANDLE) << "Invalid Chatid from call emisor";
    megaChatApi[a1]->answerChatCall(mChatIdRingInCall[a1], true /*video*/, true /*audio*/);
    megaChatApi[a1]->addChatLocalVideoListener(chatid, &localVideoListener);

    sleep(5);
    LOG_debug << "#### Test8: A mutes call ####";
    megaChatApi[a1]->disableAudio(chatCall->getCallId());
    sleep(5);
    LOG_debug << "#### Test9: A disables video ####";
    megaChatApi[a1]->disableVideo(chatCall->getCallId());
    sleep(5);
    LOG_debug << "#### Test10: A unmutes call ####";
    megaChatApi[a1]->enableAudio(chatCall->getCallId());
    sleep(5);
    LOG_debug << "#### Test11: A enables video ####";
    megaChatApi[a1]->enableVideo(chatCall->getCallId());

    sleep(10);
    LOG_debug << "#### Test12: A finishes the call ####";
    sleep(2);
    megaChatApi[a1]->hangChatCall(mChatIdInProgressCall[a1]);
    LOG_debug << "Call finished.";
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
 * - Test1: A looks for the group chat
 * - Test2: A starts a call in that group chat
 * - Test3: A waits for call was established correctly
 * - Test4: A hangs up the call
 * + Test5: A waits to receive a incoming call
 * - Test6: A answers it
 * - Test7: A hangs the call
 */
TEST_F(MegaChatApiTest, DISABLED_ManualGroupCalls)
{
    LOG_debug << "#### Test1: A looks for the group chat ####";
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
    LOG_debug << "#### Test2: A starts a call in that group chat ####";
    std::cerr << "Start Call" << std::endl;
    ChatRequestTracker crtCall(megaChatApi[a1]);
    megaChatApi[a1]->startChatCall(chatid, true, true, &crtCall);
    ASSERT_EQ(crtCall.waitForResult(), MegaChatError::ERROR_OK) << "Failed to start chat call: " << crtCall.getErrorString();
    bool *callInProgress = &mCallInProgress[a1]; *callInProgress = false;
    ASSERT_TRUE(waitForResponse(callInProgress)) << "Timeout expired for receiving a call";

    LOG_debug << "#### Test3: A waits for call was established correctly ####";
    std::cout << "Waiting for the other peer to answer the call..." << std::endl;
    sleep(60);

    MegaChatCall *chatCall = megaChatApi[a1]->getChatCall(mChatIdInProgressCall[a1]);
    ASSERT_TRUE(chatCall) << "Invalid chat call at getChatCall (by chatid)";

    MegaChatCall *chatCall2 = megaChatApi[a1]->getChatCallByCallId(chatCall->getCallId());
    ASSERT_TRUE(chatCall2) << "Invalid chat call at getChatCall (by callid)";

    delete chatCall;    chatCall = NULL;
    delete chatCall2;   chatCall2 = NULL;

    LOG_debug << "#### Test4: A hangs up the call ####";
    bool *callDestroyed= &mCallDestroyed[a1]; *callDestroyed = false;
    std::cerr << "Finish Call" << std::endl;
    megaChatApi[a1]->hangChatCall(mChatIdInProgressCall[a1]);
    std::cout << "Call finished." << std::endl;
    ASSERT_TRUE(waitForResponse(callDestroyed)) << "The call must be already finished and it is not";
    megaChatApi[a1]->removeChatLocalVideoListener(chatid, &localVideoListener);

    LOG_debug << "#### Test5: A waits to receive a incoming call ####";
    std::cout << "Waiting for the other peer to start a call..." << std::endl;
    sleep(20);

    std::cout << "Ready to receive calls..." << std::endl;
    bool *callReceived = &mCallReceivedRinging[a1]; *callReceived = false;
    mChatIdRingInCall[a1] = MEGACHAT_INVALID_HANDLE;
    ASSERT_TRUE(waitForResponse(callReceived)) << "Timeout expired for receiving a call";
    ASSERT_NE(mChatIdRingInCall[a1], MEGACHAT_INVALID_HANDLE) << "Invalid Chatid from call emisor";

    LOG_debug << "#### Test6: A answers it ####";
    megaChatApi[a1]->answerChatCall(mChatIdRingInCall[a1], true /*video*/, true /*audio*/);
    megaChatApi[a1]->addChatLocalVideoListener(chatid, &localVideoListener);

    LOG_debug << "#### Test7: A hangs the call ####";
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
 * + Test1: A starts a groupal Meeting in chat1 (without audio nor video)
 * - Test2: B answers call (without audio nor video)
 * - Test3: A mutes B in call
 * - Test4: B puts call in hold on
 * + Test5: A puts call in hold on
 * + Test6: A releases hold on
 * - Test7: B releases hold on
 * - Test8: B enables audio monitor
 * - Test9: B disables audio monitor
 * + Test10: A force reconnect => retryPendingConnections(true)
 * - Test11: B hangs up call
 * + Test12: A hangs up call
 */
TEST_F(MegaChatApiTest, EstablishedCalls)
{
    unsigned a1 = 0;
    unsigned a2 = 1;

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
    MegaChatHandle chatid = getGroupChatRoom({a1, a2}, peers.get(),
                                             megachat::MegaChatPeerList::PRIV_MODERATOR,
                                             true /*create*/,
                                             true /*meetingRoom*/,
                                             false /*waitingRoom*/);

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

    // Make some testing with limit for simultaneous input video tracks in both accounts
    LOG_debug << "Checking that default limit for simultaneous input video tracks is valid for both accounts";
    ASSERT_NE(megaChatApi[a1]->getCurrentInputVideoTracksLimit(), MegaChatApi::INVALID_CALL_VIDEO_SENDERS)
        << "Default limit for simultaneous input video tracks that call supports is invalid for primary account";

    ASSERT_NE(megaChatApi[a2]->getCurrentInputVideoTracksLimit(), MegaChatApi::INVALID_CALL_VIDEO_SENDERS)
        << "Default limit for simultaneous input video tracks that call supports is invalid for secondary account";

    LOG_debug << "Trying to set and invalid limit for simultaneous input video tracks for both accounts";
    unsigned int limitInputVideoTracks = static_cast<unsigned int>(megaChatApi[a1]->getMaxSupportedVideoCallParticipants()) + 1;

    ASSERT_FALSE(megaChatApi[a1]->setCurrentInputVideoTracksLimit(limitInputVideoTracks))
        << "setCurrentInputVideoTracksLimit should failed for an invalid numInputVideoTracks value for primary account";

    ASSERT_FALSE(megaChatApi[a2]->setCurrentInputVideoTracksLimit(limitInputVideoTracks))
        << "setCurrentInputVideoTracksLimit should failed for an invalid numInputVideoTracks value for secondary account";

    LOG_debug << "Setting a valid limit for simultaneous input video tracks for both accounts";
    limitInputVideoTracks -= 5;

    ASSERT_TRUE(megaChatApi[a1]->setCurrentInputVideoTracksLimit(limitInputVideoTracks))
        << "setCurrentInputVideoTracksLimit should succeed for a valid numInputVideoTracks value for primary account";

    ASSERT_TRUE(megaChatApi[a2]->setCurrentInputVideoTracksLimit(limitInputVideoTracks))
        << "setCurrentInputVideoTracksLimit should succeed for a valid numInputVideoTracks value for secondary account";

    LOG_debug << "Checking that limit for simultaneous input video tracks has been updated properly";
    ASSERT_EQ(megaChatApi[a1]->getCurrentInputVideoTracksLimit(), limitInputVideoTracks)
        << "Default limit for simultaneous input video tracks that call supports has not been updated for primary account";

    ASSERT_EQ(megaChatApi[a2]->getCurrentInputVideoTracksLimit(), limitInputVideoTracks)
        << "Default limit for simultaneous input video tracks that call supports has not been updated for secondary account";

    LOG_debug << "#### Test1: A starts a groupal Meeting in chat1 (without audio nor video) ####";
    mCallIdJoining[a1] = MEGACHAT_INVALID_HANDLE;
    mChatIdInProgressCall[a1] = MEGACHAT_INVALID_HANDLE;
    mCallIdRingIn[a2] = MEGACHAT_INVALID_HANDLE;
    mChatIdRingInCall[a2] = MEGACHAT_INVALID_HANDLE;

    ASSERT_NO_FATAL_FAILURE({
    waitForAction (1, // just one attempt as mCallReceivedRinging for B account could fail but call could have been created from A account
                   std::vector<bool *> { &mCallInProgress[a1], &mCallReceivedRinging[a2]},
                   std::vector<string> { "mCallInProgress[a1]", "mCallReceivedRinging[a2]"},
                   "starting chat call from A",
                   true /* wait for all exit flags*/,
                   true /*reset flags*/,
                   maxTimeout,
                   [this, a1, chatid]()
                                {
                                    ChatRequestTracker crtCall(megaChatApi[a1]);
                                    megaChatApi[a1]->startChatCall(chatid, /*enableVideo*/ false, /*enableAudio*/ false, &crtCall);
                                    ASSERT_EQ(crtCall.waitForResult(), MegaChatError::ERROR_OK)
                                    << "Failed to start call. Error: " << crtCall.getErrorString();
                                });
    });

    LOG_debug << "#### Test2: B answers call (without audio nor video) ####";
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
                   std::vector<bool *> { &mChatCallSessionStatusInProgress[a1],
                                         &mChatCallSessionStatusInProgress[a2]
                                       },
                   std::vector<string> { "mChatCallSessionStatusInProgress[a1]",
                                         "mChatCallSessionStatusInProgress[a2]"
                                       },
                   "answering chat call from B",
                   true /* wait for all exit flags*/,
                   true /*reset flags*/,
                   maxTimeout,
                   [this, a2, chatid]()
                                {
                                    ChatRequestTracker crtAnswerCall(megaChatApi[a2]);
                                    megaChatApi[a2]->answerChatCall(chatid, /*enableVideo*/ false, /*enableAudio*/ true, &crtAnswerCall);
                                    ASSERT_EQ(crtAnswerCall.waitForResult(), MegaChatError::ERROR_OK)
                                    << "Failed to answer call. Error: " << crtAnswerCall.getErrorString();
                                });
    });

    auxCall.reset(megaChatApi[a1]->getChatCall(chatid));
    ASSERT_TRUE(auxCall) << "Can't get call from chatroom: " << getChatIdStrB64(chatid);
    std::unique_ptr<MegaHandleList> hl(auxCall->getSessionsClientid());
    ASSERT_TRUE(hl && hl->size()) << "Can't get a client id list from call";
    MegaChatSession* secondarySess = auxCall->getMegaChatSession(hl->get(0));
    ASSERT_TRUE(secondarySess) << "Can't get a session for clientid: " << hl->get(0);
    MegaChatHandle secondaryCid = secondarySess->getClientid();
    ASSERT_NE(secondaryCid, MEGACHAT_INVALID_HANDLE) << "Invalid client id for secondary session";

    LOG_debug << "#### Test3: A mutes B in call ####";
    bool* remoteAvFlagsChanged = &mChatCallAudioDisabled[a1]; *remoteAvFlagsChanged = false; // a2 will receive onChatSessionUpdate (CHANGE_TYPE_REMOTE_AVFLAGS)
    exitFlag = &mChatCallAudioDisabled[a2]; *exitFlag = false; // a2 will receive onChatCallUpdate (CHANGE_TYPE_LOCAL_AVFLAGS)
    action = [this, a1, chatid, secondaryCid]()
    {
        ChatRequestTracker crtMutePeers(megaChatApi[a1]);
        megaChatApi[a1]->mutePeers(chatid, secondaryCid, &crtMutePeers);
        ASSERT_EQ(crtMutePeers.waitForResult(), MegaChatError::ERROR_OK)
            << "Failed to mute peer. Error: " << crtMutePeers.getErrorString();
    };
    ASSERT_NO_FATAL_FAILURE({
        waitForCallAction(a1 /*performer*/, MAX_ATTEMPTS, exitFlag, "receiving MUTED notification from SFU for secondary account", maxTimeout, action);
    });
    ASSERT_TRUE(waitForResponse(remoteAvFlagsChanged)) << "Timeout expired for Primary account receiving AvFlags update for Secondary account";

    LOG_debug << "#### Test4: B puts call in hold on ####";
    exitFlag = &mChatCallOnHold[a1]; *exitFlag = false;  // from receiver account
    action = [this, a2, chatid](){ megaChatApi[a2]->setCallOnHold(chatid, /*setOnHold*/ true); };
    ASSERT_NO_FATAL_FAILURE({
    waitForCallAction(a2 /*performer*/, MAX_ATTEMPTS, exitFlag, "receiving call on hold at account A", maxTimeout, action);
                            });

    LOG_debug << "#### Test5: A puts call in hold on ####";
    exitFlag = &mChatCallOnHold[a2]; *exitFlag = false; // from receiver account
    action = [this, a1, chatid](){ megaChatApi[a1]->setCallOnHold(chatid, /*setOnHold*/ true); };
    ASSERT_NO_FATAL_FAILURE({
    waitForCallAction(a2 /*performer*/, MAX_ATTEMPTS, exitFlag, "receiving call on hold at account B", maxTimeout, action);
                            });

    LOG_debug << "#### Test6: A releases hold on ####";
    exitFlag = &mChatCallOnHoldResumed[a2]; *exitFlag = false; // from receiver account
    action = [this, a1, chatid](){ megaChatApi[a1]->setCallOnHold(chatid, /*setOnHold*/ false); };
    ASSERT_NO_FATAL_FAILURE({
    waitForCallAction(a2 /*performer*/, MAX_ATTEMPTS, exitFlag, "receiving call resume from on hold at account B", maxTimeout, action);
                            });

    LOG_debug << "#### Test7: B releases hold on ####";
    exitFlag = &mChatCallOnHoldResumed[a1]; *exitFlag = false; // from receiver account
    action = [this, a2, chatid](){ megaChatApi[a2]->setCallOnHold(chatid, /*setOnHold*/ false); };
    ASSERT_NO_FATAL_FAILURE({
    waitForCallAction(a2 /*performer*/, MAX_ATTEMPTS, exitFlag, "receiving call resume from on hold at account A", maxTimeout, action);
                            });

    LOG_debug << "#### Test8: B enables audio monitor ####";
    exitFlag = &mChatCallAudioEnabled[a1]; *exitFlag = false; // from receiver account
    action = [this, a2, chatid](){ megaChatApi[a2]->enableAudio(chatid); };
    ASSERT_NO_FATAL_FAILURE({
    waitForCallAction(a2 /*performer*/, MAX_ATTEMPTS, exitFlag, "receiving audio enabled at account A", maxTimeout, action);
                            });

    LOG_debug << "#### Test9: B disables audio monitor ####";
    exitFlag = &mChatCallAudioEnabled[a2]; *exitFlag = false; // from receiver account
    action = [this, a1, chatid](){ megaChatApi[a1]->enableAudio(chatid); };
    ASSERT_NO_FATAL_FAILURE({
    waitForCallAction(a1 /*performer*/, MAX_ATTEMPTS, exitFlag, "receiving audio enabled at account B", maxTimeout, action);
                            });

    LOG_debug << "#### Test10: A force reconnect => retryPendingConnections(true) ####";
    exitFlag = &mChatCallAudioDisabled[a1]; *exitFlag = false; // from receiver account
    action = [this, a2, chatid](){ megaChatApi[a2]->disableAudio(chatid); };
    ASSERT_NO_FATAL_FAILURE({
    waitForCallAction(a2 /*performer*/, MAX_ATTEMPTS, exitFlag, "receiving audio disabled at account A", maxTimeout, action);
                            });

    LOG_debug << "A disabling audio in the call";
    exitFlag = &mChatCallAudioDisabled[a2]; *exitFlag = false; // from receiver account
    action = [this, a1, chatid](){ megaChatApi[a1]->disableAudio(chatid); };
    ASSERT_NO_FATAL_FAILURE({
    waitForCallAction(a1 /*performer*/, MAX_ATTEMPTS, exitFlag, "receiving audio disabled at account B", maxTimeout, action);
                            });

    LOG_debug << "A forcing a reconnect";
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

    ChatRequestTracker crtRetryConn(megaChatApi[a1]);
    megaChatApi[a1]->retryPendingConnections(true, &crtRetryConn);
    // wait for session destruction checks
    std::function<void()> waitForChatCallSessionDestroyedB =
        [this, &sessionWasDestroyedB]()
        {
            ASSERT_TRUE(waitForResponse(sessionWasDestroyedB))
                             << "Timeout expired for B receiving session destroyed notification";
        };
    ASSERT_NO_FATAL_FAILURE({ waitForChatCallSessionDestroyedB(); });
    std::function<void()> waitForChatCallSessionDestroyedA =
        [this, &sessionWasDestroyedA]()
        {
            ASSERT_TRUE(waitForResponse(sessionWasDestroyedA))
                             << "Timeout expired for A receiving session destroyed notification";
        };
    ASSERT_NO_FATAL_FAILURE({ waitForChatCallSessionDestroyedA(); });
    // Wait for request finish (i.e. disconnection confirmation)
    ASSERT_EQ(crtRetryConn.waitForResult(), MegaChatError::ERROR_OK) << "Client A failed to reconnect. Error: " << crtRetryConn.getErrorString();
    ASSERT_TRUE(crtRetryConn.getFlag());
    ASSERT_FALSE(crtRetryConn.getParamType());

    // B confirms new mega chat session is ready
    ASSERT_NO_FATAL_FAILURE({ waitForChatCallReadyB(); });
    // A confirms new mega chat session is ready
    ASSERT_NO_FATAL_FAILURE({ waitForChatCallReadyA(); });

    // B hangs up
    bool* callDestroyedB = &mCallDestroyed[a2]; *callDestroyedB = false;
    *sessionWasDestroyedB = false; *sessionWasDestroyedA = false; // reset flags of session destruction

    LOG_debug << "#### Test11: B hangs up call ####";
    bool hangupCallExitFlag = false;
    action = [&api = megaChatApi[a2], &ringInHandle = mCallIdRingIn[a2], &hangupCallExitFlag]()
    {
        ChatRequestTracker crtHangup(api);
        api->hangChatCall(ringInHandle, &crtHangup);
        auto res = crtHangup.waitForResult();
        hangupCallExitFlag = true;
        ASSERT_EQ(res, MegaChatError::ERROR_OK)
                << "Failed to hangup call (B). Error: " << crtHangup.getErrorString();
    };
    ASSERT_NO_FATAL_FAILURE({
    waitForCallAction(a2 /*performer*/, MAX_ATTEMPTS, &hangupCallExitFlag, "hanging up chat call at account B", maxTimeout, action);
                            });

    // wait for session destruction checks
    ASSERT_NO_FATAL_FAILURE({ waitForChatCallSessionDestroyedB(); });
    ASSERT_NO_FATAL_FAILURE({ waitForChatCallSessionDestroyedA(); });
    LOG_debug << "Call finished for B";

    bool* callDestroyedA = &mCallDestroyed[a1]; *callDestroyedA = false;
    LOG_debug << "#### Test12: A hangs up call ####";
    hangupCallExitFlag = false;
    action = [&api = megaChatApi[a1], &joinIdHandle = mCallIdJoining[a1], &hangupCallExitFlag]()
    {
        ChatRequestTracker crtHangup(api);
        api->hangChatCall(joinIdHandle, &crtHangup);
        auto res = crtHangup.waitForResult();
        hangupCallExitFlag = true;
        ASSERT_EQ(res, MegaChatError::ERROR_OK)
                << "Failed to hangup call (A). Error: " << crtHangup.getErrorString();
    };
    ASSERT_NO_FATAL_FAILURE({
    waitForCallAction(a1 /*performer*/, MAX_ATTEMPTS, &hangupCallExitFlag, "hanging up chat call at account A", maxTimeout, action);
                            });
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
 * @brief MegaChatApiTest.RaiseHandToSpeakCall
 * - Test1: A starts call in a meeting room with speak request option enabled, B answers
 * - Test2: B request to speak, A rejects it
 * - Test3: B requests to speak, A approves it
 * - Test4: B enables audio (unmute)
 * - Test5: Remove B as speaker
 */
TEST_F(MegaChatApiTest, DISABLED_RaiseHandToSpeakCall)
{
    // Note: Speak request feature is not available for current protocol SFU version, enable this test once
    // SFU code has been updated and MegaChat also has made required adjustments
    const unsigned int a1 = 0; // primary account
    const unsigned int a2 = 1; // secondary account

    // Test preparation. Prepare users, and chat room
    std::unique_ptr<char[]> primarySession(login(a1));   // user A
    ASSERT_TRUE(primarySession);
    std::unique_ptr<char[]> secondarySession(login(a2)); // user B
    ASSERT_TRUE(secondarySession);
    LOG_debug << "\tSwitching to staging (TEMPORARY)";
    megaApi[a1]->changeApiUrl("https://staging.api.mega.co.nz/");
    megaApi[a1]->setSFUid(336); // set SFU id to staging (temporary)

    if (!areContact(a1, a2))
    {
        ASSERT_NO_FATAL_FAILURE({ makeContact(a1, a2); });
    }

    // select/create a chatroom with speak request enabled
    std::unique_ptr<MegaUser> secondaryUser(megaApi[a1]->getContact(account(a2).getEmail().c_str()));
    ASSERT_TRUE(secondaryUser) << "Cannot get contact for secondary account";
    const MegaChatHandle secondaryUh = secondaryUser->getHandle();
    const std::shared_ptr<MegaChatPeerList> peerList(MegaChatPeerList::createInstance());
    const MegaChatHandle chatid = getGroupChatRoom({a1}, peerList.get(), megachat::MegaChatPeerList::PRIV_MODERATOR, true /*create*/
                                                   , true /*publicChat*/, true /*meetingRoom*/
                                                   , false /*waitingRoom*/, true /*speakRequest*/);

    ASSERT_NE(chatid, MEGACHAT_INVALID_HANDLE) << "Can't get/create a chat room with speak request enabled.";
    const std::unique_ptr<char[]> chatIdB64(MegaApi::userHandleToBase64(chatid));
    std::unique_ptr<MegaChatRoom> chatRoom(megaChatApi[a1]->getChatRoom(chatid));
    ASSERT_TRUE(chatRoom) << "Can't get chatroom with chatid: " << chatIdB64.get();
    ASSERT_TRUE(chatRoom->isMeeting() && chatRoom->isSpeakRequest()) << "Selected chatroom is not meeting room or speak "
                                                                        "request is disabled. chatid: " << chatIdB64.get();

    ASSERT_EQ(megaChatApi[a1]->getChatConnectionState(chatid), MegaChatApi::CHAT_CONNECTION_ONLINE) <<
        "Not connected to chatd for account " << (a1+1) << ": " << account(a1).getEmail();

    // chatroom listener shared by all accounts in the test
    std::shared_ptr<TestChatRoomListener>chatroomListener(new TestChatRoomListener(this, megaChatApi, chatid));
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, chatroomListener.get())) << "Can't open chatRoom user A";

    // check peer's privilege
    const auto secondaryPriv = chatRoom->getPeerPrivilegeByHandle(secondaryUser->getHandle());
    if (secondaryPriv == megachat::MegaChatPeerList::PRIV_UNKNOWN || secondaryPriv == megachat::MegaChatPeerList::PRIV_RM)
    {
        ASSERT_NO_FATAL_FAILURE(inviteToChat(a1, a2, secondaryUh, chatid, MegaChatPeerList::PRIV_STANDARD, chatroomListener));
    }
    else if (secondaryPriv != megachat::MegaChatPeerList::PRIV_STANDARD)
    {
        ASSERT_NO_FATAL_FAILURE(updateChatPermission(a1, a2, secondaryUh, chatid, megachat::MegaChatPeerList::PRIV_STANDARD, chatroomListener));
    }

    chatRoom.reset(megaChatApi[a1]->getChatRoom(chatid));
    ASSERT_TRUE(chatRoom) << "Can't get chatroom after update peer permissions, chatid: " << chatIdB64.get();
    ASSERT_TRUE(chatRoom->getPeerPrivilegeByHandle(secondaryUser->getHandle()) == megachat::MegaChatPeerList::PRIV_STANDARD)
        << "Can't update Meeting room aux user permission to standard";

    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(chatid, chatroomListener.get())) << "Can't open chatRoom user B";

    TestChatVideoListener localVideoListenerA;
    megaChatApi[a1]->addChatLocalVideoListener(chatid, &localVideoListenerA);

    TestChatVideoListener localVideoListenerB;
    megaChatApi[a2]->addChatLocalVideoListener(chatid, &localVideoListenerB);

    auto removeSpeaker = [this](const unsigned int moderatorIdx, const unsigned int peerIdx, const MegaChatHandle peerId, const MegaChatHandle chatid) -> void
    {
        std::unique_ptr<MegaChatCall> call(megaChatApi[moderatorIdx]->getChatCall(chatid));
        ASSERT_TRUE(call) << "Call could not be retrieved for account: " << moderatorIdx << ". Chatid: " << getChatIdStrB64(chatid);
        ASSERT_TRUE(call->isSpeakRequestEnabled()) << "Speak request is disabled for call: " << getCallIdStrB64(call->getCallId());
        ASSERT_TRUE(call->isOwnModerator()) << "Unexpected call permission for account: " << moderatorIdx << ". Callid: " << getCallIdStrB64(call->getCallId());

        MegaChatHandle peerCid = MEGACHAT_INVALID_HANDLE;
        std::unique_ptr<MegaHandleList> sessionsList(call->getSessionsClientid());
        for (unsigned int i = 0; i < sessionsList->size(); ++i)
        {
            auto cid = sessionsList->get(i);
            const MegaChatSession* sess = call->getMegaChatSession(cid);
            if (sess && !sess->isModerator() && sess->getPeerid() == peerId)
            {
                peerCid = cid;
                break;
            }
        }

        ASSERT_NE(peerCid, MEGACHAT_INVALID_HANDLE) << "Could not get any peer (non host) for Callid: " << getCallIdStrB64(call->getCallId());
        ASSERT_NO_FATAL_FAILURE({
            waitForAction (1,
                          { &mOwnFlagsChanged[peerIdx], &mOwnSpeakStatusChanged[peerIdx]
                           , &mSessSpeakPermChanged[moderatorIdx], &mChatCallAudioDisabled[moderatorIdx]},
                          { "mOwnFlagsChanged[moderatorIdx]", "mOwnSpeakStatusChanged[requesterIdx]", "mSessSpeakPermChanged[moderatorIdx]"
                           , "mChatCallAudioDisabled[moderatorIdx]"},
                          "Remove speaker from call",
                          true /* wait for all exit flags*/,
                          true /* reset flags */,
                          maxTimeout,
                          [this, &moderatorIdx, &peerCid, &chatid]()
                          {
                              ChatRequestTracker crtSpeakerRemove(megaChatApi[moderatorIdx]);
                              megaChatApi[moderatorIdx]->removeSpeaker(chatid, peerCid, &crtSpeakerRemove);
                              auto res = crtSpeakerRemove.waitForResult();
                              ASSERT_EQ(res, MegaChatError::ERROR_OK) << "Failed to remove speaker: " << crtSpeakerRemove.getErrorString();
                          });
        });

        ASSERT_TRUE(!mSessSpeakPerm[moderatorIdx][peerCid]) << "onChatSessionUpdate(CHANGE_TYPE_SPEAK_PERMISSION) not received for peercid: "
                                                            << peerCid << ". Callid: " << getCallIdStrB64(call->getCallId());

        ASSERT_EQ(mOwnSpeakStatus[peerIdx], MegaChatCall::SPEAKER_STATUS_DISABLED) << "Peer speak status: " << mOwnSpeakStatus[peerIdx]
                                                                                   << " it should be SPEAKER_STATUS_DISABLED"
                                                                                   << ". Callid: " << getCallIdStrB64(call->getCallId());
    };

    auto checkSpeakPermissions = [this](const unsigned int performerIdx, const bool isMod, const MegaChatHandle chatid) -> void
    {
        std::unique_ptr<MegaChatCall> call(megaChatApi[performerIdx]->getChatCall(chatid));
        ASSERT_TRUE(call) << "Call could not be retrieved for account: " << performerIdx;
        ASSERT_TRUE(call->isSpeakRequestEnabled()) << "Speak request is disabled for call: " << getChatIdStrB64(call->getCallId());

        // check own speak permissions
        ASSERT_TRUE(call->isOwnModerator() || !isMod) << "Unexpected call permission for account: " << performerIdx;
        ASSERT_TRUE(call->hasSpeakPermission() || !isMod) << "Unexpected speak permission for account: " << performerIdx;
        ASSERT_TRUE(!call->hasLocalAudio()) << "Audio is flag is enabled for account: " << performerIdx;

        // check speak permissions for the rest of participants
        std::unique_ptr<MegaHandleList> sessionsList(call->getSessionsClientid());
        for (unsigned int i = 0; i < sessionsList->size(); ++i)
        {
            const auto cid = sessionsList->get(i);
            const MegaChatSession* sess = call->getMegaChatSession(cid);
            ASSERT_TRUE(sess) << "account session could not be retrieved for cid: " << i;
            ASSERT_TRUE(!sess->hasAudio()) << "session is ummuted for cid: " << i;
            ASSERT_TRUE(!sess->isModerator() ? !sess->hasSpeakPermission() : sess->hasSpeakPermission())
                << "Unexpected speak permission for cid: " << i;
        }
    };

    auto requestSpeak = [this](const unsigned int requesterIdx, const unsigned int moderatorIdx, const bool approve, const MegaChatHandle chatid)
    {
        MegaChatHandle clientId = MEGACHAT_INVALID_HANDLE;
        mSessSpeakRequests[moderatorIdx].clear();
        ASSERT_NO_FATAL_FAILURE({
            waitForAction (1,
                          { &mSessSpeakReqRecv[moderatorIdx], &mOwnSpeakStatusChanged[requesterIdx]},
                          { "mChatCallSpeakReq[moderatorIdx]", "mSpeakStatusChanged[requesterIdx]"},
                          "Send speak request",
                          true /* wait for all exit flags*/,
                          true /* reset flags */,
                          maxTimeout,
                          [this, &requesterIdx, &chatid]()
                          {
                              ChatRequestTracker crtSpeakReq(megaChatApi[requesterIdx]);
                              megaChatApi[requesterIdx]->requestSpeak(chatid, &crtSpeakReq);
                              auto res = crtSpeakReq.waitForResult();
                              ASSERT_EQ(res, MegaChatError::ERROR_OK) << "Failed to request speak: " << crtSpeakReq.getErrorString();
                          });
        });

        ASSERT_EQ(mSessSpeakRequests[moderatorIdx].size(), 1u) << "Unexpected speak request list size for account index: " << moderatorIdx;
        ASSERT_EQ(mSessSpeakRequests[moderatorIdx].begin()->second, true) << "Speak request not received for account index: " << moderatorIdx;
        clientId = mSessSpeakRequests[moderatorIdx].begin()->first;

        std::vector<bool*> exitFlags = {&mOwnSpeakStatusChanged[requesterIdx]};
        std::vector<std::string> exitFlagsStr = {"mOwnSpeakStatusChanged[requesterIdx]"};
        if (approve)
        {
            exitFlags.emplace_back(&mSessSpeakPermChanged[moderatorIdx]);
            exitFlagsStr.emplace_back("mSessSpeakPermChanged[moderatorIdx]");
        }

        std::string msgSpeakReq = approve ? "approve speak request" : "reject speak request";
        mSessSpeakRequests[moderatorIdx].clear();
        mSessSpeakPerm[moderatorIdx].clear();
        ASSERT_NO_FATAL_FAILURE({
            waitForAction (1,
                          exitFlags,
                          exitFlagsStr,
                          msgSpeakReq.c_str(),
                          true /* wait for all exit flags*/,
                          true /* reset flags */,
                          maxTimeout,
                          [this, &moderatorIdx, &clientId, &approve, &chatid]()
                          {
                              ChatRequestTracker crtSpeakReq(megaChatApi[moderatorIdx]);
                              approve
                                  ? megaChatApi[moderatorIdx]->approveSpeakRequest(chatid, clientId, &crtSpeakReq)
                                  : megaChatApi[moderatorIdx]->rejectSpeakRequest(chatid, clientId, &crtSpeakReq);

                              auto res = crtSpeakReq.waitForResult();
                              ASSERT_EQ(res, MegaChatError::ERROR_OK) << "Failed to request speak: " << crtSpeakReq.getErrorString();

                          });
        });

        ASSERT_EQ(mOwnSpeakStatus[requesterIdx], approve ? MegaChatCall::SPEAKER_STATUS_ACTIVE : MegaChatCall::SPEAKER_STATUS_DISABLED)
            << "Peer speak status: " << mOwnSpeakStatus[requesterIdx];

        if (approve)
        {
            ASSERT_EQ(mSessSpeakPerm[moderatorIdx].size(), 1u);
            ASSERT_EQ(mSessSpeakPerm[moderatorIdx].begin()->second, approve)  << "onChatSessionUpdate(CHANGE_TYPE_SPEAK_PERMISSION) not received for peercid: " << clientId;
        }
    };

    auto updateFlags = [this](const unsigned int requesterIdx, const unsigned int moderatorIdx, const bool audio, const bool enable, const MegaChatHandle chatid)
    {
        ASSERT_NO_FATAL_FAILURE({
            waitForAction (1,
                          { &mOwnFlagsChanged[requesterIdx], &mChatCallAudioEnabled[moderatorIdx]},
                          { "mOwnFlagsChanged[requesterIdx]", "mChatCallAudioEnabled[moderatorIdx]"},
                          "Update audio flags",
                          true /* wait for all exit flags*/,
                          true /* reset flags */,
                          maxTimeout,
                          [this, &requesterIdx, &audio, &chatid, &enable]()
                          {
                              ChatRequestTracker crtFlags(megaChatApi[requesterIdx]);
                              if (audio)
                              {
                                  enable
                                      ? megaChatApi[requesterIdx]->enableAudio(chatid, &crtFlags)
                                      : megaChatApi[requesterIdx]->disableAudio(chatid, &crtFlags);
                              }
                              else
                              {
                                  enable
                                      ? megaChatApi[requesterIdx]->enableVideo(chatid, &crtFlags)
                                      : megaChatApi[requesterIdx]->disableVideo(chatid, &crtFlags);
                              }
                              ASSERT_EQ(crtFlags.waitForResult(), MegaChatError::ERROR_OK) << "Failed to update flags: " << crtFlags.getErrorString();
                          });
        });

        std::unique_ptr<MegaChatCall> call(megaChatApi[requesterIdx]->getChatCall(chatid));
        ASSERT_TRUE(call) << "Call could not be retrieved for account: " << requesterIdx << ". Chatid: " << getChatIdStrB64(chatid);
        ASSERT_TRUE(!audio || enable == call->hasLocalAudio()) << "Unexpected value for local audio that is: "
                                                               << (call->hasLocalAudio() ? "enabled" : "disabled")
                                                               << " for account index : " << requesterIdx
                                                               << ". Callid: " << getCallIdStrB64(call->getCallId());

        ASSERT_TRUE(audio || enable == call->hasLocalVideo()) << "Unexpected value for local video that is: "
                                                              << (call->hasLocalVideo() ? "enabled" : "disabled")
                                                              << " for account index : " << requesterIdx
                                                              << ". Callid: " << getCallIdStrB64(call->getCallId());
    };

    // specific test cleanup method that will be executed in MrProper dtor
    std::function<void(MegaChatHandle)> testCleanup = [this, a1, a2, crl = chatroomListener.get(),
                                                       lvlA = &localVideoListenerA, lvlB = &localVideoListenerB]
        (MegaChatHandle chatid) -> void
    {
        EXPECT_NE(chatid, MEGACHAT_INVALID_HANDLE) << "testCleanup: Invalid chatid provided";
        std::unique_ptr<MegaChatCall> call(megaChatApi[a1]->getChatCall(chatid));
        if (call)
        {
            LOG_debug << "A ends call for all participants";
            EXPECT_NE(call->getCallId(), MEGACHAT_INVALID_HANDLE) << "testCleanup: Invalid callid";
            waitForAction (1,
                          { &mCallDestroyed[a1], &mCallDestroyed[a2] },
                          { "&mCallDestroyed[a1]", "&mCallDestroyed[a2]" },
                          "A ends call for all participants",
                          true /* wait for all exit flags*/,
                          true /*reset flags*/,
                          maxTimeout,
                          [this, a1, callid = call->getCallId()]()
                          {
                              ChatRequestTracker crtEndCall(megaChatApi[a1]);
                              megaChatApi[a1]->endChatCall(callid, &crtEndCall);
                              EXPECT_EQ(crtEndCall.waitForResult(), MegaChatError::ERROR_OK)
                                  << "Failed to end call. Error: " << crtEndCall.getErrorString();
                          });

        }
        // else => call doesn't exists anymore for this chat, the main purpose of this method is cleaning up test environment
        //         so in case there's no call, we can assume that it has ended by any other reason

        LOG_debug << "Unregistering chatRoomListeners and localVideoListeners";
        megaChatApi[a1]->closeChatRoom(chatid, crl);
        megaChatApi[a2]->closeChatRoom(chatid, crl);
        megaChatApi[a1]->removeChatLocalVideoListener(chatid, lvlA);
        megaChatApi[a2]->removeChatLocalVideoListener(chatid, lvlB);
    };

    MrProper p (testCleanup, chatid);
    LOG_debug << "[Test.RaiseHandToSpeakCall] Starting test after initialization";

    LOG_debug << "#### Test1: A starts call in a meeting room with speak request option enabled, B answers ####";
    ASSERT_NO_FATAL_FAILURE({ startChatCall(chatid,  a1, std::set<unsigned int> {a2}, /*enableVideo*/ false, /*enableAudio*/ false); });
    ASSERT_NO_FATAL_FAILURE({ answerChatCall(chatid, a2, std::set<unsigned int> {a1}, /*enableVideo*/ false, /*enableAudio*/ false); });
    ASSERT_NO_FATAL_FAILURE({ checkSpeakPermissions(a1, true/*moderator*/, chatid);});
    ASSERT_NO_FATAL_FAILURE({ checkSpeakPermissions(a2, false/*moderator*/, chatid);});

    LOG_debug << "#### Test2: B request to speak, A rejects it ####";
    ASSERT_NO_FATAL_FAILURE({ requestSpeak(a2, a1, false /*approve*/, chatid); });

    LOG_debug << "#### Test3: B requests to speak, A approves it ####";
    ASSERT_NO_FATAL_FAILURE({ requestSpeak(a2, a1, true  /*approve*/, chatid); });

    LOG_debug << "#### Test4: B enables audio (unmute) ####";
    updateFlags(a2, a1, true /*audio*/, true /*enable*/, chatid);
    std::unique_ptr<MegaChatCall> call(megaChatApi[a2]->getChatCall(chatid));
    ASSERT_TRUE(call) << "Cannot get call for account index " << a2 << " with chatid " << getChatIdStrB64(chatid);
    ASSERT_TRUE(call->isSpeakAllowed()) << "Speak permission or audio flag is disabled at account index " << a2
                                        << " for callid " << getCallIdStrB64(call->getCallId());

    LOG_debug << "#### Test5: Remove B as speaker ####";
    ASSERT_NO_FATAL_FAILURE({ removeSpeaker(a1, a2, secondaryUh, chatid); });

    LOG_debug << "\tSwitching back to prod (TEMPORARY)";
    megaApi[a1]->changeApiUrl("https://g.api.mega.co.nz/");
}

/**
 * @brief TEST_EstablishedCallsRingUserIndividually
 *
 * Requirements:
 *      - 3 accounts
 *      - All accounts should be contacts
 * (if not accomplished, the test automatically solves them)
 *
 * This test does the following:
 * Test1: A starts a groupal Meeting in chat1 (without audio nor video)
 * Test2: B answers call (without audio nor video)
 * Test3: C doesn't answer the call and times out
 * <optional - just performing some actions in the call> B, A set audio, and then stop it
 * Test4: A rings C individually
 * Test5: C receives the new ring, doesn't answer, and the call times out again
 * Test6: B hangs up call
 * Test7: A hangs up call
 *
 */
TEST_F(MegaChatApiTest, EstablishedCallsRingUserIndividually)
{
    const unsigned int a1 = 0, a2 = 1, a3 = 2;

    LOG_debug << "# Prepare users, and chat room";
    std::unique_ptr<char[]> primarySession(login(a1));   // user A
    std::unique_ptr<char[]> secondarySession(login(a2)); // user B
    std::unique_ptr<char[]> tertiarySession(login(a3));  // user C
    const auto ensureContact = [this](unsigned int u1, unsigned int u2)
    {
        if (!areContact(u1, u2)) makeContact(u1, u2);
    };
    ensureContact(a1, a2);
    ensureContact(a1, a3);
    ensureContact(a2, a3);

    LOG_debug << "\tGet or create a group chatroom with all users";
    const auto getContactUserHandle = [this](const auto src, const auto target) -> MegaChatHandle
    {
        std::unique_ptr<MegaUser> user(megaApi[src]->getContact(account(target).getEmail().c_str()));
        return user->getHandle();
    };
    const MegaChatHandle uhB = getContactUserHandle(a1, a2);
    const MegaChatHandle uhC = getContactUserHandle(a1, a3);
    std::unique_ptr<MegaChatPeerList> peers(MegaChatPeerList::createInstance());
    peers->addPeer(uhB, MegaChatPeerList::PRIV_STANDARD);
    peers->addPeer(uhC, MegaChatPeerList::PRIV_STANDARD);
    const MegaChatHandle chatId = getGroupChatRoom({a1, a2, a3}, peers.get());
    ASSERT_NE(chatId, MEGACHAT_INVALID_HANDLE) << "Common chat for all users not found.";
    ASSERT_EQ(megaChatApi[a1]->getChatConnectionState(chatId), MegaChatApi::CHAT_CONNECTION_ONLINE)
        << "Not connected to chatd for account " << account(a1).getEmail() << "(" << a1 + 1 << ")";

    auto chatroomListener = std::make_unique<TestChatRoomListener>(this, megaChatApi, chatId);
    const auto openChatRoom = [this, &chatId, l = chatroomListener.get()](const auto idx, const std::string& u)
    { ASSERT_TRUE(megaChatApi[idx]->openChatRoom(chatId, l)) << "Can't open chatRoom user " + u; };
    ASSERT_NO_FATAL_FAILURE(openChatRoom(a1, "A"));
    ASSERT_NO_FATAL_FAILURE(openChatRoom(a2, "B"));
    ASSERT_NO_FATAL_FAILURE(openChatRoom(a3, "C"));
    LOG_debug << "# Chat room for the 3 users created / retrieved";

    const auto lHistory = [this, &chatId, l = chatroomListener.get()](const auto idx) { loadHistory(idx, chatId, l); };
    lHistory(a1);
    lHistory(a2);
    lHistory(a3);
    LOG_debug << "# History loaded for the 3 users";

    LOG_debug << "#### Test1: A starts a groupal Meeting in chat1 (without audio nor video) ####";
    mCallIdJoining[a1] = MEGACHAT_INVALID_HANDLE; mChatIdInProgressCall[a1] = MEGACHAT_INVALID_HANDLE;
    mCallIdRingIn[a2] = MEGACHAT_INVALID_HANDLE;  mChatIdRingInCall[a2] = MEGACHAT_INVALID_HANDLE;
    mCallIdRingIn[a3] = MEGACHAT_INVALID_HANDLE;  mChatIdRingInCall[a3] = MEGACHAT_INVALID_HANDLE;
    mCallReceivedRinging[a3] = false;
    constexpr bool waitForAllExitFlags = true;
    constexpr bool resetFlags = true;
    constexpr bool enableVideo = false;
    constexpr bool enableAudio = false;
    constexpr int maxAttempts = 1;

    std::function<void()> action = [this, &a1, &chatId, &enableVideo, &enableAudio]()
    {
        ChatRequestTracker crtCall(megaChatApi[a1]);
        megaChatApi[a1]->startChatCall(chatId, enableVideo, enableAudio, &crtCall);
        ASSERT_EQ(crtCall.waitForResult(), MegaChatError::ERROR_OK)
            << "Failed to start call. Error: " << crtCall.getErrorString();
    };
    ASSERT_NO_FATAL_FAILURE(waitForAction(maxAttempts,
                                          {&mCallInProgress[a1], &mCallReceivedRinging[a2]},
                                          {"mCallInProgress[a1]", "mCallReceivedRinging[a2]"},
                                          "starting chat call from A", waitForAllExitFlags, resetFlags, maxTimeout, action));

    LOG_debug << "#### Test2: B answers call (without audio nor video) ####";
    mCallIdExpectedReceived[a2] = MEGACHAT_INVALID_HANDLE;
    unique_ptr<MegaChatCall> auxCall(megaChatApi[a1]->getChatCall(mChatIdInProgressCall[a1]));
    if (auxCall) mCallIdExpectedReceived[a2] = auxCall->getCallId();
    ASSERT_EQ(mCallIdExpectedReceived[a2], mCallIdJoining[a1]) << "B expects same call Id as A's";
    ASSERT_NE(mChatIdRingInCall[a2], MEGACHAT_INVALID_HANDLE) << "Invalid ChatId for B from A (call emisor)";
    ASSERT_TRUE((mCallIdRingIn[a2] != MEGACHAT_INVALID_HANDLE) &&
                (mCallIdRingIn[a2] == mCallIdJoining[a1])) << "A and B are in different call";
    LOG_debug << "- B received the call";

    action = [this, &a2, &chatId, &enableVideo, &enableAudio]()
    {
        ChatRequestTracker crtAnswerCall(megaChatApi[a2]);
        megaChatApi[a2]->answerChatCall(chatId, enableVideo, enableAudio, &crtAnswerCall);
        ASSERT_EQ(crtAnswerCall.waitForResult(), MegaChatError::ERROR_OK)
            << "Failed to answer call. Error: " << crtAnswerCall.getErrorString();
    };
    ASSERT_NO_FATAL_FAILURE(waitForAction(maxAttempts,
                                          {&mChatCallSessionStatusInProgress[a1], &mChatCallSessionStatusInProgress[a2]},
                                          {"mChatCallSessionStatusInProgress[a1]", "mChatCallSessionStatusInProgress[a2]"},
                                          "answering chat call from B", waitForAllExitFlags, resetFlags, maxTimeout, action));

    LOG_debug << "#### Test3: C doesn't answer the call and times out ####";
    const auto waitRingingForC = [this, &waitForAllExitFlags, &a1, &a3, &uhC, &auxCall]()
    {
        bool* exitFlag = &mCallReceivedRinging[a3];
        ASSERT_TRUE(waitForMultiResponse({exitFlag}, waitForAllExitFlags)) << "Timeout waiting for C acknowledging the call";
        ASSERT_NE(mChatIdRingInCall[a3], MEGACHAT_INVALID_HANDLE) << "Invalid ChatId for C from A";
        ASSERT_TRUE((mCallIdRingIn[a3] != MEGACHAT_INVALID_HANDLE) &&
                    (mCallIdRingIn[a3] == mCallIdJoining[a1])) << "C and A are in different calls";
        if (auxCall)
        {
            mCallIdExpectedReceived[a3] = auxCall->getCallId();
            ASSERT_NE(auxCall->getCaller(), uhC) << "User C shouldn't be the caller";
        }
        *exitFlag = false;
        mCallStopRinging[a3] = false;
        mCallIdStopRingIn[a3] = MEGACHAT_INVALID_HANDLE;
        mChatIdStopRingInCall[a3] = MEGACHAT_INVALID_HANDLE;
        LOG_debug << "/ C doesn't pick up the call";
    };
    ASSERT_NO_FATAL_FAILURE(waitRingingForC());

    /////////// <optional>
    LOG_debug << "<optional - just performing some actions in the call> B, A set audio, and then stop it";
    const auto enableAudioFor = [this, &chatId](unsigned int performer, unsigned int receiver)
    {
        bool* exitFlag = &mChatCallAudioEnabled[receiver]; *exitFlag = false;
        const auto action = [this, &performer, &chatId](){ megaChatApi[performer]->enableAudio(chatId); };
        const std::string msg {"receiving audio enabled by " + std::to_string(performer)
                               + " at account " + std::to_string(receiver)};
        ASSERT_NO_FATAL_FAILURE(waitForCallAction(performer, MAX_ATTEMPTS, exitFlag, msg.c_str(), maxTimeout, action));
    };
    ASSERT_NO_FATAL_FAILURE(enableAudioFor(a2, a1));

    LOG_debug << "+ A enabling audio in the call";
    ASSERT_NO_FATAL_FAILURE(enableAudioFor(a1, a2));

    LOG_debug << "- B disabling audio in the call";
    const auto disableAudioFor = [this, &chatId](unsigned int p, unsigned int r)
    {
        bool* exitFlag = &mChatCallAudioDisabled[r]; *exitFlag = false;
        const auto action = [this, &p, &chatId](){ megaChatApi[p]->disableAudio(chatId); };
        const std::string msg {"receiving audio disabled by " + std::to_string(p)
                               + " at account " + std::to_string(r)};
        ASSERT_NO_FATAL_FAILURE(waitForCallAction(p, MAX_ATTEMPTS, exitFlag, msg.c_str(), maxTimeout, action));
    };
    ASSERT_NO_FATAL_FAILURE(disableAudioFor(a2, a1));

    LOG_debug << "+ A disabling audio in the call";
    ASSERT_NO_FATAL_FAILURE(disableAudioFor(a1, a2));
    /////////// </optional>

    const auto waitStopRingingForC = [this, &waitForAllExitFlags, &a3]()
    {
        LOG_debug << "# Wait for ringing timeout on C";
        bool* exitFlag = &mCallStopRinging[a3];
        ASSERT_TRUE(waitForMultiResponse({exitFlag}, waitForAllExitFlags))
            << "Timeout for C waiting the ringing to stop";

        const std::string pref {"error on C ringing timeout: "};
        ASSERT_NE(mCallIdStopRingIn[a3], MEGACHAT_INVALID_HANDLE) << pref << "missing call id";
        ASSERT_EQ(mCallIdStopRingIn[a3], mCallIdExpectedReceived[a3]) << pref << "unexpected call id";
        ASSERT_NE(mChatIdStopRingInCall[a3], MEGACHAT_INVALID_HANDLE) << pref << "missing chat id";
        *exitFlag = false;
        LOG_debug << "# C's call stop ringing";
    };
    ASSERT_NO_FATAL_FAILURE(waitStopRingingForC());

    LOG_debug << "#### Test4: A rings C individually ####";
    auto& userId = uhC;
    auto& callId = mCallIdExpectedReceived[a3];
    LOG_debug << "\tchatId " << toHandle(chatId) << " userId " << toHandle(userId) << " callId " << toHandle(callId);
    action = [this, &a1, &chatId, &userId, &callId]()
    {
        const int ringTimeout = 3; // ring timeout set specifically for this test
        ChatRequestTracker crtRingIndividualCall(megaChatApi[a1]);
        megaChatApi[a1]->ringIndividualInACall(chatId, userId, ringTimeout, &crtRingIndividualCall);
        ASSERT_EQ(crtRingIndividualCall.waitForResult(), MegaChatError::ERROR_OK)
            << "Failed to ring individual in a call. Error: " << crtRingIndividualCall.getErrorString();
    };
    ASSERT_NO_FATAL_FAILURE(action());

    LOG_debug << "#### Test5: C receives the new ring, doesn't answer, and the call times out again ####";
    ASSERT_NO_FATAL_FAILURE(waitRingingForC());
    LOG_debug << "C waits for individual ringing timeout";
    ASSERT_NO_FATAL_FAILURE(waitStopRingingForC());

    LOG_debug << "#### Test6: B hangs up the call ####";
    bool* sessionWasDestroyedA = &mChatSessionWasDestroyed[a1]; *sessionWasDestroyedA = false;
    bool* sessionWasDestroyedB = &mChatSessionWasDestroyed[a2]; *sessionWasDestroyedB = false;
    bool* callDestroyedA = &mCallDestroyed[a1]; *callDestroyedA = false;
    bool* callDestroyedB = &mCallDestroyed[a2]; *callDestroyedB = false;
    bool* callDestroyedC = &mCallDestroyed[a3]; *callDestroyedC = false;
    const auto hangUpCall = [this](const auto u, const auto callId)
    {
        bool exitFlag = false;
        const auto action = [this, &u, &callId, &exitFlag]()
        {
            ChatRequestTracker crtHangup(megaChatApi[u]);
            megaChatApi[u]->hangChatCall(callId, &crtHangup);
            auto res = crtHangup.waitForResult();
            exitFlag = true;
            ASSERT_EQ(res, MegaChatError::ERROR_OK)
                << "Failed to hangup call (" << u + 1 << "). Error: " << crtHangup.getErrorString();
        };
        const std::string msg {"hanging up chat call at account " + std::to_string(u)};
        ASSERT_NO_FATAL_FAILURE(waitForCallAction(u, MAX_ATTEMPTS, &exitFlag, msg.c_str(), maxTimeout, action));
        LOG_debug << "# Call finished for account " << u + 1;
    };
    ASSERT_NO_FATAL_FAILURE(hangUpCall(a2, mCallIdRingIn[a2]));

    LOG_debug << "#### Test7: A hangs up the call ####";
    ASSERT_NO_FATAL_FAILURE(hangUpCall(a1, mCallIdJoining[a1]));

    LOG_debug << "# Checking session B and session A destruction"; // no session for C since it didn't join
    const auto checkSessionDestroyed = [this, w = &waitForAllExitFlags](const auto& f, const std::string& msg)
    {
        ASSERT_TRUE(waitForMultiResponse({f}, w)) << "Timeout expired for " << msg << " receiving session destroyed notification";
    };
    ASSERT_NO_FATAL_FAILURE(checkSessionDestroyed(sessionWasDestroyedB, "B"));
    ASSERT_NO_FATAL_FAILURE(checkSessionDestroyed(sessionWasDestroyedA, "A"));

    LOG_debug << "# Checking call destruction for A, B, and C";
    const auto checkCallDestroyed = [this, w = &waitForAllExitFlags](const auto& f, const std::string& msg)
    { ASSERT_TRUE(waitForMultiResponse({f}, w)) << msg; };
    static const std::string err = "'s call should already be finished and it is not";
    ASSERT_NO_FATAL_FAILURE(checkCallDestroyed(callDestroyedA, "A" + err));
    ASSERT_NO_FATAL_FAILURE(checkCallDestroyed(callDestroyedB, "B" + err));
    ASSERT_NO_FATAL_FAILURE(checkCallDestroyed(callDestroyedC, "C" + err + "(it never started)"));

    LOG_debug << "# Closing chat room for each user and removing its localVideoListener";
    const auto closeChatRoom =
        [this, &chatId, l = chatroomListener.get()](const auto u){ megaChatApi[u]->closeChatRoom(chatId, l); };
    closeChatRoom(a1);
    closeChatRoom(a2);
    closeChatRoom(a3);
}

/**
 * @brief MegaChatApiTest.WaitingRooms
 * + Test1: Create chatlink and check data received from API (mcphurl)
 * + Test2: Modify chat, rejoin chatlink and check for changes
 * + Test3: A starts a groupal meeting, B it's (automatically) pushed into waiting room and A grants access to call.
 *          Call won't ring for the rest of participants
 * + Test4: A Pushes B into waiting room, (A ignores it, there's no way to reject a Join req)
 * + Test5: A kicks (completely disconnect) B from call
 * + Test6: A starts call relying on waiting room flag from chatroom.
 * + Test7: B JOINS automatically to call from Waiting Room, when he receives MOD_ADD command from SFU
 * + Test8: A starts call relying on waiting room flag from chatroom.
 */
TEST_F(MegaChatApiTest, WaitingRooms)
{
    const unsigned a1 = 0;
    const unsigned a2 = 1;
    const unsigned a3 = 2;

    // Test preparation. Prepare users, and chat room
    std::unique_ptr<char[]> primarySession(login(a1));   // user A
    ASSERT_TRUE(primarySession);
    std::unique_ptr<char[]> secondarySession(login(a2)); // user B
    ASSERT_TRUE(secondarySession);

    LOG_debug << "\tSwitching to staging (Shard 2) for group creation (TEMPORARY)";
    megaApi[a1]->changeApiUrl("https://staging.api.mega.co.nz/");

    std::unique_ptr<MegaUser> user(megaApi[a1]->getContact(account(a2).getEmail().c_str()));
    if (!user || user->getVisibility() != MegaUser::VISIBILITY_VISIBLE)
    {
        ASSERT_NO_FATAL_FAILURE({ makeContact(a1, a2); });
    }

    // Get a group chatroom with both users
    const MegaChatHandle uh = user->getHandle();
    MegaChatHandle chatid = MEGACHAT_INVALID_HANDLE;

    // Define a SchedMeetingData instance and initialize relevant fields
    SchedMeetingData smDataTests127;
    std::string timeZone = "Europe/Madrid";
    const time_t now = time(nullptr);
    const MegaChatTimeStamp startDate = now + 300;
    const MegaChatTimeStamp endDate =  startDate + 600;
    std::string title = "SMChat_" + std::to_string(now);
    const std::shared_ptr<MegaChatPeerList> peerList(MegaChatPeerList::createInstance());
    // create MegaChatScheduledRules
    std::shared_ptr<MegaChatScheduledRules> rules(MegaChatScheduledRules::createInstance(MegaChatScheduledRules::FREQ_DAILY,
                                                                                         MegaChatScheduledRules::INTERVAL_INVALID,
                                                                                         MEGACHAT_INVALID_TIMESTAMP,
                                                                                         nullptr, nullptr, nullptr));
    peerList->addPeer(user->getHandle(), MegaChatPeerList::PRIV_STANDARD);
    smDataTests127.peerList = peerList;
    smDataTests127.isMeeting = true;
    smDataTests127.publicChat = true;
    smDataTests127.title = title;
    smDataTests127.speakRequest = false;
    smDataTests127.waitingRoom = true;
    smDataTests127.openInvite = false;
    smDataTests127.timeZone = timeZone;
    smDataTests127.startDate = startDate;
    smDataTests127.endDate = endDate;
    smDataTests127.description = ""; // description is not a mandatory field
    smDataTests127.flags = nullptr;  // flags is not a mandatory field
    smDataTests127.rules = rules;

    // Test preconditions: Get a meeting room with a scheduled meeting associated
    // Waiting rooms currently just works if there's a scheduled meeting created for the chatroom
    LOG_debug << "Test preconditions: Get a meeting room with a scheduled meeting associated";
    chatid = getGroupChatRoom({a1, a2}, peerList.get(), megachat::MegaChatPeerList::PRIV_MODERATOR, true /*create*/,
                              true /*publicChat*/, true /*meetingRoom*/, true /*waitingRoom*/, false /*speakRequest*/, &smDataTests127);

    ASSERT_NE(chatid, MEGACHAT_INVALID_HANDLE) << "Can't get/create a Meeting room with waiting room enabled";
    const std::unique_ptr<char[]> chatIdB64(MegaApi::userHandleToBase64(chatid));
    std::unique_ptr<MegaChatRoom> chatRoom(megaChatApi[a1]->getChatRoom(chatid));
    ASSERT_TRUE(chatRoom && chatRoom->isMeeting() && chatRoom->isWaitingRoom()) << "Can't retrieve Meeting room with waiting room enabled. chatid: "
                                                                                << chatIdB64.get();
    // get scheduled meeting for chatroom created
    std::unique_ptr <MegaChatScheduledMeetingList> schedlist(megaChatApi[a1]->getScheduledMeetingsByChat(chatid));
    ASSERT_TRUE(schedlist && schedlist->size() == 1) << "Chat doesn't have scheduled meetings";
    const MegaChatScheduledMeeting* sm = schedlist->at(0);
    ASSERT_TRUE(sm && sm->parentSchedId() == MEGACHAT_INVALID_HANDLE && sm->schedId() != MEGACHAT_INVALID_HANDLE) << "Invalid schedid";
    const MegaChatHandle schedId = sm->schedId();

    ASSERT_EQ(megaChatApi[a1]->getChatConnectionState(chatid), MegaChatApi::CHAT_CONNECTION_ONLINE) <<
        "Not connected to chatd for account " << (a1+1) << ": " << account(a1).getEmail();

    std::shared_ptr<TestChatRoomListener>chatroomListener(new TestChatRoomListener(this, megaChatApi, chatid));
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, chatroomListener.get())) << "Can't open chatRoom user A";

    if (chatRoom->getPeerPrivilegeByHandle(user->getHandle()) == megachat::MegaChatPeerList::PRIV_UNKNOWN
        || chatRoom->getPeerPrivilegeByHandle(user->getHandle()) == megachat::MegaChatPeerList::PRIV_RM)
    {
        ASSERT_NO_FATAL_FAILURE(inviteToChat(a1, a2, uh, chatid, MegaChatPeerList::PRIV_STANDARD, chatroomListener));
    }
    else if (chatRoom->getPeerPrivilegeByHandle(user->getHandle()) != megachat::MegaChatPeerList::PRIV_STANDARD)
    {
        ASSERT_NO_FATAL_FAILURE(updateChatPermission(a1, a2, uh, chatid, megachat::MegaChatPeerList::PRIV_STANDARD, chatroomListener));
    }

    chatRoom.reset(megaChatApi[a1]->getChatRoom(chatid));
    ASSERT_TRUE(chatRoom->getPeerPrivilegeByHandle(user->getHandle()) == megachat::MegaChatPeerList::PRIV_STANDARD)
        << "Can't update Meeting room aux user permission to standard:";

    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(chatid, chatroomListener.get())) <<
        "Can't open chatRoom user B";

    loadHistory(a1, chatid, chatroomListener.get());
    loadHistory(a2, chatid, chatroomListener.get());

    TestChatVideoListener localVideoListenerA;
    megaChatApi[a1]->addChatLocalVideoListener(chatid, &localVideoListenerA);
    TestChatVideoListener localVideoListenerB;
    megaChatApi[a2]->addChatLocalVideoListener(chatid, &localVideoListenerB);
    unique_ptr<MegaChatCall> auxCall;

    auto grantsJoinPermission = [this, a1, a2, chatid, uh]()
    {
        // A grants permission to B for joining call
        mUsersAllowJoin[a1].clear();
        bool* allowJoin = &mUsersAllowJoin[a1][uh]; *allowJoin = false; // important to initialize, otherwise key won't exists on map
        ASSERT_NO_FATAL_FAILURE({
            waitForAction (1,
                          std::vector<bool *> {allowJoin,
                              &mCallWrAllow[a2]
                          },
                          std::vector<string> {
                              "allowJoin",
                              "&mCallWrAllow[a2]"
                          },
                          "grants B Join permission to call from A",
                          true /* wait for all exit flags*/,
                          true /*reset flags*/,
                          maxTimeout,
                          [this, a1, chatid, uh](){
                              ChatRequestTracker crtAllowJoin(megaChatApi[a1]);
                              std::unique_ptr <::mega::MegaHandleList> hl(::mega::MegaHandleList::createInstance());
                              hl->addMegaHandle(uh);
                              megaChatApi[a1]->allowUsersJoinCall(chatid, hl.get(), false /*all*/, &crtAllowJoin);
                              ASSERT_EQ(crtAllowJoin.waitForResult(), MegaChatError::ERROR_OK)
                                  << "Failed to allow join users from WR. Error: " << crtAllowJoin.getErrorString();
                          });
        });
    };

    auto pushIntoWr = [this, a1, a2, chatid, uh]()
    {
        ASSERT_NO_FATAL_FAILURE({
            waitForAction (1,
                          std::vector<bool *> {&mCallWrChanged[a1], &mCallWR[a2]},
                          std::vector<string> {"&mCallWrChanged[a1]", "&mCallWR[a2]"},
                          "grants B Join permission to call from A",
                          true /* wait for all exit flags*/,
                          true /* reset flags*/,
                          maxTimeout,
                          [this, a1, chatid, uh](){
                              ChatRequestTracker crtPushWr(megaChatApi[a1]);
                              std::unique_ptr <::mega::MegaHandleList> hl(::mega::MegaHandleList::createInstance());
                              hl->addMegaHandle(uh);
                              megaChatApi[a1]->pushUsersIntoWaitingRoom(chatid, hl.get(), false /*all*/, &crtPushWr);
                              ASSERT_EQ(crtPushWr.waitForResult(), MegaChatError::ERROR_OK)
                                  << "Failed to push users into WR. Error: " << crtPushWr.getErrorString();
                          });
        });
    };

    auto kickFromCall = [this, a1, a2, chatid, uh]()
    {
        ASSERT_NO_FATAL_FAILURE({
            waitForAction (1,
                          std::vector<bool *> {&mCallLeft[a2]},
                          std::vector<string> {"&mCallDestroyed[a2]"},
                          "grants B Join permission to call from A",
                          true /* wait for all exit flags*/,
                          true /* reset flags*/,
                          maxTimeout,
                          [this, a1, chatid, uh](){
                              ChatRequestTracker crtKickWr(megaChatApi[a1]);
                              std::unique_ptr <::mega::MegaHandleList> hl(::mega::MegaHandleList::createInstance());
                              hl->addMegaHandle(uh);
                              megaChatApi[a1]->kickUsersFromCall(chatid, hl.get(), &crtKickWr);
                              ASSERT_EQ(crtKickWr.waitForResult(), MegaChatError::ERROR_OK)
                                  << "Failed to kick users from call. Error: " << crtKickWr.getErrorString();
                          });
        });
        ASSERT_TRUE(mTerminationCode[a2] == MegaChatCall::TERM_CODE_KICKED) << "Unexpected termcode" << MegaChatCall::termcodeToString(mTerminationCode[a2]);
    };

    auto startWaitingRoomCallPrimaryAccount = [this, &a1, &a2, &chatid](const MegaChatHandle schedIdWr, const bool notRinging){

        mCallIdJoining[a1] = MEGACHAT_INVALID_HANDLE;
        mChatIdInProgressCall[a1] = MEGACHAT_INVALID_HANDLE;
        mCallIdRingIn[a2] = MEGACHAT_INVALID_HANDLE;
        mChatIdRingInCall[a2] = MEGACHAT_INVALID_HANDLE;
        ASSERT_TRUE(schedIdWr == MEGACHAT_INVALID_HANDLE || !notRinging) << "Schedid and notRinging cannot be set in conjunction";

        bool* receivedSecondary = nullptr;
        receivedSecondary = schedIdWr != MEGACHAT_INVALID_HANDLE || notRinging
                                ? &mCallReceived[a2]
                                : &mCallReceivedRinging[a2];

        ASSERT_NO_FATAL_FAILURE({
            waitForAction (1, // just one attempt as mCallReceivedRinging for B account could fail but call could have been created from A account
                          std::vector<bool *> {&mCallInProgress[a1], receivedSecondary},
                          std::vector<string> {"mCallInProgress[a1]", "mCallReceivedRinging[a2]"},
                          "starting chat call from A",
                          true /* wait for all exit flags*/,
                          true /*reset flags*/,
                          maxTimeout,
                          [this, &a1, &chatid, &schedIdWr, &notRinging]()
                          {
                              ChatRequestTracker crtStartCall(megaChatApi[a1]);
                              schedIdWr != MEGACHAT_INVALID_HANDLE
                                  ? megaChatApi[a1]->startMeetingInWaitingRoomChat(chatid, schedIdWr, /*enableVideo*/ false, /*enableAudio*/ false, &crtStartCall) // legacy
                                  : megaChatApi[a1]->startCallInChat(chatid, /*enableVideo*/ false, /*enableAudio*/ false, notRinging, &crtStartCall);

                              ASSERT_EQ(crtStartCall.waitForResult(), MegaChatError::ERROR_OK)
                                  << "Failed to start call. Error: " << crtStartCall.getErrorString();
                          });
        });
    };

    const auto answerCallSecondaryAccount = [this, &a1, &a2, &chatid](const bool redirectToWaitingRoom){

        bool* waitingPrimary = nullptr;
        bool* waitingSecondary = nullptr;

        if (redirectToWaitingRoom) // peers that answers call will be redirected into waiting room
        {
            waitingPrimary = &mCallWrChanged[a1];
            waitingSecondary = &mCallWR[a2];
        }
        else // waiting room will be bypassed by participants that answers the call
        {
            waitingPrimary = &mChatCallSessionStatusInProgress[a1];
            waitingSecondary = &mChatCallSessionStatusInProgress[a2];
        }

        ASSERT_NO_FATAL_FAILURE({
            waitForAction (1, // just one attempt as call could be answered properly at B account but any of the other flags not received
                          std::vector<bool *> { waitingPrimary, waitingSecondary },
                          std::vector<string> { "waitingPrimary", "waitingSecondary" },
                          "answering chat call from B",
                          true /* wait for all exit flags*/,
                          true /*reset flags*/,
                          maxTimeout,
                          [this, a2, chatid]()
                          {
                              ChatRequestTracker crtAnswerCall(megaChatApi[a2]);
                              megaChatApi[a2]->answerChatCall(chatid, /*enableVideo*/ false, /*enableAudio*/ false, &crtAnswerCall);
                              ASSERT_EQ(crtAnswerCall.waitForResult(), MegaChatError::ERROR_OK)
                                  << "Failed to answer call. Error: " << crtAnswerCall.getErrorString();
                          });
        });
    };

    auto endCallPrimaryAccount = [this, &a1, &a2](const MegaChatHandle callId){
        bool* callDestroyedA = &mCallDestroyed[a1]; *callDestroyedA = false;
        bool* callDestroyedB = &mCallDestroyed[a2]; *callDestroyedB = false;
        ASSERT_NO_FATAL_FAILURE({
            waitForAction (1,
                          std::vector<bool *> { &mCallDestroyed[a1], &mCallDestroyed[a2] },
                          std::vector<string> { "&mCallDestroyed[a1]", "&mCallDestroyed[a2]" },
                          "A ends call for all participants",
                          true /* wait for all exit flags*/,
                          true /*reset flags*/,
                          maxTimeout,
                          [this, a1, callDestroyedA, callDestroyedB, callId]()
                          {
                              ChatRequestTracker crtEndCall(megaChatApi[a1]);
                              megaChatApi[a1]->endChatCall(callId, &crtEndCall);
                              ASSERT_EQ(crtEndCall.waitForResult(), MegaChatError::ERROR_OK)
                                  << "Failed to end call. Error: " << crtEndCall.getErrorString();

                              // Check the call was destroyed at both ends
                              LOG_debug << "Now that A and B hung up, we can check if the call is destroyed";
                              ASSERT_TRUE(waitForResponse(callDestroyedA)) <<
                                  "The call for A should be already finished and it is not";
                              LOG_debug << "Destroyed for A is OK, checking for B";
                              ASSERT_TRUE(waitForResponse(callDestroyedB)) <<
                                  "The call for B should be already finished and it is not";
                              LOG_debug << "Destroyed for B is OK.";
                          });
        });
    };

    auto picksUpCallSecondaryAccount = [this, &a1, &a2](const bool isRingingExpected) -> unique_ptr<MegaChatCall>
    {
        mCallIdExpectedReceived[a2] = MEGACHAT_INVALID_HANDLE;
        unique_ptr<MegaChatCall> auxCall(megaChatApi[a1]->getChatCall(mChatIdInProgressCall[a1]));
        if (!auxCall)
        {
            return nullptr;
        }

        mCallIdExpectedReceived[a2] = auxCall->getCallId();
        if (isRingingExpected)
        {
            EXPECT_TRUE((mCallIdJoining[a1] == mCallIdRingIn[a2]) && (mCallIdRingIn[a2] != MEGACHAT_INVALID_HANDLE)) << "A and B are in different call";
            EXPECT_NE(mChatIdRingInCall[a2], MEGACHAT_INVALID_HANDLE) << "Invalid Chatid for B from A (call emisor)";
        }
        LOG_debug << "B received the call";
        return auxCall;
    };

    std::function<void(MegaChatHandle)> testCleanup = [this, a1, a2, crl = chatroomListener.get(),
                                                       lvlA = &localVideoListenerA, lvlB = &localVideoListenerB]
        (MegaChatHandle chatid) -> void
    {
        EXPECT_NE(chatid, MEGACHAT_INVALID_HANDLE) << "testCleanup: Invalid chatid provided";
        std::unique_ptr<MegaChatCall> call(megaChatApi[a1]->getChatCall(chatid));
        if (call)
        {
            LOG_debug << "testCleanup: A ends call for all participants";
            EXPECT_NE(call->getCallId(), MEGACHAT_INVALID_HANDLE) << "testCleanup: Invalid callid";
            waitForAction (1,
                          std::vector<bool *> { &mCallDestroyed[a1], &mCallDestroyed[a2] },
                          std::vector<string> { "&mCallDestroyed[a1]", "&mCallDestroyed[a2]" },
                          "A ends call for all participants",
                          true /* wait for all exit flags*/,
                          true /*reset flags*/,
                          maxTimeout,
                          [this, a1, callid = call->getCallId()]()
                          {
                              ChatRequestTracker crtEndCall(megaChatApi[a1]);
                              megaChatApi[a1]->endChatCall(callid, &crtEndCall);
                              EXPECT_EQ(crtEndCall.waitForResult(), MegaChatError::ERROR_OK)
                                  << "Failed to end call. Error: " << crtEndCall.getErrorString();
                          });
        }
        // else => call doesn't exists anymore for this chat, the main purpose of this method is cleaning up test environment
        //         so in case there's no call, we can assume that it has ended by any other reason


        LOG_debug << "Unregistering chatRoomListeners and localVideoListeners";
        megaChatApi[a1]->closeChatRoom(chatid, crl);
        megaChatApi[a2]->closeChatRoom(chatid, crl);
        megaChatApi[a1]->removeChatLocalVideoListener(chatid, lvlA);
        megaChatApi[a2]->removeChatLocalVideoListener(chatid, lvlB);
    };

    auto kickAndEndCall = [kickFromCall, endCallPrimaryAccount](const MegaChatHandle callId)
    {
        kickFromCall();
        endCallPrimaryAccount(callId);
    };

    auto pickupAndAnswerSecondary = [&auxCall, picksUpCallSecondaryAccount, answerCallSecondaryAccount](const bool isRingingExpected, const bool redirectToWaitingRoom)
    {
        auxCall = picksUpCallSecondaryAccount(isRingingExpected);
        LOG_debug << "B received the call";

        // B answers the call
        LOG_debug << "B Answers the call";
        ASSERT_NO_FATAL_FAILURE({answerCallSecondaryAccount(redirectToWaitingRoom);});
    };

    // when this object goes out of scope testCleanup will be executed ending any call in this chat and freeing any resource associated to it
    MrProper p (testCleanup, chatid);

    LOG_debug << "#### Test1: Create chatlink and check data received from API (mcphurl) ####";
    if (!chatRoom->isOpenInvite())
    {
        ChatRequestTracker crtChatOpt1(megaChatApi[a1]);
        megaChatApi[a1]->setOpenInvite(chatid, true, &crtChatOpt1);
        ASSERT_EQ(crtChatOpt1.waitForResult(), MegaChatError::ERROR_OK) << "Failed to enable open invite. Error: " << crtChatOpt1.getErrorString();
    }

    // Create chat link
    ChatRequestTracker crtCreateLink(megaChatApi[a1]);
    megaChatApi[a1]->createChatLink(chatid, &crtCreateLink);
    ASSERT_EQ(crtCreateLink.waitForResult(), MegaChatError::ERROR_OK) << "Creating chat link failed. Should have succeeded!";

    // Init anonymous in terciary account and connect
    initState[a3] = megaChatApi[a3]->initAnonymous();
    ASSERT_EQ(initState[a3], MegaChatApi::INIT_ANONYMOUS) << "Init sesion in anonymous mode for terciary account failed";
    std::unique_ptr<char[]>tertiarySession(megaApi[a3]->dumpSession());

    // Open chat link and check that wr flag and scheduled meetings have been received upon onRequestFinish(TYPE_LOAD_PREVIEW)
    ChatRequestTracker crtOpenLink(megaChatApi[a3]);
    bool *previewsUpdated = &chatroomListener->previewsUpdated[a1]; *previewsUpdated = false;
    megaChatApi[a3]->openChatPreview(crtCreateLink.getText().c_str(), &crtOpenLink);
    ASSERT_EQ(crtOpenLink.waitForResult(), MegaChatError::ERROR_OK) << "Opening chat link failed. Should have succeeded!";
    ASSERT_TRUE(waitForResponse(previewsUpdated)) << "Timeout expired for open preview";
    // check chatroom options upon onRequestFinish (TYPE_LOAD_PREVIEW)
    const int chatOptions = crtOpenLink.getPrivilege();
    ASSERT_TRUE(crtOpenLink.hasScheduledMeetings()) << "Chatroom doesn't have scheduled meeting";
    ASSERT_TRUE(MegaChatApi::hasChatOptionEnabled(MegaChatApi::CHAT_OPTION_WAITING_ROOM, chatOptions))  << "Waiting room is disabled";
    //
    //ASSERT_TRUE(MegaChatApi::hasChatOptionEnabled(MegaChatApi::CHAT_OPTION_SPEAK_REQUEST, chatOptions)) << "Speak request is disabled";
    ASSERT_TRUE(MegaChatApi::hasChatOptionEnabled(MegaChatApi::CHAT_OPTION_OPEN_INVITE, chatOptions))   << "Open invite is disabled";

    // get scheduled meeting list from chatroom
    std::unique_ptr<MegaChatScheduledMeetingList> smlist(megaChatApi[a3]->getScheduledMeetingsByChat(chatid));
    ASSERT_TRUE(smlist && smlist->size()) << "Chatroom doesn't have scheduled meetings";

    LOG_debug << "#### Test2: Modify chat, rejoin chatlink and check for changes ####";

    // Modify chatroom disabling chatoptions
    ChatRequestTracker crtChatOpt2(megaChatApi[a1]);
    megaChatApi[a1]->setOpenInvite(chatid, false, &crtChatOpt2);
    ASSERT_EQ(crtChatOpt2.waitForResult(), MegaChatError::ERROR_OK) << "Failed to disable open invite. Error: " << crtChatOpt2.getErrorString();

    // Modify chatroom changing title
    title.append("_MOD");
    changeTitle(a1, chatroomListener.get(), chatid, title);

    // Modify chatroom sched meeting
    const MegaChatScheduledMeeting *smaux = smlist->at(0)->copy();
    ASSERT_TRUE(smaux) << "Invalid scheduled meeting";
    SchedMeetingData d;
    d.chatId = smaux->chatId();
    d.schedId = smaux->schedId();
    d.timeZone = smaux->timezone();
    d.startDate = smaux->startDateTime() + 60;
    d.endDate = smaux->endDateTime() + 60;
    d.title = smaux->title();
    d.description = smaux->title();
    d.cancelled = smaux->cancelled();
    d.flags.reset(smaux->flags()->copy());
    d.rules.reset(smaux->rules()->copy());
    ASSERT_NO_FATAL_FAILURE({ updateSchedMeeting(a1, a2, MegaChatError::ERROR_OK, d, false /*updateChatTitle*/); });

    // Open chat link again and check that onchatlistIemUpdate with change CHANGE_TYPE_PREVIEW_CLOSED is received
    bool *chatPreviewClosedChanged = &chatPreviewClosed[a3]; *chatPreviewClosedChanged = false;
    ChatRequestTracker crtOpenLink2(megaChatApi[a3]);
    *previewsUpdated = false;
    megaChatApi[a3]->openChatPreview(crtCreateLink.getText().c_str(), &crtOpenLink2);
    ASSERT_EQ(crtOpenLink2.waitForResult(), MegaChatError::ERROR_OK) << "Opening chat link failed. Should have succeeded!";
    ASSERT_TRUE(waitForResponse(chatPreviewClosedChanged)) << "Timeout expired for automatically preview close";

    // check that modified options above have been properly updated on chatroom
    std::unique_ptr<MegaChatRoom> room(megaChatApi[a3]->getChatRoom(d.chatId));
    ASSERT_TRUE(room) << "Cannot get chatroom for id " << getChatIdStrB64(d.chatId);
    ASSERT_TRUE(!room->isOpenInvite()) << "Open invite option is not disabled, chatid: "<< getChatIdStrB64(d.chatId);
    ASSERT_TRUE(!title.compare(room->getTitle())) << "Title doesn't matych with expected one, chatid: "<< getChatIdStrB64(d.chatId);

    std::unique_ptr<MegaChatScheduledMeetingList> auxsmlist(megaChatApi[a3]->getScheduledMeetingsByChat(d.chatId));
    ASSERT_TRUE(auxsmlist && auxsmlist->size()) << "";
    for (unsigned long i = 0; i < auxsmlist->size(); ++i)
    {
        const MegaChatScheduledMeeting* sm = auxsmlist->at(i);
        if (sm->schedId() != d.schedId) { continue; }
        ASSERT_TRUE(sm->startDateTime() == d.startDate && sm->endDateTime() == d.endDate);
    }

    // Close preview
    *previewsUpdated = false;
    megaChatApi[a3]->closeChatPreview(chatid);
    ASSERT_TRUE(waitForResponse(previewsUpdated)) << "Timeout expired for close preview";

    // logout from terciary account
    ASSERT_NO_FATAL_FAILURE({ logout(a3); });

    chatRoom.reset(megaChatApi[a1]->getChatRoom(chatid));
    ASSERT_TRUE(chatRoom) << "Cannot get chatroom for id " << getChatIdStrB64(chatid);

    if (chatRoom->isSpeakRequest())
    {
        // disable speak request again
        ChatRequestTracker crtChatOpt(megaChatApi[a1]);
        megaChatApi[a1]->setSpeakRequest(chatid, false, &crtChatOpt);
        ASSERT_EQ(crtChatOpt.waitForResult(), MegaChatError::ERROR_OK) << "Failed to disable speak request. Error: " << crtChatOpt.getErrorString();
    }

    LOG_debug << "#### Test3: A starts a groupal meeting, B it's (automatically) pushed into waiting room and A grants access to call ####";
    // Call won't ring for the rest of participants as schedId is provided
    ASSERT_NO_FATAL_FAILURE({startWaitingRoomCallPrimaryAccount(schedId, false /*notRinging*/);});
    auxCall.reset(megaChatApi[a1]->getChatCall(chatid));

    // B picks up the call
    LOG_debug << "B Pickups the call (should not ring)";
    auxCall = picksUpCallSecondaryAccount(false /*isRingingExpected*/);

    // B answers call and it's pushed into waiting room
    LOG_debug << "B Answers the call";
    ASSERT_NO_FATAL_FAILURE({answerCallSecondaryAccount(true /*waitingRoom*/);});

    std::unique_ptr<MegaChatCall> call(megaChatApi[a1]->getChatCall(chatid));
    std::unique_ptr<MegaChatWaitingRoom> wr(call && call->getWaitingRoom()
                                                ? call->getWaitingRoom()->copy()
                                                : nullptr);

    ASSERT_TRUE(wr && wr->getUserStatus(uh) == MegaChatWaitingRoom::MWR_NOT_ALLOWED)
        << (!wr ? "Waiting room can't be retrieved for user A" : "B it's not in the waiting room");

    // ** note: can't simulate use case where a2 sends JOIN without any moderator has allowed to enter the call (WR_DENY would be received for a2 from SFU),
    // because JOIN command is automatically managed by karere, and is only sent when user has permission to JOIN
    grantsJoinPermission();

    LOG_debug << " #### Test4: A Pushes B into waiting room, (A ignores it, there's no way to reject a Join req) ####";
    pushIntoWr();

    // ** note: can't simulate use case where a1 sends WR_PUSH for a2, and a2 is still in waiting room, but has already received WR_ALLOW.
    // In that case SFU would send WR_USERS_DENY to all moderators, however this is a race condition, as upon WR_ALLOW, karere automatically
    // sends JOIN command

    LOG_debug << "#### Test5: A kicks (completely disconnect) B from call and then ends call for all participants ####";
    kickAndEndCall(auxCall->getCallId());

    LOG_debug << "#### Test6: A starts call Bypassing waiting room, B Joins directly to the call (Addhoc call) ####";
    mCallIdExpectedReceived[a1] = mCallIdExpectedReceived[a2] = MEGACHAT_INVALID_HANDLE;
    ASSERT_NO_FATAL_FAILURE({startWaitingRoomCallPrimaryAccount(MEGACHAT_INVALID_HANDLE /*schedId*/, false /*notRinging*/);});

    // B answers the call bypassing waiting room
    LOG_debug << "B Pickups the call and wait for ringing";
    pickupAndAnswerSecondary(true /*isRingingExpected*/, false /*redirectToWaitingRoom*/);

//    ASSERT_NO_FATAL_FAILURE({answerCallSecondaryAccount(false /*waitingRoom*/);});
    endCallPrimaryAccount(mCallIdJoining[a1]);

    LOG_debug << "#### Test7: B JOINS automatically to call from Waiting Room, when he receives MOD_ADD command from SFU ####";
    // A starts call
    bool* a2CallProgress = &mCallInProgress[a2]; *a2CallProgress = false;
    bool* a2CallPermChanged = &mOwnCallPermissionsChanged[a2]; *a2CallPermChanged = false;
    ASSERT_NO_FATAL_FAILURE({startWaitingRoomCallPrimaryAccount(schedId, false/*notRinging*/);});

    // B picks up the call
    LOG_debug << "B Pickups the call (should not ring)";
    auxCall = picksUpCallSecondaryAccount(false /*isRingingExpected*/);

    // B answers call and it's pushed into waiting room
    LOG_debug << "B Answers the call";
    ASSERT_NO_FATAL_FAILURE({answerCallSecondaryAccount(true /*waitingRoom*/);});

    // A grants chat room host permissions to B
    int* priv = &chatroomListener->priv[a1]; *priv = MegaChatRoom::PRIV_UNKNOWN;
    bool* peerUpdated0 = &peersUpdated[a1]; *peerUpdated0 = false;
    bool* peerUpdated1 = &peersUpdated[a2]; *peerUpdated1 = false;
    ChatRequestTracker crtUpdateToHost(megaChatApi[a1]);
    megaChatApi[a1]->updateChatPermissions(chatid, uh, MegaChatRoom::PRIV_MODERATOR, &crtUpdateToHost);
    ASSERT_EQ(crtUpdateToHost.waitForResult(), MegaChatError::ERROR_OK) << "Failed to update privilege of peer. Error: " << crtUpdateToHost.getErrorString();
    ASSERT_TRUE(waitForResponse(peerUpdated0)) << "Timeout expired for receiving peer update";
    ASSERT_TRUE(waitForResponse(peerUpdated1)) << "Timeout expired for receiving peer update";
    ASSERT_EQ(*priv, MegaChatRoom::PRIV_MODERATOR) << "Privilege is incorrect";

    // B waits to receive MOD_ADD and autojoins call automatically
    ASSERT_TRUE(waitForResponse(a2CallPermChanged)) << "Timeout expired for receiving MOD_ADD command from SFU";
    ASSERT_TRUE(waitForResponse(a2CallProgress)) << "Timeout expired for JOINING call from a2, after being promoted to host";
    endCallPrimaryAccount(mCallIdJoining[a1]);

    // Test preconditions: Callee user must be non-host, otherwise it won't be redirected to waiting room by SFU
    LOG_debug << "#### Test8: A starts call with waiting room, B is redirected to waiting room ####";
    // Call won't ring for the rest of participants as notRinging is true,
    // B will be redirected to waiting room when he answers the call
    chatRoom.reset(megaChatApi[a1]->getChatRoom(chatid));
    ASSERT_TRUE(chatRoom) << "Cannot get chatroom for id " << getChatIdStrB64(chatid);
    if (chatRoom->getPeerPrivilegeByHandle(uh) != megachat::MegaChatPeerList::PRIV_STANDARD)
    {
        ASSERT_NO_FATAL_FAILURE(updateChatPermission(a1, a2, uh, chatid, megachat::MegaChatPeerList::PRIV_STANDARD, chatroomListener));
    }
    mChatIdInProgressCall[a1] = MEGACHAT_INVALID_HANDLE;
    ASSERT_NO_FATAL_FAILURE({startWaitingRoomCallPrimaryAccount(MEGACHAT_INVALID_HANDLE /*schedId*/, true /*notRinging*/);});

    LOG_debug << "B Pickups the call";
    pickupAndAnswerSecondary(false /*isRingingExpected*/, true /*redirectToWaitingRoom*/);
    auxCall.reset(megaChatApi[a1]->getChatCall(mChatIdInProgressCall[a1]));
    ASSERT_TRUE(auxCall) << "Cannot get call for chatid: " << getChatIdStrB64(mChatIdInProgressCall[a1]);
    endCallPrimaryAccount(auxCall->getCallId());

    LOG_debug << "\tSwitching back from staging (Shard 2) for group creation (TEMPORARY)";
    megaApi[a1]->changeApiUrl("https://g.api.mega.co.nz/");
}

/**
 * @brief MegaChatApiTest.EditMessageFromDifferentSender
 *
 * Requirements:
 *      - Both accounts should be conctacts
 *      - The 1on1 chatroom between them should exist
 * (if not accomplished, the test automatically solves them)
 *
 * This test does the following:
 *
 * Test1: Send a message to chatroom
 * Test2: Receive message
 * Test3: Try to edit a message by a different user than composer
 *
 */
TEST_F(MegaChatApiTest, EditMessageFromDifferentSender)
{
    const unsigned a1 = 0;
    const unsigned a2 = 1;

    LOG_debug << "# Prepare users and chat room";
    std::unique_ptr<char[]> sessionPrimary {login(a1)};
    ASSERT_TRUE(sessionPrimary.get()) << "User A login failed";
    std::unique_ptr<char[]> sessionSecondary {login(a2)};
    ASSERT_TRUE(sessionSecondary.get()) << "User B login failed";

    if (!areContact(a1, a2)) makeContact(a1, a2);

    LOG_debug << "\tGet or create a peer to peer chatroom with both users";
    MegaChatHandle chatId = getPeerToPeerChatRoom(a1, a2);
    ASSERT_NE(chatId, MEGACHAT_INVALID_HANDLE) << "Failed to get peer to peer chat room";

    auto chatroomListener = std::make_unique<TestChatRoomListener>(this, megaChatApi, chatId);
    const auto openChatRoom = [this, &chatId, l = chatroomListener.get()](const auto idx, const std::string& u)
    { ASSERT_TRUE(megaChatApi[idx]->openChatRoom(chatId, l)) << "Can't open chatRoom user " + u; };
    ASSERT_NO_FATAL_FAILURE(openChatRoom(a1, "A"));
    ASSERT_NO_FATAL_FAILURE(openChatRoom(a2, "B"));


    MrProper p {[this, &a1, &a2, crl = chatroomListener.get()](MegaChatHandle pChatId)
                {
                    LOG_debug << "# Clearing history and closing chat room for each user";
                    EXPECT_NO_FATAL_FAILURE(clearHistory(a1, a2, pChatId, crl););
                    megaChatApi[a1]->closeChatRoom(pChatId, crl);
                    megaChatApi[a2]->closeChatRoom(pChatId, crl);
                }, chatId};

    LOG_debug << "\tLoad some messages to feed history";
    const auto lHistory = [this, &chatId, l = chatroomListener.get()](const auto idx) { loadHistory(idx, chatId, l); };
    lHistory(a1);
    lHistory(a2);

    LOG_debug << "#### Test1: Send a message to chatroom ####";
    const std::string formatDate = dateToString();
    std::unique_ptr<MegaChatMessage> msgSent {sendTextMessageOrUpdate(a1, a2, chatId, formatDate, chatroomListener.get())};
    ASSERT_TRUE(msgSent.get()) << "Initial message sent failed";
    const MegaChatHandle msgId = msgSent->getMsgId();

    LOG_debug << "#### Test2: Receive message ####";
    const bool hasArrived = chatroomListener->hasArrivedMessage(a1, msgId);
    ASSERT_TRUE(hasArrived) << "Id of sent message has not been received yet";

    LOG_debug << "#### Test3: Try to edit a message by a different user than composer ####";
    const std::string str {"diff"};
    std::unique_ptr<MegaChatMessage>msg2 {megaChatApi[a2]->editMessage(chatId, msgId, str.c_str())};
    ASSERT_FALSE(msg2.get()) << "Message update should have failed";

    LOG_debug << "# Comparing message content";
    std::unique_ptr<MegaChatListItem> itemAccount1 {megaChatApi[a1]->getChatListItem(chatId)};
    std::unique_ptr<MegaChatListItem> itemAccount2 {megaChatApi[a2]->getChatListItem(chatId)};
    ASSERT_STREQ(formatDate.c_str(), itemAccount1->getLastMessage()) <<
                     "Content of last-message doesn't match.\n Sent vs Received.";
    ASSERT_EQ(itemAccount1->getLastMessageId(), msgId) << "Last message id is different from message sent id";
    ASSERT_EQ(itemAccount2->getLastMessageId(), msgId) << "Last message id is different from message received id";
    std::unique_ptr<MegaChatMessage> messageConfirm {megaChatApi[a1]->getMessage(chatId, msgId)};
    ASSERT_STREQ(messageConfirm->getContent(), itemAccount1->getLastMessage()) <<
                     "Content of last-message reported id is different than last-message reported content";

}

/**
 * @brief MegaChatApiTest.WaitingRoomsTimeout
 * + Test1: A starts a groupal meeting, B it's (automatically) pushed into waiting room and A grants access to call.
 *          Call won't ring for the rest of participants
 * + Test2: A Pushes B into waiting room, (A ignores it, there's no way to reject a Join req)
 * + Test3: B waits into waiting room until SFU timeout expires and BYE command is received with termcode: TERM_CODE_WR_TIMEOUT
 */
TEST_F(MegaChatApiTest, DISABLED_WaitingRoomsTimeout)
{
    const unsigned a1 = 0;
    const unsigned a2 = 1;

    // Test preparation. Prepare users, and chat room
    std::unique_ptr<char[]> primarySession(login(a1));   // user A
    ASSERT_TRUE(primarySession);
    std::unique_ptr<char[]> secondarySession(login(a2)); // user B
    ASSERT_TRUE(secondarySession);

    if (!areContact(a1, a2))
    {
        ASSERT_NO_FATAL_FAILURE({ makeContact(a1, a2); });
    }

    std::unique_ptr<MegaUser> user(megaApi[a1]->getContact(account(a2).getEmail().c_str()));
    ASSERT_TRUE(user) << "Cannot get contact for secondary account";
    const MegaChatHandle uh = user->getHandle();
    // Define a SchedMeetingData instance and initialize relevant fields
    SchedMeetingData smDataTest;
    std::string timeZone = "Europe/Madrid";
    const time_t now = time(nullptr);
    const MegaChatTimeStamp startDate = now + 300;
    const MegaChatTimeStamp endDate =  startDate + 600;
    std::string title = "SMChat_" + std::to_string(now);
    const std::shared_ptr<MegaChatPeerList> peerList(MegaChatPeerList::createInstance());
    // create MegaChatScheduledRules
    std::shared_ptr<MegaChatScheduledRules> rules(MegaChatScheduledRules::createInstance(MegaChatScheduledRules::FREQ_DAILY,
                                                                                         MegaChatScheduledRules::INTERVAL_INVALID,
                                                                                         MEGACHAT_INVALID_TIMESTAMP,
                                                                                         nullptr, nullptr, nullptr));
    peerList->addPeer(user->getHandle(), MegaChatPeerList::PRIV_STANDARD);
    smDataTest.peerList = peerList;
    smDataTest.isMeeting = true;
    smDataTest.publicChat = true;
    smDataTest.title = title;
    smDataTest.speakRequest = false;
    smDataTest.waitingRoom = true;
    smDataTest.openInvite = false;
    smDataTest.timeZone = timeZone;
    smDataTest.startDate = startDate;
    smDataTest.endDate = endDate;
    smDataTest.description = ""; // description is not a mandatory field
    smDataTest.flags = nullptr;  // flags is not a mandatory field
    smDataTest.rules = rules;

    // Test preconditions: Get a meeting room with a scheduled meeting associated
    // Waiting rooms currently just works if there's a scheduled meeting created for the chatroom
    LOG_debug << "Test preconditions: Get a meeting room with a scheduled meeting associated";
    const MegaChatHandle chatid = getGroupChatRoom({a1, a2}, peerList.get(), megachat::MegaChatPeerList::PRIV_MODERATOR, true /*create*/,
                              true /*publicChat*/, true /*meetingRoom*/, true /*waitingRoom*/, false /*speakRequest*/, &smDataTest);

    ASSERT_NE(chatid, MEGACHAT_INVALID_HANDLE) << "Can't get/create a Meeting room with waiting room enabled";
    const std::unique_ptr<char[]> chatIdB64(MegaApi::userHandleToBase64(chatid));
    std::unique_ptr<MegaChatRoom> chatRoom(megaChatApi[a1]->getChatRoom(chatid));
    ASSERT_TRUE(chatRoom && chatRoom->isMeeting() && chatRoom->isWaitingRoom()) << "Can't retrieve Meeting room with waiting room enabled. chatid: "
                                                                                << chatIdB64.get();
    // get scheduled meeting for chatroom created
    std::unique_ptr <MegaChatScheduledMeetingList> schedlist(megaChatApi[a1]->getScheduledMeetingsByChat(chatid));
    ASSERT_TRUE(schedlist && schedlist->size() == 1) << "Chat doesn't have scheduled meetings";
    const MegaChatScheduledMeeting* sm = schedlist->at(0);
    ASSERT_TRUE(sm && sm->parentSchedId() == MEGACHAT_INVALID_HANDLE && sm->schedId() != MEGACHAT_INVALID_HANDLE) << "Invalid scheduled meeting";
    const MegaChatHandle schedId = sm->schedId();

    ASSERT_EQ(megaChatApi[a1]->getChatConnectionState(chatid), MegaChatApi::CHAT_CONNECTION_ONLINE) <<
        "Not connected to chatd for account " << (a1+1) << ": " << account(a1).getEmail();

    std::shared_ptr<TestChatRoomListener>chatroomListener(new TestChatRoomListener(this, megaChatApi, chatid));
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, chatroomListener.get())) << "Can't open chatRoom user A";

    if (chatRoom->getPeerPrivilegeByHandle(user->getHandle()) == megachat::MegaChatPeerList::PRIV_UNKNOWN
        || chatRoom->getPeerPrivilegeByHandle(user->getHandle()) == megachat::MegaChatPeerList::PRIV_RM)
    {
        ASSERT_NO_FATAL_FAILURE(inviteToChat(a1, a2, uh, chatid, MegaChatPeerList::PRIV_STANDARD, chatroomListener));
    }
    else if (chatRoom->getPeerPrivilegeByHandle(user->getHandle()) != megachat::MegaChatPeerList::PRIV_STANDARD)
    {
        ASSERT_NO_FATAL_FAILURE(updateChatPermission(a1, a2, uh, chatid, megachat::MegaChatPeerList::PRIV_STANDARD, chatroomListener));
    }

    // Create chat link
    ChatRequestTracker crtCreateLink(megaChatApi[a1]);
    megaChatApi[a1]->createChatLink(chatid, &crtCreateLink);
    ASSERT_EQ(crtCreateLink.waitForResult(), MegaChatError::ERROR_OK) << "Creating chat link failed. Should have succeeded!";
    ASSERT_TRUE(chatRoom->getPeerPrivilegeByHandle(user->getHandle()) == megachat::MegaChatPeerList::PRIV_STANDARD)
        << "Can't update Meeting room aux user permission to standard:";

    ASSERT_TRUE(megaChatApi[a2]->openChatRoom(chatid, chatroomListener.get())) <<
        "Can't open chatRoom user B";

    loadHistory(a1, chatid, chatroomListener.get());
    loadHistory(a2, chatid, chatroomListener.get());

    TestChatVideoListener localVideoListenerA;
    megaChatApi[a1]->addChatLocalVideoListener(chatid, &localVideoListenerA);
    TestChatVideoListener localVideoListenerB;
    megaChatApi[a2]->addChatLocalVideoListener(chatid, &localVideoListenerB);

    auto grantsJoinPermission = [this, &a1, &a2, &chatid, &uh]()
    {
        // A grants permission to B for joining call
        mUsersAllowJoin[a1].clear();
        bool* allowJoin = &mUsersAllowJoin[a1][uh]; *allowJoin = false; // important to initialize, otherwise key won't exists on map
        ASSERT_NO_FATAL_FAILURE({
            waitForAction (1,
                          { allowJoin, &mCallWrAllow[a2] },
                          { "allowJoin", "&mCallWrAllow[a2]" },
                          "grants B Join permission to call from A",
                          true /* wait for all exit flags*/,
                          true /*reset flags*/,
                          maxTimeout,
                          [this, &a1, &chatid, &uh](){
                              ChatRequestTracker crtAllowJoin(megaChatApi[a1]);
                              std::unique_ptr <::mega::MegaHandleList> hl(::mega::MegaHandleList::createInstance());
                              hl->addMegaHandle(uh);
                              megaChatApi[a1]->allowUsersJoinCall(chatid, hl.get(), false /*all*/, &crtAllowJoin);
                              ASSERT_EQ(crtAllowJoin.waitForResult(), MegaChatError::ERROR_OK)
                                  << "Failed to allow join users from WR. Error: " << crtAllowJoin.getErrorString();
                          });
        });
    };

    auto pushIntoWr = [this, &a1, &a2, &chatid, &uh]()
    {
        ASSERT_NO_FATAL_FAILURE({
            waitForAction (1,
                          { &mCallWrChanged[a1], &mCallWR[a2] },
                          { "&mCallWrChanged[a1]", "&mCallWR[a2]" },
                          "grants B Join permission to call from A",
                          true /* wait for all exit flags*/,
                          true /* reset flags*/,
                          maxTimeout,
                          [this, &a1, &chatid, &uh](){
                              ChatRequestTracker crtPushWr(megaChatApi[a1]);
                              std::unique_ptr <::mega::MegaHandleList> hl(::mega::MegaHandleList::createInstance());
                              hl->addMegaHandle(uh);
                              megaChatApi[a1]->pushUsersIntoWaitingRoom(chatid, hl.get(), false /*all*/, &crtPushWr);
                              ASSERT_EQ(crtPushWr.waitForResult(), MegaChatError::ERROR_OK)
                                  << "Failed to push users into WR. Error: " << crtPushWr.getErrorString();
                          });
        });
    };

    auto startWaitingRoomCallPrimaryAccount = [this, &a1, &a2, &chatid](const MegaChatHandle schedIdWr = MEGACHAT_INVALID_HANDLE){

        mCallIdJoining[a1] = MEGACHAT_INVALID_HANDLE;
        mChatIdInProgressCall[a1] = MEGACHAT_INVALID_HANDLE;
        mCallIdRingIn[a2] = MEGACHAT_INVALID_HANDLE;
        mChatIdRingInCall[a2] = MEGACHAT_INVALID_HANDLE;

        bool* receivedSecondary = schedIdWr != MEGACHAT_INVALID_HANDLE
                                      ? &mCallReceived[a2]
                                      : &mCallReceivedRinging[a2];

        ASSERT_NO_FATAL_FAILURE({
            waitForAction (1, // just one attempt as mCallReceivedRinging for B account could fail but call could have been created from A account
                          { &mCallInProgress[a1], receivedSecondary },
                          { "mCallInProgress[a1]", "mCallReceivedRinging[a2]" },
                          "starting chat call from A",
                          true /* wait for all exit flags*/,
                          true /*reset flags*/,
                          maxTimeout,
                          [this, &a1, &chatid, &schedIdWr]()
                          {
                              ChatRequestTracker crtStartCall(megaChatApi[a1]);
                              megaChatApi[a1]->startMeetingInWaitingRoomChat(chatid, schedIdWr, /*enableVideo*/ false, /*enableAudio*/ false, &crtStartCall);
                              ASSERT_EQ(crtStartCall.waitForResult(), MegaChatError::ERROR_OK)
                                  << "Failed to start call. Error: " << crtStartCall.getErrorString();
                          });
        });
    };

    const auto answerCallSecondaryAccount = [this, &a1, &a2, &chatid](const bool waitingRoom){

        bool* waitingPrimary = nullptr;
        bool* waitingSecondary = nullptr;

        if (waitingRoom) // peers that answers call will be redirectedinto waitinf room
        {
            waitingPrimary = &mCallWrChanged[a1];
            waitingSecondary = &mCallWR[a2];
        }
        else // waiting room will be bypassed by participants that answers the call
        {
            waitingPrimary = &mChatCallSessionStatusInProgress[a1];
            waitingSecondary = &mChatCallSessionStatusInProgress[a2];
        }

        ASSERT_NO_FATAL_FAILURE({
            waitForAction (1, // just one attempt as call could be answered properly at B account but any of the other flags not received
                          { waitingPrimary, waitingSecondary },
                          { "waitingPrimary", "waitingSecondary" },
                          "answering chat call from B",
                          true /* wait for all exit flags*/,
                          true /*reset flags*/,
                          maxTimeout,
                          [this, &a2, &chatid]()
                          {
                              ChatRequestTracker crtAnswerCall(megaChatApi[a2]);
                              megaChatApi[a2]->answerChatCall(chatid, /*enableVideo*/ false, /*enableAudio*/ false, &crtAnswerCall);
                              ASSERT_EQ(crtAnswerCall.waitForResult(), MegaChatError::ERROR_OK)
                                  << "Failed to answer call. Error: " << crtAnswerCall.getErrorString();
                          });
        });
    };

    auto picksUpCallSecondaryAccount = [this, &a1, &a2](const bool isRingingExpected) -> unique_ptr<MegaChatCall>
    {
        mCallIdExpectedReceived[a2] = MEGACHAT_INVALID_HANDLE;
        unique_ptr<MegaChatCall> auxCall(megaChatApi[a1]->getChatCall(mChatIdInProgressCall[a1]));
        if (!auxCall)
        {
            return nullptr;
        }

        mCallIdExpectedReceived[a2] = auxCall->getCallId();
        if (isRingingExpected)
        {
            EXPECT_NE(mChatIdRingInCall[a2], MEGACHAT_INVALID_HANDLE) << "Invalid Chatid for B from A (call emisor)";
            EXPECT_TRUE((mCallIdJoining[a1] == mCallIdRingIn[a2]) && (mCallIdRingIn[a2] != MEGACHAT_INVALID_HANDLE)) << "A and B are in different call";
        }
        LOG_debug << "B received the call";
        return auxCall;
    };

    std::function<void(MegaChatHandle)> testCleanup = [this, &a1, &a2, crl = chatroomListener.get(),
                                                       lvlA = &localVideoListenerA, lvlB = &localVideoListenerB]
        (MegaChatHandle chatid) -> void
    {
        EXPECT_NE(chatid, MEGACHAT_INVALID_HANDLE) << "testCleanup: Invalid chatid provided";
        std::unique_ptr<MegaChatCall> call(megaChatApi[a1]->getChatCall(chatid));
        if (call)
        {
            LOG_debug << "testCleanup: A ends call for all participants";
            EXPECT_NE(call->getCallId(), MEGACHAT_INVALID_HANDLE) << "testCleanup: Invalid callid";
            waitForAction (1,
                          { &mCallDestroyed[a1], &mCallDestroyed[a2] },
                          { "&mCallDestroyed[a1]", "&mCallDestroyed[a2]" },
                          "A ends call for all participants",
                          true /* wait for all exit flags*/,
                          true /*reset flags*/,
                          maxTimeout,
                          [this, &a1, callid = call->getCallId()]()
                          {
                              ChatRequestTracker crtEndCall(megaChatApi[a1]);
                              megaChatApi[a1]->endChatCall(callid, &crtEndCall);
                              EXPECT_EQ(crtEndCall.waitForResult(), MegaChatError::ERROR_OK)
                                  << "Failed to end call. Error: " << crtEndCall.getErrorString();
                          });
        }
        // else => call doesn't exists anymore for this chat, the main purpose of this method is cleaning up test environment
        //         so in case there's no call, we can assume that it has ended by any other reason


        LOG_debug << "Unregistering chatRoomListeners and localVideoListeners";
        megaChatApi[a1]->closeChatRoom(chatid, crl);
        megaChatApi[a2]->closeChatRoom(chatid, crl);
        megaChatApi[a1]->removeChatLocalVideoListener(chatid, lvlA);
        megaChatApi[a2]->removeChatLocalVideoListener(chatid, lvlB);
    };

    // when this object goes out of scope testCleanup will be executed ending any call in this chat and freeing any resource associated to it
    MrProper p (testCleanup, chatid);

    LOG_debug << "#### Test1: A starts a groupal meeting, B it's (automatically) pushed into waiting room and A grants access to call ####";
    // Call won't ring for the rest of participants as schedId is provided
    ASSERT_NO_FATAL_FAILURE({startWaitingRoomCallPrimaryAccount(schedId);});
    unique_ptr<MegaChatCall> auxCall(megaChatApi[a1]->getChatCall(chatid));

    // B picks up the call
    LOG_debug << "B Pickups the call (should not ring)";
    auxCall = picksUpCallSecondaryAccount(false /*isRingingExpected*/);

    LOG_debug << "B Answers the call";
    ASSERT_NO_FATAL_FAILURE({answerCallSecondaryAccount(true /*waitingRoom*/);});

    std::unique_ptr<MegaChatCall> call(megaChatApi[a1]->getChatCall(chatid));
    std::unique_ptr<MegaChatWaitingRoom> wr(call && call->getWaitingRoom()
                                                ? call->getWaitingRoom()->copy()
                                                : nullptr);

    ASSERT_TRUE(wr && wr->getUserStatus(uh) == MegaChatWaitingRoom::MWR_NOT_ALLOWED)
        << (!wr ? "Waiting room can't be retrieved for user A" : "B it's not in the waiting room");

    grantsJoinPermission();

    LOG_debug << "#### Test2: A Pushes B into waiting room, (A ignores it, there's no way to reject a Join req) ####";
    pushIntoWr();

    LOG_debug << "#### Test3: B waits into waiting room until SFU timeout expires and BYE command is received with termcode: TERM_CODE_WR_TIMEOUT ####";
    unsigned int timeout = 660; // SFU timeout for waiting room is 10 minutes, so we need to add an extra period to deal with any posible delay
    bool* callLeftSecondary = &mCallLeft[a2]; *callLeftSecondary = false;
    int* termcodeLeftSecondary = &mTerminationCode[a2]; *termcodeLeftSecondary = MegaChatCall::TERM_CODE_INVALID;
    ASSERT_TRUE(waitForResponse(callLeftSecondary, timeout)) << "Call not ended after expire SFU waiting room timeout";
    ASSERT_EQ(*termcodeLeftSecondary, MegaChatCall::TERM_CODE_WR_TIMEOUT) << "Unexpected termcode received upon SFU waiting room timeout: " << *termcodeLeftSecondary;
}

/**
 * @brief MegaChatApiTest.ScheduledMeetings
 *
 * Requirements:
 *      - Both accounts should be conctacts
 * (if not accomplished, the test automatically solves them)
 *
 * This test does the following:
 * Test 1.  A Creates a Meeting room and a recurrent scheduled meeting in one step
 * Test 2.  A Updates a recurrent scheduled meeting with invalid TimeZone (Error)
 * Test 3.  A Updates previous recurrent scheduled meeting with valid data
 * Test 4.  A Updates scheduled meeting title along with chatroom title
 * Test 5.  A Updates a scheduled meeting occurrence with invalid schedId (Error)
 * Test 6.  A Updates a scheduled meeting occurrence (new child sched meeting created)
 * Test 7.  A Fetch scheduled meetings occurrences chatroom
 * Test 8.  A Cancels previous scheduled meeting occurrence
 * Test 9.  A Sets negative offset at byMonthWeekDay
 * Test 10. A Cancels entire series
 * Test 11. A Deletes scheduled meeting with invalid schedId (Error)
 * Test 12. A Deletes scheduled meeting
 */
TEST_F(MegaChatApiTest, ScheduledMeetings)
{
    unsigned a1 = 0;
    unsigned a2 = 1;

    LOG_debug << "\tSwitching to staging (Shard 2) for group creation (TEMPORARY)";
    megaApi[a1]->changeApiUrl("https://staging.api.mega.co.nz/");

    // aux data structure to handle lambdas' arguments
    SchedMeetingData smDataTests1, smDataTests2;

    // remove scheduled meeting
    const auto deleteSchedMeeting = [this, &a1, &a2](const unsigned int index, const int expectedError, const SchedMeetingData& smData) -> void
    {
        bool exitFlag = false;
        mSchedMeetingUpdated[a1] = mSchedMeetingUpdated[a2] = false;         // reset sched meetings updated flags
        mSchedIdRemoved[a1] = mSchedIdRemoved[a2] = MEGACHAT_INVALID_HANDLE; // reset sched meetings id's (do after assign vars above)

        // wait for onRequestFinish
        ASSERT_NO_FATAL_FAILURE({
        waitForAction (1,
                       std::vector<bool *> { &exitFlag },
                       std::vector<string> { "TYPE_DELETE_SCHEDULED_MEETING[a1]"},
                       "Removing scheduled meeting from A",
                       true /* wait for all exit flags*/,
                       true /*reset flags*/,
                       maxTimeout,
                       [&api = megaChatApi[index], &d = smData, &expectedError, &exitFlag]()
                       {
                            ChatRequestTracker crtRemoveMeeting(api);
                            api->removeScheduledMeeting(d.chatId, d.schedId, &crtRemoveMeeting);
                            auto res = crtRemoveMeeting.waitForResult();
                            exitFlag = true;
                            ASSERT_EQ(res, expectedError)
                                        << "Unexpected error while removing scheduled meeting. Error: " << crtRemoveMeeting.getErrorString();
                       });
        });
        if (expectedError != MegaChatError::ERROR_OK) { return; }

        // wait for onChatSchedMeetingUpdate (just in case expectedError is ERROR_OK)
        waitForMultiResponse(std::vector<bool *> {&mSchedMeetingUpdated[a1], &mSchedMeetingUpdated[a2]}, true, maxTimeout);
        ASSERT_NE(mSchedIdRemoved[a1], MEGACHAT_INVALID_HANDLE) << "Scheduled meeting for primary account could not be removed. scheduled meeting id: "
                                                                << getSchedIdStrB64(smData.schedId);

        ASSERT_NE(mSchedIdRemoved[a2], MEGACHAT_INVALID_HANDLE) << "Scheduled meeting for secondary account could not be removed. scheduled meeting id: "
                                                                << getSchedIdStrB64(smData.schedId);
    };

    // fetch scheduled meeting occurrences
    std::unique_ptr<::megachat::MegaChatScheduledMeetingOccurrList> occurrences;
    const auto fetchOccurrences = [this, &occurrences](const unsigned int index, const int expectedError,
                                                       const SchedMeetingData& smData) -> void
    {
        occurrences.reset();
        // check if occurrence is inside requested range
        bool exitFlag = false;

        // wait for onRequestFinish
        ASSERT_NO_FATAL_FAILURE({
            waitForAction (1,
                          std::vector<bool *> { &exitFlag },
                          std::vector<string> { "TYPE_FETCH_SCHEDULED_MEETING_OCCURRENCES[a1]" },
                          "Fetching scheduled meeting occurrences",
                          true /* wait for all exit flags */,
                          true /* reset flags */,
                          maxTimeout,
                          [&api = megaChatApi[index], &d = smData, &expectedError, &exitFlag, &occurrences]()
                          {
                              const MegaChatTimeStamp sinceTs = d.startDate;
                              const auto isValidOccurr = [&sinceTs](const MegaChatTimeStamp& ts)
                              {
                                  return sinceTs <= ts; // check until limit in this method when apps can filter ocurrences by that field
                              };

                              ChatRequestTracker crtFetchOccurrences(api);
                              api->fetchScheduledMeetingOccurrencesByChat(d.chatId, d.startDate, &crtFetchOccurrences);
                              const int errCode = crtFetchOccurrences.waitForResult();
                              exitFlag = true; // set exitFlag as true as waitForAction is waiting for it
                              ASSERT_EQ(errCode, expectedError) << "Unexpected error while fetching scheduled meetings. Error: " << crtFetchOccurrences.getErrorString();

                              if (!errCode)
                              {
                                  ASSERT_TRUE(crtFetchOccurrences.hasScheduledMeetingOccurrList()) << "fetchScheduledMeetingOccurrencesByChat finished Ok "
												   << "but no scheduled meeting occurrences list received";
                                  occurrences = crtFetchOccurrences.getScheduledMeetingsOccurrences();
                                  if (occurrences)
                                  {
                                      for (size_t i =  0; i < occurrences->size(); ++i)
                                      {
                                          const auto occurr = occurrences->at(i);
                                          ASSERT_TRUE(isValidOccurr(occurr->startDateTime())) << "StartDateTime out of specified range for occurrence";
                                          ASSERT_TRUE(isValidOccurr(occurr->endDateTime()))   << "EndDateTime out of specified range for occurrence";
                                      }
                                  }
                              }
                          });
        });
    };

    auto printOccurrences = [](const::megachat::MegaChatScheduledMeetingOccurrList* l, const int expectedOccurr) -> void
    {
        if (!l) { return; }
        std::string text = "Error fetching occurrences. \nExpected occurrences: (";
        text.append(std::to_string(expectedOccurr)).append(")");
        text.append("\nReceived occurrences: (").append(std::to_string(l->size())).append(")\n{\n");
        std::string schedIdB64;
        MegaChatHandle schedId = MEGACHAT_INVALID_HANDLE;
        for (size_t i = 0; i < l->size(); ++i)
        {
            if (schedId != l->at(i)->schedId())
            {
                schedId = l->at(i)->schedId();
                const std::unique_ptr<char[]> auxId(MegaApi::userHandleToBase64(schedId));
                schedIdB64 = auxId.get();
            }
            text.append("\tscheId: ").append(schedIdB64).append(" | startTs: ")
                .append(std::to_string(l->at(i)->startDateTime())).append("\n");
        }
        text.append("}");
        LOG_err << text;
    };

    // update scheduled meeting occurrence
    const auto updateOccurrence = [this, &a1, &a2, &fetchOccurrences, &printOccurrences, &occurrences](const unsigned int index, const unsigned int maxAttempts,
                                                                      const int expectedError, const int repeatError, const SchedMeetingData& smData) -> void
    {
        bool exitFlag = false;
        mSchedMeetingUpdated[a1] = mSchedMeetingUpdated[a2] = false;         // reset sched meetings updated flags
        mSchedIdUpdated[a1] = mSchedIdUpdated[a2] = MEGACHAT_INVALID_HANDLE; // reset sched meetings id's (do after assign vars above)
        int res = MegaChatError::ERROR_UNKNOWN;

        // wait for onRequestFinish
        ASSERT_NO_FATAL_FAILURE({
        waitForAction (1,
                       std::vector<bool *> { &exitFlag },
                       std::vector<string> { "TYPE_UPDATE_SCHEDULED_MEETING_OCCURRENCE[a1]"},
                       "Updating scheduled meeting occurrence",
                       true /* wait for all exit flags */,
                       true /* reset flags */,
                       maxTimeout,
                       [&api = megaChatApi[index], &d = smData, &repeatError, &maxAttempts, &exitFlag, &res]()
                       {
                            unsigned int attempts = 0;
                            do
                            {
                                ChatRequestTracker crtUpdateOccurrence(api);
                                api->updateScheduledMeetingOccurrence(d.chatId, d.schedId, d.overrides, d.newStartDate, d.newEndDate, d.newCancelled, &crtUpdateOccurrence);
                                res = crtUpdateOccurrence.waitForResult();
                                if (res == repeatError && ++attempts < maxAttempts) { continue; }
                                exitFlag = true;
                                break;
                            }
                            while (1);
                       });
        });

        if (res == MegaChatError::ERROR_OK)
        {
            // wait for onChatSchedMeetingUpdate (just in case expectedError is ERROR_OK)
            waitForMultiResponse(std::vector<bool *> {&mSchedMeetingUpdated[a1], &mSchedMeetingUpdated[a2]}, true, maxTimeout);
            ASSERT_NE(mSchedIdUpdated[a1], MEGACHAT_INVALID_HANDLE) << "Scheduled meeting occurrence for primary account could not be updated scheduled meeting id: "
                                                                    << getSchedIdStrB64(smData.schedId) << " overrides: " << std::to_string(smData.overrides);

            ASSERT_NE(mSchedIdUpdated[a2], MEGACHAT_INVALID_HANDLE) << "Scheduled meeting occurrence for secondary account could not be updated scheduled meeting id: "
                                                                    << getSchedIdStrB64(smData.schedId) << " overrides: " << std::to_string(smData.overrides);
        }
        else
        {
            if (res == MegaChatError::ERROR_NOENT)
            {
                LOG_err << "Can't update scheduled meeting occurrence, fetching occurrences";
                ASSERT_NO_FATAL_FAILURE({ fetchOccurrences(a1, MegaChatError::ERROR_OK, smData); });
                if (occurrences) { printOccurrences(occurrences.get(), MegaChatScheduledMeeting::NUM_OCURRENCES_REQ); }
            }
            ASSERT_EQ(res, expectedError) << "Unexpected error while updating scheduled meeting occurrence. Error: " << res;
        }

    };

    // get scheduled meeting
    const auto getSchedMeeting = [this](const unsigned int index, const SchedMeetingData& smData) -> std::unique_ptr<MegaChatScheduledMeeting>
    {
        const auto smList = std::unique_ptr<megachat::MegaChatScheduledMeetingList>(megaChatApi[index]->getScheduledMeetingsByChat(smData.chatId));
        const bool validSchedId = smData.schedId != MEGACHAT_INVALID_HANDLE;
        for (size_t i = 0, sz = smList->size(); i < sz; ++i)
        {
            if (!validSchedId && smList->at(i)->parentSchedId() == MEGACHAT_INVALID_HANDLE)
            {
                // if no schedId provided return the parent sched meeting for this chat
                return std::unique_ptr<MegaChatScheduledMeeting>(smList->at(i)->copy());
            }

            if (validSchedId && smList->at(i)->schedId() == smData.schedId)
            {
                // if schedId provided return the sched meeting that matches with provided schedId, if any
                return std::unique_ptr<MegaChatScheduledMeeting>(smList->at(i)->copy());
            }
        }
        return nullptr;
    };

    // create chatroom and scheduled meeting
    MegaChatHandle chatid = MEGACHAT_INVALID_HANDLE;

    auto getLastMsgIfManagement = [this] (const unsigned uIndex, const MegaChatHandle chatid, const int expectedType) -> MegaChatMessage*
    {
        auto crListener = std::make_unique<TestChatRoomListener>(this, megaChatApi, chatid);
        if (!megaChatApi[uIndex]->openChatRoom(chatid, crListener.get()))
        {
            LOG_debug << "getLastMsgIfManagement: Cannot open chatroom" << getChatIdStrB64(chatid);
            return nullptr;
        }
        loadHistory(uIndex, chatid, crListener.get());
        megaChatApi[uIndex]->closeChatRoom(chatid, crListener.get());

        std::unique_ptr<MegaChatListItem> lItem(megaChatApi[uIndex]->getChatListItem(chatid));
        if (!lItem)
        {
            LOG_debug << "getLastMsgIfManagement: Cannot get chatlist item for chat" << getChatIdStrB64(chatid);
            return nullptr;
        }

        if (lItem->getLastMessageType() != expectedType)
        {
            LOG_debug << "getLastMsgIfManagement: last message in chat: " << getChatIdStrB64(chatid) << "is not of type: " << expectedType;
            return nullptr;
        }

        MegaChatMessage* msg = megaChatApi[uIndex]->getMessage(chatid, lItem->getLastMessageId());
        if (!msg || msg->getType() != expectedType)
        {
            return nullptr;
        }

        return msg;
    };

    const auto schedChangeToString = [] (const unsigned int flag) -> std::string
    {
        if (flag == MegaChatScheduledMeeting::SC_NEW_SCHED)     { return "New"; }
        if (flag == MegaChatScheduledMeeting::SC_PARENT)        { return "p"; }
        if (flag == MegaChatScheduledMeeting::SC_TZONE)         { return "tz"; }
        if (flag == MegaChatScheduledMeeting::SC_START)         { return "s"; }
        if (flag == MegaChatScheduledMeeting::SC_END)           { return "e"; }
        if (flag == MegaChatScheduledMeeting::SC_TITLE)         { return "t"; }
        if (flag == MegaChatScheduledMeeting::SC_DESC)          { return "d"; }
        if (flag == MegaChatScheduledMeeting::SC_ATTR)          { return "at"; }
        if (flag == MegaChatScheduledMeeting::SC_OVERR)         { return "o"; }
        if (flag == MegaChatScheduledMeeting::SC_CANC)          { return "c"; }
        if (flag == MegaChatScheduledMeeting::SC_FLAGS)         { return "f"; }
        if (flag == MegaChatScheduledMeeting::SC_RULES)         { return "r"; }
        if (flag == MegaChatScheduledMeeting::SC_FLAGS_SIZE)    { return "Invalid"; }
        return "Unknown";
    };

    auto checkSchedMeetMsgChanges = [&schedChangeToString] (const MegaChatMessage* msg, const std::vector<unsigned int>& flags) -> bool
    {
        if (!msg) { return false;}

        bool match = true;
        for (const auto flag: flags)
        {
            if (!msg->hasSchedMeetingChanged(flag))
            {
                match = false;
                break;
            }
        };

        if (!match)
        {
            std::string changesStr = "checkSchedMeetMsgChanges: Expected changes => [  ";
            for (auto f: flags)
            {
                changesStr.append(schedChangeToString(f).append("  "));
            }

            changesStr.append("] Received changes: {  ");
            for (unsigned int i = MegaChatScheduledMeeting::SC_NEW_SCHED; i < MegaChatScheduledMeeting::SC_FLAGS_SIZE; ++i)
            {
                changesStr.append(schedChangeToString(i)).append(": ").append(std::to_string(msg->hasSchedMeetingChanged(i)).append("  "));
            }
            changesStr.append("}");
            LOG_err << changesStr;
        }

        return match;
    };

    auto checkSchedParentId = [] (const MegaChatMessage* msg, const MegaChatHandle parentSchedId) -> bool
    {
        if (!msg) { return false;}
        const MegaStringList* l = msg->getScheduledMeetingChange(MegaChatScheduledMeeting::SC_PARENT);
        const bool msgHasParentId = l && l->size() == 1;

        return parentSchedId != MEGACHAT_INVALID_HANDLE
            ? msgHasParentId && MegaApi::base64ToUserHandle(l->get(0)) == parentSchedId
            : !msgHasParentId;
    };

    const auto checkSchedMeetMsg = [&getLastMsgIfManagement, &checkSchedMeetMsgChanges, &checkSchedParentId]
        (unsigned int index, const MegaChatHandle chatid, const MegaChatHandle parentSchedId
         , const std::vector<unsigned int> changes, const std::string msg) -> void
    {
        // fetch last message that must be of type: TYPE_SCHED_MEETING and check that msg changes, matches with expected ones
        std::unique_ptr<MegaChatMessage> lastManagementMsg(getLastMsgIfManagement(index, chatid, megachat::MegaChatMessage::TYPE_SCHED_MEETING));
        ASSERT_TRUE(lastManagementMsg) << msg << " .Can't retrieve last message or it's type is not expected one";

        ASSERT_TRUE(checkSchedMeetMsgChanges(lastManagementMsg.get(), changes))
            << msg << " .Unexpected changeset received for sched meeting management msg";

        ASSERT_TRUE(checkSchedParentId(lastManagementMsg.get(), parentSchedId /*expected parent SchedId*/))
            << msg << " .Unexpected parentSchedId in sched meeting management msg";
    };

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

    LOG_debug << "#### Test1. A Creates a Meeting room and a recurrent scheduled meeting in one step ####";
    const std::shared_ptr<MegaChatPeerList> peerList(MegaChatPeerList::createInstance());
    const time_t now = time(nullptr);
    peerList->addPeer(user->getHandle(), MegaChatPeerList::PRIV_STANDARD);
    std::string title = "SMChat_" + std::to_string(now);
    std::string description = "SMChat_Description";
    std::string timeZone = "Europe/Madrid";
    const MegaChatTimeStamp startDate = now + 300;
    const MegaChatTimeStamp endDate =  startDate + 600;

    // create MegaChatScheduledFlags
    std::shared_ptr<MegaChatScheduledFlags> flags(MegaChatScheduledFlags::createInstance());
    flags->setSendEmails(true);

    // create MegaChatScheduledRules
    std::shared_ptr<MegaChatScheduledRules> rules(MegaChatScheduledRules::createInstance(MegaChatScheduledRules::FREQ_DAILY,
                                                                                         MegaChatScheduledRules::INTERVAL_INVALID,
                                                                                         MEGACHAT_INVALID_TIMESTAMP,
                                                                                         nullptr, nullptr, nullptr));
    smDataTests1.peerList = peerList;
    smDataTests1.isMeeting = true;
    smDataTests1.publicChat = true;
    smDataTests1.title = title;
    smDataTests1.speakRequest = false;
    smDataTests1.waitingRoom = false;
    smDataTests1.openInvite = false;
    smDataTests1.timeZone = timeZone;
    smDataTests1.startDate = startDate;
    smDataTests1.endDate = endDate;
    smDataTests1.description = ""; // description is not a mandatory field
    smDataTests1.flags = nullptr;  // flags is not a mandatory field
    smDataTests1.rules = rules;
    ASSERT_NO_FATAL_FAILURE({ createChatroomAndSchedMeeting (chatid, a1, a2, smDataTests1); });

    // check that SC_NEW_SCHED management msg content is expected
    ASSERT_NO_FATAL_FAILURE({ checkSchedMeetMsg(a2
                                                , chatid
                                                , MEGACHAT_INVALID_HANDLE
                                                , std::vector<unsigned int> { MegaChatScheduledMeeting::SC_NEW_SCHED }
                                                , "Test1"); });

    const MegaChatHandle schedId = mSchedIdUpdated[a1];
    SchedMeetingData smData; // Designated initializers generate too many warnings (gcc)
    smData.chatId = chatid;
    smData.schedId = MEGACHAT_INVALID_HANDLE;

    const auto schedMeet = getSchedMeeting(a1, smData);
    ASSERT_TRUE(schedMeet) << "Can't retrieve scheduled meeting for new chat " << getChatIdStrB64(chatid);
    ASSERT_TRUE(!schedMeet->flags() && !schedMeet->description()) << "Scheduled meeting flags must be unset and description must be an empty string" ;
    ASSERT_TRUE(flags->sendEmails()) << "Scheduled meeting created doesn't have send emails flag enabled but it was set on creation";

    LOG_debug << "#### Test2. A Updates a recurrent scheduled meeting with invalid TimeZone (Error) ####";
    timeZone = "Europe/Borlin"; // invalid timezone
    title.append("(updated)");
    description.append("(updated)");
    smDataTests1.chatId = chatid;
    smDataTests1.schedId = schedId;
    smDataTests1.timeZone = timeZone;
    smDataTests1.title = title;
    smDataTests1.cancelled = false;
    ASSERT_NO_FATAL_FAILURE({ updateSchedMeeting(a1, a2, MegaChatError::ERROR_ARGS, smDataTests1, false /*updateChatTitle*/); });

    LOG_debug << "#### Test3. A Updates previous recurrent scheduled meeting with valid data ####";
    timeZone = "Europe/Dublin";
    smDataTests1.timeZone = timeZone;
    ASSERT_NO_FATAL_FAILURE({ updateSchedMeeting(a1, a2, MegaChatError::ERROR_OK, smDataTests1, false /*updateChatTitle*/); });

    // check that management msg content is expected
    ASSERT_NO_FATAL_FAILURE({ checkSchedMeetMsg(a2
                                                , chatid
                                                , MEGACHAT_INVALID_HANDLE
                                                , std::vector<unsigned int> { MegaChatScheduledMeeting::SC_TZONE }
                                                , "Test3"); });

    LOG_debug << "#### Test4. A Updates scheduled meeting title along with chatroom title ####";
    // no management message for chatroom title change will be received (API specs)
    // open chatroom
    auto crl = std::make_unique<TestChatRoomListener>(this, megaChatApi, chatid);
    ASSERT_TRUE(megaChatApi[a1]->openChatRoom(chatid, crl.get())) << "Can't open chatRoom user A";
    bool *titleItemChanged0 = &titleUpdated[a1]; *titleItemChanged0 = false;
    bool *titleChanged0 = &crl->titleUpdated[a1]; *titleChanged0 = false;

    // update sched meeting and chatroom title
    smDataTests1.title += "_upd";
    ASSERT_NO_FATAL_FAILURE({ updateSchedMeeting(a1, a2, MegaChatError::ERROR_OK, smDataTests1, true /*updateChatTitle*/); });

    // wait for onChatListItemUpdate(CHANGE_TYPE_TITLE) and onChatRoomUpdate (CHANGE_TYPE_TITLE)
    ASSERT_TRUE(waitForResponse(titleItemChanged0)) << "Timeout expired for receiving chat list item update";
    ASSERT_TRUE(waitForResponse(titleChanged0))     << "Timeout expired for receiving chatroom update";
    megaChatApi[a1]->closeChatRoom(chatid, crl.get());

    // check that management msg content is expected
    ASSERT_NO_FATAL_FAILURE({ checkSchedMeetMsg(a2
                                                , chatid
                                                , MEGACHAT_INVALID_HANDLE
                                                , std::vector<unsigned int> { MegaChatScheduledMeeting::SC_TITLE }
                                                , "Test4"); });

    LOG_debug << "#### Test5. A Updates a scheduled meeting occurrence with invalid schedId (Error) ####";
    smDataTests2.chatId = chatid;
    smDataTests2.overrides = startDate;
    smDataTests2.newStartDate = startDate;
    smDataTests2.newEndDate = endDate;
    smDataTests2.newCancelled = false;
    ASSERT_NO_FATAL_FAILURE({ updateOccurrence(a1, 1/*maxAttempts*/, MegaChatError::ERROR_NOENT, MegaChatError::ERROR_TOOMANY, smDataTests2); });

    LOG_debug << "#### Test6. A Updates a scheduled meeting occurrence (new child sched meeting created) ####";
    MegaChatTimeStamp overrides =  startDate;
    const MegaChatTimeStamp auxStartDate =  startDate + 120;
    const MegaChatTimeStamp auxEndDate = endDate + 120;
    // update occurrence and ensure that we have received a new child scheduled meeting whose parent is the original sched meeting and contains the updated occurrence
    smDataTests2.schedId = schedId;
    smDataTests2.overrides = overrides;
    smDataTests2.newStartDate = auxStartDate;
    smDataTests2.newEndDate = auxEndDate;
    ASSERT_NO_FATAL_FAILURE({ updateOccurrence(a1, 3/*maxAttempts*/, MegaChatError::ERROR_OK, MegaChatError::ERROR_TOOMANY, smDataTests2); });
    auto sched = std::unique_ptr<MegaChatScheduledMeeting>(megaChatApi[a1]->getScheduledMeeting(chatid, mSchedIdUpdated[a1]));
    ASSERT_TRUE(sched);
    ASSERT_EQ(sched->parentSchedId(), schedId) << "Child scheduled meeting for primary account has not been received scheduled meeting id: " <<  getSchedIdStrB64(schedId);

    // check that SC_NEW_SCHED management msg content is expected
    ASSERT_NO_FATAL_FAILURE({ checkSchedMeetMsg(a2
                                                , chatid
                                                , schedId
                                                , std::vector<unsigned int> { MegaChatScheduledMeeting::SC_START
                                                                          , MegaChatScheduledMeeting::SC_END }
                                                , "Test6"); });

    const MegaChatHandle childSchedId = sched->schedId();
    smData = SchedMeetingData(); // Designated initializers generate too many warnings (gcc)
    smData.chatId = chatid;
    smData.schedId = childSchedId;
    ASSERT_TRUE(getSchedMeeting(a1, smData)) << "Can't retrieve child scheduled meeting for chat: " << getChatIdStrB64(chatid);

    LOG_debug << "#### Test7. A Fetch scheduled meetings occurrences chatroom ####";
    smData = SchedMeetingData(); // Designated initializers generate too many warnings (gcc)
    smData.chatId = chatid;
    smData.startDate = MEGACHAT_INVALID_TIMESTAMP;
    ASSERT_NO_FATAL_FAILURE({ fetchOccurrences(a1, MegaChatError::ERROR_OK, smData); });
    ASSERT_TRUE(occurrences) << "Null scheduled meeting occurrences list received for chat: " << getChatIdStrB64(chatid)
                             << ". We should have received a valid occurrences list";

    if (occurrences->size() != MegaChatScheduledMeeting::NUM_OCURRENCES_REQ)
    {
        printOccurrences(occurrences.get(), MegaChatScheduledMeeting::NUM_OCURRENCES_REQ);
        ASSERT_TRUE(false) << "Error fetching occurrences for primary account for chat: " << getChatIdStrB64(chatid);
    }

    const MegaChatScheduledMeetingOccurr* lastestOcurr = occurrences->at(occurrences->size() -1);
    if (lastestOcurr && lastestOcurr->startDateTime() != MEGACHAT_INVALID_TIMESTAMP)
    {
        smData = SchedMeetingData(); // Designated initializers generate too many warnings (gcc)
        smData.chatId = chatid;
        smData.startDate = lastestOcurr->startDateTime();
        ASSERT_NO_FATAL_FAILURE({ fetchOccurrences(a1, MegaChatError::ERROR_OK, smData); });
        if (!occurrences || occurrences->size() != MegaChatScheduledMeeting::NUM_OCURRENCES_REQ)
        {
            if (occurrences) { printOccurrences(occurrences.get(), MegaChatScheduledMeeting::NUM_OCURRENCES_REQ); }
            ASSERT_TRUE(false) << "Error fetching more occurrences for primary account for chat: " << getChatIdStrB64(chatid);
        }
    }

    LOG_debug << "#### Test8. A Cancels previous scheduled meeting occurrence ####";
    overrides = auxStartDate;
    smDataTests2.schedId = childSchedId;
    smDataTests2.overrides = overrides;
    smDataTests2.newCancelled = true;
    ASSERT_NO_FATAL_FAILURE({ updateOccurrence(a1, 3/*maxAttempts*/, MegaChatError::ERROR_OK, MegaChatError::ERROR_TOOMANY, smDataTests2); });
    sched = std::unique_ptr<MegaChatScheduledMeeting>(megaChatApi[a1]->getScheduledMeeting(chatid, mSchedIdUpdated[a1]));
    ASSERT_TRUE(sched);
    ASSERT_EQ(sched->schedId(), childSchedId) << "Scheduled meeting id: " << getSchedIdStrB64(schedId)
                                              << " does not match with expected one: " << getSchedIdStrB64(childSchedId);

    ASSERT_TRUE(sched->cancelled()) << "Scheduled meeting occurrence could not be cancelled, scheduled meeting id: "
                                    <<  getSchedIdStrB64(schedId) << " overrides: " << std::to_string(overrides);

    // check that SC_NEW_SCHED management msg content is expected
    ASSERT_NO_FATAL_FAILURE({ checkSchedMeetMsg(a2
                                                , chatid
                                                , schedId
                                                , std::vector<unsigned int> { MegaChatScheduledMeeting::SC_CANC }
                                                , "Test8"); });

    LOG_debug << "#### Test9. A Sets negative offset at byMonthWeekDay ####";
    const int interval = 1;
    const int offset = -1;
    const int day = 1;
    std::unique_ptr<::mega::MegaIntegerMap> byMonthWeekDay(::mega::MegaIntegerMap::createInstance());
    byMonthWeekDay->set(offset, day);
    smDataTests1.rules.reset(MegaChatScheduledRules::createInstance(MegaChatScheduledRules::FREQ_MONTHLY,
                                                                      interval,
                                                                      MEGACHAT_INVALID_TIMESTAMP,
                                                                      nullptr, nullptr, byMonthWeekDay.get()));

    ASSERT_NO_FATAL_FAILURE({ updateSchedMeeting(a1, a2, MegaChatError::ERROR_OK, smDataTests1, false /*updateChatTitle*/); });
    auto aschedMeet = getSchedMeeting(a1, smDataTests1);
    ASSERT_TRUE(aschedMeet) << "Can't retrieve scheduled meeting for chat " << getChatIdStrB64(chatid);
    const auto recvRules = aschedMeet->rules();
    ASSERT_TRUE(recvRules) << "Can't retrieve scheduled meeting rules for chat " << getChatIdStrB64(chatid);
    const ::mega::MegaIntegerMap* recvByMonthWeekDay = recvRules->byMonthWeekDay();
    ASSERT_TRUE(recvByMonthWeekDay) << "Can't retrieve ByMonthWeekDay for chat " << getChatIdStrB64(chatid);
    ASSERT_EQ(recvByMonthWeekDay->size(), smDataTests1.rules->byMonthWeekDay()->size())
        << "Unexpected size for ByMonthWeekDay for chat " << getChatIdStrB64(chatid);

    // check negative offset values at ByMonthWeekDay (-1, 1) Last Monday of each month
    std::unique_ptr<MegaIntegerList> days(recvByMonthWeekDay->get(offset));
    ASSERT_TRUE(days) << "No key : " << offset << " exists at auxByMonthWeekDay for chat " << getChatIdStrB64(chatid);;
    ASSERT_EQ(days->size(), smDataTests1.rules->byMonthWeekDay()->size()) << "Unexpected byMonthWeekDay size "
                                                                            << days->size() << " for chat " << getChatIdStrB64(chatid);
    ASSERT_EQ(days->get(0), day) << "Unexpected value: " << days->get(0)
                                 << ", expected one(" << day
                                 << ") for key: " << offset << "in chat " << getChatIdStrB64(chatid);

    LOG_debug << "#### Test10. A Cancels entire series ####";
    smDataTests1.cancelled = true;
    ASSERT_NO_FATAL_FAILURE({ updateSchedMeeting(a1, a2, MegaChatError::ERROR_OK, smDataTests1, false /*updateChatTitle*/); });

    smData = SchedMeetingData(); // Designated initializers generate too many warnings (gcc)
    smData.chatId = chatid;
    smData.startDate = MEGACHAT_INVALID_TIMESTAMP;
    ASSERT_NO_FATAL_FAILURE({ fetchOccurrences(a1, MegaChatError::ERROR_OK, smData); });
    ASSERT_TRUE(occurrences) << "Null scheduled meeting occurrences list received for chat: " << getChatIdStrB64(chatid)
                             << ". We should have received an empty list";
    if (occurrences->size())
    {
        printOccurrences(occurrences.get(), 0);
        ASSERT_TRUE(false) << "Non empty scheduled meeting occurrences list received for chat: " << getChatIdStrB64(chatid);
    }

    // check that SC_NEW_SCHED management msg content is expected
    ASSERT_NO_FATAL_FAILURE({ checkSchedMeetMsg(a2
                                                , chatid
                                                , MEGACHAT_INVALID_HANDLE
                                                , std::vector<unsigned int> { MegaChatScheduledMeeting::SC_CANC }
                                                , "Test10"); });

    LOG_debug << "#### Test11. A Deletes scheduled meeting with invalid schedId (Error) ####";
    smData = SchedMeetingData(); // Designated initializers generate too many warnings (gcc)
    smData.chatId = chatid;
    smData.schedId = MEGACHAT_INVALID_HANDLE;
    ASSERT_NO_FATAL_FAILURE({ deleteSchedMeeting(a1, MegaChatError::ERROR_ARGS, smData); });

    LOG_debug << "#### Test12. A Deletes scheduled meeting ####";
    smData = SchedMeetingData(); // Designated initializers generate too many warnings (gcc)
    smData.chatId = chatid;
    smData.schedId = schedId;
    ASSERT_NO_FATAL_FAILURE({ deleteSchedMeeting(a1, MegaChatError::ERROR_OK, smData); });

    LOG_debug << "\tSwitching back from staging (Shard 2) for group creation (TEMPORARY)";
    megaApi[a1]->changeApiUrl("https://g.api.mega.co.nz/");

}
#endif

/**
 * @brief MegaChatApiTest.RichLinkUserAttribute
 *
 * This test does the following:
 *
 * - Test1: Get state for rich link user attribute
 * - Test2: Enable/disable rich link generation
 * - Test3: Check if value has been established correctly
 * - Test4: Change value for rich link counter
 * - Test5: Check if value has been established correctly
 *
 */
TEST_F(MegaChatApiTest, RichLinkUserAttribute)
{
    unsigned a1 = 0;

   char *primarySession = login(a1);
   ASSERT_TRUE(primarySession);

   LOG_debug << "#### Test1: Get state for rich link user attribute ####";
   TestMegaRequestListener requestListener(megaApi[a1], nullptr);
   megaApi[a1]->shouldShowRichLinkWarning(&requestListener);
   ASSERT_TRUE(requestListener.waitForResponse()) << "Expired timeout for rich Link";
   int error = requestListener.getErrorCode();
   ASSERT_TRUE(!error || error == ::mega::API_ENOENT) << "Should show richLink warning. Error: " << error;
   ASSERT_EQ(requestListener.getMegaRequest()->getNumDetails(), 1) << "Active at shouldShowRichLink";

   LOG_debug << "#### Test2: Enable/disable rich link generation ####";
   bool enableRichLink = !(requestListener.getMegaRequest()->getFlag());
   requestListener = TestMegaRequestListener(megaApi[a1], nullptr);
   megaApi[a1]->enableRichPreviews(enableRichLink, &requestListener);
   ASSERT_TRUE(requestListener.waitForResponse()) << "User attribute retrieval not finished after timeout";
   ASSERT_TRUE(!requestListener.getErrorCode()) << "Failed to enable rich preview. Error: " << requestListener.getErrorCode();

   LOG_debug << "#### Test3: Check if value has been established correctly ####";
   requestListener = TestMegaRequestListener(megaApi[a1], nullptr);
   megaApi[a1]->shouldShowRichLinkWarning(&requestListener);
   ASSERT_TRUE(requestListener.waitForResponse()) << "Expired timeout for rich Link";
   error = requestListener.getErrorCode();
   ASSERT_TRUE(!error || error == ::mega::API_ENOENT) << "Should show richLink warning. Error: " << error;
   ASSERT_FALSE(requestListener.getMegaRequest()->getFlag()) << "Rich link enable/disable has not worked, (Rich link warning hasn't to be shown)";

   LOG_debug << "#### Test4: Change value for rich link counter ####";
   int counter = 1;
   requestListener = TestMegaRequestListener(megaApi[a1], nullptr);
   megaApi[a1]->setRichLinkWarningCounterValue(counter, &requestListener);
   ASSERT_TRUE(requestListener.waitForResponse()) << "User attribute retrieval not finished after timeout";
   ASSERT_FALSE(requestListener.getErrorCode()) << "Failed to set rich preview count. Error: " << requestListener.getErrorCode();

   LOG_debug << "#### Test5: Check if value has been established correctly ####";
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
 * - Test1: Enable rich links
 * - Test2: Send a message with a url
 * - Test3. Remove richlink (used to remove preview) from previous message with removeRichLink()
 * - Test4. Edit previous non-richlinked message by removing the URL.
 * - Test5. Edit previous message by adding a new URL.
 * - Test6. Edit previous message by modifying the previous URL
 * - Test7. Edit previous richlinked message by deleting the URL
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

    LOG_debug << "#### Test1: Enable rich links ####";
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

    LOG_debug << "#### Test2: Send a message with a url ####";
    std::string messageToSend = "Hello friend, http://mega.nz";
    // Need to do this for the first message as it's send and edited
    chatroomListener->msgEdited[a1] = false;
    chatroomListener->msgEdited[a2] = false;
    MegaChatMessage* msgSent = sendTextMessageOrUpdate(a1, a2, chatid, messageToSend, chatroomListener);
    ASSERT_TRUE(msgSent);
    ASSERT_NO_FATAL_FAILURE({ checkMessages(msgSent, messageToSend, true); });

    LOG_debug << "#### Test3. Remove richlink ####";
    // No call to sendTextMessageOrUpdate, so we must manually waitForUpdate for msgEdited to be set to 'true'
    chatroomListener->msgEdited[a1] = false;
    chatroomListener->msgEdited[a2] = false;
    MegaChatMessage* msgUpdated1 = megaChatApi[a1]->removeRichLink(chatid, msgSent->getMsgId());
    ASSERT_NO_FATAL_FAILURE({ checkMessages(msgUpdated1, messageToSend, false, true); });

    LOG_debug << "#### Test4. Edit previous non-richlinked message by removing the URL ####";
    std::string messageToUpdate2 = "Hello friend";
    MegaChatMessage* msgUpdated2 = sendTextMessageOrUpdate(a1, a2, chatid, messageToUpdate2, chatroomListener, msgUpdated1->getMsgId());
    ASSERT_TRUE(msgUpdated2);
    ASSERT_NO_FATAL_FAILURE({ checkMessages(msgUpdated2, messageToUpdate2, false); });

    LOG_debug << "#### Test5. Edit previous message by adding a new URL ####";
    std::string messageToUpdate3 = "Hello friend, sorry, the URL is https://mega.nz";
    MegaChatMessage* msgUpdated3 = sendTextMessageOrUpdate(a1, a2, chatid, messageToUpdate3, chatroomListener, msgUpdated2->getMsgId());
    ASSERT_TRUE(msgUpdated3);
    ASSERT_NO_FATAL_FAILURE({ checkMessages(msgUpdated3, messageToUpdate3, true); });

    LOG_debug << "#### Test6. Edit previous message by modifying the previous URL ####";
    std::string messageToUpdate4 = "Argghhh!!! Sorry again!! I meant https://mega.io that's the good one!!!";
    MegaChatMessage* msgUpdated4 = sendTextMessageOrUpdate(a1, a2, chatid, messageToUpdate4, chatroomListener, msgUpdated3->getMsgId());
    ASSERT_TRUE(msgUpdated4);
    ASSERT_NO_FATAL_FAILURE({ checkMessages(msgUpdated4, messageToUpdate4, true); });

    LOG_debug << "#### Test7. Edit previous richlinked message by deleting the URL ####";
    std::string messageToUpdate5 = "No more richlinks please!!!!";
    MegaChatMessage* msgUpdated5 = sendTextMessageOrUpdate(a1, a2, chatid, messageToUpdate5, chatroomListener, msgUpdated4->getMsgId());
    ASSERT_TRUE(msgUpdated5);
    ASSERT_NO_FATAL_FAILURE({ checkMessages(msgUpdated5, messageToUpdate5, false); });

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
 * - Test1: Send a message with a giphy
 * - Test2: Check if the receiver can get get the message correctly
 * - Test3: Check if the json can be parsed correctly
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

    LOG_debug << "#### Test1: Send a message with a giphy ####";
    MegaChatMessage* msgSent = megaChatApi[a1]->sendGiphy(chatid, srcMp4, srcWebp, sizeMp4, sizeWebp, giphyWidth, giphyHeight, giphyTitle);

    LOG_debug << "#### Test2: Check if the receiver can get get the message correctly ####";
    ASSERT_TRUE(waitForResponse(flagConfirmed)) << "Timeout expired for receiving confirmation by server. Timeout: " << maxTimeout << " seconds";

    LOG_debug << "#### Test3: Check if the json can be parsed correctly ####";
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

int MegaChatApiTest::loadHistory(const unsigned int accountIndex, const MegaChatHandle chatid, TestChatRoomListener* chatroomListener)
{
    // first of all, ensure the chatd connection is ready
    bool *flagChatdOnline = &mChatConnectionOnline[accountIndex]; *flagChatdOnline = false;
    while (megaChatApi[accountIndex]->getChatConnectionState(chatid) != MegaChatApi::CHAT_CONNECTION_ONLINE)
    {
        postLog("Attempt to load history when still offline. Waiting for connection...");
        bool responseOk = waitForResponse(flagChatdOnline);
        EXPECT_TRUE(responseOk) << "Timeout expired for connecting to chatd";
        *flagChatdOnline = false;
        if (!responseOk) return MegaChatError::ERROR_TOOMANY;
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
        if (!responseOk) return MegaChatError::ERROR_ACCESS;
    }

    return chatroomListener->msgCount[accountIndex];
}

void MegaChatApiTest::makeContact(const unsigned int a1, const unsigned int a2)
{
    if (areContact(a1, a2)) { return; }
    const std::string contactRequestMessage = "Contact Request Message";
    bool* flagContactRequestUpdatedSecondary = &mContactRequestUpdated[a2];
    *flagContactRequestUpdatedSecondary = false;
    RequestTracker rtInvite(megaApi[a1]);
    // a1 sends contact request to a2
    megaApi[a1]->inviteContact(account(a2).getEmail().c_str(),
                               contactRequestMessage.c_str(),
                               MegaContactRequest::INVITE_ACTION_ADD,
                               &rtInvite);

    ASSERT_EQ(rtInvite.waitForResult(), API_OK) << "Error invite contact. Error: " << rtInvite.getErrorString();
    ASSERT_TRUE(waitForResponse(flagContactRequestUpdatedSecondary)) << "Expired timeout for receive contact request at a2";
    ASSERT_NO_FATAL_FAILURE({ getContactRequest(a2, false); });
    ASSERT_TRUE(mContactRequest[a2]) << "Contact request not received for a2";

    // a2 replies contact request
    bool* flagContactRequestUpdatedPrimary = &mContactRequestUpdated[a1];
    *flagContactRequestUpdatedPrimary = false;

    bool* usersUpdateA1 = &mUsersUpdate[a1]; *usersUpdateA1 = false;
    bool* usersUpdateA2 = &mUsersUpdate[a2]; *usersUpdateA2 = false;

    RequestTracker rtReplyCR(megaApi[a2]);
    megaApi[a2]->replyContactRequest(mContactRequest[a2].get(), MegaContactRequest::REPLY_ACTION_ACCEPT, &rtReplyCR);
    ASSERT_EQ(rtReplyCR.waitForResult(), API_OK) << "Error reply contact request. Error: " << rtReplyCR.getErrorString();
    ASSERT_TRUE(waitForResponse(flagContactRequestUpdatedPrimary)) << "Expired timeout for receive contact request reply at a1";
    ASSERT_TRUE(waitForResponse(usersUpdateA1)) << "Expired timeout for a2 as contact at a1";
    ASSERT_TRUE(waitForResponse(usersUpdateA2)) << "Expired timeout for a1 as contact at a2";
}

bool MegaChatApiTest::areContact(unsigned int a1, unsigned int a2)
{
    const std::string a2Email {account(a2).getEmail()};
    LOG_debug << "areContact: " << account(a1).getEmail() << " (a1) and " << a2Email << " (a2)";
    std::unique_ptr<MegaUser> user2(megaApi[a1]->getContact(a2Email.c_str()));
    return user2 && user2->getVisibility() == MegaUser::VISIBILITY_VISIBLE;
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

bool MegaChatApiTest::addChatVideoListener(const unsigned int idx, const megachat::MegaChatHandle chatid)
{
#ifndef KARERE_DISABLE_WEBRTC
    auto res = mData.mMapLocalVideoListeners.emplace(idx, TestChatVideoListener());
    if (res.second)
    {
       TestChatVideoListener& vl = res.first->second;
       megaChatApi[idx]->addChatLocalVideoListener(chatid, &vl);
       return true;
    }
#else
    LOG_debug << "KARERE_DISABLE_WEBRTC is defined so you cannot use TestChatVideoListener";
#endif
    return false;
}

void MegaChatApiTest::cleanChatVideoListeners()
{
#ifndef KARERE_DISABLE_WEBRTC
    std::for_each(mData.mMapLocalVideoListeners.begin(), mData.mMapLocalVideoListeners.end(), [this](auto& it)
    {
        removeChatVideoListener(it.first, mData.mChatid, it.second);
    });
#else
    LOG_debug << "cleanChatVideoListeners: KARERE_DISABLE_WEBRTC is defined so there's no TestChatVideoListeners registered";
#endif
}

void MegaChatApiTest::logoutTestAccounts()
{
    std::for_each(mData.mSessions.begin(), mData.mSessions.end(), [this](const auto& it)
    {
        ASSERT_NO_FATAL_FAILURE( logout(it.first, true /*destroy session*/); );
    });
}

void MegaChatApiTest::closeOpenedChatrooms()
{
    std::for_each(mData.mChatroomListeners.begin(), mData.mChatroomListeners.end(), [this](const auto& it)
    {
        megaChatApi[it.first]->closeChatRoom(mData.mChatid, it.second.get());
    });
}

bool MegaChatApiTest::removeChatVideoListener(const unsigned int idx, const megachat::MegaChatHandle chatid, TestChatVideoListener& vl)
{
#ifndef KARERE_DISABLE_WEBRTC
    megaChatApi[idx]->removeChatLocalVideoListener(chatid, &vl);
    return true;
#else
    LOG_debug << "removeChatVideoListener: KARERE_DISABLE_WEBRTC is defined so you cannot use TestChatVideoListener";
    return false;
#endif
}

MegaChatHandle MegaChatApiTest::getGroupChatRoom()
{
    ChatroomCreationOptions& opt = mData.mChatOptions;
    return getGroupChatRoom(mData.getIdxVector(), opt.mChatPeerList.get(),
                            opt.mOpPriv, opt.mCreate, opt.mPublicChat,
                            opt.mMeetingRoom, opt.mWaitingRoom, opt.mSpeakRequest, opt.mSchedMeetingData.get());
}

void MegaChatApiTest::initLocalSchedMeeting(const MegaChatHandle chatId, const MegaChatHandle schedId, const std::string& timeZone,
                                            const std::string& title, const std::string& description, const MegaChatTimeStamp startDate,
                                            const MegaChatTimeStamp endDate, const MegaChatTimeStamp overrides, const MegaChatTimeStamp newStartDate,
                                            const MegaChatTimeStamp newEndDate, const bool cancelled, const bool newCancelled, const bool publicChat,
                                            const bool speakRequest, const bool waitingRoom, const bool openInvite, const bool isMeeting,
                                            const bool sendEmails, const int rulesFreq, const int rulesInterval, const MegaChatTimeStamp rulesUntil,
                                            const ::megachat::MegaChatPeerList* peerlist,
                                            const ::mega::MegaIntegerList* rulesByWeekDay,
                                            const ::mega::MegaIntegerList* rulesByMonthDay,
                                            const ::mega::MegaIntegerMap* rulesByMonthWeekDay)
{
    mData.mChatOptions.mSchedMeetingData.reset(new SchedMeetingData());
    SchedMeetingData* sm = mData.mChatOptions.mSchedMeetingData.get();
    sm->chatId          = chatId;
    sm->schedId         = schedId;
    sm->timeZone        = timeZone;
    sm->title           = title;
    sm->description     = description;
    sm->startDate       = startDate;
    sm->endDate         = endDate;
    sm->overrides       = overrides;
    sm->newStartDate    = newStartDate;
    sm->newEndDate      = newEndDate;
    sm->cancelled       = cancelled;
    sm->newCancelled    = newCancelled;
    sm->publicChat      = publicChat;
    sm->speakRequest    = speakRequest;
    sm->waitingRoom     = waitingRoom;
    sm->openInvite      = openInvite;
    sm->isMeeting       = isMeeting;
    sm->peerList.reset(peerlist ? peerlist->copy() : nullptr);

    // add sched meeting flags
    sm->flags.reset(megachat::MegaChatScheduledFlags::createInstance());
    sm->flags->setSendEmails(sendEmails);

    // add sched meeting rules
    sm->rules.reset(megachat::MegaChatScheduledRules::createInstance(rulesFreq, rulesInterval, rulesUntil,
                                                                     rulesByWeekDay,rulesByMonthDay,
                                                                     rulesByMonthWeekDay));
}

void MegaChatApiTest::clearTemporalVars()
{
    boolVars().cleanAll();
    handleVars().cleanAll();
}

void MegaChatApiTest::addHandleVar(const unsigned int i, const std::string& n, const MegaChatHandle val)
{
    // adds a handle var to mAuxHandles <var_name, MegaChatHandle>[NUM_ACCOUNTS]
    ASSERT_TRUE(handleVars().add(i, n, val)) << n << " couldn't be added to mAuxHandles for account " << std::to_string(i);
}

bool* MegaChatApiTest::addBoolVar(const unsigned int i, const std::string& n, const bool val)
{
    return boolVars().add(i, n, val);
}

void MegaChatApiTest::addBoolVarAndExitFlag(const unsigned int i, ExitBoolFlags &eF, const std::string& n, const bool val)
{
    // adds a boolean var to mAuxBool <var_name, boolVal>[NUM_ACCOUNTS]
    bool* f = addBoolVar(i, n, val);
    ASSERT_TRUE(f) << n << " couldn't be added to mAuxBool for account " << std::to_string(i);

    // add entry above, to temporal ExitBoolFlags <var_name(i), *boolVal>
    // we may want to preserve all mAuxBool entries, but store a subset of them in ExitBoolFlags,
    // used as an array of conditions that need to be met to exit from a waitForAction call
    //
    // (i) is added to var_name to differentiate this var from the other accounts.
    // i.e: { {CallReceived0, *boolVal0}, {CallReceived1, *boolVal1} }
    //
    ASSERT_TRUE(eF.add(n + std::to_string(i), f)) << n << " couldn't be added to eF for account " << std::to_string(i);
}

#ifndef KARERE_DISABLE_WEBRTC
void MegaChatApiTest::endChatCallTestCleanup(unsigned int performerIdx, ExitBoolFlags& eF, const MegaChatHandle chatId)
{
    std::unique_ptr<MegaChatCall> call(megaChatApi[performerIdx]->getChatCall(chatId));
    if (call)
    {
       std::string errMsg = "ending call for all participants from account " + std::to_string(performerIdx);
       LOG_debug << errMsg;
       EXPECT_NE(call->getCallId(), MEGACHAT_INVALID_HANDLE) << "endChatCall: Invalid callId";
       waitForAction (1,  /* just one attempt */
                     eF,
                     errMsg,
                     true /* wait for all exit flags */,
                     true /* reset flags */,
                     maxTimeout,
                     [this, performerIdx, callid = call->getCallId()]()
                     {
                         ChatRequestTracker crtEndCall(megaChatApi[performerIdx]);
                         megaChatApi[performerIdx]->endChatCall(callid, &crtEndCall);
                         EXPECT_EQ(crtEndCall.waitForResult(), MegaChatError::ERROR_OK)
                             << "endChatCall: Failed to end call. Error: " << crtEndCall.getErrorString();
                     });
    }
}

void MegaChatApiTest::startCallInChat(const unsigned int callerIdx, ExitBoolFlags& eF, const MegaChatHandle chatId,
                                      const bool enableVideo, const bool enableAudio, const bool notRinging)
{
    ASSERT_NO_FATAL_FAILURE({
        waitForAction (1,  /* just one attempt */
                      eF,
                      "starting chat call",
                      true /* wait for all exit flags */,
                      true /* reset flags */,
                      maxTimeout,
                      [this, &chatId, &notRinging, &enableVideo, &enableAudio, &callerIdx]()
                      {
                          ChatRequestTracker crtStartCall(megaChatApi[callerIdx]);
                          megaChatApi[callerIdx]->startCallInChat(chatId, enableVideo, enableAudio, notRinging, &crtStartCall);
                          ASSERT_EQ(crtStartCall.waitForResult(), MegaChatError::ERROR_OK)
                              << "Failed to start call. Error: " << crtStartCall.getErrorString();
                      });
    });
}

void MegaChatApiTest::rejectCall(const unsigned int performerIdx, ExitBoolFlags& eF, const MegaChatHandle chatId)
{
    std::unique_ptr<MegaChatCall> call(megaChatApi[performerIdx]->getChatCall(chatId));
    ASSERT_TRUE(call) << "Can't get call for " << std::to_string(performerIdx) <<". CallId: " << getChatIdStrB64(mData.mChatid);

    MegaChatHandle callId = call->getCallId();

    ASSERT_NO_FATAL_FAILURE({
        waitForAction (1,  /* just one attempt */
                      eF,
                      "rejecting chat call",
                      true /* wait for all exit flags */,
                      true /* reset flags */,
                      maxTimeout,
                      [this, &callId, &performerIdx]()
                      {
                          ChatRequestTracker crtRejectCall(megaChatApi[performerIdx]);
                          megaChatApi[performerIdx]->rejectCall(callId, &crtRejectCall);
                          ASSERT_EQ(crtRejectCall.waitForResult(), MegaChatError::ERROR_OK)
                              << "Failed to reject call. Error: " << crtRejectCall.getErrorString();
                      });
    });
}

void MegaChatApiTest::startWaitingRoomCall(const unsigned int callerIdx, ExitBoolFlags& eF, const MegaChatHandle chatid, const MegaChatHandle schedIdWr,
                                           const bool enableVideo, const bool enableAudio)
{
    ASSERT_NO_FATAL_FAILURE({
        waitForAction (1,  /* just one attempt */
                      eF,
                      "starting call in a chatroom with waiting room option enabled",
                      true /* wait for all exit flags */,
                      true /* reset flags */,
                      maxTimeout,
                      [this, &chatid, &schedIdWr, &enableVideo, &enableAudio, &callerIdx]()
                      {
                          ChatRequestTracker crtStartCall(megaChatApi[callerIdx]);
                          megaChatApi[callerIdx]->startMeetingInWaitingRoomChat(chatid, schedIdWr, enableVideo, enableAudio, &crtStartCall);
                          ASSERT_EQ(crtStartCall.waitForResult(), MegaChatError::ERROR_OK)
                              << "Failed to start call. Error: " << crtStartCall.getErrorString();
                      });
    });
}

void MegaChatApiTest::answerChatCall(unsigned int calleeIdx, ExitBoolFlags& eF, const MegaChatHandle chatid,
                                           const bool enableVideo, const bool enableAudio)
{
    ASSERT_NO_FATAL_FAILURE({
        waitForAction (1, /* just one attempt */
                      eF,
                      "answering chat call",
                      true /* wait for all exit flags*/,
                      true /*reset flags*/,
                      maxTimeout,
                      [this, &calleeIdx, &chatid, &enableVideo, &enableAudio]()
                      {
                          ChatRequestTracker crtAnswerCall(megaChatApi[calleeIdx]);
                          megaChatApi[calleeIdx]->answerChatCall(chatid, enableVideo, enableAudio, &crtAnswerCall);
                          ASSERT_EQ(crtAnswerCall.waitForResult(), MegaChatError::ERROR_OK)
                              << "Failed to answer call. Error: " << crtAnswerCall.getErrorString();
                      });
    });
}
#endif

void MegaChatApiTest::setChatTitle(const std::string& title, const unsigned int waitSecs)
{
    auto& crlisteners = mData.mChatroomListeners;
    std::for_each(crlisteners.begin(), crlisteners.end(), [this](const auto& it)
    {
        auto idx = it.first;
        // add flag to wait for onChatListItemUpdate(CHANGE_TYPE_TITLE)
        ASSERT_TRUE(addBoolVar(idx, "titleItemChanged", false /*val*/)) << "titleItemChanged couldn't be added for account "
                                                                          << std::to_string(idx);

        // add flag to wait for onChatRoomUpdate(CHANGE_TYPE_TITLE)
        ASSERT_TRUE(addBoolVar(idx, "titleChanged", false /*val*/)) << "titleChanged couldn't be added for account "
                                                                      << std::to_string(idx);
    });

    auto opIdx = getOpIdx();
    ChatRequestTracker crtSetTitle(megaChatApi[opIdx]);
    megaChatApi[opIdx]->setChatTitle(mData.mChatid, title.c_str(), &crtSetTitle);
    ASSERT_EQ(crtSetTitle.waitForResult(static_cast<int>(waitSecs)), MegaChatError::ERROR_OK)
                                                                    << "Failed to set chat title from account "
                                                                    << mData.mOpIdx <<". Error: "
                                                                    << crtSetTitle.getErrorString();

    std::for_each(crlisteners.begin(), crlisteners.end(), [this](const auto& it)
    {
        auto idx = it.first;
        bool* f1 = boolVars().getVar(idx, "titleItemChanged");
        ASSERT_TRUE(f1) << "titleItemChanged wait flag not found for account: " << idx;
        ASSERT_TRUE(waitForResponse(f1)) << "Timeout expired for receiving chat list item update";

        bool* f2 = boolVars().getVar(idx, "titleChanged");
        ASSERT_TRUE(f2) << "titleChanged wait flag not found for account: " << idx;
        ASSERT_TRUE(waitForResponse(f2)) << "Timeout expired for receiving chatroom update";
    });
};

MegaChatHandle MegaChatApiTest::getGroupChatRoom(const std::vector<unsigned int>& a, MegaChatPeerList* peers,
                                                 const int a1Priv, const bool create, const bool publicChat,
                                                 const bool meetingRoom, const bool waitingRoom, const bool speakRequest, SchedMeetingData* schedMeetingData)
{
    LOG_debug << "getGroupChatRoom: get a chatroom for test";
    static const std::string errBadParam = "getGroupChatRoom: Attempting to get a group chat for ";
    if (a.size() > NUM_ACCOUNTS)
    {
        LOG_err << errBadParam << "too many accounts. Current tests accept maximum of " << NUM_ACCOUNTS;
        return MEGACHAT_INVALID_HANDLE;
    }
    if (a.empty())
    {
        LOG_err << errBadParam << " no clients/users provided. At least group chat creator is required";
        return MEGACHAT_INVALID_HANDLE;
    }
    if (!peers)
    {
        LOG_err << errBadParam << "an empty list of peers";
        return MEGACHAT_INVALID_HANDLE;
    }
    if (a.size() != static_cast<std::size_t>(peers->size() + 1)) // a1 (AKA perfomer) not included in peers list
    {
        LOG_err << errBadParam << "different test accounts (" << a.size() << ") and peers total (" << peers->size() << ")";
        return MEGACHAT_INVALID_HANDLE;
    }
    for (std::vector<unsigned int>::size_type i = 1; i < a.size(); ++i)
    {
        const auto& currentA = a[i];
        for (std::vector<unsigned int>::size_type j = i + 1; j < a.size(); ++j)
        {
            const auto& followerA = a[j];
            if (!areContact(currentA, followerA))
            {
                LOG_err << errBadParam << " accounts " << account(currentA).getEmail() << " (" << currentA + 1 << ") and "
                        << account(followerA).getEmail() << " (" << followerA + 1 << ") are not contact";
                return MEGACHAT_INVALID_HANDLE;
            }
        }
    }

    auto hasValidSchedMeeting = [this](const MegaHandle chatid) -> bool
    {
        std::unique_ptr<MegaChatScheduledMeetingList> list(megaChatApi[0]->getScheduledMeetingsByChat(chatid));
        if (!list || list->size() != 1) { return false; } // just consider valid chatroom, those without childred scheduled meeting
        for (unsigned long i = 0; i < list->size(); i++)
        {
            const auto sm = list->at(i);
            if (sm && !sm->cancelled())
            {
                return true;
            }
        }
        return false;
    };

    auto waitForChatCreation = [this, &a, &hasValidSchedMeeting](ChatRequestTracker& crtCreateChat, const bool schedMeeting) -> MegaChatHandle
    {
        // wait for creator client's request to be finished
        if (crtCreateChat.waitForResult() != MegaChatError::ERROR_OK)
        {
            LOG_err << "getGroupChatRoom: Failed to create chatroom. Error: " << crtCreateChat.getErrorString();
            return MEGACHAT_INVALID_HANDLE;
        }

        MegaChatHandle createdChatid = crtCreateChat.getChatHandle();
        if (createdChatid == MEGACHAT_INVALID_HANDLE)
        {
            LOG_err << "getGroupChatRoom: Wrong chat id received as create chat request response";
            return MEGACHAT_INVALID_HANDLE;
        }
        unique_ptr<char[]> base64(::MegaApi::handleToBase64(createdChatid));
        LOG_debug << "getGroupChatRoom: New chat created, chatid: " << base64.get();

        // wait for chat joining confirmation
        for (std::vector<unsigned int>::size_type i = 0; i < a.size(); ++i)
        {
            bool done = a.size() == 1; // if there is only creator client
            do
            {
                bool* chatItemReceived = &chatItemUpdated[a[i]];
                if (!waitForResponse(chatItemReceived))
                {
                    LOG_err << "getGroupChatRoom: Expired timeout for receiving the new chat list item";
                    return MEGACHAT_INVALID_HANDLE;
                }
                *chatItemReceived = false; // possible race

                if (!done) // check we received the right chat notification in case it is not the chat creator
                {
                    std::unique_ptr<MegaChatListItem> chatItemCreated(megaChatApi[a[i]]->getChatListItem(createdChatid));
                    done = chatItemCreated && chatItemCreated->getChatId() == createdChatid;
                }
            } while (!done);
        }

        if (schedMeeting && !hasValidSchedMeeting(createdChatid))
        {
            LOG_err << "getGroupChatRoom: Created chatroom doesn't have a scheduled meeting associated as expected";
            return MEGACHAT_INVALID_HANDLE;
        }

        return createdChatid;
    };

    auto createChat =
        [this, &a, &peers, &waitingRoom, &meetingRoom, &publicChat, &speakRequest, &waitForChatCreation, schedMeetingData]() -> MegaChatHandle
    {
        std::for_each(std::begin(a), std::end(a), [this](const auto& ai)
        {
            chatItemUpdated[ai] = false;
            mChatConnectionOnline[ai] = false;
        });

        const auto& chatUserCreator = a[0];
        ChatRequestTracker crtCreateChat(megaChatApi[chatUserCreator]);
        const std::string title = "chat_" + std::to_string(m_time(nullptr));
        if (schedMeetingData)
        {
            LOG_debug << "getGroupChatRoom: Creating a chatroom with scheduled meeting associated";
            SchedMeetingData& d = *schedMeetingData;
            megaChatApi[chatUserCreator]->createChatroomAndSchedMeeting(d.peerList.get(), d.isMeeting, d.publicChat,
                                               d.title.c_str(), d.speakRequest, d.waitingRoom,
                                               d.openInvite, d.timeZone.c_str(), d.startDate, d.endDate,
                                               d.description.c_str(), d.flags.get(), d.rules.get(), nullptr /*attributes*/,
                                               &crtCreateChat);
        }
        else if (meetingRoom)
        {
            LOG_debug << "getGroupChatRoom: Creating a meetingroom";
            if (peers->size())
            {
                LOG_err << "there's no interface to create a Meeting room with more participants";
            }
            megaChatApi[chatUserCreator]->createMeeting(title.c_str(), speakRequest /*speakRequest*/, waitingRoom,
                                                        false /*openInvite*/, &crtCreateChat);
        }
        else if (publicChat)
        {
            LOG_debug << "getGroupChatRoom: Creating a public chat";
            megaChatApi[chatUserCreator]->createPublicChat(peers, title.c_str(), &crtCreateChat);
        }
        else
        {
            LOG_debug << "getGroupChatRoom: Creating a group chatroom";
            megaChatApi[chatUserCreator]->createChat(true, peers, &crtCreateChat);
        }

        return waitForChatCreation(crtCreateChat, schedMeetingData);
    };

    auto findChat =
        [this, &a, &peers, &a1Priv, &waitingRoom, &speakRequest, &meetingRoom, &publicChat, &schedMeeting = schedMeetingData, hasValidSchedMeeting]() -> MegaChatHandle
    {
        const auto isChatCandidate =
            [&peers, &a1Priv, &publicChat, &waitingRoom, &meetingRoom, &speakRequest, &schedMeeting, hasValidSchedMeeting](const MegaChatRoom* chat) -> bool
        {
            return !(!chat->isGroup() || !chat->isActive()
                    || (chat->isPublic() != publicChat)
                    || (chat->isWaitingRoom() != waitingRoom)
                    || (chat->isMeeting() != meetingRoom)
                    || (chat->isSpeakRequest() != speakRequest)
                    || (schedMeeting && !hasValidSchedMeeting(chat->getChatId()))
                    || (static_cast<int>(chat->getPeerCount()) != peers->size())
                    || (a1Priv != megachat::MegaChatPeerList::PRIV_UNKNOWN && a1Priv != chat->getOwnPrivilege()));
        };
        const auto inPeersParam = [&peers](const MegaChatHandle& ph) -> bool
        {
            bool found = false;
            for (int idx = 0; !found && idx < peers->size(); ++idx) found = peers->getPeerHandle(idx) == ph;
            return found;
        };
        const auto& chatUserCreator = a[0];
        std::unique_ptr<MegaChatRoomList> chats(megaChatApi[chatUserCreator]->getChatRooms());
        for (unsigned i = 0; i < chats->size(); ++i)
        {
            const MegaChatRoom* chat = chats->get(i);
            if (!isChatCandidate(chat)) continue;

            // all peers must be in this chat, otherwise this is not the chat we are looking for
            bool skip = false;
            for (unsigned int u = 0; u < chat->getPeerCount(); ++u)
            {
                skip = !inPeersParam(chat->getPeerHandle(u));
            }
            if (skip) continue;

            const auto foundChatid = chat->getChatId();
            unique_ptr<char[]> base64(::MegaApi::handleToBase64(foundChatid));
            LOG_debug << "getGroupChatRoom: existing chat found, chatid: " << base64.get();
            return foundChatid;
        }
        return MEGACHAT_INVALID_HANDLE;
    };

    MegaChatHandle targetChatid = findChat();
    if (targetChatid == MEGACHAT_INVALID_HANDLE && create)
    {
        targetChatid = createChat();
    }
    // wait for all clients to be connected to chatd for the chatroom
    if (targetChatid != MEGACHAT_INVALID_HANDLE)
    {
        const bool allConnected = std::all_of(std::begin(a), std::end(a), [this, &targetChatid](const auto& ai)
        {
            if (!megaChatApi[ai]) return false; // depends on FIXME@410 and reconsideration of chatToSkip feature
            while (megaChatApi[ai]->getChatConnectionState(targetChatid) != MegaChatApi::CHAT_CONNECTION_ONLINE)
            {
                LOG_debug << "getGroupChatRoom: waiting for connection to chatd for new chat with chatId "
                          << ::mega::toHandle(targetChatid) << " before proceeding with test for account "
                          << ai + 1 << ": " << account(ai).getEmail();
                bool* flagChatdOnline = &mChatConnectionOnline[ai];
                if (!waitForResponse(flagChatdOnline))
                {
                    LOG_err << "getGroupChatRoom: timeout expired for connecting to chatd after creation for account " << ai+1;
                    return false;
                }
                *flagChatdOnline = false;
            }
            return true;
        });

        if (!allConnected) return MEGACHAT_INVALID_HANDLE;
    }

    return targetChatid;
}

// create chatroom and scheduled meeting
void MegaChatApiTest::createChatroomAndSchedMeeting(MegaChatHandle& chatid, const unsigned int a1,
                                                    const unsigned int a2, const SchedMeetingData& smData)
{
    // reset sched meetings id and chatid to invalid handle
    mSchedIdUpdated[a1] = mSchedIdUpdated[a2] = MEGACHAT_INVALID_HANDLE;

    // create Meeting room and scheduled meeting
    ASSERT_NO_FATAL_FAILURE({
        waitForAction (1,
                      std::vector<bool *> { &mSchedMeetingUpdated[a1], &mSchedMeetingUpdated[a2], &chatItemUpdated[a2]},
                      std::vector<string> { "mChatSchedMeeting[a1]", "mChatSchedMeeting[a2]", "chatItemUpdated[a2]"},
                      "Creating meeting room and scheduled meeting from A",
                      true /* wait for all exit flags*/,
                      true /*reset flags*/,
                      maxTimeout,
                      [&api = megaChatApi[a1], &d = smData, &chatid]()
                      {
                          ChatRequestTracker crtCreateAndSchedule(api);
                          api->createChatroomAndSchedMeeting(d.peerList.get(), d.isMeeting, d.publicChat,
                                                             d.title.c_str(), d.speakRequest, d.waitingRoom,
                                                             d.openInvite, d.timeZone.c_str(), d.startDate, d.endDate,
                                                             d.description.c_str(), d.flags.get(), d.rules.get(), nullptr /*attributes*/,
                                                             &crtCreateAndSchedule);
                          ASSERT_EQ(crtCreateAndSchedule.waitForResult(), MegaChatError::ERROR_OK)
                              << "Failed to create chatroom and scheduled meeting. Error: " << crtCreateAndSchedule.getErrorString();
                          chatid = crtCreateAndSchedule.getChatHandle();
                          ASSERT_NE(chatid, MEGACHAT_INVALID_HANDLE) << "Invalid chatroom handle";
                      });
    });

    ASSERT_NE(mSchedIdUpdated[a1], MEGACHAT_INVALID_HANDLE) << "Scheduled meeting for primary account could not be created. chatId: " << getChatIdStrB64(chatid);
    ASSERT_NE(mSchedIdUpdated[a2], MEGACHAT_INVALID_HANDLE) << "Scheduled meeting for secondary account could not be created. chatId: " << getChatIdStrB64(chatid);
};

MegaChatHandle MegaChatApiTest::getPeerToPeerChatRoom(const unsigned int a1, const unsigned int a2)
{
    MegaChatHandle chatid0 = MEGACHAT_INVALID_HANDLE;
    std::unique_ptr<MegaUser> a1User(megaApi[a1]->getContact(account(a2).getEmail().c_str()));
    std::unique_ptr<MegaUser> a2User(megaApi[a2]->getContact(account(a1).getEmail().c_str()));
    EXPECT_TRUE(a1User && a2User) << "getPeerToPeerChatRoom: failed to get a1 and a2 users";
    if (!a1User || !a2User) { return MEGACHAT_INVALID_HANDLE; }

    std::unique_ptr<MegaChatRoom> a1Room(megaChatApi[a1]->getChatRoomByUser(a1User->getHandle()));
    if (!a1Room) // chat 1on1 doesn't exist yet --> create it
    {
        bool* chatCreated = &chatItemUpdated[a1]; *chatCreated = false;
        bool* chatReceived = &chatItemUpdated[a2]; *chatReceived = false;
        bool* flagChatdOnline1 = &mChatConnectionOnline[a1]; *flagChatdOnline1 = false;
        bool* flagChatdOnline2 = &mChatConnectionOnline[a2]; *flagChatdOnline2 = false;
        std::unique_ptr<MegaChatPeerList> peers(MegaChatPeerList::createInstance());
        peers->addPeer(a1User->getHandle(), MegaChatPeerList::PRIV_STANDARD);

        ChatRequestTracker crt(megaChatApi[a1]);
        megaChatApi[a1]->createChat(false /*group*/, peers.get(), &crt);
        auto result = crt.waitForResult();
        if (result != MegaChatError::ERROR_OK)
        {
            EXPECT_EQ(result, MegaChatError::ERROR_OK) << "getPeerToPeerChatRoom: failed to create new chatroom. Error: "
                                                       << crt.getErrorString();
            return MEGACHAT_INVALID_HANDLE;
        }

        bool responseOk = waitForResponse(chatCreated);
        if (!responseOk)
        {
            EXPECT_TRUE(responseOk) << "getPeerToPeerChatRoom: expired timeout for create new chatroom";
            return MEGACHAT_INVALID_HANDLE;
        }

        responseOk = waitForResponse(chatReceived);
        if (!responseOk)
        {
            EXPECT_TRUE(responseOk) << "getPeerToPeerChatRoom: expired timeout for create new chatroom";
            return MEGACHAT_INVALID_HANDLE;
        }

        a1Room.reset(megaChatApi[a1]->getChatRoomByUser(a1User->getHandle()));
        if (!a1Room)
        {
            EXPECT_TRUE(a1Room) << "getPeerToPeerChatRoom failed to get newly created chatroom with user "
                                << megaApi[a1]->userHandleToBase64(a1User->getHandle());
            return MEGACHAT_INVALID_HANDLE;
        }

        chatid0 = a1Room->getChatId();
        if (chatid0 == MEGACHAT_INVALID_HANDLE)
        {
            EXPECT_NE(chatid0, MEGACHAT_INVALID_HANDLE) << "getPeerToPeerChatRoom: invalid chatid";
            return MEGACHAT_INVALID_HANDLE;
        }

        // Wait until both accounts are connected to chatd
        while (megaChatApi[a1]->getChatConnectionState(chatid0) != MegaChatApi::CHAT_CONNECTION_ONLINE)
        {
            postLog("getPeerToPeerChatRoom: waiting for connection to chatd...");
            responseOk = waitForResponse(flagChatdOnline1);
            if (!responseOk)
            {
                EXPECT_TRUE(responseOk) << "getPeerToPeerChatRoom: timeout expired for connecting to chatd, "
                                           "account " << (a1+1);
                return MEGACHAT_INVALID_HANDLE;
            }
            *flagChatdOnline1 = false;
        }
        while (megaChatApi[a2]->getChatConnectionState(chatid0) != MegaChatApi::CHAT_CONNECTION_ONLINE)
        {
            postLog("getPeerToPeerChatRoom: waiting for connection to chatd...");
            responseOk = waitForResponse(flagChatdOnline2);
            if (!responseOk)
            {
                EXPECT_TRUE(waitForResponse(flagChatdOnline2)) << "getPeerToPeerChatRoom: timeout expired for "
                                                                  "connecting to chatd, account " << (a2+1);
                return MEGACHAT_INVALID_HANDLE;
            }
            *flagChatdOnline2 = false;
        }
    }
    else
    {
        // --> Ensure we are connected to chatd for the chatroom
        chatid0 = a1Room->getChatId();
        if (chatid0 == MEGACHAT_INVALID_HANDLE)
        {
            EXPECT_NE(chatid0, MEGACHAT_INVALID_HANDLE) << "getPeerToPeerChatRoom: invalid chatid";
            return MEGACHAT_INVALID_HANDLE;
        }

        if (megaChatApi[a1]->getChatConnectionState(chatid0) != MegaChatApi::CHAT_CONNECTION_ONLINE)
        {
            EXPECT_EQ(megaChatApi[a1]->getChatConnectionState(chatid0), MegaChatApi::CHAT_CONNECTION_ONLINE)
                << "getPeerToPeerChatRoom: not connected to chatd for account " << (a1+1) << ": " << account(a1).getEmail();
            return MEGACHAT_INVALID_HANDLE;
        }

        if (megaChatApi[a2]->getChatConnectionState(chatid0) != MegaChatApi::CHAT_CONNECTION_ONLINE)
        {
            EXPECT_EQ(megaChatApi[a2]->getChatConnectionState(chatid0), MegaChatApi::CHAT_CONNECTION_ONLINE) <<
                "getPeerToPeerChatRoom: not connected to chatd for account " << (a2+1) << ": " << account(a2).getEmail();
            return MEGACHAT_INVALID_HANDLE;
        }
    }

    std::unique_ptr<MegaChatRoom> a2Room(megaChatApi[a2]->getChatRoomByUser(a2User->getHandle()));
    if (!a2Room)
    {
        EXPECT_TRUE(a2Room) << "getPeerToPeerChatRoom failed to get newly created chatroom with user for a2 "
                            << megaApi[a1]->userHandleToBase64(a1User->getHandle());

        return MEGACHAT_INVALID_HANDLE;
    }

    MegaChatHandle chatid1 = a2Room->getChatId();
    if (chatid0 != chatid1)
    {
        EXPECT_EQ(chatid0, chatid1) << "getPeerToPeerChatRoom: chat identificator is different for account0 and account1.";
        return MEGACHAT_INVALID_HANDLE;
    }
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

    bool *flagConfirmed = &chatroomListener->msgConfirmed[a1]; *flagConfirmed = false;
    bool *flagReceived = &chatroomListener->msgReceived[a2]; *flagReceived = false;

    ChatRequestTracker crtAttach(megaChatApi[a1]);
    megaChatApi[a1]->attachNodes(chatid, megaNodeList, &crtAttach);
    auto result = crtAttach.waitForResult();
    EXPECT_EQ(result, MegaChatError::ERROR_OK) << "Failed to attach node. Error: " << crtAttach.getErrorString();
    if (result != MegaChatError::ERROR_OK) return nullptr;
    delete megaNodeList;

    bool responseOk = waitForResponse(flagConfirmed);
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
    bool *flagTruncatedPrimary = &chatroomListener->historyTruncated[a1]; *flagTruncatedPrimary = false;
    bool *flagTruncatedSecondary = &chatroomListener->historyTruncated[a2]; *flagTruncatedSecondary = false;
    bool *chatItemUpdated0 = &chatItemUpdated[a1]; *chatItemUpdated0 = false;
    bool *chatItemUpdated1 = &chatItemUpdated[a2]; *chatItemUpdated1 = false;
    ChatRequestTracker crtClearHist(megaChatApi[a1]);
    megaChatApi[a1]->clearChatHistory(chatid, &crtClearHist);
    ASSERT_EQ(crtClearHist.waitForResult(), MegaChatError::ERROR_OK) << "Failed to truncate history. Error: " << crtClearHist.getErrorString();
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
    bool *chatClosed = &chatItemClosed[accountIndex]; *chatClosed = false;
    ChatRequestTracker crtLeaveChat(megaChatApi[accountIndex]);
    megaChatApi[accountIndex]->leaveChat(chatid, &crtLeaveChat);
    TEST_LOG_ERROR(crtLeaveChat.waitForResult() == MegaChatError::ERROR_OK, "Failed to leave chatroom. Error: " + crtLeaveChat.getErrorString());
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
                                         MegaTransfer::COLLISION_CHECK_FINGERPRINT,
                                         MegaTransfer::COLLISION_RESOLUTION_OVERWRITE,
                                         false,    /*undelete*/
                                         this);
    EXPECT_TRUE(waitForResponse(&isNotTransferRunning(accountIndex))) << "Expired timeout for download file";
    return lastErrorTransfer[accountIndex] == API_OK;
}

bool MegaChatApiTest::importNode(int accountIndex, MegaNode *node, const string &targetName)
{
    mNodeCopiedHandle[accountIndex] = INVALID_HANDLE;
    megaApi[accountIndex]->authorizeNode(node);
    unique_ptr<MegaNode> parentNode(megaApi[accountIndex]->getRootNode());
    RequestTracker copyNodeTracker(megaApi[accountIndex]);
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
            mContactRequest[accountIndex].reset(crl->get(0)->copy());
        }
    }
    else
    {
        crl = megaApi[accountIndex]->getIncomingContactRequests();
        ASSERT_EQ(expectedSize, crl->size());

        if (expectedSize)
        {
            mContactRequest[accountIndex].reset(crl->get(0)->copy());
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

        RequestTracker removeTracker(megaApi[accountIndex]);
        megaApi[accountIndex]->remove(childrenNode, &removeTracker);
        int removeResult = removeTracker.waitForResult();
        TEST_LOG_ERROR((removeResult == API_OK), "Failed to remove node. Error: "
                       + std::to_string(removeResult) + ' ' + removeTracker.getErrorString());
    }

    delete children;
}

void MegaChatApiTest::clearAndLeaveChats(unsigned accountIndex, const vector<MegaChatHandle>& skipChats)
{
    std::unique_ptr<MegaChatRoomList> chatRooms(megaChatApi[accountIndex]->getChatRooms());

    for (unsigned int i = 0; i < chatRooms->size(); ++i)
    {
        const MegaChatRoom *chatroom = chatRooms->get(i);

        if (chatroom->isActive() && chatroom->getOwnPrivilege() == MegaChatRoom::PRIV_MODERATOR)
        {
            ChatRequestTracker crtClearHist(megaChatApi[accountIndex]);
            megaChatApi[accountIndex]->clearChatHistory(chatroom->getChatId(), &crtClearHist);
            TEST_LOG_ERROR(crtClearHist.waitForResult() == MegaChatError::ERROR_OK, "Failed to truncate history. Error: " + crtClearHist.getErrorString());
        }

        if (chatroom->isGroup() && chatroom->isActive() &&
            std::find(skipChats.begin(), skipChats.end(), chatroom->getChatId()) == skipChats.end())
        {
            leaveChat(accountIndex, chatroom->getChatId());
        }
    }
}

void MegaChatApiTest::removePendingContactRequest(unsigned int accountIndex)
{
    MegaContactRequestList *contactRequests = megaApi[accountIndex]->getOutgoingContactRequests();

    for (int i = 0; i < contactRequests->size(); i++)
    {
        MegaContactRequest *contactRequest = contactRequests->get(i);
        RequestTracker inviteContactTracker(megaApi[accountIndex]);
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
    RequestTracker setUserAttributeTracker(megaApi[accountIndex]);
    megaApi[accountIndex]->setUserAttribute(MegaApi::USER_ATTR_LASTNAME, lastName.c_str(),
                                            &setUserAttributeTracker);
    ASSERT_EQ(setUserAttributeTracker.waitForResult(), API_OK)
            << "Failed SDK request to change lastname. Error: " << setUserAttributeTracker.getErrorString();

    RequestTracker getUserAttributeTracker(megaApi[accountIndex]);
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

void MegaChatApiTest::inviteToChat (const unsigned int& a1, const unsigned int& a2, const megachat::MegaChatHandle& uh,
                                   const megachat::MegaChatHandle& chatid, const int privilege, std::shared_ptr<TestChatRoomListener>chatroomListener)
{
    bool* chatItemJoined0 = &chatItemUpdated[a1]; *chatItemJoined0 = false;
    bool* chatItemJoined1 = &chatItemUpdated[a2]; *chatItemJoined1 = false;
    bool* chatJoined0 = &chatroomListener->chatUpdated[a1]; *chatJoined0 = false;
    bool* chatJoined1 = &chatroomListener->chatUpdated[a2]; *chatJoined1 = false;
    bool* flagChatsUpdated1 = &mChatsUpdated[a2]; *flagChatsUpdated1 = false;
    mChatListUpdated[a2].clear();
    bool* mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
    MegaChatHandle* uhAction = &chatroomListener->uhAction[a1]; *uhAction = MEGACHAT_INVALID_HANDLE;
    int* priv = &chatroomListener->priv[a1]; *priv = MegaChatRoom::PRIV_UNKNOWN;
    ChatRequestTracker crtInvite(megaChatApi[a1]);
    megaChatApi[a1]->inviteToChat(chatid, uh, privilege, &crtInvite);
    ASSERT_EQ(crtInvite.waitForResult(), MegaChatError::ERROR_OK)
        << "Failed to invite user to chat. Error: " << crtInvite.getErrorString();
    ASSERT_TRUE(waitForResponse(chatItemJoined0)) << "Chat list item update for main account not received after " << maxTimeout << " seconds";
    ASSERT_TRUE(waitForResponse(chatItemJoined1)) << "Chat list item update for auxiliar account not received after " << maxTimeout << " seconds";
    ASSERT_TRUE(waitForResponse(chatJoined0)) << "Chatroom update for main account not received after " << maxTimeout << " seconds";
    ASSERT_TRUE(waitForResponse(mngMsgRecv)) << "Management message not received after " << maxTimeout << " seconds";
    ASSERT_EQ(*uhAction, uh) << "User handle from message doesn't match";
    ASSERT_EQ(*priv, MegaChatRoom::PRIV_UNKNOWN) << "Privilege is incorrect";    // the message doesn't report the new priv
    ASSERT_TRUE(waitForResponse(flagChatsUpdated1)) << "Failed to receive onChatsUpdate " << maxTimeout << " seconds";
    ASSERT_TRUE(isChatroomUpdated(a2, chatid)) << "Chatroom " << chatid << " is not included in onChatsUpdate";
    mChatListUpdated[a2].clear();
}

#ifndef KARERE_DISABLE_WEBRTC

void MegaChatApiTest::startChatCall(const MegaChatHandle chatid, const unsigned int performerIdx, const std::set<unsigned int> participants, const bool enableVideo, const bool enableAudio)
{
    std::string msg = "Account with index " + std::to_string(performerIdx) + " starts call";
    std::vector<bool *> exitFlags   { &mCallInProgress[performerIdx] };
    std::vector<string> exiFlagsStr { "mCallInProgress[" + std::to_string(performerIdx) + "]" };
    for (auto idx : participants)
    {
        exitFlags.emplace_back(&mCallReceivedRinging[idx]);
        exiFlagsStr.emplace_back("mCallReceivedRinging[" + std::to_string(idx) + "]");
    }

    // Start chat call
    ASSERT_NO_FATAL_FAILURE({
        waitForAction (1,
                      exitFlags,
                      exiFlagsStr,
                      msg.c_str(),
                      true /* wait for all exit flags*/,
                      true /* reset flags */,
                      maxTimeout,
                      [this, &chatid, &performerIdx, &enableVideo, &enableAudio]()
                      {
                          ChatRequestTracker crtCall(megaChatApi[performerIdx]);
                          megaChatApi[performerIdx]->startChatCall(chatid, enableVideo, enableAudio, &crtCall);
                          ASSERT_EQ(crtCall.waitForResult(), MegaChatError::ERROR_OK)
                              << "Failed to start call from account: " << performerIdx
                              << ". Error: " << crtCall.getErrorString();
                      });
    });
}

void MegaChatApiTest::answerChatCall(const MegaChatHandle chatid, const unsigned int performerIdx, const std::set<unsigned int> participants, const bool enableVideo, const bool enableAudio)
{
    std::string msg = "Account with index " + std::to_string(performerIdx) + " answers call";
    std::vector<bool *> exiFlags;
    std::vector<string> exiFlagsStr;
    for (auto idx : participants)
    {
        exiFlags.emplace_back(&mChatCallSessionStatusInProgress[idx]);
        exiFlagsStr.emplace_back("mChatCallSessionStatusInProgress[" + std::to_string(idx) + "]");
    }

    ASSERT_NO_FATAL_FAILURE({
        waitForAction (1,
                      exiFlags,
                      exiFlagsStr,
                      msg.c_str(),
                      true /* wait for all exit flags*/,
                      true /* reset flags */,
                      maxTimeout,
                      [this, &performerIdx, &chatid, &enableVideo, &enableAudio]()
                      {
                          ChatRequestTracker crtAnswerCall(megaChatApi[performerIdx]);
                          megaChatApi[performerIdx]->answerChatCall(chatid, enableVideo, enableAudio, &crtAnswerCall);
                          ASSERT_EQ(crtAnswerCall.waitForResult(), MegaChatError::ERROR_OK)
                              << "Failed to answer call from account: " << performerIdx
                              << ". Error: " << crtAnswerCall.getErrorString();
                      });
    });
}

bool* MegaChatApiTest::getChatCallStateFlag (unsigned int index, int state)
{
    switch (state)
    {
    case megachat::MegaChatCall::CALL_STATUS_INITIAL:     return &mCallWithIdReceived[index];
    case megachat::MegaChatCall::CALL_STATUS_CONNECTING:  return &mCallConnecting[index];
    case megachat::MegaChatCall::CALL_STATUS_IN_PROGRESS: return &mCallInProgress[index];
    default:                                              break;
    }

    ADD_FAILURE() << "Invalid account state " << state;
    return nullptr;
}

void MegaChatApiTest::resetTestChatCallState (unsigned int index, int state)
{
    bool* statusReceived = getChatCallStateFlag(index, state);
    if (statusReceived)    { *statusReceived = false; }
}

void MegaChatApiTest::waitForChatCallState(unsigned int index, int state)
{
    bool* statusReceived = getChatCallStateFlag(index, state);
    if (statusReceived)
    {
        ASSERT_TRUE(waitForResponse(statusReceived)) <<
            "Timeout expired for receiving call state: " << state <<
            " for account index [" << index << "]";
    }
}

void MegaChatApiTest::waitForCallAction (unsigned int pIdx, int maxAttempts, bool* exitFlag,  const char* errMsg, unsigned int timeout, std::function<void()>action)
{
    int retries = 0;
    std::string errStr = errMsg ? errMsg : "executing provided action";
    bool* callConnecting = getChatCallStateFlag(pIdx, megachat::MegaChatCall::CALL_STATUS_CONNECTING);
    while (!*exitFlag)
    {
        ASSERT_TRUE(action) << "waitForCallAction: no valid action provided";

        // reset call state flags to false before executing the required action
        resetTestChatCallState(pIdx, megachat::MegaChatCall::CALL_STATUS_CONNECTING);
        resetTestChatCallState(pIdx, megachat::MegaChatCall::CALL_STATUS_IN_PROGRESS);

        // execute custom user action and wait until exitFlag is set true, OR performer account gets disconnected from SFU for the target call
        action();
        ASSERT_TRUE(waitForMultiResponse(std::vector<bool *> { exitFlag, callConnecting }, false /*waitForAll*/, timeout)) << "Timeout expired for " << errStr;

        // if performer account gets disconnected from SFU for the target call, wait until reconnect and retry <action>
        if (*callConnecting)
        {
            ASSERT_LT(++retries, maxAttempts) << "Max attempts exceeded for " << errStr;
            ASSERT_NO_FATAL_FAILURE({ waitForChatCallState(pIdx, megachat::MegaChatCall::CALL_STATUS_IN_PROGRESS); });
        }
    }
}
#endif

void MegaChatApiTest::updateChatPermission (const unsigned int& a1, const unsigned int& a2, const MegaChatHandle& uh, const MegaChatHandle& chatid,
                                           const int privilege, std::shared_ptr<TestChatRoomListener>chatroomListener)
{
    // --> Change peer privileges to Moderator
    bool* peerUpdated0 = &peersUpdated[a1]; *peerUpdated0 = false;
    bool* peerUpdated1 = &peersUpdated[a2]; *peerUpdated1 = false;
    bool* mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
    MegaChatHandle* uhAction = &chatroomListener->uhAction[a1]; *uhAction = MEGACHAT_INVALID_HANDLE;
    int* priv = &chatroomListener->priv[a1]; *priv = MegaChatRoom::PRIV_UNKNOWN;
    ChatRequestTracker crtPerm(megaChatApi[a1]);
    megaChatApi[a1]->updateChatPermissions(chatid, uh, privilege, &crtPerm);
    ASSERT_EQ(crtPerm.waitForResult(), MegaChatError::ERROR_OK)
        << "Failed to update user permissions. Error: " << crtPerm.getErrorString();
    ASSERT_TRUE(waitForResponse(peerUpdated0)) << "Timeout expired for receiving peer update";
    ASSERT_TRUE(waitForResponse(peerUpdated1)) << "Timeout expired for receiving peer update";
    ASSERT_TRUE(waitForResponse(mngMsgRecv)) << "Timeout expired for receiving management message";
    ASSERT_EQ(*uhAction, uh) << "User handle from message doesn't match";
    ASSERT_EQ(*priv, privilege) << "Privilege is incorrect";
}

void MegaChatApiTest::updateSchedMeeting(const unsigned int a1, const unsigned int a2, const int expectedError, const SchedMeetingData& smData, const bool updateChatTitle)
{
    bool exitFlag = false;
    mSchedMeetingUpdated[a1] = mSchedMeetingUpdated[a2] = false;         // reset sched meetings updated flags
    mSchedIdUpdated[a1] = mSchedIdUpdated[a2] = MEGACHAT_INVALID_HANDLE; // reset sched meetings id's (do after assign vars above)

    // wait for onRequestFinish
    ASSERT_NO_FATAL_FAILURE({
        waitForAction (1,
                      std::vector<bool *> { &exitFlag },
                      std::vector<string> { "TYPE_UPDATE_SCHEDULED_MEETING[a1]"},
                      "Updating meeting room and scheduled meeting from A",
                      true /* wait for all exit flags*/,
                      true /*reset flags*/,
                      maxTimeout,
                      [&api = megaChatApi[a1], &d = smData, &expectedError, &updateChatTitle, &exitFlag]()
                      {
                          ChatRequestTracker crtUpdateMeeting(api);
                          api->updateScheduledMeeting(d.chatId, d.schedId, d.timeZone.c_str(), d.startDate, d.endDate, d.title.c_str(),
                                                      d.description.c_str(), d.cancelled, d.flags.get(), d.rules.get(),
                                                      updateChatTitle, &crtUpdateMeeting);

                          auto res = crtUpdateMeeting.waitForResult();
                          exitFlag = true;
                          ASSERT_EQ(res, expectedError)
                              << "Unexpected error when updating scheduled meeting. Error: " << crtUpdateMeeting.getErrorString();
                      });
    });

    if (expectedError != MegaChatError::ERROR_OK) { return; }

    // wait for onChatSchedMeetingUpdate (just in case expectedError is ERROR_OK)
    waitForMultiResponse(std::vector<bool *> {&mSchedMeetingUpdated[a1], &mSchedMeetingUpdated[a2]}, true, maxTimeout);
    ASSERT_NE(mSchedIdUpdated[a1], MEGACHAT_INVALID_HANDLE) << "Scheduled meeting for primary account could not be updated. scheduled meeting id: "
                                                            << getSchedIdStrB64(smData.schedId);

    ASSERT_NE(mSchedIdUpdated[a2], MEGACHAT_INVALID_HANDLE) << "Scheduled meeting for secondary account could not be updated. scheduled meeting id: "
                                                            << getSchedIdStrB64(smData.schedId);
}

void MegaChatApiTest::changeTitle(const unsigned int a1, TestChatRoomListener* chatroomListener, const megachat::MegaChatHandle chatid, const std::string& title)
{
    bool *titleItemChanged0 = &titleUpdated[a1]; *titleItemChanged0 = false;
    bool *titleChanged0 = &chatroomListener->titleUpdated[a1]; *titleChanged0 = false;
    bool *mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
    string *msgContent = &chatroomListener->content[a1]; *msgContent = "";
    ChatRequestTracker crtSetTitle(megaChatApi[a1]);
    megaChatApi[a1]->setChatTitle(chatid, title.c_str(), &crtSetTitle);
    ASSERT_EQ(crtSetTitle.waitForResult(), MegaChatError::ERROR_OK) << "Failed to set chat title. Error: " << crtSetTitle.getErrorString();
    ASSERT_TRUE(waitForResponse(titleItemChanged0)) << "Timeout expired for receiving chat list item update";
    ASSERT_TRUE(waitForResponse(titleChanged0)) << "Timeout expired for receiving chatroom update";
    ASSERT_TRUE(waitForResponse(mngMsgRecv)) << "Timeout expired for receiving management message";
    ASSERT_EQ(title, *msgContent) << "Title received doesn't match the title set";
}

void MegaChatApiTest::onRequestFinish(MegaApi *api, MegaRequest *request, MegaError *e)
{
    unsigned int apiIndex = getMegaApiIndex(api);
    ASSERT_NE(apiIndex, UINT_MAX) << "MegaChatApiTest::onRequestFinish(MegaApi *api, ...)";

    if (e->getErrorCode() == API_OK)
    {
        switch(request->getType())
        {
            case MegaRequest::TYPE_GET_ATTR_USER:
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
    ASSERT_NE(apiIndex, UINT_MAX) << "MegaChatApiTest::onChatsUpdate()";
    mChatsUpdated[apiIndex] = true;
    for (int i = 0; i < chats->size(); i++)
    {
         mChatListUpdated[apiIndex].emplace_back(chats->get(i)->getHandle());
    }
}

void MegaChatApiTest::onContactRequestsUpdate(MegaApi* api, MegaContactRequestList* /*requests*/)
{
    unsigned int apiIndex = getMegaApiIndex(api);
    ASSERT_NE(apiIndex, UINT_MAX) << "MegaChatApiTest::onContactRequestsUpdate()";

    mContactRequestUpdated[apiIndex] = true;
}

void MegaChatApiTest::onUsersUpdate(::mega::MegaApi* api, ::mega::MegaUserList* userList)
{
    if (!userList) return;

    unsigned int accountIndex = getMegaApiIndex(api);
    mUsersUpdate[accountIndex] = true;
    ASSERT_NE(accountIndex, UINT_MAX) << "MegaChatApiTest::onUsersUpdate()";
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

void MegaChatApiTest::onChatInitStateUpdate(MegaChatApi *api, int newState)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);
    ASSERT_NE(apiIndex, UINT_MAX) << "MegaChatApiTest::onChatInitStateUpdate()";

    initState[apiIndex] = newState;
    initStateChanged[apiIndex] = true;
}

void MegaChatApiTest::onChatListItemUpdate(MegaChatApi *api, MegaChatListItem *item)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);
    ASSERT_NE(apiIndex, UINT_MAX) << "MegaChatApiTest::onChatListItemUpdate()";

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
            boolVars().updateIfExists(apiIndex, "titleItemChanged", true);
            titleUpdated[apiIndex] = true;
        }
        if (item->hasChanged(MegaChatListItem::CHANGE_TYPE_ARCHIVE))
        {
            chatArchived[apiIndex] = true;
        }
        if (item->hasChanged(MegaChatListItem::CHANGE_TYPE_PREVIEW_CLOSED))
        {
            chatPreviewClosed[apiIndex] = true;
        }

        chatItemUpdated[apiIndex] = true;
    }
}

void MegaChatApiTest::onChatOnlineStatusUpdate(MegaChatApi* api, MegaChatHandle userhandle, int status, bool)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);
    ASSERT_NE(apiIndex, UINT_MAX) << "MegaChatApiTest::onChatOnlineStatusUpdate()";
    if (userhandle == megaChatApi[apiIndex]->getMyUserHandle())
    {
        mOnlineStatusUpdated[apiIndex] = true;
        mOnlineStatus[apiIndex] = status;
    }
}

void MegaChatApiTest::onChatPresenceConfigUpdate(MegaChatApi *api, MegaChatPresenceConfig */*config*/)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);
    ASSERT_NE(apiIndex, UINT_MAX) << "MegaChatApiTest::onChatPresenceConfigUpdate()";
    mPresenceConfigUpdated[apiIndex] = true;
}

void MegaChatApiTest::onChatConnectionStateUpdate(MegaChatApi *api, MegaChatHandle chatid, int state)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);
    ASSERT_NE(apiIndex, UINT_MAX) << "MegaChatApiTest::onChatConnectionStateUpdate()";
    mChatConnectionOnline[apiIndex] = (state == MegaChatApi::CHAT_CONNECTION_ONLINE);
    mLoggedInAllChats[apiIndex] = (state == MegaChatApi::CHAT_CONNECTION_ONLINE) && (chatid == MEGACHAT_INVALID_HANDLE);
}

void MegaChatApiTest::onTransferStart(MegaApi */*api*/, MegaTransfer */*transfer*/)
{

}

void MegaChatApiTest::onTransferFinish(MegaApi *api, MegaTransfer *transfer, MegaError *error)
{
    unsigned int apiIndex = getMegaApiIndex(api);
    ASSERT_NE(apiIndex, UINT_MAX) << "MegaChatApiTest::onTransferFinish()";

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
    ASSERT_NE(apiIndex, UINT_MAX) << "MegaChatApiTest::onChatCallUpdate()";

    if (call->hasChanged(MegaChatCall::CHANGE_TYPE_RINGING_STATUS))
    {
        if (api->getNumCalls() > 1)
        {
            // Hangup in coming call
            api->hangChatCall(call->getCallId());
        }

        if (mCallIdExpectedReceived[apiIndex] == MEGACHAT_INVALID_HANDLE
                || mCallIdExpectedReceived[apiIndex] == call->getCallId())
        {
            if (call->isRinging())
            {
                /* we are waiting to receive a ringing call for a specific callid, this could be util
                 * for those scenarios where we receive multiple onChatCallUpdate like a login */
                mCallReceivedRinging[apiIndex] = true;
                mChatIdRingInCall[apiIndex] = call->getChatid();
                mCallIdRingIn[apiIndex] = call->getCallId();
                boolVars().updateIfExists(apiIndex, "CallIsRinging", true);
            }
            else
            {
                mCallStopRinging[apiIndex] = true;
                mCallIdStopRingIn[apiIndex] = call->getCallId();
                mChatIdStopRingInCall[apiIndex] = call->getChatid();
                boolVars().updateIfExists(apiIndex, "CallStopsRinging", true);
            }
        }
    }

    if (call->hasChanged(MegaChatCall::CHANGE_TYPE_WR_USERS_ENTERED)
        || call->hasChanged(MegaChatCall::CHANGE_TYPE_WR_COMPOSITION))
    {
        boolVars().updateIfExists(apiIndex, "CallWrChanged", true);
        mCallWrChanged[apiIndex] = true;
    }

    if (call->hasChanged(MegaChatCall::CHANGE_TYPE_WR_ALLOW))
    {
         mCallWrAllow[apiIndex] = true;
    }

    if (call->hasChanged(MegaChatCall::CHANGE_TYPE_WR_DENY))
    {
         mCallWrDeny[apiIndex] = true;
    }

    if (call->hasChanged(MegaChatCall::CHANGE_TYPE_WR_USERS_ALLOW))
    {
        const ::mega::MegaHandleList* usersAllowWr = call->getHandleList();
        ASSERT_TRUE(usersAllowWr) << "Invalid allowed user Join list";
        for (unsigned int i = 0; i < usersAllowWr->size(); i++)
        {
            mUsersAllowJoin[apiIndex][usersAllowWr->get(i)] = true;
        }
    }

    if (call->hasChanged(MegaChatCall::CHANGE_TYPE_WR_USERS_DENY))
    {
        const ::mega::MegaHandleList* usersAllowWr = call->getHandleList();
        ASSERT_TRUE(usersAllowWr) << "Invalid allowed user Join list";
        for (unsigned int i = 0; i < usersAllowWr->size(); i++)
        {
            mUsersRejectJoin[apiIndex][usersAllowWr->get(i)] = true;
        }
    }

    if (call->hasChanged(MegaChatCall::CHANGE_TYPE_STATUS))
    {
        unsigned int apiIndex = getMegaChatApiIndex(api); // why is this needed again?
        ASSERT_NE(apiIndex, UINT_MAX) << "MegaChatApiTest::onChatCallUpdate() (2)";
        switch (call->getStatus())
        {
        case MegaChatCall::CALL_STATUS_INITIAL:
            if (mCallIdExpectedReceived[apiIndex] != MEGACHAT_INVALID_HANDLE
                    && mCallIdExpectedReceived[apiIndex] == call->getCallId())
            {
                /* we are waiting to receive a call status change (CALL_STATUS_INITIAL) generated in
                 * Call ctor, for a specific callid, this could be util for those scenarios where
                 * we receive multiple onChatCallUpdate like a login */
                mCallWithIdReceived[apiIndex] = true;
            }
            boolVars().updateIfExists(apiIndex, "CallReceived", true);
            mCallReceived[apiIndex] = true;
            break;

        case MegaChatCall::CALL_STATUS_IN_PROGRESS:
            handleVars().updateIfExists(apiIndex, "CallIdInProgress", call->getCallId());
            boolVars().updateIfExists(apiIndex, "CallInProgress", true);
            mCallInProgress[apiIndex] = true;
            mChatIdInProgressCall[apiIndex] = call->getChatid();
            break;

        case MegaChatCall::CALL_STATUS_JOINING:
            mCallIdJoining[apiIndex] = call->getCallId();
            break;

        case MegaChatCall::CALL_STATUS_TERMINATING_USER_PARTICIPATION:
        {
            mTerminationCode[apiIndex] = call->getTermCode();
            mCallLeft[apiIndex] = true;
            break;
        }

        case MegaChatCall::CALL_STATUS_DESTROYED:
            boolVars().updateIfExists(apiIndex, "CallDestroyed", true);
            mCallDestroyed[apiIndex] = true;
            break;

        case MegaChatCall::CALL_STATUS_CONNECTING:
            mCallConnecting[apiIndex] = true;
            break;

        case MegaChatCall::CALL_STATUS_WAITING_ROOM:
        {
            boolVars().updateIfExists(apiIndex, "CallWR", true);
            mCallWR[apiIndex] = true;
            break;
        }

        default:
            break;
        }
    }

    if (call->hasChanged(MegaChatCall::CHANGE_TYPE_LOCAL_AVFLAGS))
    {
        mChatCallAudioEnabled[apiIndex] = call->hasLocalAudio();
        mChatCallAudioDisabled[apiIndex] = !call->hasLocalAudio();
        mOwnFlagsChanged[apiIndex] = true;
    }

    if (call->hasChanged(MegaChatCall::CHANGE_TYPE_CALL_SPEAK))
    {
        mOwnSpeakStatusChanged[apiIndex] = true;
        mOwnSpeakStatus[apiIndex] = call->getSpeakerState();
    }

    if (call->hasChanged(MegaChatCall::CHANGE_TYPE_OWN_PERMISSIONS))
    {
        mOwnCallPermissionsChanged[apiIndex] = true;
    }

    LOG_debug << "On chat call change state ";
}

void MegaChatApiTest::onChatSessionUpdate(MegaChatApi* api, MegaChatHandle,
                                          MegaChatHandle, MegaChatSession *session)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);
    ASSERT_NE(apiIndex, UINT_MAX) << "MegaChatApiTest::onChatSessionUpdate()";
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
            mSessSpeakRequests[apiIndex][session->getClientid()] = session->hasPendingSpeakRequest();
            mSessSpeakReqRecv[apiIndex] = true;
            break;
        case MegaChatSession::CHANGE_TYPE_SESSION_ON_HOLD:
            mChatCallOnHold[apiIndex] = session->isOnHold();
            mChatCallOnHoldResumed[apiIndex] = !session->isOnHold();
            break;
        case MegaChatSession::CHANGE_TYPE_REMOTE_AVFLAGS:
            mChatCallAudioEnabled[apiIndex] = session->hasAudio();
            mChatCallAudioDisabled[apiIndex] = !session->hasAudio();
            break;
        case MegaChatSession::CHANGE_TYPE_SPEAK_PERMISSION:
            mSessSpeakPerm[apiIndex][session->getClientid()] = session->hasSpeakPermission();
            mSessSpeakPermChanged[apiIndex] = true;
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
    ASSERT_NE(apiIndex, UINT_MAX) << "MegaChatApiTest::onChatSchedMeetingUpdate()";
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
    ASSERT_NE(apiIndex, UINT_MAX) << "MegaChatApiTest::onSchedMeetingOccurrencesUpdate()";
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
    ASSERT_NE(apiIndex, UINT_MAX) << "TestChatRoomListener::onChatRoomUpdate()";

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
            t->boolVars().updateIfExists(apiIndex, "titleChanged", true);
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
        else if (chat->hasChanged(MegaChatRoom::CHANGE_TYPE_CHAT_MODE))
        {
            chatModeUpdated[apiIndex] = true;
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
    ASSERT_NE(apiIndex, UINT_MAX) << "TestChatRoomListener::onMessageLoaded()";

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
        // else if MegaChatMessage::TYPE_SCHED_MEETING => no changes required

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
    ASSERT_NE(apiIndex, UINT_MAX) << "TestChatRoomListener::onMessageReceived()";

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
    ASSERT_NE(apiIndex, UINT_MAX) << "TestChatRoomListener::onReactionUpdate()";
    reactionReceived[apiIndex] = true;
}

void TestChatRoomListener::onHistoryTruncatedByRetentionTime(MegaChatApi *api, MegaChatMessage *msg)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);
    ASSERT_NE(apiIndex, UINT_MAX) << "TestChatRoomListener::onHistoryTruncatedByRetentionTime()";
    mRetentionMessageHandle[apiIndex] = msg->getMsgId();
    retentionHistoryTruncated[apiIndex] = true;
}

void TestChatRoomListener::onMessageUpdate(MegaChatApi *api, MegaChatMessage *msg)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);
    ASSERT_NE(apiIndex, UINT_MAX) << "TestChatRoomListener::onMessageUpdate()";

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

TEST_F(MegaChatApiUnitaryTest, ParseUrl)
{
    LOG_info << "___TEST ParseUrl___";

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

    std::string url;
    for (const auto& testCase : checkUrls)
    {
        EXPECT_EQ(chatd::Message::hasUrl(testCase.first, url), testCase.second) << "Failed to parse " << testCase.first;
        // url could have some content even in failed cases, so ignore it
    }
}

#ifndef KARERE_DISABLE_WEBRTC
TEST_F(MegaChatApiUnitaryTest, SfuDataReception)
{
    LOG_info << "___TEST SfuDataReception___";

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

    for (const auto& testCase : checkCommands)
    {
        rapidjson::Document document;
        sfu::SfuConnection::SfuData outdata;
        EXPECT_TRUE(sfu::SfuConnection::parseSfuData(testCase.first.c_str(), document, outdata))
                << "[FAILED processing SFU command]: " << testCase.first << ". " << outdata.msg;

        if (outdata.notificationType == sfu::SfuConnection::SfuData::SFU_COMMAND)
        {
            bool commandProcSuccess = (commands.find(outdata.notification) != commands.end()
                    && commands[outdata.notification]->processCommand(document));
            EXPECT_EQ(commandProcSuccess, testCase.second)
                    << "[FAILED processing SFU command (notification)]: " << testCase.first << ". " << outdata.msg;
        }
        // else => SFU_WARN | SFU_ERROR | SFU_DENY
    }
}
#endif

#ifdef USE_CRYPTOPP
TEST_F(MegaChatApiUnitaryTest, EncryptMediaKeyWithEphemKey)
{
    LOG_info << "___TEST EncryptMediaKeyWithEphemKey___";

    std::string encryptedMediaKeyBin, decryptedMediaKeyBin;
    const std::string expEncryptedMediaKeyB64     = "IqVDFXcCDQKfazBoZxhNSjKMvk9eZYQISMYl_7S71K4";
    const std::vector<::mega::byte> mediaKeyBin   = { 60,181,43,125,112,4,248,203,228,50,177,231,232,185,172,194 };
    const std::vector<::mega::byte> ephemKeyBin   = { 129,216,111,114,44,70,116,227,184,43,159,102,5,134,9,84,125,16,221,217,31,4,37,11,89,137,120,133,205,7,141,247 };
    const std::string ephemeralkeyStr(ephemKeyBin.begin(), ephemKeyBin.end());
    const std::string mediaKeyStr(mediaKeyBin.begin(), mediaKeyBin.end());

    // Encrypt media key with ephemeral key
    ::mega::SymmCipher mSymCipher;
    bool encryptResult = mSymCipher.cbc_encrypt_with_key(mediaKeyStr, encryptedMediaKeyBin, reinterpret_cast<const unsigned char *>(ephemeralkeyStr.data()), ephemeralkeyStr.size(), nullptr);
    EXPECT_TRUE(encryptResult) << "Failed Media key cbc_encrypt";

    // Check encrypted key with expected one
    const std::string encryptedMediaKeyB64 = ::mega::Base64::btoa(encryptedMediaKeyBin);
    EXPECT_EQ(encryptedMediaKeyB64.compare(expEncryptedMediaKeyB64), 0) << "Expected encrypted key:" << expEncryptedMediaKeyB64 << " doesn't match with obtained: " << encryptedMediaKeyB64;

    // Decrypt media key with ephemeral key
    bool decryptResult = mSymCipher.cbc_decrypt_with_key(encryptedMediaKeyBin, decryptedMediaKeyBin, reinterpret_cast<const unsigned char*>(ephemeralkeyStr.data()), ephemeralkeyStr.size(), nullptr);
    EXPECT_TRUE(decryptResult) << "Failed Media key cbc_decrypt";

    // Check decrypted key with expected one
    EXPECT_EQ(decryptedMediaKeyBin.compare(mediaKeyStr), 0) << "Expected decrypted key: " << mediaKeyStr << " doesn't match with obtained: " << decryptedMediaKeyBin;
}
#endif

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

bool MockupCall::handleAnswerCommand(Cid_t, std::shared_ptr<sfu::Sdp>, uint64_t, std::vector<sfu::Peer>&, const std::map<Cid_t, std::string>&, const std::map<Cid_t, sfu::TrackDescriptor>&, const std::map<Cid_t, sfu::TrackDescriptor>&)
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

bool MockupCall::handleBye(const unsigned, const bool, const std::string&)
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

void MockupCall::onByeCommandSent()
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

bool MockupCall::handleHello(const Cid_t /*cid*/, const unsigned int /*nAudioTracks*/,
                             const std::set<karere::Id>& /*mods*/, const bool /*wr*/, const bool /*allowed*/,
                             const bool /*speakRequest*/, const sfu::WrUserList& /*wrUsers*/)
{
    return true;
}

bool MockupCall::handleWrDump(const sfu::WrUserList& /*users*/)
{
    return true;
}

bool MockupCall::handleWrEnter(const sfu::WrUserList& /*users*/)
{
    return true;
}

bool MockupCall::handleWrLeave(const karere::Id& /*user*/)
{
    return true;
}

bool MockupCall::handleWrAllow(const Cid_t& /*cid*/, const std::set<karere::Id>& /*mods*/)
{
    return true;
}

bool MockupCall::handleWrDeny(const std::set<karere::Id>& /*mods*/)
{
    return true;
}

bool MockupCall::handleWrUsersAllow(const std::set<karere::Id>& /*users*/)
{
    return true;
}

bool MockupCall::handleWrUsersDeny(const std::set<karere::Id>& /*users*/)
{
    return true;
}

bool MockupCall::handleMutedCommand(const unsigned /*av*/)
{
    return true;
}
#endif
