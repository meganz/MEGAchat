#include "sdk_test.h"

#include <megaapi.h>
#include "../../src/chatd.h"
#include "../../src/megachatapi.h"
#include "../../src/karereCommon.h" // for logging with karere facility

#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace mega;
using namespace megachat;
using namespace std;

const std::string MegaChatApiTest::DEFAULT_PATH = "../../tests/sdk_test/";
const std::string MegaChatApiTest::FILE_IMAGE_NAME = "logo.png";
const std::string MegaChatApiTest::PATH_IMAGE = "PATH_IMAGE";

const std::string MegaChatApiTest::LOCAL_PATH = "./tmp"; // no ending slash
const std::string MegaChatApiTest::REMOTE_PATH = "/";
const std::string MegaChatApiTest::DOWNLOAD_PATH = LOCAL_PATH + "/download/";

int main(int argc, char **argv)
{
    remove("test.log");
    MegaChatApiTest t;
    t.init();

    // Tests that requires a groupchat (start with public chat, converted into private)
    EXECUTE_TEST(t.TEST_PublicChatManagement(0, 1), "TEST Publicchat management");
    EXECUTE_TEST(t.TEST_GroupChatManagement(0, 1), "TEST Groupchat management");
    EXECUTE_TEST(t.TEST_Reactions(0, 1), "TEST Chat Reactions");
    EXECUTE_TEST(t.TEST_RetentionHistory(0, 1), "TEST Retention history");
    EXECUTE_TEST(t.TEST_ClearHistory(0, 1), "TEST Clear history");
    EXECUTE_TEST(t.TEST_GroupLastMessage(0, 1), "TEST Last message (group)");

    // Test using a 1on1 chat
    EXECUTE_TEST(t.TEST_SetOnlineStatus(0), "TEST Online status");
    EXECUTE_TEST(t.TEST_GetChatRoomsAndMessages(0), "TEST Load chatrooms & messages");
    EXECUTE_TEST(t.TEST_EditAndDeleteMessages(0, 1), "TEST Edit & delete messages");
    EXECUTE_TEST(t.TEST_SwitchAccounts(0, 1), "TEST Switch accounts");
    EXECUTE_TEST(t.TEST_ResumeSession(0), "TEST Resume session");
    EXECUTE_TEST(t.TEST_Attachment(0, 1), "TEST Attachments");
    EXECUTE_TEST(t.TEST_SendContact(0, 1), "TEST Send contact");
    EXECUTE_TEST(t.TEST_LastMessage(0, 1), "TEST Last message");
    EXECUTE_TEST(t.TEST_ChangeMyOwnName(0), "TEST Change my name");
    EXECUTE_TEST(t.TEST_RichLinkUserAttribute(0), "TEST Rich link user attributes");
    EXECUTE_TEST(t.TEST_SendRichLink(0, 1), "TEST Send Rich link");
    EXECUTE_TEST(t.TEST_SendGiphy(0, 1), "TEST Send Giphy");

#ifndef KARERE_DISABLE_WEBRTC
    EXECUTE_TEST(t.TEST_Calls(0, 1), "TEST Signalling calls");
#endif

    // The tests below are manual tests. They require the call to be answered from another client
//    EXECUTE_TEST(t.TEST_ManualCalls(0, 1), "TEST Manual Calls");
//    EXECUTE_TEST(t.TEST_ManualGroupCalls(0, <name_of_groupchat>), "TEST Manual Calls");

    // The test below is a manual test. It requires to stop the intenet conection
    //EXECUTE_TEST(t.TEST_OfflineMode(0), "TEST Offline mode");

    t.terminate();

    MegaChatApiUnitaryTest unitaryTest;
    std::cout << "[========] Unitary tests " << std::endl;
    unitaryTest.UNITARYTEST_ParseUrl();
    std::cout << "[========] End Unitary tests " << std::endl;

    return t.mFailedTests + unitaryTest.mFailedTests;
}

ChatTestException::ChatTestException(const std::string &file, int line, const std::string &msg)
    : mFile(file)
    , mLine(line)
    , mMsg(msg)
{
    mExceptionText = mFile + ":" + std::to_string(mLine) + ": Failure";
}

const char *ChatTestException::what() const throw()
{
    return mExceptionText.c_str();
}

const char *ChatTestException::msg() const throw()
{
    return !mMsg.empty() ? mMsg.c_str() : NULL;
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
    for (int i = 0; i < NUM_ACCOUNTS; i++)
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
        mail = mAccounts[accountIndex].getEmail();
        pwd = mAccounts[accountIndex].getPassword();
    }
    else
    {
        mail = email;
        pwd = password;
    }

    // 1. Initialize chat engine
    bool *flagInit = &initStateChanged[accountIndex]; *flagInit = false;
    int initializationState = megaChatApi[accountIndex]->init(session);
    ASSERT_CHAT_TEST(initializationState >= 0, "MegaChatApiImpl::init returns MegaChatApi::INIT_ERROR");
    MegaApi::removeLoggerObject(logger);
    ASSERT_CHAT_TEST(waitForResponse(flagInit), "Initialization failed");
    int initStateValue = initState[accountIndex];
    if (!session)
    {
        ASSERT_CHAT_TEST(initStateValue == MegaChatApi::INIT_WAITING_NEW_SESSION,
                         "Wrong chat initialization state. Expected: " + std::to_string(MegaChatApi::INIT_WAITING_NEW_SESSION) + "   Received: " + std::to_string(initStateValue));
    }
    else
    {
        ASSERT_CHAT_TEST(initStateValue == MegaChatApi::INIT_OFFLINE_SESSION,
                         "Wrong chat initialization state. Expected: " + std::to_string(MegaChatApi::INIT_OFFLINE_SESSION) + "   Received: " + std::to_string(initStateValue));
    }

    // 2. login
    bool *flagLogin = &requestFlags[accountIndex][MegaRequest::TYPE_LOGIN]; *flagLogin = false;
    session ? megaApi[accountIndex]->fastLogin(session) : megaApi[accountIndex]->login(mail.c_str(), pwd.c_str());
    ASSERT_CHAT_TEST(waitForResponse(flagLogin), "Login failed after" + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(!lastError[accountIndex], "Login failed. Error: " + std::to_string(lastError[accountIndex]));

    // 3. fetchnodes
    flagInit = &initStateChanged[accountIndex]; *flagInit = false;
    bool *flagRequestFectchNodes = &requestFlags[accountIndex][MegaRequest::TYPE_FETCH_NODES]; *flagRequestFectchNodes = false;
    megaApi[accountIndex]->fetchNodes();
    ASSERT_CHAT_TEST(waitForResponse(flagRequestFectchNodes), "Expired timeout for fetch nodes");
    ASSERT_CHAT_TEST(!lastError[accountIndex], "Error fetch nodes. Error: " + std::to_string(lastError[accountIndex]));
    // after fetchnodes, karere should be ready for offline, at least
    ASSERT_CHAT_TEST(waitForResponse(flagInit), "Expired timeout for change init state");
    initStateValue = initState[accountIndex];
    ASSERT_CHAT_TEST(initStateValue == MegaChatApi::INIT_ONLINE_SESSION,
                     "Wrong chat initialization state. Expected: " + std::to_string(MegaChatApi::INIT_ONLINE_SESSION) + "   Received: " + std::to_string(initStateValue));

    // 4. Connect to chat servers
    bool *flagRequestConnect = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_CONNECT]; *flagRequestConnect = false;
    bool *loggedInFlag = &mLoggedInAllChats[accountIndex]; *loggedInFlag = false;
    mChatConnectionOnline[accountIndex] = false;
    megaChatApi[accountIndex]->connect();
    ASSERT_CHAT_TEST(waitForResponse(flagRequestConnect), "Expired timeout for connect request");
    ASSERT_CHAT_TEST(!lastErrorChat[accountIndex], "Error connect to chat. Error: " + std::to_string(lastErrorChat[accountIndex]));

    // if there are chatrooms in this account, wait to be joined to all of them
    std::unique_ptr<MegaChatListItemList> items(megaChatApi[accountIndex]->getChatListItems());
    if (items->size())
    {
        ASSERT_CHAT_TEST(waitForResponse(loggedInFlag, 120), "Expired timeout for login to all chats in account '" + mail + "'. (DDOS protection triggered?)");
    }

    return megaApi[accountIndex]->dumpSession();
}

void MegaChatApiTest::logout(unsigned int accountIndex, bool closeSession)
{
    bool *flagRequestLogout = &requestFlags[accountIndex][MegaRequest::TYPE_LOGOUT]; *flagRequestLogout = false;
    bool *flagRequestLogoutChat = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_LOGOUT]; *flagRequestLogoutChat = false;
    closeSession ? megaApi[accountIndex]->logout() : megaApi[accountIndex]->localLogout();
    ASSERT_CHAT_TEST(waitForResponse(flagRequestLogout), "Expired timeout for logout from sdk");
    ASSERT_CHAT_TEST(!lastError[accountIndex] || lastError[accountIndex] == MegaError::API_ESID, "Error sdk logout. Error: " + std::to_string(lastError[accountIndex]));

    if (!closeSession)  // for closed session, karere automatically logs out itself
    {
        megaChatApi[accountIndex]->localLogout();

    }

    ASSERT_CHAT_TEST(waitForResponse(flagRequestLogoutChat), "Expired timeout for chat logout");
    ASSERT_CHAT_TEST(!lastErrorChat[accountIndex], "Error chat logout. Error: " + lastErrorMsgChat[accountIndex] + " (" + std::to_string(lastErrorChat[accountIndex]) + ")");
    MegaApi::addLoggerObject(logger);   // need to restore customized logger

}

void MegaChatApiTest::init()
{
    std::cout << "[========] Global test environment initialization" << endl;

    mOKTests = mFailedTests = 0;

    logger = new MegaLoggerTest("test.log");
    MegaApi::addLoggerObject(logger);
    MegaApi::setLogToConsole(false);    // already disabled by default
    MegaChatApi::setLoggerObject(logger);
    MegaChatApi::setLogToConsole(false);
    MegaChatApi::setCatchException(false);

    for (int i = 0; i < NUM_ACCOUNTS; i++)
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

        Account accountPrimary(email, pwd);
        mAccounts[i] = accountPrimary;
    }
}

void MegaChatApiTest::terminate()
{
    std::cout << "[==========] Global test environment termination" << endl; \

    std::cout << "[ PASSED ] " << mOKTests << " test/s." << endl;
    if (mFailedTests)
    {
        std::cout << "[ FAILED ] " << mFailedTests << " test/s, see above." << endl;
    }

    MegaApi::removeLoggerObject(logger);
    MegaChatApi::setLoggerObject(NULL);
    delete logger;  logger = NULL;
}

void MegaChatApiTest::SetUp()
{
    struct stat st = {0};
    if (stat(LOCAL_PATH.c_str(), &st) == -1)
    {
        mkdir(LOCAL_PATH.c_str(), 0700);
    }

    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        char path[1024];
        getcwd(path, sizeof path);
        megaApi[i] = new MegaApi(APPLICATION_KEY.c_str(), path, USER_AGENT_DESCRIPTION.c_str());
        megaApi[i]->setLogLevel(MegaApi::LOG_LEVEL_DEBUG);
        megaApi[i]->addListener(this);
        megaApi[i]->addRequestListener(this);

        megaChatApi[i] = new MegaChatApi(megaApi[i]);
        megaChatApi[i]->setLogLevel(MegaChatApi::LOG_LEVEL_DEBUG);
        megaChatApi[i]->addChatRequestListener(this);
        megaChatApi[i]->addChatListener(this);

#ifndef KARERE_DISABLE_WEBRTC
        megaChatApi[i]->addChatCallListener(this);
#endif

        // kill all sessions to ensure no interferences from other tests running in parallel
        bool *flagLogin = &requestFlags[i][MegaRequest::TYPE_LOGIN]; *flagLogin = false;
        megaApi[i]->login(mAccounts[i].getEmail().c_str(), mAccounts[i].getPassword().c_str());
        ASSERT_CHAT_TEST(waitForResponse(flagLogin), "Login failed in SetUp() after " + std::to_string(maxTimeout) + " seconds");
        ASSERT_CHAT_TEST(!lastError[i], "Login failed in SetUp(). Error: " + std::to_string(lastError[i]));
        bool *flagKillSessions = &requestFlags[i][MegaRequest::TYPE_KILL_SESSION]; *flagKillSessions = false;
        megaApi[i]->killSession(INVALID_HANDLE);
        ASSERT_CHAT_TEST(waitForResponse(flagKillSessions), "Kill sessions failed in SetUp() after " + std::to_string(maxTimeout) + " seconds");
        ASSERT_CHAT_TEST(!lastError[i], "Kill sessions failed in SetUp(). Error: " + std::to_string(lastError[i]));
        bool *flagLogout = &requestFlags[i][MegaRequest::TYPE_LOGOUT]; *flagLogout = false;
        megaApi[i]->logout();
        ASSERT_CHAT_TEST(waitForResponse(flagLogout), "Expired timeout for logout in SetUp()");
        ASSERT_CHAT_TEST(!lastError[i] || lastError[i] == MegaError::API_ESID, "Logout failed in SetUp(). Error: " + std::to_string(lastError[i]));

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
        lastError[i] = -1;
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
        mCallAnswered[i] = false;
        mCallDestroyed[i] = false;
        mChatIdRingInCall[i] = MEGACHAT_INVALID_HANDLE;
        mTerminationLocal[i]= false;
        mTerminationCode[i] = MegaChatCall::TERM_CODE_NOT_FINISHED;
        mChatIdInProgressCall[i] = MEGACHAT_INVALID_HANDLE;
        mCallIdRingIn[i] = MEGACHAT_INVALID_HANDLE;
        mCallIdRequestSent[i] = MEGACHAT_INVALID_HANDLE;
        mPeerIsRinging[i] = false;
        mVideoLocal[i] = false;
        mLocalVideoListener[i] = NULL;
        mRemoteVideoListener[i] = NULL;
#endif

        mChatFirstname = "";
        mChatLastname = "";
        mChatEmail = "";
    }
}

void MegaChatApiTest::TearDown()
{
    for (unsigned int i = 0; i < NUM_ACCOUNTS; i++)
    {
        if (megaChatApi[i])
        {
            if (megaChatApi[i]->getInitState() == MegaChatApi::INIT_ONLINE_SESSION ||
                    megaChatApi[i]->getInitState() == MegaChatApi::INIT_OFFLINE_SESSION )
            {
                int a2 = (i == 0) ? 1 : 0;  // FIXME: find solution for more than 2 accounts
                MegaChatHandle chatToSkip = MEGACHAT_INVALID_HANDLE;
                MegaChatHandle uh = megaChatApi[i]->getUserHandleByEmail(mAccounts[a2].getEmail().c_str());
                if (uh != MEGACHAT_INVALID_HANDLE)
                {
                    MegaChatPeerList *peers = MegaChatPeerList::createInstance();
                    peers->addPeer(uh, MegaChatPeerList::PRIV_STANDARD);
                    chatToSkip = getGroupChatRoom(i, a2, peers, false);
                    delete peers;
                    peers = NULL;
                }

                clearAndLeaveChats(i, chatToSkip);

                bool *flagRequestLogout = &requestFlagsChat[i][MegaChatRequest::TYPE_LOGOUT]; *flagRequestLogout = false;
                megaChatApi[i]->logout();
                TEST_LOG_ERROR(waitForResponse(flagRequestLogout), "Time out MegaChatApi logout");
                TEST_LOG_ERROR(!lastErrorChat[i], "Failed to logout from Chat. Error: " + lastErrorMsgChat[i] + " (" + std::to_string(lastErrorChat[i]) + ")");
                MegaApi::addLoggerObject(logger);   // need to restore customized logger
            }

#ifndef KARERE_DISABLE_WEBRTC
            megaChatApi[i]->removeChatCallListener(this);
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

                bool *flagRequestLogout = &requestFlags[i][MegaRequest::TYPE_LOGOUT]; *flagRequestLogout = false;
                megaApi[i]->logout();
                TEST_LOG_ERROR(waitForResponse(flagRequestLogout), "Time out MegaApi logout");
                TEST_LOG_ERROR(!lastError[i], "Failed to logout from SDK. Error: " + std::to_string(lastError[i]));
            }

            megaApi[i]->removeRequestListener(this);

            delete megaApi[i];
            megaApi[i] = NULL;
        }
    }

    purgeLocalTree(LOCAL_PATH);
}

void MegaChatApiTest::logoutAccounts(bool closeSession)
{
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        if (megaApi[i]->isLoggedIn())
        {
            logout(i, closeSession);
        }
    }
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
    logger->postLog(msg.c_str());
}

bool MegaChatApiTest::waitForResponse(bool *responseReceived, unsigned int timeout) const
{
    timeout *= 1000000; // convert to micro-seconds
    unsigned int tWaited = 0;    // microseconds
    bool connRetried = false;
    while(!(*responseReceived))
    {
        usleep(pollingT);

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


/**
 * @brief TEST_ResumeSession
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
bool MegaChatApiTest::TEST_ResumeSession(unsigned int accountIndex)
{
    // ___ Create a new session ___
    char *session = login(accountIndex);

    checkEmail(accountIndex);

    // Test for management of ESID:
    // (uncomment the following block)
//    {
//        bool *flag = &requestFlagsChat[0][MegaChatRequest::TYPE_LOGOUT]; *flag = false;
//        // ---> NOW close session remotely ---
//        sleep(30);
//        // and wait for forced logout of megachatapi due to ESID
//        ASSERT_CHAT_TEST(waitForResponse(flag), "");
//        session = login(0);
//    }

    // ___ Resume an existing session ___
    logout(accountIndex, false); // keeps session alive
    char *tmpSession = login(accountIndex, session);
    ASSERT_CHAT_TEST(!strcmp(session, tmpSession), "Bad session key");
    delete [] tmpSession;   tmpSession = NULL;

    checkEmail(accountIndex);

    // ___ Resume an existing session without karere cache ___
    // logout from SDK keeping cache
    bool *flagSdkLogout = &requestFlags[accountIndex][MegaRequest::TYPE_LOGOUT]; *flagSdkLogout = false;
    megaApi[accountIndex]->localLogout();
    ASSERT_CHAT_TEST(waitForResponse(flagSdkLogout), "Expired timeout for local sdk logout");
    ASSERT_CHAT_TEST(!lastError[accountIndex], "Error local sdk logout. Error: " + std::to_string(lastError[accountIndex]));
    // logout from Karere removing cache
    bool *flagChatLogout = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_LOGOUT]; *flagChatLogout = false;
    megaChatApi[accountIndex]->logout();
    ASSERT_CHAT_TEST(waitForResponse(flagChatLogout), "Expired timeout for chat logout");
    ASSERT_CHAT_TEST(!lastErrorChat[accountIndex], "Error chat logout. Error: " + lastErrorMsgChat[accountIndex] + " (" + std::to_string(lastErrorChat[accountIndex]) + ")");
    MegaApi::addLoggerObject(logger);   // need to restore customized logger
    // try to initialize chat engine with cache --> should fail
    ASSERT_CHAT_TEST(megaChatApi[accountIndex]->init(session) == MegaChatApi::INIT_NO_CACHE,
                     "Wrong chat initialization state. Expected " + std::to_string(MegaChatApi::INIT_NO_CACHE) + "   Received: " + std::to_string(megaChatApi[accountIndex]->init(session)));
    MegaApi::removeLoggerObject(logger);
    megaApi[accountIndex]->invalidateCache();

    // ___ Re-create Karere cache without login out from SDK___
    bool *flagInit = &initStateChanged[accountIndex]; *flagInit = false;
    // login in SDK
    bool *flagLogin = &requestFlags[accountIndex][MegaRequest::TYPE_LOGIN]; *flagLogin = false;
    session ? megaApi[accountIndex]->fastLogin(session) : megaApi[accountIndex]->login(mAccounts[accountIndex].getEmail().c_str(), mAccounts[accountIndex].getPassword().c_str());
    ASSERT_CHAT_TEST(waitForResponse(flagLogin), "Expired timeout for sdk fast login");
    ASSERT_CHAT_TEST(!lastError[accountIndex], "Error sdk fast login. Error: " + std::to_string(lastError[accountIndex]));
    // fetchnodes in SDK
    bool *flagFetchNodes = &requestFlags[accountIndex][MegaRequest::TYPE_FETCH_NODES]; *flagFetchNodes = false;
    megaApi[accountIndex]->fetchNodes();
    ASSERT_CHAT_TEST(waitForResponse(flagFetchNodes), "Expired timeout for fetch nodes");
    ASSERT_CHAT_TEST(!lastError[accountIndex], "Error fetch nodes. Error: " + std::to_string(lastError[accountIndex]));
    ASSERT_CHAT_TEST(waitForResponse(flagInit), "Expired timeout for change init state");
    int initStateValue = initState[accountIndex];
    ASSERT_CHAT_TEST(initStateValue == MegaChatApi::INIT_ONLINE_SESSION,
                     "Wrong chat initialization state. Expected: " + std::to_string(MegaChatApi::INIT_ONLINE_SESSION) + "   Received: " + std::to_string(initStateValue));

    // check there's a list of chats already available
    MegaChatListItemList *list = megaChatApi[accountIndex]->getChatListItems();
    ASSERT_CHAT_TEST(list->size(), "Chat list item is empty");
    delete list; list = NULL;

    // ___ Close session ___
    logout(accountIndex, true);
    delete [] session; session = NULL;

    // ___ Login with chat enabled, transition to disabled and back to enabled
    session = login(accountIndex);
    ASSERT_CHAT_TEST(session, "Empty session key");
    // fully disable chat: logout + remove logger + delete MegaChatApi instance
    flagChatLogout = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_LOGOUT]; *flagChatLogout = false;
    megaChatApi[accountIndex]->logout();
    ASSERT_CHAT_TEST(waitForResponse(flagChatLogout), "Expired timeout for megachat logout");
    ASSERT_CHAT_TEST(!lastErrorChat[accountIndex], "Error megachat logout. Error: " + lastErrorMsgChat[accountIndex] + " (" + std::to_string(lastErrorChat[accountIndex]) + ")");
    MegaApi::addLoggerObject(logger);   // need to restore customized logger
    delete megaChatApi[accountIndex];
    // create a new MegaChatApi instance
    megaChatApi[accountIndex] = new MegaChatApi(megaApi[accountIndex]);
    megaChatApi[accountIndex]->setLogLevel(MegaChatApi::LOG_LEVEL_DEBUG);
    megaChatApi[accountIndex]->addChatRequestListener(this);
    megaChatApi[accountIndex]->addChatListener(this);
    MegaChatApi::setLoggerObject(logger);
    // back to enabled: init + fetchnodes + connect
    ASSERT_CHAT_TEST(megaChatApi[accountIndex]->init(session) == MegaChatApi::INIT_NO_CACHE,
                     "Wrong chat initialization state. Expected: " + std::to_string(MegaChatApi::INIT_NO_CACHE) + "   Received: " + std::to_string(megaChatApi[accountIndex]->init(session)));

    MegaApi::removeLoggerObject(logger);
    flagInit = &initStateChanged[accountIndex]; *flagInit = false;
    flagFetchNodes = &requestFlags[accountIndex][MegaRequest::TYPE_FETCH_NODES]; *flagFetchNodes = false;
    megaApi[accountIndex]->fetchNodes();
    ASSERT_CHAT_TEST(waitForResponse(flagFetchNodes), "Expired timeout for fetch nodes");
    ASSERT_CHAT_TEST(!lastError[accountIndex], "Error fetch nodes. Error: " + std::to_string(lastError[accountIndex]));
    ASSERT_CHAT_TEST(waitForResponse(flagInit), "Expired timeout for change init state");
    initStateValue = initState[accountIndex];
    ASSERT_CHAT_TEST(initStateValue == MegaChatApi::INIT_ONLINE_SESSION,
                     "Wrong chat initialization state. Expected: " + std::to_string(MegaChatApi::INIT_ONLINE_SESSION) + "   Received: " + std::to_string(initStateValue));

    bool *flagConnect = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_CONNECT]; *flagConnect = false;
    megaChatApi[accountIndex]->connect();
    ASSERT_CHAT_TEST(waitForResponse(flagConnect), "Expired timeout for connect");
    ASSERT_CHAT_TEST(!lastErrorChat[accountIndex], "Error connect. Error: " + lastErrorMsgChat[accountIndex] + " (" + std::to_string(lastErrorChat[accountIndex]) + ")");
    // check there's a list of chats already available
    list = megaChatApi[accountIndex]->getChatListItems();
    ASSERT_CHAT_TEST(list->size(), "Chat list item is empty");
    delete list; list = NULL;
    // close session and remove cache
    logout(accountIndex, true);
    delete [] session; session = NULL;

    // ___ Login with chat disabled, transition to enabled ___
    // fully disable chat: remove logger + delete MegaChatApi instance
    delete megaChatApi[accountIndex];
    // create a new MegaChatApi instance
    megaChatApi[accountIndex] = new MegaChatApi(megaApi[accountIndex]);
    megaChatApi[accountIndex]->setLogLevel(MegaChatApi::LOG_LEVEL_DEBUG);
    megaChatApi[accountIndex]->addChatRequestListener(this);
    megaChatApi[accountIndex]->addChatListener(this);
    MegaChatApi::setLoggerObject(logger);
    // login in SDK
    flagLogin = &requestFlags[accountIndex][MegaRequest::TYPE_LOGIN]; *flagLogin = false;
    megaApi[accountIndex]->login(mAccounts[accountIndex].getEmail().c_str(), mAccounts[accountIndex].getPassword().c_str());
    ASSERT_CHAT_TEST(waitForResponse(flagLogin), "Expired timeout for fast login");
    ASSERT_CHAT_TEST(!lastError[accountIndex], "Error fast login. Error: " + std::to_string(lastError[accountIndex]));
    session = megaApi[accountIndex]->dumpSession();
    // fetchnodes in SDK
    flagFetchNodes = &requestFlags[accountIndex][MegaRequest::TYPE_FETCH_NODES]; *flagFetchNodes = false;
    megaApi[accountIndex]->fetchNodes();
    ASSERT_CHAT_TEST(waitForResponse(flagFetchNodes), "Expired timeout for fetch nodes");
    ASSERT_CHAT_TEST(!lastError[accountIndex], "Error fetch nodes. Error: " + std::to_string(lastError[accountIndex]));
    // init in Karere
    ASSERT_CHAT_TEST(megaChatApi[accountIndex]->init(session) == MegaChatApi::INIT_NO_CACHE,
                     "Bad Megachat state expected: " + std::to_string(MegaChatApi::INIT_NO_CACHE) + "   Received: " + std::to_string(megaChatApi[accountIndex]->init(session)));
    MegaApi::removeLoggerObject(logger);
    // full-fetchndoes in SDK to regenerate cache in Karere
    flagInit = &initStateChanged[accountIndex]; *flagInit = false;
    flagFetchNodes = &requestFlags[accountIndex][MegaRequest::TYPE_FETCH_NODES]; *flagFetchNodes = false;
    megaApi[accountIndex]->fetchNodes();
    ASSERT_CHAT_TEST(waitForResponse(flagFetchNodes), "Expired timeout for fetch nodes");
    ASSERT_CHAT_TEST(!lastError[accountIndex], "Error fetch nodes. Error: " + std::to_string(lastError[accountIndex]));
    ASSERT_CHAT_TEST(waitForResponse(flagInit), "Expired timeout for change init state");
    initStateValue = initState[accountIndex];
    ASSERT_CHAT_TEST(initStateValue == MegaChatApi::INIT_ONLINE_SESSION,
                     "Bad Megachat state expected: " + std::to_string(MegaChatApi::INIT_ONLINE_SESSION) + "   Received: " + std::to_string(initStateValue));

    // connect in Karere
    flagConnect = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_CONNECT]; *flagConnect = false;
    megaChatApi[accountIndex]->connect();
    ASSERT_CHAT_TEST(waitForResponse(flagConnect), "Expired timeout for connect");
    ASSERT_CHAT_TEST(!lastErrorChat[accountIndex], "Error connect. Error: " + lastErrorMsgChat[accountIndex] + " (" + std::to_string(lastErrorChat[accountIndex]) + ")");
    // check there's a list of chats already available
    list = megaChatApi[accountIndex]->getChatListItems();
    ASSERT_CHAT_TEST(list->size(), "Chat list item is empty");
    delete list;
    list = NULL;

    // ___ Test going into background, sleep and back to foreground ___
    for(int i = 0; i < 3; i++)
    {
        bool *flag = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_SET_BACKGROUND_STATUS]; *flag = false;
        megaChatApi[accountIndex]->setBackgroundStatus(true);
        ASSERT_CHAT_TEST(waitForResponse(flag), "Failed to set background status after " + std::to_string(maxTimeout) + " seconds");

        logger->postLog("========== Enter background status ================= ");
        sleep(15);

        flag = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_SET_BACKGROUND_STATUS]; *flag = false;
        megaChatApi[accountIndex]->setBackgroundStatus(false);
        ASSERT_CHAT_TEST(waitForResponse(flag), "Failed to set background status after " + std::to_string(maxTimeout) + " seconds");

        logger->postLog("========== Enter foreground status ================= ");
        sleep(5);
    }


//  MegaChatApi::disconnect() is now obsolete and always returns ERROR_EACCESS (is not allowed anymore)
//    // ___ Disconnect from chat server and reconnect ___
//    for (int i = 0; i < 5; i++)
//    {
//        int conState = megaChatApi[accountIndex]->getConnectionState();
//        ASSERT_CHAT_TEST(conState == MegaChatApi::CONNECTED, "Wrong connection state: " + std::to_string(conState));

//        bool *flagDisconnect = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_DISCONNECT]; *flagDisconnect = false;
//        megaChatApi[accountIndex]->disconnect();
//        ASSERT_CHAT_TEST(waitForResponse(flagDisconnect), "Expired timeout for disconnect");
//        ASSERT_CHAT_TEST(!lastErrorChat[accountIndex], "Error disconect. Error: " + lastErrorMsgChat[accountIndex] + " (" + std::to_string(lastErrorChat[accountIndex]) + ")");
//        conState = megaChatApi[accountIndex]->getConnectionState();
//        ASSERT_CHAT_TEST(conState == MegaChatApi::DISCONNECTED, "Wrong connection state: " + std::to_string(conState));

//        // reconnect
//        flagConnect = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_CONNECT]; *flagConnect = false;
//        megaChatApi[accountIndex]->connect();
//        ASSERT_CHAT_TEST(waitForResponse(flagConnect), "Expired timeout for connect");
//        ASSERT_CHAT_TEST(!lastErrorChat[accountIndex], "Error connect. Error: " + lastErrorMsgChat[accountIndex] + " (" + std::to_string(lastErrorChat[accountIndex]) + ")");
//        conState = megaChatApi[accountIndex]->getConnectionState();
//        ASSERT_CHAT_TEST(conState == MegaChatApi::CONNECTED, "Wrong connection state: " + std::to_string(conState));

//        // check there's a list of chats already available
//        list = megaChatApi[accountIndex]->getChatListItems();
//        ASSERT_CHAT_TEST(list->size(), "Chat list item is empty");
//        delete list;
//        list = NULL;
//    }

    delete [] session; session = NULL;
}

/**
 * @brief TEST_SetOnlineStatus
 *
 * This test does the following:
 *
 * - Login
 * - Set status busy
 *
 */
void MegaChatApiTest::TEST_SetOnlineStatus(unsigned int accountIndex)
{
    bool *flagPresence = &mPresenceConfigUpdated[accountIndex]; *flagPresence = false;

    char *sesion = login(accountIndex);

    ASSERT_CHAT_TEST(waitForResponse(flagPresence), "Presence config not received after " + std::to_string(maxTimeout) + " seconds");

    // Reset status to online before starting the test
    bool *flagStatus = &mOnlineStatusUpdated[accountIndex]; *flagStatus = false;
    bool *flag = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_SET_ONLINE_STATUS]; *flag = false;
    if (megaChatApi[accountIndex]->getPresenceConfig()->getOnlineStatus() != MegaChatApi::STATUS_ONLINE)
    {
        megaChatApi[accountIndex]->setOnlineStatus(MegaChatApi::STATUS_ONLINE);
        ASSERT_CHAT_TEST(waitForResponse(flag), "Failed to set online status after " + std::to_string(maxTimeout) + " seconds");
        ASSERT_CHAT_TEST(!lastErrorChat[accountIndex], "Failed to set online status. Error: " + lastErrorMsgChat[accountIndex] + " (" + std::to_string(lastErrorChat[accountIndex]) + ")");
    }

    flagPresence = &mPresenceConfigUpdated[accountIndex]; *flagPresence = false;
    flagStatus = &mOnlineStatusUpdated[accountIndex]; *flagStatus = false;
    flag = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_SET_ONLINE_STATUS]; *flag = false;
    megaChatApi[accountIndex]->setOnlineStatus(MegaChatApi::STATUS_BUSY);
    ASSERT_CHAT_TEST(waitForResponse(flag), "Failed to set online status after " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(!lastErrorChat[accountIndex], "Failed to set online status. Error: " + lastErrorMsgChat[accountIndex] + " (" + std::to_string(lastErrorChat[accountIndex]) + ")");
    ASSERT_CHAT_TEST(waitForResponse(flagPresence), "Presence config not received after " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(waitForResponse(flagStatus), "Online status not received after " + std::to_string(maxTimeout) + " seconds");

    // set online status
    flagStatus = &mOnlineStatusUpdated[accountIndex]; *flagStatus = false;
    flagPresence = &mPresenceConfigUpdated[accountIndex]; *flagPresence = false;
    flag = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_SET_ONLINE_STATUS]; *flag = false;
    megaChatApi[accountIndex]->setOnlineStatus(MegaChatApi::STATUS_ONLINE);
    ASSERT_CHAT_TEST(waitForResponse(flag), "Failed to set online status after " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(!lastErrorChat[accountIndex], "Failed to set online status. Error: " + lastErrorMsgChat[accountIndex] + " (" + std::to_string(lastErrorChat[accountIndex]) + ")");
    ASSERT_CHAT_TEST(waitForResponse(flagPresence), "Presence config not received after " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(waitForResponse(flagStatus), "Online status not received after " + std::to_string(maxTimeout) + " seconds");

    // Update autoway timeout to force to send values to the server
    int64_t autowayTimeout = 5;
    if (megaChatApi[accountIndex]->getPresenceConfig()->getAutoawayTimeout() == autowayTimeout)
    {
        autowayTimeout ++;
    }

    // enable auto-away with 5 seconds timeout
    flagPresence = &mPresenceConfigUpdated[accountIndex]; *flagPresence = false;
    megaChatApi[accountIndex]->setPresenceAutoaway(true, autowayTimeout);
    ASSERT_CHAT_TEST(waitForResponse(flagPresence), "Presence config not received after " + std::to_string(maxTimeout) + " seconds");

    // disable persist
    if (megaChatApi[accountIndex]->getPresenceConfig()->isPersist())
    {
        flagPresence = &mPresenceConfigUpdated[accountIndex]; *flagPresence = false;
        megaChatApi[accountIndex]->setPresencePersist(false);
        ASSERT_CHAT_TEST(waitForResponse(flagPresence), "Presence config not received after " + std::to_string(maxTimeout) + " seconds");
    }

    // Set signal activity true, signal activity to false is sent automatically by presenced client
    megaChatApi[accountIndex]->signalPresenceActivity();
    ASSERT_CHAT_TEST(!lastErrorChat[accountIndex], "Failed to set activity. Error: " + lastErrorMsgChat[accountIndex] + " (" + std::to_string(lastErrorChat[accountIndex]) + ")");

    // now wait for timeout to expire
    flagStatus = &mOnlineStatusUpdated[accountIndex]; *flagStatus = false;

    LOG_debug << "Going to sleep for longer than autoaway timeout";
    MegaChatPresenceConfig *config = megaChatApi[accountIndex]->getPresenceConfig();

    sleep(config->getAutoawayTimeout() + 12);   // +12 to ensure at least one heartbeat (every 10s), where the `USERACTIVE 0` is sent for transition to Away

    // and check the status is away
    ASSERT_CHAT_TEST(mOnlineStatus[accountIndex] == MegaChatApi::STATUS_AWAY,
                     "Online status didn't changed to away automatically after timeout");
    int onlineStatus = megaChatApi[accountIndex]->getOnlineStatus();
    ASSERT_CHAT_TEST(onlineStatus == MegaChatApi::STATUS_AWAY,
                     "Online status didn't changed to away automatically after timeout. Received: " + std::string(MegaChatRoom::statusToString(onlineStatus)));

    // now signal user's activity to become online again
    flagStatus = &mOnlineStatusUpdated[accountIndex]; *flagStatus = false;
    megaChatApi[accountIndex]->signalPresenceActivity();
    ASSERT_CHAT_TEST(waitForResponse(flagStatus), "Online status not received after " + std::to_string(maxTimeout) + " seconds");

    // and check the status is online
    ASSERT_CHAT_TEST(mOnlineStatus[accountIndex] == MegaChatApi::STATUS_ONLINE,
                     "Online status didn't changed to online from autoaway after signaling activity");
    onlineStatus = megaChatApi[accountIndex]->getOnlineStatus();
    ASSERT_CHAT_TEST(onlineStatus == MegaChatApi::STATUS_ONLINE,
                     "Online status didn't changed to online from autoaway after signaling activity. Received: " + std::string(MegaChatRoom::statusToString(onlineStatus)));


    delete [] sesion;
    sesion = NULL;
}

/**
 * @brief TEST_GetChatRoomsAndMessages
 *
 * This test does the following:
 *
 * - Print chatrooms information
 * - Load history from one chatroom
 * - Close chatroom
 * - Load history from cache
 *
 */
void MegaChatApiTest::TEST_GetChatRoomsAndMessages(unsigned int accountIndex)
{
    char *session = login(accountIndex);

    MegaChatRoomList *chats = megaChatApi[accountIndex]->getChatRooms();
    std::stringstream buffer;
    buffer << chats->size() << " chat/s received: " << endl;
    postLog(buffer.str());

    // Open chats and print history
    for (int i = 0; i < chats->size(); i++)
    {
        // Open a chatroom
        const MegaChatRoom *chatroom = chats->get(i);
        if (chatroom->isPublic())
        {
            continue;
        }

        MegaChatHandle chatid = chatroom->getChatId();
        TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
        ASSERT_CHAT_TEST(megaChatApi[accountIndex]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account " + std::to_string(accountIndex+1));

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
                ASSERT_CHAT_TEST(waitForResponse(flagNameReceived), "Failed to retrieve firstname after " + std::to_string(maxTimeout) + " seconds");
                ASSERT_CHAT_TEST(!lastErrorChat[accountIndex], "Failed to retrieve firstname. Error: " + lastErrorMsgChat[accountIndex] + " (" + std::to_string(lastErrorChat[accountIndex]) + ")");
                buffer << "Peer firstname (" << uh << "): " << mChatFirstname << " (len: " << mChatFirstname.length() << ")" << endl;

                flagNameReceived = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_GET_LASTNAME]; *flagNameReceived = false; mChatLastname = "";
                megaChatApi[0]->getUserLastname(uh, NULL);
                ASSERT_CHAT_TEST(waitForResponse(flagNameReceived), "Failed to retrieve lastname after " + std::to_string(maxTimeout) + " seconds");
                ASSERT_CHAT_TEST(!lastErrorChat[accountIndex], "Failed to retrieve lastname. Error: " + lastErrorMsgChat[accountIndex] + " (" + std::to_string(lastErrorChat[accountIndex]) + ")");
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
                    ASSERT_CHAT_TEST(waitForResponse(flagNameReceived), "Failed to retrieve email after " + std::to_string(maxTimeout) + " seconds");
                    ASSERT_CHAT_TEST(!lastErrorChat[accountIndex], "Failed to retrieve email. Error: " + lastErrorMsgChat[accountIndex] + " (" + std::to_string(lastErrorChat[accountIndex]) + ")");
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
        ASSERT_CHAT_TEST(megaChatApi[accountIndex]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account " + std::to_string(accountIndex+1));
        buffer << "Loading messages locally for chat " << chatroom->getTitle() << " (id: " << chatroom->getChatId() << ")" << endl;
        loadHistory(accountIndex, chatid, chatroomListener);

        // Close the chatroom
        megaChatApi[accountIndex]->closeChatRoom(chatid, chatroomListener);
        delete chatroomListener;

        delete chatroom;
        chatroom = NULL;
    }

    logger->postLog(buffer.str().c_str());

    delete [] session;
    session = NULL;
}

/**
 * @brief TEST_EditAndDeleteMessages
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
void MegaChatApiTest::TEST_EditAndDeleteMessages(unsigned int a1, unsigned int a2)
{
    char *primarySession = login(a1);
    char *secondarySession = login(a2);

    MegaUser *user = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    if (!user || user->getVisibility() != MegaUser::VISIBILITY_VISIBLE)
    {
        makeContact(a1, a2);
    }
    delete user;
    user = NULL;

    MegaChatHandle chatid = getPeerToPeerChatRoom(a1, a2);

    // 1. A sends a message to B while B has the chat opened.
    // --> check the confirmed in A, the received message in B, the delivered in A
    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_CHAT_TEST(megaChatApi[a1]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account " + std::to_string(a1+1));
    ASSERT_CHAT_TEST(megaChatApi[a2]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account " + std::to_string(a2+1));

    // Load some message to feed history
    loadHistory(a1, chatid, chatroomListener);
    loadHistory(a2, chatid, chatroomListener);

    std::string messageToSend = "HI " + mAccounts[a1].getEmail() + " - This is a testing message automatically sent to you";
    MegaChatMessage *msgSent = sendTextMessageOrUpdate(a1, a2, chatid, messageToSend, chatroomListener);

    // edit the message
    std::string messageToUpdate = "This is an edited message to " + mAccounts[a1].getEmail();
    MegaChatMessage *msgUpdated = sendTextMessageOrUpdate(a1, a2, chatid, messageToUpdate, chatroomListener, msgSent->getMsgId());
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
 * @brief TEST_GroupChatManagement
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
void MegaChatApiTest::TEST_GroupChatManagement(unsigned int a1, unsigned int a2)
{
    char *sessionPrimary = login(a1);
    char *sessionSecondary = login(a2);

    // Prepare peers, privileges...
    MegaUser *user = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    if (!user || (user->getVisibility() != MegaUser::VISIBILITY_VISIBLE))
    {
        makeContact(a1, a2);
        delete user;
        user = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    }

    MegaChatHandle uh = user->getHandle();
    delete user;
    user = NULL;

    MegaChatPeerList *peers = MegaChatPeerList::createInstance();
    peers->addPeer(uh, MegaChatPeerList::PRIV_STANDARD);

    MegaChatHandle chatid = getGroupChatRoom(a1, a2, peers);
    delete peers;
    peers = NULL;

    // --> Open chatroom
    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_CHAT_TEST(megaChatApi[a1]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account " + std::to_string(a1+1));
    ASSERT_CHAT_TEST(megaChatApi[a2]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account " + std::to_string(a2+1));

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
    ASSERT_CHAT_TEST(waitForResponse(flagRemoveFromChat), "Failed to remove peer from chatroom " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to remove peer from chatroom" + std::to_string(lastErrorChat[a1]));
    ASSERT_CHAT_TEST(waitForResponse(mngMsgRecv), "Failed to receive management message " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(*uhAction == uh, "User handle from message doesn't match");
    ASSERT_CHAT_TEST(*priv == MegaChatRoom::PRIV_RM, "Privilege is incorrect");
    ASSERT_CHAT_TEST(waitForResponse(flagChatsUpdated1), "Failed to receive onChatsUpdate " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(isChatroomUpdated(a2, chatid), "Chatroom " + std::to_string(chatid) + " is not included in onChatsUpdate");
    mChatListUpdated[a2].clear();

    MegaChatRoom *chatroom = megaChatApi[a2]->getChatRoom(chatid);
    ASSERT_CHAT_TEST(chatroom, "Cannot get chatroom for id" + std::to_string(chatid));
    ASSERT_CHAT_TEST(chatroom->getOwnPrivilege() == MegaChatRoom::PRIV_RM, "Invalid own privilege expected(PRIV_RM), current(" + std::to_string(chatroom->getOwnPrivilege())+ ")");
    delete chatroom;

    chatroom = megaChatApi[a1]->getChatRoom(chatid);
    ASSERT_CHAT_TEST(chatroom, "Cannot get chatroom for id" + std::to_string(chatid));
    ASSERT_CHAT_TEST(chatroom->getPeerCount() == 0, "Wrong number of peers in chatroom" + std::to_string(chatid));
    delete chatroom;

    ASSERT_CHAT_TEST(waitForResponse(chatItemLeft0), "Chat list item update not received for main account after " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(waitForResponse(chatItemLeft1), "Chat list item update not received for auxiliar account after " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(waitForResponse(chatItemClosed1), "Chat list item close notification for auxiliar account not received after " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(waitForResponse(chatLeft0), "Chat list item leave notification for main account not received after " + std::to_string(maxTimeout) + " seconds");

    ASSERT_CHAT_TEST(waitForResponse(chatLeft1), "Chat list item leave notification for auxiliar account not received after " + std::to_string(maxTimeout) + " seconds");
    chatroom = megaChatApi[a1]->getChatRoom(chatid);
    ASSERT_CHAT_TEST(chatroom, "Cannot get chatroom for id" + std::to_string(chatid));
    ASSERT_CHAT_TEST(chatroom->getPeerCount() == 0, "Wrong number of peers in chatroom" + std::to_string(chatid));
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
    ASSERT_CHAT_TEST(waitForResponse(flagInviteToChatRoom), "Failed to invite a new peer after " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to invite a new peer. Error: " + lastErrorMsgChat[a1] + " (" + std::to_string(lastErrorChat[a1]) + ")");
    ASSERT_CHAT_TEST(waitForResponse(chatItemJoined0), "Chat list item update for main account not received after " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(waitForResponse(chatItemJoined1), "Chat list item update for auxiliar account not received after " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(waitForResponse(chatJoined0), "Chatroom update for main account not received after " + std::to_string(maxTimeout) + " seconds");
//    ASSERT_CHAT_TEST(waitForResponse(chatJoined1), ""); --> account 1 haven't opened chat, won't receive callback
    ASSERT_CHAT_TEST(waitForResponse(mngMsgRecv), "Management message not received after " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(*uhAction == uh, "User handle from message doesn't match");
    ASSERT_CHAT_TEST(*priv == MegaChatRoom::PRIV_UNKNOWN, "Privilege is incorrect");    // the message doesn't report the new priv
    ASSERT_CHAT_TEST(waitForResponse(flagChatsUpdated1), "Failed to receive onChatsUpdate " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(isChatroomUpdated(a2, chatid), "Chatroom " + std::to_string(chatid) + " is not included in onChatsUpdate");
    mChatListUpdated[a2].clear();

    chatroom = megaChatApi[a2]->getChatRoom(chatid);
    ASSERT_CHAT_TEST(chatroom, "Cannot get chatroom for id" + std::to_string(chatid));
    ASSERT_CHAT_TEST(chatroom->getOwnPrivilege() == MegaChatRoom::PRIV_STANDARD, "Invalid own privilege expected(PRIV_RM), current(" + std::to_string(chatroom->getOwnPrivilege())+ ")");
    delete chatroom;

    chatroom = megaChatApi[a1]->getChatRoom(chatid);
    ASSERT_CHAT_TEST(chatroom, "Cannot get chatroom for id" + std::to_string(chatid));
    ASSERT_CHAT_TEST(chatroom->getPeerCount() == 1, "Wrong number of peers in chatroom" + std::to_string(chatid));
    delete chatroom;

    // since we were expulsed from chatroom, we need to open it again
    ASSERT_CHAT_TEST(megaChatApi[a2]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account " + std::to_string(a2+1));

    // invite again --> error
    flagInviteToChatRoom = &requestFlagsChat[a1][MegaChatRequest::TYPE_INVITE_TO_CHATROOM]; *flagInviteToChatRoom = false;
    megaChatApi[a1]->inviteToChat(chatid, uh, MegaChatPeerList::PRIV_STANDARD);
    ASSERT_CHAT_TEST(waitForResponse(flagInviteToChatRoom), "Failed to invite a new peer after " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(lastErrorChat[a1] == MegaChatError::ERROR_EXIST, "Invitation should have failed, but it succeed");

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
    ASSERT_CHAT_TEST(waitForResponse(flagChatRoomName), "Timeout expired for set chat title");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to set chat title. Error: " + lastErrorMsgChat[a1] + " (" + std::to_string(lastErrorChat[a1]) + ")");
    ASSERT_CHAT_TEST(waitForResponse(titleItemChanged0), "Timeout expired for receiving chat list item update");
    ASSERT_CHAT_TEST(waitForResponse(titleItemChanged1), "Timeout expired for receiving chat list item update");
    ASSERT_CHAT_TEST(waitForResponse(titleChanged0), "Timeout expired for receiving chatroom update");
    ASSERT_CHAT_TEST(waitForResponse(titleChanged1), "Timeout expired for receiving chatroom update");
    ASSERT_CHAT_TEST(waitForResponse(mngMsgRecv), "Timeout expired for receiving management message");
    ASSERT_CHAT_TEST(!strcmp(title.c_str(), msgContent->c_str()), "Title received doesn't match the title set");

    chatroom = megaChatApi[a2]->getChatRoom(chatid);
    ASSERT_CHAT_TEST(chatroom, "Cannot get chatroom for id" + std::to_string(chatid));
    ASSERT_CHAT_TEST(!strcmp(chatroom->getTitle(), title.c_str()), "Titles don't match");
    delete chatroom;    chatroom = NULL;

    // --> Change peer privileges to Moderator
    bool *flagUpdatePeerPermision = &requestFlagsChat[a1][MegaChatRequest::TYPE_UPDATE_PEER_PERMISSIONS]; *flagUpdatePeerPermision = false;
    bool *peerUpdated0 = &peersUpdated[a1]; *peerUpdated0 = false;
    bool *peerUpdated1 = &peersUpdated[a2]; *peerUpdated1 = false;
    mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
    uhAction = &chatroomListener->uhAction[a1]; *uhAction = MEGACHAT_INVALID_HANDLE;
    priv = &chatroomListener->priv[a1]; *priv = MegaChatRoom::PRIV_UNKNOWN;
    megaChatApi[a1]->updateChatPermissions(chatid, uh, MegaChatRoom::PRIV_MODERATOR);
    ASSERT_CHAT_TEST(waitForResponse(flagUpdatePeerPermision), "Timeout expired for update privilege of peer");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to update privilege of peer Error: " + lastErrorMsgChat[a1] + " (" + std::to_string(lastErrorChat[a1]) + ")");;
    ASSERT_CHAT_TEST(waitForResponse(peerUpdated0), "Timeout expired for receiving peer update");
    ASSERT_CHAT_TEST(waitForResponse(peerUpdated1), "Timeout expired for receiving peer update");
    ASSERT_CHAT_TEST(waitForResponse(mngMsgRecv), "Timeout expired for receiving management message");
    ASSERT_CHAT_TEST(*uhAction == uh, "User handle from message doesn't match");
    ASSERT_CHAT_TEST(*priv == MegaChatRoom::PRIV_MODERATOR, "Privilege is incorrect");

    // --> Change peer privileges to Read-only
    flagUpdatePeerPermision = &requestFlagsChat[a1][MegaChatRequest::TYPE_UPDATE_PEER_PERMISSIONS]; *flagUpdatePeerPermision = false;
    peerUpdated0 = &peersUpdated[a1]; *peerUpdated0 = false;
    peerUpdated1 = &peersUpdated[a2]; *peerUpdated1 = false;
    mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
    uhAction = &chatroomListener->uhAction[a1]; *uhAction = MEGACHAT_INVALID_HANDLE;
    priv = &chatroomListener->priv[a1]; *priv = MegaChatRoom::PRIV_UNKNOWN;
    megaChatApi[a1]->updateChatPermissions(chatid, uh, MegaChatRoom::PRIV_RO);
    ASSERT_CHAT_TEST(waitForResponse(flagUpdatePeerPermision), "Timeout expired for update privilege of peer");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to update privilege of peer Error: " + lastErrorMsgChat[a1] + " (" + std::to_string(lastErrorChat[a1]) + ")");
    ASSERT_CHAT_TEST(waitForResponse(peerUpdated0), "Timeout expired for receiving peer update");
    ASSERT_CHAT_TEST(waitForResponse(peerUpdated1), "Timeout expired for receiving peer update");
    ASSERT_CHAT_TEST(waitForResponse(mngMsgRecv), "Timeout expired for receiving management message");
    ASSERT_CHAT_TEST(*uhAction == uh, "User handle from message doesn't match");
    ASSERT_CHAT_TEST(*priv == MegaChatRoom::PRIV_RO, "Privilege is incorrect");

    // --> Try to send a message without the right privilege
    string msg1 = "HI " + mAccounts[a1].getEmail()+ " - This message can't be send because I'm read-only";
    bool *flagRejected = &chatroomListener->msgRejected[a2]; *flagRejected = false;
    chatroomListener->clearMessages(a2);   // will be set at reception
    MegaChatMessage *msgSent = megaChatApi[a2]->sendMessage(chatid, msg1.c_str());
    ASSERT_CHAT_TEST(msgSent, "Succeed to send message, when it should fail");
    delete msgSent; msgSent = NULL;
    ASSERT_CHAT_TEST(waitForResponse(flagRejected), "Timeout expired for rejection of message");    // for confirmation, sendMessage() is synchronous
    ASSERT_CHAT_TEST(chatroomListener->mConfirmedMessageHandle[a2] == MEGACHAT_INVALID_HANDLE, "Message confirmed, when it should fail");

    // --> Load some message to feed history
    loadHistory(a1, chatid, chatroomListener);
    loadHistory(a2, chatid, chatroomListener);

    // --> Send typing notification
    bool *flagTyping1 = &chatroomListener->userTyping[a2]; *flagTyping1 = false;
    uhAction = &chatroomListener->uhAction[a2]; *uhAction = MEGACHAT_INVALID_HANDLE;
    megaChatApi[a1]->sendTypingNotification(chatid);
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to send user typing: " + lastErrorMsgChat[a1] + " (" + std::to_string(lastErrorChat[a1]) + ")");
    ASSERT_CHAT_TEST(waitForResponse(flagTyping1), "Timeout expired for sending typing notification");
    ASSERT_CHAT_TEST(*uhAction == megaChatApi[a1]->getMyUserHandle(), "My user handle is wrong at typing");

    // --> Send stop typing notification
    flagTyping1 = &chatroomListener->userTyping[a2]; *flagTyping1 = false;
    uhAction = &chatroomListener->uhAction[a2]; *uhAction = MEGACHAT_INVALID_HANDLE;
    megaChatApi[a1]->sendStopTypingNotification(chatid);
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to send user has stopped typing: " + lastErrorMsgChat[a1] + " (" + std::to_string(lastErrorChat[a1]) + ")");
    ASSERT_CHAT_TEST(waitForResponse(flagTyping1), "Timeout expired for sending stop typing notification");
    ASSERT_CHAT_TEST(*uhAction == megaChatApi[a1]->getMyUserHandle(), "My user handle is wrong at stop typing");

    // --> Archive the chatroom
    chatroom = megaChatApi[a1]->getChatRoom(chatid);
    delete chatroom; chatroom = NULL;
    bool *flagChatArchived = &requestFlagsChat[a1][MegaChatRequest::TYPE_ARCHIVE_CHATROOM]; *flagChatArchived = false;
    bool *chatArchiveChanged = &chatArchived[a1]; *chatArchiveChanged = false;
    bool *chatroomArchiveChanged = &chatroomListener->archiveUpdated[a1]; *chatroomArchiveChanged = false;
    megaChatApi[a1]->archiveChat(chatid, true);
    ASSERT_CHAT_TEST(waitForResponse(flagChatArchived), "Timeout expired for archiving chat");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to archive chat. Error: " + lastErrorMsgChat[a1] + " (" + std::to_string(lastErrorChat[a1]) + ")");
    ASSERT_CHAT_TEST(waitForResponse(chatArchiveChanged), "Timeout expired for receiving chat list item update about archive");
    ASSERT_CHAT_TEST(waitForResponse(chatroomArchiveChanged), "Timeout expired for receiving chatroom update about archive (This time out is usually produced by missing api notification)");
    chatroom = megaChatApi[a1]->getChatRoom(chatid);
    ASSERT_CHAT_TEST(chatroom->isArchived(), "Chatroom is not archived when it should");
    delete chatroom; chatroom = NULL;

    // TODO: Redmine ticket: #10596
    {
        // give some margin to API-chatd synchronization, so chatd knows the room is archived and needs
        // to be unarchived upon new message
        sleep(3);
    }

    // --> Send a message and wait for reception by target user
    string msg0 = "HI " + mAccounts[a1].getEmail() + " - Testing groupchats";
    bool *msgConfirmed = &chatroomListener->msgConfirmed[a1]; *msgConfirmed = false;
    bool *msgReceived = &chatroomListener->msgReceived[a2]; *msgReceived = false;
    bool *msgDelivered = &chatroomListener->msgDelivered[a1]; *msgDelivered = false;
    chatArchiveChanged = &chatArchived[a1]; *chatArchiveChanged = false;
    chatroomArchiveChanged = &chatroomListener->archiveUpdated[a1]; *chatroomArchiveChanged = false;
    chatroomListener->clearMessages(a1);
    chatroomListener->clearMessages(a2);
    MegaChatMessage *messageSent = megaChatApi[a1]->sendMessage(chatid, msg0.c_str());
    ASSERT_CHAT_TEST(waitForResponse(msgConfirmed), "Timeout expired for receiving confirmation by server");    // for confirmation, sendMessage() is synchronous
    MegaChatHandle msgId = chatroomListener->mConfirmedMessageHandle[a1];
    ASSERT_CHAT_TEST(chatroomListener->hasArrivedMessage(a1, msgId), "Message not received");
    ASSERT_CHAT_TEST(msgId != MEGACHAT_INVALID_HANDLE, "Wrong message id at origin");
    ASSERT_CHAT_TEST(waitForResponse(msgReceived), "Timeout expired for receiving message by target user");    // for reception
    ASSERT_CHAT_TEST(chatroomListener->hasArrivedMessage(a2, msgId), "Wrong message id at destination");
    MegaChatMessage *messageReceived = megaChatApi[a2]->getMessage(chatid, msgId);   // message should be already received, so in RAM
    ASSERT_CHAT_TEST(messageReceived && !strcmp(msg0.c_str(), messageReceived->getContent()), "Content of message doesn't match");
    // now wait for automatic unarchive, due to new message
    ASSERT_CHAT_TEST(waitForResponse(chatArchiveChanged), "Timeout expired for receiving chat list item update after new message");
    ASSERT_CHAT_TEST(waitForResponse(chatroomArchiveChanged), "Timeout expired for receiving chatroom update after new message");
    chatroom = megaChatApi[a1]->getChatRoom(chatid);
    ASSERT_CHAT_TEST(chatroom->isArchived() == false, "Chatroom is not unarchived automatically upon new message");
    delete chatroom; chatroom = NULL;


    // --> Archive the chatroom
    flagChatArchived = &requestFlagsChat[a1][MegaChatRequest::TYPE_ARCHIVE_CHATROOM]; *flagChatArchived = false;
    chatArchiveChanged = &chatArchived[a1]; *chatArchiveChanged = false;
    chatroomArchiveChanged = &chatroomListener->archiveUpdated[a1]; *chatroomArchiveChanged = false;
    megaChatApi[a1]->archiveChat(chatid, true);
    ASSERT_CHAT_TEST(waitForResponse(flagChatArchived), "Timeout expired for archiving chat");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to archive chat. Error: " + lastErrorMsgChat[a1] + " (" + std::to_string(lastErrorChat[a1]) + ")");
    ASSERT_CHAT_TEST(waitForResponse(chatArchiveChanged), "Timeout expired for receiving chat list item update about archive");
    ASSERT_CHAT_TEST(waitForResponse(chatroomArchiveChanged), "Timeout expired for receiving chatroom update about archive");
    chatroom = megaChatApi[a1]->getChatRoom(chatid);
    ASSERT_CHAT_TEST(chatroom->isArchived(), "Chatroom is not archived when it should");
    delete chatroom; chatroom = NULL;

    // --> Unarchive the chatroom
    delete chatroom; chatroom = NULL;
    flagChatArchived = &requestFlagsChat[a1][MegaChatRequest::TYPE_ARCHIVE_CHATROOM]; *flagChatArchived = false;
    chatArchiveChanged = &chatArchived[a1]; *chatArchiveChanged = false;
    chatroomArchiveChanged = &chatroomListener->archiveUpdated[a1]; *chatroomArchiveChanged = false;
    megaChatApi[a1]->archiveChat(chatid, false);
    ASSERT_CHAT_TEST(waitForResponse(flagChatArchived), "Timeout expired for archiving chat");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to archive chat. Error: " + lastErrorMsgChat[a1] + " (" + std::to_string(lastErrorChat[a1]) + ")");
    ASSERT_CHAT_TEST(waitForResponse(chatArchiveChanged), "Timeout expired for receiving chat list item update about archive");
    ASSERT_CHAT_TEST(waitForResponse(chatroomArchiveChanged), "Timeout expired for receiving chatroom update about archive");
    chatroom = megaChatApi[a1]->getChatRoom(chatid);
    ASSERT_CHAT_TEST(!chatroom->isArchived(), "Chatroom is archived when it shouldn't");
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
    ASSERT_CHAT_TEST(waitForResponse(flagRemoveFromChatRoom), "Timeout expired for ");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Error remove peer from group chat. Error: " + lastErrorMsgChat[a1] + " (" + std::to_string(lastErrorChat[a1]) + ")");
    ASSERT_CHAT_TEST(waitForResponse(chatClosed), "Timeout expired for ");
    chatroom = megaChatApi[a2]->getChatRoom(chatid);
    ASSERT_CHAT_TEST(chatroom, "Cannot get chatroom for id" + std::to_string(chatid));
    ASSERT_CHAT_TEST(!chatroom->isActive(), "Chatroom should be inactive, but it's still active");
    delete chatroom;    chatroom = NULL;

    // --> Invite to chat
    flagInviteToChatRoom = &requestFlagsChat[a1][MegaChatRequest::TYPE_INVITE_TO_CHATROOM]; *flagInviteToChatRoom = false;
    megaChatApi[a1]->inviteToChat(chatid, uh, MegaChatPeerList::PRIV_STANDARD);
    ASSERT_CHAT_TEST(waitForResponse(flagInviteToChatRoom), "Failed to invite a new peer after " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to invite a new peer. Error: " + lastErrorMsgChat[a1] + " (" + std::to_string(lastErrorChat[a1]) + ")");

    delete chatroomListener;
    delete [] sessionPrimary;
    sessionPrimary = NULL;
    delete [] sessionSecondary;
    sessionSecondary = NULL;
}


/**
 * @brief TEST_PublicChatManagement
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
void MegaChatApiTest::TEST_PublicChatManagement(unsigned int a1, unsigned int a2)
{
    /// Anonymous mode test
    // Login in primary account
    char *sessionPrimary = login(a1);

    // Init anonymous in secondary account and connect
    initState[a2] = megaChatApi[a2]->initAnonymous();
    ASSERT_CHAT_TEST(initState[a2] == MegaChatApi::INIT_ANONYMOUS, "Init sesion in anonymous mode failed");
    char *sessionAnonymous = megaApi[a2]->dumpSession();

    bool *flagRequestConnect = &requestFlagsChat[a2][MegaChatRequest::TYPE_CONNECT]; *flagRequestConnect = false;
    bool *loggedInFlag = &mLoggedInAllChats[a2]; *loggedInFlag = false;
    mChatConnectionOnline[a2] = false;
    megaChatApi[a2]->connect();
    ASSERT_CHAT_TEST(waitForResponse(flagRequestConnect), "Expired timeout for connect request");
    ASSERT_CHAT_TEST(!lastErrorChat[a2], "Error connect to chat. Error: " + std::to_string(lastErrorChat[a2]));

    // Create a public chat with no peers nor title, this chat will be reused by the rest of the tests
    MegaChatHandle chatid = MEGACHAT_INVALID_HANDLE;
    bool *flagCreateChatRoom = &requestFlagsChat[a1][MegaChatRequest::TYPE_CREATE_CHATROOM]; *flagCreateChatRoom = false;
    MegaChatPeerList *peers = MegaChatPeerList::createInstance();
    megaChatApi[a1]->createPublicChat(peers);
    ASSERT_CHAT_TEST(waitForResponse(flagCreateChatRoom), "Expired timeout for creating public groupchat");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to create public groupchat. Error: " + lastErrorMsgChat[a1] + " (" + std::to_string(lastErrorChat[a1]) + ")");
    chatid = this->chatid[a1];
    ASSERT_CHAT_TEST(chatid != MEGACHAT_INVALID_HANDLE, "Wrong chat id");
    delete peers;
    peers = NULL;

    // Open chatroom
    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_CHAT_TEST(megaChatApi[a1]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account " + std::to_string(a1+1));
    bool *flagChatdOnline = &mChatConnectionOnline[a1]; *flagChatdOnline = false;
    while (megaChatApi[a1]->getChatConnectionState(chatid) != MegaChatApi::CHAT_CONNECTION_ONLINE)
    {
        postLog("Waiting for connection to chatd...");
        ASSERT_CHAT_TEST(waitForResponse(flagChatdOnline), "Timeout expired for connecting to chatd");
        *flagChatdOnline = false;
    }

    // Create chat link (ERR No title)
    bool *flagCreateChatLink = &requestFlagsChat[a1][MegaChatRequest::TYPE_CHAT_LINK_HANDLE]; *flagCreateChatLink = false;
    megaChatApi[a1]->createChatLink(chatid, this);
    ASSERT_CHAT_TEST(waitForResponse(flagCreateChatLink), "Timeout expired for create chat link");

    // Set title
    string title = "TestPublicChatWithTitle_" + dateToString().substr(dateToString().length() - 5, 5);
    bool *flagChatRoomName = &requestFlagsChat[a1][MegaChatRequest::TYPE_EDIT_CHATROOM_NAME]; *flagChatRoomName = false;
    bool *titleItemChanged0 = &titleUpdated[a1]; *titleItemChanged0 = false;
    bool *titleChanged0 = &chatroomListener->titleUpdated[a1]; *titleChanged0 = false;
    bool *mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
    string *msgContent = &chatroomListener->content[a1]; *msgContent = "";
    megaChatApi[a1]->setChatTitle(chatid, title.c_str());
    ASSERT_CHAT_TEST(waitForResponse(flagChatRoomName), "Timeout expired for set chat title");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to set chat title. Error: " + lastErrorMsgChat[a1] + " (" + std::to_string(lastErrorChat[a1]) + ")");
    ASSERT_CHAT_TEST(waitForResponse(titleItemChanged0), "Timeout expired for receiving chat list item update");
    ASSERT_CHAT_TEST(waitForResponse(titleChanged0), "Timeout expired for receiving chatroom update");
    ASSERT_CHAT_TEST(waitForResponse(mngMsgRecv), "Timeout expired for receiving management message");
    ASSERT_CHAT_TEST(!strcmp(title.c_str(), msgContent->c_str()), "Title received doesn't match the title set");

    // Create chat link
    flagCreateChatLink = &requestFlagsChat[a1][MegaChatRequest::TYPE_CHAT_LINK_HANDLE]; *flagCreateChatLink = false;
    megaChatApi[a1]->createChatLink(chatid, this);
    ASSERT_CHAT_TEST(waitForResponse(flagCreateChatLink), "Timeout expired for create chat link");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Error creating chat link. Error: " + lastErrorMsgChat[a1] + " (" + std::to_string(lastErrorChat[a1]) + ")");
    std::string chatLink = this->chatLinks[a1];
    assert(!chatLink.empty());

    // Load chat link
    bool *previewsUpdated = &chatroomListener->previewsUpdated[a1]; *previewsUpdated = false;
    bool *flagPreviewChat = &requestFlagsChat[a2][MegaChatRequest::TYPE_LOAD_PREVIEW]; *flagPreviewChat = false;
    megaChatApi[a2]->openChatPreview(chatLink.c_str(), this);
    ASSERT_CHAT_TEST(waitForResponse(flagPreviewChat), "Timeout expired for load chat link");
    ASSERT_CHAT_TEST(!lastErrorChat[a2], "Failed to open chat preview. Error: " + lastErrorMsgChat[a2] + " (" + std::to_string(lastErrorChat[a2]) + ")");

    // Open chatroom
    ASSERT_CHAT_TEST(megaChatApi[a2]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account " + std::to_string(a2+1));
    ASSERT_CHAT_TEST(waitForResponse(previewsUpdated), "Timeout expired for update previewers");

    // Send a message (ERR)
    string msg = "HI " + mAccounts[a1].getEmail()+ " - This message will be rejected because now I'm a previewer";
    bool *flagRejected = &chatroomListener->msgRejected[a2]; *flagRejected = false;
    chatroomListener->clearMessages(a2);   // will be set at reception
    MegaChatMessage *msgSent = megaChatApi[a2]->sendMessage(chatid, msg.c_str());
    delete msgSent; msgSent = NULL;
    ASSERT_CHAT_TEST(waitForResponse(flagRejected), "Timeout expired for rejection of message");    // for confirmation, sendMessage() is synchronous
    ASSERT_CHAT_TEST(chatroomListener->mConfirmedMessageHandle[a2] == MEGACHAT_INVALID_HANDLE, "Message confirmed, when it should fail");

    // Send a message
    msg = "HI Anonymous user, This message will be sent";
    flagRejected = &chatroomListener->msgRejected[a1]; *flagRejected = false;
    chatroomListener->clearMessages(a1);   // will be set at reception
    msgSent = megaChatApi[a1]->sendMessage(chatid, msg.c_str());
    ASSERT_CHAT_TEST(msgSent, "Succeed to send message");
    delete msgSent; msgSent = NULL;
    ASSERT_CHAT_TEST(chatroomListener->mConfirmedMessageHandle[a1] == MEGACHAT_INVALID_HANDLE, "Message confirmed, when it should fail");

    // Close preview
    previewsUpdated = &chatroomListener->previewsUpdated[a1]; *previewsUpdated = false;
    megaChatApi[a2]->closeChatPreview(chatid);
    ASSERT_CHAT_TEST(waitForResponse(previewsUpdated), "Timeout expired for close preview");

    // Remove chat link
    bool *setRemoveChatLink = &requestFlagsChat[a1][MegaChatRequest::TYPE_CHAT_LINK_HANDLE]; *setRemoveChatLink = false;
    megaChatApi[a1]->removeChatLink(chatid, this);
    ASSERT_CHAT_TEST(waitForResponse(setRemoveChatLink), "Timeout expired for close chat link");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to remove chat link. Error: " + lastErrorMsgChat[a1] + " (" + std::to_string(lastErrorChat[a1]) + ")");

    // Preview chat link (ERR)
    flagPreviewChat = &requestFlagsChat[a2][MegaChatRequest::TYPE_LOAD_PREVIEW]; *flagPreviewChat = false;
    megaChatApi[a2]->openChatPreview(chatLink.c_str(), this);
    ASSERT_CHAT_TEST(waitForResponse(flagPreviewChat), "Timeout expired for load chat link");
    ASSERT_CHAT_TEST(lastErrorChat[a2] == API_ENOENT, "Unexpected error loading an invalid chat-link: " + lastErrorMsgChat[a2] + " (" + std::to_string(lastErrorChat[a2]) + ")");

    // Logout in anonymous mode
    logout(a2);
    delete [] sessionAnonymous;
    sessionAnonymous = NULL;

    /// Public chats test
    // Login in secondary account
    char *sessionSecondary = login(a2);

    // Create chat link
    flagCreateChatLink = &requestFlagsChat[a1][MegaChatRequest::TYPE_CHAT_LINK_HANDLE]; *flagCreateChatLink = false;
    megaChatApi[a1]->createChatLink(chatid, this);
    ASSERT_CHAT_TEST(waitForResponse(flagCreateChatLink), "Timeout expired for create chat link");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Error creating chat link. Error: " + lastErrorMsgChat[a1] + " (" + std::to_string(lastErrorChat[a1]) + ")");
    chatLink = this->chatLinks[a1];
    assert(!chatLink.empty());

    // Load chat link (OK)
    previewsUpdated = &chatroomListener->previewsUpdated[a1]; *previewsUpdated = false;
    flagPreviewChat = &requestFlagsChat[a2][MegaChatRequest::TYPE_LOAD_PREVIEW]; *flagPreviewChat = false;
    megaChatApi[a2]->openChatPreview(chatLink.c_str(), this);
    ASSERT_CHAT_TEST(waitForResponse(flagPreviewChat), "Timeout expired for load chat link");
    ASSERT_CHAT_TEST(!lastErrorChat[a2], "Failed to open chat preview. Error: " + lastErrorMsgChat[a2] + " (" + std::to_string(lastErrorChat[a2]) + ")");

    // Open chatroom
    ASSERT_CHAT_TEST(megaChatApi[a2]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account " + std::to_string(a2+1));
    ASSERT_CHAT_TEST(waitForResponse(previewsUpdated), "Timeout expired for update previewers");

    // Try to send a message (ERR)
    string msgaux = "HI " + mAccounts[a1].getEmail()+ " - This message can't be send because I'm in preview mode (read-only)";
    flagRejected = &chatroomListener->msgRejected[a2]; *flagRejected = false;
    chatroomListener->clearMessages(a2);   // will be set at reception
    msgSent = megaChatApi[a2]->sendMessage(chatid, msgaux.c_str());
    ASSERT_CHAT_TEST(msgSent, "Fail to send message, when it should succeed");
    delete msgSent; msgSent = NULL;
    ASSERT_CHAT_TEST(waitForResponse(flagRejected), "Timeout expired for rejection of message");
    ASSERT_CHAT_TEST(chatroomListener->mConfirmedMessageHandle[a2] == MEGACHAT_INVALID_HANDLE, "Message confirmed, when it should fail");

    // Autojoin chat link
    bool *flagJoinChatLink = &requestFlagsChat[a2][MegaChatRequest::TYPE_AUTOJOIN_PUBLIC_CHAT]; *flagJoinChatLink = false;
    previewsUpdated = &chatroomListener->previewsUpdated[a1]; *previewsUpdated = false;
    megaChatApi[a2]->autojoinPublicChat(chatid, this);
    ASSERT_CHAT_TEST(waitForResponse(flagJoinChatLink), "Timeout expired for autojoin chat-link");
    ASSERT_CHAT_TEST(!lastErrorChat[a2], "Failed to autojoin chat-link. Error: " + lastErrorMsgChat[a2] + " (" + std::to_string(lastErrorChat[a2]) + ")");
    ASSERT_CHAT_TEST(waitForResponse(previewsUpdated), "Timeout expired for update previewers");
    MegaChatListItem *item = megaChatApi[a2]->getChatListItem(chatid);
    ASSERT_CHAT_TEST((item->getNumPreviewers() == 0), "Wrong number of previewers. Current: " + item->getNumPreviewers());
    delete item;
    item = NULL;

    // Send a message
    msgaux = "HI " + mAccounts[a1].getEmail()+ " - I have autojoined to this chat";
    flagRejected = &chatroomListener->msgRejected[a2]; *flagRejected = false;
    chatroomListener->clearMessages(a2);   // will be set at reception
    msgSent = megaChatApi[a2]->sendMessage(chatid, msgaux.c_str());
    ASSERT_CHAT_TEST(msgSent, "Succeed to send message, when it should fail");
    delete msgSent; msgSent = NULL;
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);

    // Set chat to private mode
    bool *setPublicChatToPrivate = &requestFlagsChat[a1][MegaChatRequest::TYPE_SET_PRIVATE_MODE]; *setPublicChatToPrivate = false;
    megaChatApi[a1]->setPublicChatToPrivate(chatid, this);
    ASSERT_CHAT_TEST(waitForResponse(setPublicChatToPrivate), "Timeout expired for close chat link");

    // Remove peer from groupchat
    auto uh =  megaChatApi[a2]->getMyUserHandle();
    bool *flagRemoveFromChatRoom = &requestFlagsChat[a1][MegaChatRequest::TYPE_REMOVE_FROM_CHATROOM]; *flagRemoveFromChatRoom = false;
    bool *chatClosed = &chatItemClosed[a2]; *chatClosed = false;
    megaChatApi[a1]->removeFromChat(chatid, uh);
    ASSERT_CHAT_TEST(waitForResponse(flagRemoveFromChatRoom), "Timeout expired for remove peer from chat");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Error remove peer from group chat. Error: " + lastErrorMsgChat[a1] + " (" + std::to_string(lastErrorChat[a1]) + ")");
    ASSERT_CHAT_TEST(waitForResponse(chatClosed), "Timeout expired for remove peer from chat");

    MegaChatRoom * auxchatroom = megaChatApi[a2]->getChatRoom(chatid);
    ASSERT_CHAT_TEST(auxchatroom, "Cannot get chatroom for id" + std::to_string(chatid));
    ASSERT_CHAT_TEST(!auxchatroom->isActive(), "Chatroom should be inactive, but it's still active");
    delete auxchatroom;    auxchatroom = NULL;

    // Preview chat link (ERR)
    flagPreviewChat = &requestFlagsChat[a2][MegaChatRequest::TYPE_LOAD_PREVIEW]; *flagPreviewChat = false;
    megaChatApi[a2]->openChatPreview(chatLink.c_str(), this);
    ASSERT_CHAT_TEST(waitForResponse(flagPreviewChat), "Timeout expired for load chat link");
    ASSERT_CHAT_TEST(lastErrorChat[a2] == API_ENOENT, "Unexpected error loading an invalid chat-link: " + lastErrorMsgChat[a2] + " (" + std::to_string(lastErrorChat[a2]) + ")");

    // --> Invite to chat
    bool *flagInviteToChatRoom = &requestFlagsChat[a1][MegaChatRequest::TYPE_INVITE_TO_CHATROOM]; *flagInviteToChatRoom = false;
    megaChatApi[a1]->inviteToChat(chatid, uh, MegaChatPeerList::PRIV_STANDARD);
    ASSERT_CHAT_TEST(waitForResponse(flagInviteToChatRoom), "Failed to invite a new peer after " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to invite a new peer. Error: " + lastErrorMsgChat[a1] + " (" + std::to_string(lastErrorChat[a1]) + ")");

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
 * @brief TEST_Reactions
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
void MegaChatApiTest::TEST_Reactions(unsigned int a1, unsigned int a2)
{
    // Login both accounts
    char *sessionPrimary = login(a1);
    char *sessionSecondary = login(a2);

    // Prepare peers, privileges...
    MegaUser *user = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    if (!user || (user->getVisibility() != MegaUser::VISIBILITY_VISIBLE))
    {
        makeContact(a1, a2);
        delete user;
        user = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    }

    // Get a group chatroom with both users
    MegaChatHandle uh = user->getHandle();
    delete user;
    user = NULL;
    MegaChatPeerList *peers = MegaChatPeerList::createInstance();
    peers->addPeer(uh, MegaChatPeerList::PRIV_STANDARD);
    MegaChatHandle chatid = getGroupChatRoom(a1, a2, peers);
    delete peers;
    peers = NULL;

    // Open chatroom
    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_CHAT_TEST(megaChatApi[a1]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account " + std::to_string(a1+1));
    ASSERT_CHAT_TEST(megaChatApi[a2]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account " + std::to_string(a2+1));
    ::mega::unique_ptr <MegaChatRoom> chatroom (megaChatApi[a1]->getChatRoom(chatid));
    ::mega::unique_ptr<char[]> chatidB64(megaApi[a1]->handleToBase64(chatid));
    ASSERT_CHAT_TEST(chatroom, "Cannot get chatroom for id" + std::string(chatidB64.get()));

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
        ASSERT_CHAT_TEST(auxrequestListener.waitForResponse(), "Timeout expired for update privilege of peer");
        ASSERT_CHAT_TEST(!auxrequestListener.getErrorCode(), "Failed to update privilege of peer Error: " + std::to_string(auxrequestListener.getErrorCode()));
        ASSERT_CHAT_TEST(waitForResponse(peerUpdated0), "Timeout expired for receiving peer update");
        ASSERT_CHAT_TEST(waitForResponse(peerUpdated1), "Timeout expired for receiving peer update");
        ASSERT_CHAT_TEST(waitForResponse(mngMsgRecv), "Timeout expired for receiving management message");
        ASSERT_CHAT_TEST(*uhAction == uh, "User handle from message doesn't match");
        ASSERT_CHAT_TEST(*priv == MegaChatRoom::PRIV_RO, "Privilege is incorrect");
    }

    // Send a message and wait for reception by target user
    string msg0 = "HI " + mAccounts[a1].getEmail() + " - Testing reactions";
    bool *msgConfirmed = &chatroomListener->msgConfirmed[a1]; *msgConfirmed = false;
    bool *msgReceived = &chatroomListener->msgReceived[a2]; *msgReceived = false;
    bool *msgDelivered = &chatroomListener->msgDelivered[a1]; *msgDelivered = false;
    chatroomListener->clearMessages(a1);
    chatroomListener->clearMessages(a2);
    MegaChatMessage *messageSent = megaChatApi[a1]->sendMessage(chatid, msg0.c_str());
    ASSERT_CHAT_TEST(waitForResponse(msgConfirmed), "Timeout expired for receiving confirmation by server");    // for confirmation, sendMessage() is synchronous
    MegaChatHandle msgId = chatroomListener->mConfirmedMessageHandle[a1];
    ASSERT_CHAT_TEST(chatroomListener->hasArrivedMessage(a1, msgId), "Message not received");
    ASSERT_CHAT_TEST(msgId != MEGACHAT_INVALID_HANDLE, "Wrong message id at origin");
    ASSERT_CHAT_TEST(waitForResponse(msgReceived), "Timeout expired for receiving message by target user");    // for reception
    ASSERT_CHAT_TEST(chatroomListener->hasArrivedMessage(a2, msgId), "Wrong message id at destination");
    MegaChatMessage *messageReceived = megaChatApi[a2]->getMessage(chatid, msgId);   // message should be already received, so in RAM
    ASSERT_CHAT_TEST(messageReceived && !strcmp(msg0.c_str(), messageReceived->getContent()), "Content of message doesn't match");

    // Check reactions for the message sent above (It shouldn't exist any reaction)
    ::mega::unique_ptr <MegaStringList> reactionsList;
    reactionsList.reset(megaChatApi[a1]->getMessageReactions(chatid, msgId));
    ASSERT_CHAT_TEST(!reactionsList->size(), "getMessageReactions Error: The message shouldn't have reactions");
    int userCount = megaChatApi[a1]->getMessageReactionCount(chatid, msgId, "😰");
    ASSERT_CHAT_TEST(!userCount, "getReactionUsers Error: The reaction shouldn't exist");

    // Add invalid reaction (error)
    TestMegaChatRequestListener requestListener(nullptr, megaChatApi[a1]);
    megaChatApi[a1]->addReaction(chatid, msgId, nullptr, &requestListener);
    ASSERT_CHAT_TEST(requestListener.waitForResponse(), "Timeout expired for add reaction");
    ASSERT_CHAT_TEST(requestListener.getErrorCode() == MegaChatError::ERROR_ARGS, "addReaction: Unexpected error for NULL reaction param. Error:" + std::string(std::to_string(requestListener.getErrorCode())));

    // Add reaction for invalid chat (error)
    requestListener = TestMegaChatRequestListener (nullptr, megaChatApi[a1]);
    megaChatApi[a1]->addReaction(MEGACHAT_INVALID_HANDLE, msgId, "😰", &requestListener);
    ASSERT_CHAT_TEST(requestListener.waitForResponse(), "Timeout expired for add reaction");
    ASSERT_CHAT_TEST(requestListener.getErrorCode() == MegaChatError::ERROR_NOENT, "addReaction: Unexpected error for invalid chat. Error:" + std::string(std::to_string(requestListener.getErrorCode())));

    // Add reaction for invalid message (error)
    requestListener = TestMegaChatRequestListener (nullptr, megaChatApi[a1]);
    megaChatApi[a1]->addReaction(chatid, MEGACHAT_INVALID_HANDLE, "😰", &requestListener);
    ASSERT_CHAT_TEST(requestListener.waitForResponse(), "Timeout expired for add reaction");
    ASSERT_CHAT_TEST(requestListener.getErrorCode() == MegaChatError::ERROR_NOENT, "addReaction: Unexpected error for invalid message. Error:" + std::string(std::to_string(requestListener.getErrorCode())));

    // Add reaction without enough permissions (error)
    requestListener = TestMegaChatRequestListener (nullptr, megaChatApi[a2]);
    megaChatApi[a2]->addReaction(chatid, msgId, "😰", &requestListener);
    ASSERT_CHAT_TEST(requestListener.waitForResponse(), "Timeout expired for add reaction");
    ASSERT_CHAT_TEST(requestListener.getErrorCode() == MegaChatError::ERROR_ACCESS, "addReaction: Unexpected error adding reaction without enough permissions. Error:" + std::string(std::to_string(requestListener.getErrorCode())));

    // Add reaction (ok)
    requestListener = TestMegaChatRequestListener (nullptr, megaChatApi[a1]);
    megaChatApi[a1]->addReaction(chatid, msgId, "😰", &requestListener);
    ASSERT_CHAT_TEST(requestListener.waitForResponse(), "Timeout expired for add reaction");
    ASSERT_CHAT_TEST(requestListener.getErrorCode() == MegaChatError::ERROR_OK, "addReaction: Unexpected error adding reaction. Error:" + std::string(std::to_string(requestListener.getErrorCode())));

    // Add existing reaction (error)
    requestListener = TestMegaChatRequestListener (nullptr, megaChatApi[a1]);
    megaChatApi[a1]->addReaction(chatid, msgId, "😰", &requestListener);
    ASSERT_CHAT_TEST(requestListener.waitForResponse(), "Timeout expired for add reaction");
    ASSERT_CHAT_TEST(requestListener.getErrorCode() == MegaChatError::ERROR_EXIST, "addReaction: Unexpected error for adding an existing reaction. Error:" + std::string(std::to_string(requestListener.getErrorCode())));

    // Check reactions
    reactionsList.reset(megaChatApi[a1]->getMessageReactions(chatid, msgId));
    ASSERT_CHAT_TEST(reactionsList->size(), "getMessageReactions Error: The message doesn't have reactions");
    userCount = megaChatApi[a1]->getMessageReactionCount(chatid, msgId, "😰");
    ASSERT_CHAT_TEST(userCount, "getReactionUsers Error: The reaction doesn't exists");

    // Remove reaction (ok)
    requestListener = TestMegaChatRequestListener (nullptr, megaChatApi[a1]);
    megaChatApi[a1]->delReaction(chatid, msgId, "😰", &requestListener);
    ASSERT_CHAT_TEST(requestListener.waitForResponse(), "Timeout expired for del reaction");
    ASSERT_CHAT_TEST(requestListener.getErrorCode() == MegaChatError::ERROR_OK, "delReaction: Unexpected error removing reaction. Error:" + std::string(std::to_string(requestListener.getErrorCode())));

    // Remove unexisting reaction (error)
    requestListener = TestMegaChatRequestListener (nullptr, megaChatApi[a1]);
    megaChatApi[a1]->delReaction(chatid, msgId, "😰", &requestListener);
    ASSERT_CHAT_TEST(requestListener.waitForResponse(), "Timeout expired for del reaction");
    ASSERT_CHAT_TEST(requestListener.getErrorCode() == MegaChatError::ERROR_EXIST, "delReaction: Unexpected error for removing a non-existent reaction. Error:" + std::string(std::to_string(requestListener.getErrorCode())));

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
 * @brief TEST_OfflineMode
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
void MegaChatApiTest::TEST_OfflineMode(unsigned int a1, unsigned int a2)
{
    char *sessionPrimary = login(a1);
    char *sessionSecondary = login(a2);

    // Prepare peers, privileges...
    MegaUser *user = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    if (!user || (user->getVisibility() != MegaUser::VISIBILITY_VISIBLE))
    {
        makeContact(a1, a2);
        delete user;
        user = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    }

    MegaChatHandle uh = user->getHandle();
    delete user;
    user = NULL;

    MegaChatPeerList *peers = MegaChatPeerList::createInstance();
    peers->addPeer(uh, MegaChatPeerList::PRIV_STANDARD);

    MegaChatHandle chatid = getGroupChatRoom(a1, a2, peers);
    ASSERT_CHAT_TEST((megaChatApi[a1]->getChatConnectionState(chatid) == MegaChatApi::CHAT_CONNECTION_ONLINE),
                             "Not connected to chatd for account " + std::to_string(a1+1) + ": " + mAccounts[a1].getEmail());
    ASSERT_CHAT_TEST((megaChatApi[a2]->getChatConnectionState(chatid) == MegaChatApi::CHAT_CONNECTION_ONLINE),
                             "Not connected to chatd for account " + std::to_string(a2+1) + ": " + mAccounts[a2].getEmail());

    MegaChatRoom *chatRoom = megaChatApi[a1]->getChatRoom(chatid);    
    ASSERT_CHAT_TEST(chatRoom && (chatid != MEGACHAT_INVALID_HANDLE), "Can't get a chatroom");
    delete peers;
    peers = NULL;

    const char *info = MegaChatApiTest::printChatRoomInfo(chatRoom);
    postLog(info);
    delete [] info; info = NULL;
    delete chatRoom;

    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_CHAT_TEST(megaChatApi[a1]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account " + std::to_string(a1+1));

    // Load some message to feed history
    bool *flagHistoryLoaded = &chatroomListener->historyLoaded[a1]; *flagHistoryLoaded = false;
    megaChatApi[a1]->loadMessages(chatid, 16);
    ASSERT_CHAT_TEST(waitForResponse(flagHistoryLoaded), "Expired timeout for loading history");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to load history. Error: " + lastErrorMsgChat[a1] + " (" + std::to_string(lastErrorChat[a1]) + ")");

    std::stringstream buffer;
    buffer << endl << endl << "Disconnect from the Internet now" << endl << endl;
    postLog(buffer.str());

//        system("pause");

    string msg0 = "This is a test message sent without Internet connection";
    chatroomListener->clearMessages(a1);
    MegaChatMessage *msgSent = megaChatApi[a1]->sendMessage(chatid, msg0.c_str());
    ASSERT_CHAT_TEST(msgSent, "Failed to send message");
    ASSERT_CHAT_TEST(msgSent->getStatus() == MegaChatMessage::STATUS_SENDING, "Wrong message status: " + std::to_string(msgSent->getStatus()));

    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);

    // close session and resume it while offline
    logout(a1, false);
    bool *flagInit = &initStateChanged[a1]; *flagInit = false;
    megaChatApi[a1]->init(sessionPrimary);
    MegaApi::removeLoggerObject(logger);
    ASSERT_CHAT_TEST(waitForResponse(flagInit), "Expired timeout for initialization");
    int initStateValue = initState[a1];
    ASSERT_CHAT_TEST(initStateValue == MegaChatApi::INIT_OFFLINE_SESSION,
                     "Wrong chat initialization state. Expected: " + std::to_string(MegaChatApi::INIT_OFFLINE_SESSION) + "   Received: " + std::to_string(initStateValue));

    // check the unsent message is properly loaded
    flagHistoryLoaded = &chatroomListener->historyLoaded[a1]; *flagHistoryLoaded = false;
    bool *msgUnsentLoaded = &chatroomListener->msgLoaded[a1]; *msgUnsentLoaded = false;
    chatroomListener->clearMessages(a1);
    ASSERT_CHAT_TEST(megaChatApi[a1]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account " + std::to_string(a1+1));
    bool msgUnsentFound = false;
    do
    {
        ASSERT_CHAT_TEST(waitForResponse(msgUnsentLoaded), "Expired timeout to load unsent message");
        if (chatroomListener->hasArrivedMessage(a1, msgSent->getMsgId()))
        {
            msgUnsentFound = true;
            break;
        }
        *msgUnsentLoaded = false;
    } while (*flagHistoryLoaded);
    ASSERT_CHAT_TEST(msgUnsentFound, "Failed to load unsent message");
    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);

    buffer.str("");
    buffer << endl << endl << "Connect from the Internet now" << endl << endl;
    postLog(buffer.str());

//        system("pause");

    bool *flagRetry = &requestFlagsChat[a1][MegaChatRequest::TYPE_RETRY_PENDING_CONNECTIONS]; *flagRetry = false;
    megaChatApi[a1]->retryPendingConnections();
    ASSERT_CHAT_TEST(waitForResponse(flagRetry), "Timeout expired for retry pending connections");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to retry pending connections");

    flagHistoryLoaded = &chatroomListener->historyLoaded[a1]; *flagHistoryLoaded = false;
    bool *msgSentLoaded = &chatroomListener->msgLoaded[a1]; *msgSentLoaded = false;
    chatroomListener->clearMessages(a1);
    ASSERT_CHAT_TEST(megaChatApi[a1]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account " + std::to_string(a1+1));
    bool msgSentFound = false;
    do
    {
        ASSERT_CHAT_TEST(waitForResponse(msgSentLoaded), "Expired timeout to load sent message");
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
    logout(a1);
    sessionPrimary = login(a1);

    ASSERT_CHAT_TEST(msgSentFound, "Failed to load sent message");
    delete msgSent; msgSent = NULL;
    delete chatroomListener;
    chatroomListener = NULL;

    delete [] sessionPrimary;
    delete [] sessionSecondary;
}

/**
 * @brief TEST_ClearHistory
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
void MegaChatApiTest::TEST_ClearHistory(unsigned int a1, unsigned int a2)
{
    char *sessionPrimary = login(a1);
    char *sessionSecondary = login(a2);

    MegaUser *user = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    if (!user || (user->getVisibility() != MegaUser::VISIBILITY_VISIBLE))
    {
        makeContact(a1, a2);
    }
    delete user;
    user = NULL;

    MegaChatHandle chatid = getPeerToPeerChatRoom(a1, a2);

    // Open chatrooms
    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_CHAT_TEST(megaChatApi[a1]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account " + std::to_string(a1+1));
    ASSERT_CHAT_TEST(megaChatApi[a2]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account " + std::to_string(a2+1));

    // Load some message to feed history
    loadHistory(a1, chatid, chatroomListener);
    loadHistory(a2, chatid, chatroomListener);

    // Send 5 messages to have some history
    for (int i = 0; i < 5; i++)
    {
        string msg0 = "HI " + mAccounts[a2].getEmail() + " - Testing clearhistory. This messages is the number " + std::to_string(i);

        MegaChatMessage *message = sendTextMessageOrUpdate(a1, a2, chatid, msg0, chatroomListener);

        delete message;
        message = NULL;
    }

    // Close the chatrooms
    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);
    delete chatroomListener;

    // Open chatrooms
    chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_CHAT_TEST(megaChatApi[a1]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account " + std::to_string(a1+1));
    ASSERT_CHAT_TEST(megaChatApi[a2]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account " + std::to_string(a2+1));

    // --> Load some message to feed history
    int count = loadHistory(a1, chatid, chatroomListener);
    // we sent 5 messages, but if the chat already existed, there was a "Clear history" message already
    ASSERT_CHAT_TEST(count == 5 || count == 6, "Wrong count of messages: " + std::to_string(count));
    count = loadHistory(a2, chatid, chatroomListener);
    ASSERT_CHAT_TEST(count == 5 || count == 6, "Wrong count of messages: " + std::to_string(count));

    // Clear history
    clearHistory(a1, a2, chatid, chatroomListener);
    // TODO: in this case, it's not just to clear the history, but
    // to also check the other user received the corresponding message.

    // Close and re-open chatrooms
    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);
    delete chatroomListener;
    chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_CHAT_TEST(megaChatApi[a1]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account " + std::to_string(a1+1));
    ASSERT_CHAT_TEST(megaChatApi[a2]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account " + std::to_string(a2+1));

    // --> Check history is been truncated
    count = loadHistory(a1, chatid, chatroomListener);
    ASSERT_CHAT_TEST(count == 1, "Wrong count of messages: " + std::to_string(count));
    count = loadHistory(a2, chatid, chatroomListener);
    ASSERT_CHAT_TEST(count == 1, "Wrong count of messages: " + std::to_string(count));

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
 * @brief TEST_SwitchAccounts
 *
 * This test does the following:
 * - Login with accoun1 email and pasword
 * - Logout
 * - With the same megaApi and megaChatApi, login with account2
 */
void MegaChatApiTest::TEST_SwitchAccounts(unsigned int a1, unsigned int a2)
{
    char *session = login(a1);

    MegaChatListItemList *items = megaChatApi[a1]->getChatListItems();
    for (int i = 0; i < items->size(); i++)
    {
        const MegaChatListItem *item = items->get(i);
        if (item->isPublic())
        {
            continue;
        }

        const char *info = MegaChatApiTest::printChatListItemInfo(item);
        postLog(info);
        delete [] info; info = NULL;

        sleep(3);

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

    logout(a1, true);    // terminate() and destroy Client

    delete [] session;
    session = NULL;

    // Login over same index account but with other user
    session = login(a1, NULL, mAccounts[a2].getEmail().c_str(), mAccounts[a2].getPassword().c_str());

    delete [] session;
    session = NULL;
}

/**
 * @brief TEST_Attachment
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
void MegaChatApiTest::TEST_Attachment(unsigned int a1, unsigned int a2)
{
    char *primarySession = login(a1);
    char *secondarySession = login(a2);

    // 0. Ensure both accounts are contacts and there's a 1on1 chatroom
    MegaUser *user = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    if (!user || (user->getVisibility() != MegaUser::VISIBILITY_VISIBLE))
    {
        makeContact(a1, a2);
    }
    delete user;
    user = NULL;

    MegaChatHandle chatid = getPeerToPeerChatRoom(a1, a2);

    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_CHAT_TEST(megaChatApi[a1]->openChatRoom(chatid, chatroomListener), "Cannot open chatroom");
    ASSERT_CHAT_TEST(megaChatApi[a2]->openChatRoom(chatid, chatroomListener), "Cannot open chatroom");

    // Load some messages to feed history
    loadHistory(a1, chatid, chatroomListener);
    loadHistory(a2, chatid, chatroomListener);

    chatroomListener->clearMessages(a1);   // will be set at confirmation
    chatroomListener->clearMessages(a2);   // will be set at reception

    std::string formatDate = dateToString();

    // A uploads a new file
    createFile(formatDate, LOCAL_PATH, formatDate);
    MegaNode* nodeSent = uploadFile(a1, formatDate, LOCAL_PATH, REMOTE_PATH);

    // A sends the file as attachment to the chatroom
    MegaChatMessage *msgSent = attachNode(a1, a2, chatid, nodeSent, chatroomListener);
    MegaNode *nodeReceived = msgSent->getMegaNodeList()->get(0)->copy();

    // B downloads the node
    ASSERT_CHAT_TEST(downloadNode(a2, nodeReceived), "Cannot download node attached to message");

    // B imports the node
    ASSERT_CHAT_TEST(importNode(a2, nodeReceived, FILE_IMAGE_NAME), "Cannot import node attached to message");

    // A revokes access to node
    bool *flagRequest = &requestFlagsChat[a1][MegaChatRequest::TYPE_REVOKE_NODE_MESSAGE]; *flagRequest = false;
    bool *flagConfirmed = &chatroomListener->msgConfirmed[a1]; *flagConfirmed = false;
    bool *flagReceived = &chatroomListener->msgReceived[a2]; *flagReceived = false;
    chatroomListener->mConfirmedMessageHandle[a1] = MEGACHAT_INVALID_HANDLE;
    chatroomListener->clearMessages(a1);   // will be set at confirmation
    chatroomListener->clearMessages(a2);   // will be set at reception
    megachat::MegaChatHandle revokeAttachmentNode = nodeSent->getHandle();
    megaChatApi[a1]->revokeAttachment(chatid, revokeAttachmentNode, this);
    ASSERT_CHAT_TEST(waitForResponse(flagRequest), "Failed to revoke access to node after " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to revoke access: " + std::to_string(lastErrorChat[a1]));
    ASSERT_CHAT_TEST(waitForResponse(flagConfirmed), "Timeout expired for receiving confirmation by server");
    MegaChatHandle msgId0 = chatroomListener->mConfirmedMessageHandle[a1];
    ASSERT_CHAT_TEST(msgId0 != MEGACHAT_INVALID_HANDLE, "Wrong message id");

    // Wait for message recived has same id that message sent. It can fail if we receive a message
    ASSERT_CHAT_TEST(waitForResponse(flagReceived), "Timeout expired for receiving message by target user");

    ASSERT_CHAT_TEST(chatroomListener->hasArrivedMessage(a2, msgId0), "Message ids don't match");
    MegaChatMessage *msgReceived = megaChatApi[a2]->getMessage(chatid, msgId0);   // message should be already received, so in RAM
    ASSERT_CHAT_TEST(msgReceived, "Message not found");
    ASSERT_CHAT_TEST(msgReceived->getType() == MegaChatMessage::TYPE_REVOKE_NODE_ATTACHMENT, "Unexpected type of message");
    ASSERT_CHAT_TEST(msgReceived->getHandleOfAction() == nodeSent->getHandle(), "Handle of attached nodes don't match");

    // Remove the downloaded file to try to download it again after revoke
    std::string filePath = DOWNLOAD_PATH + std::string(formatDate);
    std::string secondaryFilePath = DOWNLOAD_PATH + std::string("remove");
    rename(filePath.c_str(), secondaryFilePath.c_str());

    // B attempt to download the file after access revocation
    ASSERT_CHAT_TEST(!downloadNode(1, nodeReceived), "Download succeed, when it should fail");

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
    msgSent = attachNode(a1, a2, chatid, nodeSent, chatroomListener);
    nodeReceived = msgSent->getMegaNodeList()->get(0)->copy();

    // A gets the thumbnail of the uploaded image
    bool *flagRequestThumbnail0 = &requestFlags[a1][MegaRequest::TYPE_GET_ATTR_FILE]; *flagRequestThumbnail0 = false;
    std::string thumbnailPath = LOCAL_PATH + "/thumbnail0.jpg";
    megaApi[a1]->getThumbnail(nodeSent, thumbnailPath.c_str(), this);
    ASSERT_CHAT_TEST(waitForResponse(flagRequestThumbnail0), "Failed to get own thumbnail after " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(!lastError[a1], "Failed to get thumbnail. Error: " + std::to_string(lastError[a1]));

    // B gets the thumbnail of the attached image
    bool *flagRequestThumbnail1 = &requestFlags[a2][MegaRequest::TYPE_GET_ATTR_FILE]; *flagRequestThumbnail1 = false;
    thumbnailPath = LOCAL_PATH + "/thumbnail1.jpg";
    megaApi[a2]->getThumbnail(nodeReceived, thumbnailPath.c_str(), this);
    ASSERT_CHAT_TEST(waitForResponse(flagRequestThumbnail1), "Failed to get thumbnail after " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(!lastError[a2], "Failed to get thumbnail. Error: " + std::to_string(lastError[a2]));

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
 * @brief TEST_LastMessage
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
void MegaChatApiTest::TEST_LastMessage(unsigned int a1, unsigned int a2)
{
    char *sessionPrimary = login(a1);
    char *sessionSecondary = login(a2);

    MegaUser *user = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    if (!user || (user->getVisibility() != MegaUser::VISIBILITY_VISIBLE))
    {
        makeContact(a1, a2);
    }
    delete user;
    user = NULL;

    MegaChatHandle chatid = getPeerToPeerChatRoom(a1, a2);

    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_CHAT_TEST(megaChatApi[a1]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account " + std::to_string(a1+1));
    ASSERT_CHAT_TEST(megaChatApi[a2]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account " + std::to_string(a2+1));

    // Load some message to feed history
    loadHistory(a1, chatid, chatroomListener);
    loadHistory(a2, chatid, chatroomListener);

    chatroomListener->clearMessages(a1);
    chatroomListener->clearMessages(a2);
    std::string formatDate = dateToString();

    MegaChatMessage *msgSent = sendTextMessageOrUpdate(a1, a2, chatid, formatDate, chatroomListener);
    MegaChatHandle msgId = msgSent->getMsgId();
    bool hasArrived = chatroomListener->hasArrivedMessage(a1, msgId);
    ASSERT_CHAT_TEST(hasArrived, "Id of sent message has not been received yet");
    MegaChatListItem *itemAccount1 = megaChatApi[a1]->getChatListItem(chatid);
    MegaChatListItem *itemAccount2 = megaChatApi[a2]->getChatListItem(chatid);
    ASSERT_CHAT_TEST(strcmp(formatDate.c_str(), itemAccount1->getLastMessage()) == 0,
                     "Content of last-message doesn't match.\n Sent: " + formatDate + " Received: " + itemAccount1->getLastMessage());
    ASSERT_CHAT_TEST(itemAccount1->getLastMessageId() == msgId, "Last message id is different from message sent id");
    ASSERT_CHAT_TEST(itemAccount2->getLastMessageId() == msgId, "Last message id is different from message received id");
    MegaChatMessage *messageConfirm = megaChatApi[a1]->getMessage(chatid, msgId);
    ASSERT_CHAT_TEST(strcmp(messageConfirm->getContent(), itemAccount1->getLastMessage()) == 0,
                     "Content of last-message reported id is different than last-message reported content");

    delete itemAccount1;
    itemAccount1 = NULL;
    delete itemAccount2;
    itemAccount2 = NULL;

    delete msgSent;
    msgSent = NULL;
    delete messageConfirm;
    messageConfirm = NULL;

    clearHistory(a1, a2, chatid, chatroomListener);
    chatroomListener->clearMessages(a1);
    chatroomListener->clearMessages(a2);

    formatDate = dateToString();
    createFile(formatDate, LOCAL_PATH, formatDate);
    MegaNode* nodeSent = uploadFile(a1, formatDate, LOCAL_PATH, REMOTE_PATH);
    msgSent = attachNode(a1, a2, chatid, nodeSent, chatroomListener);
    MegaNode *nodeReceived = msgSent->getMegaNodeList()->get(0)->copy();
    msgId = msgSent->getMsgId();
    hasArrived = chatroomListener->hasArrivedMessage(a1, msgId);
    ASSERT_CHAT_TEST(hasArrived, "Id of sent message has not been received yet");
    itemAccount1 = megaChatApi[a1]->getChatListItem(chatid);
    itemAccount2 = megaChatApi[a2]->getChatListItem(chatid);
    ASSERT_CHAT_TEST(strcmp(formatDate.c_str(), itemAccount1->getLastMessage()) == 0,
                     "Last message content differs from content of message sent.\n Sent: " + formatDate + " Received: " + itemAccount1->getLastMessage());
    ASSERT_CHAT_TEST(itemAccount1->getLastMessageId() == msgId, "Last message id is different from message sent id");
    ASSERT_CHAT_TEST(itemAccount2->getLastMessageId() == msgId, "Last message id is different from message received id");
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
 * @brief TEST_SendContact
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
void MegaChatApiTest::TEST_SendContact(unsigned int a1, unsigned int a2)
{
    char *primarySession = login(a1);
    char *secondarySession = login(a2);

    MegaUser *user = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    if (!user || (user->getVisibility() != MegaUser::VISIBILITY_VISIBLE))
    {
        makeContact(a1, a2);
    }
    delete user;
    user = NULL;

    MegaChatHandle chatid = getPeerToPeerChatRoom(a1, a2);

    // 1. A sends a message to B while B has the chat opened.
    // --> check the confirmed in A, the received message in B, the delivered in A

    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_CHAT_TEST(megaChatApi[a1]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account 1");
    ASSERT_CHAT_TEST(megaChatApi[a2]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account 2");

    loadHistory(a1, chatid, chatroomListener);
    loadHistory(a2, chatid, chatroomListener);

    bool *flagConfirmed = &chatroomListener->msgConfirmed[a1]; *flagConfirmed = false;
    bool *flagReceived = &chatroomListener->msgContactReceived[a2]; *flagReceived = false;
    bool *flagDelivered = &chatroomListener->msgDelivered[a1]; *flagDelivered = false;
    chatroomListener->clearMessages(a1);
    chatroomListener->clearMessages(a2);

    user = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    ASSERT_CHAT_TEST(user, "Failed to get contact with email" + mAccounts[a2].getEmail());
    MegaChatHandle uh1 = user->getHandle();
    delete user;
    user = NULL;

    MegaHandleList* contactList = MegaHandleList::createInstance();
    contactList->addMegaHandle(uh1);
    MegaChatMessage *messageSent = megaChatApi[a1]->attachContacts(chatid, contactList);

    ASSERT_CHAT_TEST(waitForResponse(flagConfirmed), "Timeout expired for receiving confirmation by server");
    MegaChatHandle msgId0 = chatroomListener->mConfirmedMessageHandle[a1];
    ASSERT_CHAT_TEST(msgId0 != MEGACHAT_INVALID_HANDLE, "Wrong message id at origin");

    ASSERT_CHAT_TEST(waitForResponse(flagReceived), "Timeout expired for receiving message by target user");    // for reception
    ASSERT_CHAT_TEST(chatroomListener->hasArrivedMessage(a2, msgId0), "Wrong message id at destination");
    MegaChatMessage *msgReceived = megaChatApi[a2]->getMessage(chatid, msgId0);   // message should be already received, so in RAM
    ASSERT_CHAT_TEST(msgReceived, "Failed to get message by id");

    ASSERT_CHAT_TEST(msgReceived->getType() == MegaChatMessage::TYPE_CONTACT_ATTACHMENT, "Wrong type of message. Type: " + std::to_string(msgReceived->getType()));
    ASSERT_CHAT_TEST(msgReceived->getUsersCount() == 1, "Wrong number of users in message. Count: " + std::to_string(msgReceived->getUsersCount()));
    ASSERT_CHAT_TEST(strcmp(msgReceived->getUserEmail(0), mAccounts[a2].getEmail().c_str()) == 0, "Wrong email address in message. Address: " + std::string(msgReceived->getUserEmail(0)));

    // Check if reception confirmation is active and, in this case, only 1on1 rooms have acknowledgement of receipt
    if (megaChatApi[a1]->isMessageReceptionConfirmationActive()
            && !megaChatApi[a1]->getChatRoom(chatid)->isGroup())
    {
        ASSERT_CHAT_TEST(waitForResponse(flagDelivered), "Timeout expired for receiving delivery notification");    // for delivery
    }

    flagConfirmed = &chatroomListener->msgConfirmed[a2]; *flagConfirmed = false;
    flagReceived = &chatroomListener->msgContactReceived[a1]; *flagReceived = false;
    flagDelivered = &chatroomListener->msgDelivered[a2]; *flagDelivered = false;
    chatroomListener->clearMessages(a1);
    chatroomListener->clearMessages(a2);

    MegaChatMessage *messageForwared = megaChatApi[a2]->forwardContact(chatid, msgId0, chatid);
    ASSERT_CHAT_TEST(messageForwared, "Failed to forward a contact message");
    ASSERT_CHAT_TEST(waitForResponse(flagConfirmed), "Timeout expired for receiving confirmation by server");
    MegaChatHandle msgId1 = chatroomListener->mConfirmedMessageHandle[a2];
    ASSERT_CHAT_TEST(msgId1 != MEGACHAT_INVALID_HANDLE, "Wrong message id at origin");

    ASSERT_CHAT_TEST(waitForResponse(flagReceived), "Timeout expired for receiving message by target user");    // for reception
    ASSERT_CHAT_TEST(chatroomListener->hasArrivedMessage(a1, msgId1), "Wrong message id at destination");
    MegaChatMessage *msgReceived1 = megaChatApi[a2]->getMessage(chatid, msgId1);   // message should be already received, so in RAM
    ASSERT_CHAT_TEST(msgReceived1, "Failed to get message by id");

    ASSERT_CHAT_TEST(msgReceived1->getType() == MegaChatMessage::TYPE_CONTACT_ATTACHMENT, "Wrong type of message. Type: " + std::to_string(msgReceived1->getType()));
    ASSERT_CHAT_TEST(msgReceived1->getUsersCount() == 1, "Wrong number of users in message. Count: " + std::to_string(msgReceived1->getUsersCount()));
    ASSERT_CHAT_TEST(strcmp(msgReceived1->getUserEmail(0), mAccounts[a2].getEmail().c_str()) == 0, "Wrong email address in message. Address: " + std::string(msgReceived1->getUserEmail(0)));

    // Check if reception confirmation is active and, in this case, only 1on1 rooms have acknowledgement of receipt
    if (megaChatApi[a2]->isMessageReceptionConfirmationActive()
            && !megaChatApi[a2]->getChatRoom(chatid)->isGroup())
    {
        ASSERT_CHAT_TEST(waitForResponse(flagDelivered), "Timeout expired for receiving delivery notification");    // for delivery
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
 * @brief TEST_GroupLastMessage
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
void MegaChatApiTest::TEST_GroupLastMessage(unsigned int a1, unsigned int a2)
{
    char *session0 = login(a1);
    char *session1 = login(a2);

    // Prepare peers, privileges...
    MegaUser *user = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    if (!user || (user->getVisibility() != MegaUser::VISIBILITY_VISIBLE))
    {
        makeContact(a1, a2);
        delete user;
        user = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    }

    MegaChatHandle uh = user->getHandle();
    delete user;
    user = NULL;

    MegaChatPeerList *peers = MegaChatPeerList::createInstance();
    peers->addPeer(uh, MegaChatPeerList::PRIV_STANDARD);

    MegaChatHandle chatid = getGroupChatRoom(a1, a2, peers);
    delete peers;
    peers = NULL;

    // --> Open chatroom
    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_CHAT_TEST(megaChatApi[a1]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account 1");
    ASSERT_CHAT_TEST(megaChatApi[a2]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account 2");

    // Load some message to feed history
    loadHistory(a1, chatid, chatroomListener);
    loadHistory(a2, chatid, chatroomListener);

    chatroomListener->clearMessages(a1);
    chatroomListener->clearMessages(a2);

    std::string textToSend = "Last Message";
    MegaChatMessage *msgSent = sendTextMessageOrUpdate(a1, a2, chatid, textToSend, chatroomListener);
    MegaChatHandle msgId = msgSent->getMsgId();
    bool hasArrived = chatroomListener->hasArrivedMessage(a1, msgId);
    ASSERT_CHAT_TEST(hasArrived, "Id of sent message has not been received yet");
    MegaChatListItem *itemAccount1 = megaChatApi[a1]->getChatListItem(chatid);
    MegaChatListItem *itemAccount2 = megaChatApi[a2]->getChatListItem(chatid);
    ASSERT_CHAT_TEST(itemAccount1->getLastMessageId() == msgId, "Last message id is different from message sent id");
    ASSERT_CHAT_TEST(itemAccount2->getLastMessageId() == msgId, "Last message id is different from message received id");
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
    ASSERT_CHAT_TEST(waitForResponse(flagChatRoomName), "Timeout expired for changing name");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to change name. Error: " + lastErrorMsgChat[a1] + " (" + std::to_string(lastErrorChat[a1]) + ")");
    ASSERT_CHAT_TEST(waitForResponse(titleItemChanged0), "Timeout expired for receiving chat list title update for main account");
    ASSERT_CHAT_TEST(waitForResponse(titleItemChanged1), "Timeout expired for receiving chat list title update for auxiliar account");
    ASSERT_CHAT_TEST(waitForResponse(titleChanged0), "Timeout expired for receiving chatroom title update for main account");
    ASSERT_CHAT_TEST(waitForResponse(titleChanged1), "Timeout expired for receiving chatroom title update for auxiliar account");
    ASSERT_CHAT_TEST(waitForResponse(mngMsgRecv), "Timeout expired for receiving management");
    ASSERT_CHAT_TEST(!strcmp(title.c_str(), msgContent->c_str()),
                     "Title name has not changed correctly.\nName established by a1: " + title + "\nName received in a2: " + *msgContent);
    MegaChatHandle managementMsg1 = chatroomListener->msgId[a1].back();
    MegaChatHandle managementMsg2 = chatroomListener->msgId[a2].back();

    itemAccount1 = megaChatApi[a1]->getChatListItem(chatid);
    itemAccount2 = megaChatApi[a2]->getChatListItem(chatid);
    ASSERT_CHAT_TEST(strcmp(title.c_str(), itemAccount1->getLastMessage()) == 0,
                     "Last message content has not the tittle at account 1.\n Tittle: " + title + " .Last message: " + itemAccount1->getLastMessage());

    ASSERT_CHAT_TEST(strcmp(title.c_str(), itemAccount2->getLastMessage()) == 0,
                     "Last message content has not the tittle at account 2.\n Tittle: " + title + " .Last message: " + itemAccount2->getLastMessage());

    ASSERT_CHAT_TEST(itemAccount1->getLastMessageId() == managementMsg1, "Last message id is different from management message id at account1");
    ASSERT_CHAT_TEST(itemAccount2->getLastMessageId() == managementMsg2, "Last message id is different from management message id at account2");
    ASSERT_CHAT_TEST(itemAccount2->getLastMessageId() == itemAccount1->getLastMessageId(), "Last message id is different from account1 and account2");

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
 * @brief TEST_RetentionHistory
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
void MegaChatApiTest::TEST_RetentionHistory(unsigned int a1, unsigned int a2)
{
    // Login both accounts
    ::mega::unique_ptr<char[]>sessionPrimary(login(a1));
    ::mega::unique_ptr<char[]>sessionSecondary(login(a2));

    // Prepare peers, privileges...
    ::mega::unique_ptr<MegaUser>user(megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str()));
    if (!user || (user->getVisibility() != MegaUser::VISIBILITY_VISIBLE))
    {
        makeContact(a1, a2);
        user.reset(megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str()));
    }

    // Get a group chatroom with both users
    MegaChatHandle uh = user->getHandle();
    ::mega::unique_ptr<MegaChatPeerList> peers(MegaChatPeerList::createInstance());
    peers->addPeer(uh, MegaChatPeerList::PRIV_STANDARD);
    MegaChatHandle chatid = getGroupChatRoom(a1, a2, peers.get());

    // Open chatroom
    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_CHAT_TEST(megaChatApi[a1]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account " + std::to_string(a1+1));
    ASSERT_CHAT_TEST(megaChatApi[a2]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account " + std::to_string(a2+1));
    ::mega::unique_ptr <MegaChatRoom> chatroom (megaChatApi[a1]->getChatRoom(chatid));
    ::mega::unique_ptr<char[]> chatidB64(megaApi[a1]->handleToBase64(chatid));
    ASSERT_CHAT_TEST(chatroom, "Cannot get chatroom for id" + std::string(chatidB64.get()));

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
        ASSERT_CHAT_TEST(waitForResponse(flagUpdatePeerPermision), "Timeout expired for update privilege of peer");
        ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to update privilege of peer Error: " + lastErrorMsgChat[a1] + " (" + std::to_string(lastErrorChat[a1]) + ")");
        ASSERT_CHAT_TEST(waitForResponse(peerUpdated0), "Timeout expired for receiving peer update");
        ASSERT_CHAT_TEST(waitForResponse(peerUpdated1), "Timeout expired for receiving peer update");
        ASSERT_CHAT_TEST(waitForResponse(mngMsgRecv), "Timeout expired for receiving management message");
        ASSERT_CHAT_TEST(*uhAction == uh, "User handle from message doesn't match");
        ASSERT_CHAT_TEST(*priv == MegaChatRoom::PRIV_RO, "Privilege is incorrect");
    }

    // Set retention time for an invalid handle
    bool *flagChatRetentionTime = &requestFlagsChat[a2][MegaChatRequest::TYPE_SET_RETENTION_TIME]; *flagChatRetentionTime = false;
    megaChatApi[a2]->setChatRetentionTime(MEGACHAT_INVALID_HANDLE, 1);
    ASSERT_CHAT_TEST(waitForResponse(flagChatRetentionTime), "Timeout expired set chat retention time");
    ASSERT_CHAT_TEST(lastErrorChat[a2] == MegaChatError::ERROR_ARGS, "Set retention time: Unexpected error for Invalid handle. Error:" + std::string(lastErrorMsgChat[a2]));

    // Set retention time for a not found chatroom
    flagChatRetentionTime = &requestFlagsChat[a2][MegaChatRequest::TYPE_SET_RETENTION_TIME]; *flagChatRetentionTime = false;
    megaChatApi[a2]->setChatRetentionTime(123456, 1);
    ASSERT_CHAT_TEST(waitForResponse(flagChatRetentionTime), "Timeout expired set chat retention time");
    ASSERT_CHAT_TEST(lastErrorChat[a2] == MegaChatError::ERROR_NOENT, "Set retention time: Unexpected error for a not found chatroom. Error:" + std::string(lastErrorMsgChat[a2]));

    // Set retention time without enough permissions
    flagChatRetentionTime = &requestFlagsChat[a2][MegaChatRequest::TYPE_SET_RETENTION_TIME]; *flagChatRetentionTime = false;
    megaChatApi[a2]->setChatRetentionTime(chatid, 1);
    ASSERT_CHAT_TEST(waitForResponse(flagChatRetentionTime), "Timeout expired set chat retention time");
    ASSERT_CHAT_TEST(lastErrorChat[a2] == MegaChatError::ERROR_ACCESS, "Set retention time: Unexpected error for not enough permissions. Error:" + std::string(lastErrorMsgChat[a2]));

    // Disable retention time
    if (chatroom->getRetentionTime() != 0)
    {
        // Disable retention time if any
        bool *retentionTimeChanged0 = &chatroomListener->retentionTimeUpdated[a1]; *retentionTimeChanged0 = false;
        bool *retentionTimeChanged1 = &chatroomListener->retentionTimeUpdated[a2]; *retentionTimeChanged1 = false;
        bool *mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
        flagChatRetentionTime = &requestFlagsChat[a1][MegaChatRequest::TYPE_SET_RETENTION_TIME]; *flagChatRetentionTime = false;
        megaChatApi[a1]->setChatRetentionTime(chatid, 0);
        ASSERT_CHAT_TEST(waitForResponse(flagChatRetentionTime), "Timeout expired set chat retention time");
        ASSERT_CHAT_TEST(lastErrorChat[a1] == MegaChatError::ERROR_OK, "Set retention time: Unexpected error. Error:" + std::string(lastErrorMsgChat[a1]));
        ASSERT_CHAT_TEST(waitForResponse(retentionTimeChanged0), "Timeout expired for receiving chatroom update");
        ASSERT_CHAT_TEST(waitForResponse(retentionTimeChanged1), "Timeout expired for receiving chatroom update");
        ASSERT_CHAT_TEST(waitForResponse(mngMsgRecv), "Timeout expired for receiving management message");
    }

    // Send 5 messages
    std::string messageToSend = "Msg from " +mAccounts[a1].getEmail();
    for (int i = 0; i < 5; i++)
    {
        sendTextMessageOrUpdate(a1, a2, chatid, messageToSend, chatroomListener);
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
    ASSERT_CHAT_TEST(waitForResponse(flagChatRetentionTime), "Timeout expired set chat retention time");
    ASSERT_CHAT_TEST(lastErrorChat[a1] == MegaChatError::ERROR_OK, "Set retention time: Unexpected error. Error:" + std::string(lastErrorMsgChat[a1]));
    ASSERT_CHAT_TEST(waitForResponse(retentionTimeChanged0), "Timeout expired for receiving chatroom update");
    ASSERT_CHAT_TEST(waitForResponse(retentionTimeChanged1), "Timeout expired for receiving chatroom update");
    ASSERT_CHAT_TEST(waitForResponse(mngMsgRecv), "Timeout expired for receiving management message");

    // Wait a considerable time period to ensure that retentionTime has been processed successfully
    sleep(chatd::Client::kMinRetentionTimeout + 10);
    ASSERT_CHAT_TEST(waitForResponse(flagConfirmed0), "Retention history autotruncate hasn't been received for account" + std::to_string(a1+1) + " after timeout: " +  std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(*msgId0 != MEGACHAT_INVALID_HANDLE, "Wrong message id");
    ASSERT_CHAT_TEST(waitForResponse(flagConfirmed1), "Retention history autotruncate hasn't been received for account" + std::to_string(a2+1) + " after timeout: " +  std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(*msgId1 != MEGACHAT_INVALID_HANDLE, "Wrong message id");
    ASSERT_CHAT_TEST(!loadHistory(a1, chatid, chatroomListener), "History should be empty after retention history autotruncate");

    // Disable retention time
    retentionTimeChanged0 = &chatroomListener->retentionTimeUpdated[a1]; *retentionTimeChanged0 = false;
    retentionTimeChanged1 = &chatroomListener->retentionTimeUpdated[a2]; *retentionTimeChanged1 = false;
    mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
    flagChatRetentionTime = &requestFlagsChat[a1][MegaChatRequest::TYPE_SET_RETENTION_TIME]; *flagChatRetentionTime = false;
    megaChatApi[a1]->setChatRetentionTime(chatid, 0);
    ASSERT_CHAT_TEST(waitForResponse(flagChatRetentionTime), "Timeout expired set chat retention time");
    ASSERT_CHAT_TEST(lastErrorChat[a1] == MegaChatError::ERROR_OK, "Set retention time: Unexpected error. Error:" + std::string(lastErrorMsgChat[a1]));
    ASSERT_CHAT_TEST(waitForResponse(retentionTimeChanged0), "Timeout expired for receiving chatroom update");
    ASSERT_CHAT_TEST(waitForResponse(retentionTimeChanged1), "Timeout expired for receiving chatroom update");
    ASSERT_CHAT_TEST(waitForResponse(mngMsgRecv), "Timeout expired for receiving management message");

    // Send 5 messages
    messageToSend = "Msg from " +mAccounts[a1].getEmail();
    for (int i = 0; i < 5; i++)
    {
        sendTextMessageOrUpdate(a1, a2, chatid, messageToSend, chatroomListener);
    }

    // Close chatrooms
    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);
    delete chatroomListener;

    // Logout and login
    logout(a1, true);
    sessionPrimary.reset(login(a1));
    logout(a2, true);
    sessionSecondary.reset(login(a2));

    // Open chatroom
    chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_CHAT_TEST(megaChatApi[a1]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account " + std::to_string(a1+1));
    ASSERT_CHAT_TEST(megaChatApi[a2]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account " + std::to_string(a2+1));

    // Check history has 5 messages + setRetentionTime management message
    int count = loadHistory(a1, chatid, chatroomListener);
    ASSERT_CHAT_TEST(count == 6 || count == 7, "Wrong count of messages: " + std::to_string(count));
    count = loadHistory(a2, chatid, chatroomListener);
    ASSERT_CHAT_TEST(count == 6 || count == 7, "Wrong count of messages: " + std::to_string(count));

    // Close the chatrooms
    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);
    delete chatroomListener;
}

/**
 * @brief TEST_ChangeMyOwnName
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
void MegaChatApiTest::TEST_ChangeMyOwnName(unsigned int a1)
{
    char *sessionPrimary = login(a1);
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
    changeLastName(a1, newLastName);

    nameFromApi = megaChatApi[a1]->getMyLastname();
    std::string finalLastName;
    if (nameFromApi)
    {
        finalLastName = nameFromApi;
        delete [] nameFromApi;
        nameFromApi = NULL;
    }

    logout(a1, false);

    char *newSession = login(a1, sessionPrimary);

    nameFromApi = megaChatApi[a1]->getMyLastname();
    std::string lastNameAfterLogout;
    if (nameFromApi)
    {
        lastNameAfterLogout = nameFromApi;
        delete [] nameFromApi;
        nameFromApi = NULL;
    }

    //Name comes back to old value.
    changeLastName(a1, myAccountLastName);

    ASSERT_CHAT_TEST(newLastName == finalLastName,
                     "Failed to change fullname (checked from memory) Name established: \""
                     + newLastName + "\" Name in memory: \"" + finalLastName + "\"");
    ASSERT_CHAT_TEST(lastNameAfterLogout == finalLastName,
                     "Failed to change fullname (checked from DB) Name established: \""
                     + finalLastName + "\" Name in DB: \"" + lastNameAfterLogout + "\"");

    delete [] sessionPrimary;
    sessionPrimary = NULL;

    delete [] newSession;
    newSession = NULL;
}

#ifndef KARERE_DISABLE_WEBRTC
/**
 * @brief TEST_Calls
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
void MegaChatApiTest::TEST_Calls(unsigned int a1, unsigned int a2)
{
    char *primarySession = login(a1);
    char *secondarySession = login(a2);

    MegaUser *user = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    if (!user || user->getVisibility() != MegaUser::VISIBILITY_VISIBLE)
    {
        makeContact(a1, a2);
    }
    delete user;

    MegaChatHandle chatid = getPeerToPeerChatRoom(a1, a2);

    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);

    ASSERT_CHAT_TEST(megaChatApi[a1]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account 1");
    ASSERT_CHAT_TEST(megaChatApi[a2]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account 2");

    loadHistory(a1, chatid, chatroomListener);
    loadHistory(a2, chatid, chatroomListener);

    bool *audioVideoDeviceListLoaded0 = &requestFlagsChat[a1][MegaChatRequest::TYPE_LOAD_AUDIO_VIDEO_DEVICES]; *audioVideoDeviceListLoaded0 = false;
    megaChatApi[a1]->loadAudioVideoDeviceList();
    ASSERT_CHAT_TEST(waitForResponse(audioVideoDeviceListLoaded0), "Timeout expired for load audio video devices in account 1");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to load Devide list account 1: " + std::to_string(lastErrorChat[a1]));

    bool *audioVideoDeviceListLoaded1 = &requestFlagsChat[a2][MegaChatRequest::TYPE_LOAD_AUDIO_VIDEO_DEVICES]; *audioVideoDeviceListLoaded1 = false;
    megaChatApi[a2]->loadAudioVideoDeviceList();
    ASSERT_CHAT_TEST(waitForResponse(audioVideoDeviceListLoaded1), "Timeout expired for load audio video devices in account 2");
    ASSERT_CHAT_TEST(!lastErrorChat[a2], "Failed to load Devide list account 2: " + std::to_string(lastErrorChat[a2]));

    mLocalVideoListener[a1] = new TestChatVideoListener();
    mLocalVideoListener[a2] = new TestChatVideoListener();
    megaChatApi[a1]->addChatLocalVideoListener(chatid, mLocalVideoListener[a1]);
    megaChatApi[a2]->addChatLocalVideoListener(chatid, mLocalVideoListener[a2]);
    // Remote video listener aren't necessary because call is never ging to be answered at tests

    // A calls B and B hangs up the call
    bool *flagStartCall = &requestFlagsChat[a1][MegaChatRequest::TYPE_START_CHAT_CALL]; *flagStartCall = false;
    bool *callReceived = &mCallReceived[a2]; *callReceived = false;
    mChatIdRingInCall[a2] = MEGACHAT_INVALID_HANDLE;
    bool *termLocal0 = &mTerminationLocal[a1]; *termLocal0 = false;
    bool *termLocal1 = &mTerminationLocal[a2]; *termLocal1 = false;
    bool *callDestroyed0 = &mCallDestroyed[a1]; *callDestroyed0 = false;
    bool *callDestroyed1 = &mCallDestroyed[a2]; *callDestroyed1 = false;
    int *termCode0 = &mTerminationCode[a1]; *termCode0 = MegaChatCall::TERM_CODE_NOT_FINISHED;
    int *termCode1 = &mTerminationCode[a2]; *termCode1 = MegaChatCall::TERM_CODE_NOT_FINISHED;
    bool *flagHangUpCall = &requestFlagsChat[a2][MegaChatRequest::TYPE_HANG_CHAT_CALL]; *flagHangUpCall = false;
    mCallIdRingIn[a2] = MEGACHAT_INVALID_HANDLE;
    mCallIdRequestSent[a1] = MEGACHAT_INVALID_HANDLE;
    bool *flagPeerRinging = &mPeerIsRinging[a1]; *flagPeerRinging = false;
    mVideoLocal[a1] = true;
    megaChatApi[a1]->startChatCall(chatid, mVideoLocal[a1]);
    ASSERT_CHAT_TEST(waitForResponse(flagStartCall), "Timeout after start chat call " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to start chat call: " + std::to_string(lastErrorChat[a1]));

    ASSERT_CHAT_TEST(waitForResponse(callReceived), "Timeout expired for receiving a call");
    ASSERT_CHAT_TEST(mChatIdRingInCall[a2] == chatid, "Incorrect chat id at call receptor");
    ASSERT_CHAT_TEST(mCallIdRequestSent[a1] == mCallIdRingIn[a2], "Differents call id between caller and answer");
    MegaChatCall *call = megaChatApi[a2]->getChatCall(chatid);
    ASSERT_CHAT_TEST(mVideoLocal[a1] == call->hasVideoInitialCall(), "Video flags must have the same values at local account 1 and remote account 2");
    delete call;
    ASSERT_CHAT_TEST(waitForResponse(flagPeerRinging), "Remote Peer hasn't started to ring");

    sleep(5);

    megaChatApi[a2]->hangChatCall(chatid);
    ASSERT_CHAT_TEST(waitForResponse(flagHangUpCall), "Timeout after hang up chat call " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(!lastErrorChat[a2], "Failed to hang up chat call: " + std::to_string(lastErrorChat[a2]));
    ASSERT_CHAT_TEST(waitForResponse(callDestroyed0), "The call has to be finished account 1");
    ASSERT_CHAT_TEST(waitForResponse(callDestroyed1), "The call has to be finished account 2");

    ASSERT_CHAT_TEST(*termCode0 == MegaChatCall::TERM_CODE_CALL_REJECT && *termCode0 == *termCode1,
                     "Invalid Termination code. TermCode1: "
                     + std::to_string(*termCode0)
                     + "  TermCode2: "
                     + std::to_string(*termCode1));

    ASSERT_CHAT_TEST(*termLocal0 == false && *termLocal0 != *termLocal1, "Invalid Termination local");

    // A calls B and A hangs up the call before B answers
    flagStartCall = &requestFlagsChat[a1][MegaChatRequest::TYPE_START_CHAT_CALL]; *flagStartCall = false;
    callReceived = &mCallReceived[a2]; *callReceived = false;
    mChatIdRingInCall[a2] = MEGACHAT_INVALID_HANDLE;
    termLocal0 = &mTerminationLocal[a1]; *termLocal0 = false;
    termLocal1 = &mTerminationLocal[a2]; *termLocal1 = false;
    callDestroyed0 = &mCallDestroyed[a1]; *callDestroyed0 = false;
    callDestroyed1 = &mCallDestroyed[a2]; *callDestroyed1 = false;
    termCode0 = &mTerminationCode[a1]; *termCode0 = MegaChatCall::TERM_CODE_NOT_FINISHED;
    termCode1 = &mTerminationCode[a2]; *termCode1 = MegaChatCall::TERM_CODE_NOT_FINISHED;
    flagHangUpCall = &requestFlagsChat[a1][MegaChatRequest::TYPE_HANG_CHAT_CALL]; *flagHangUpCall = false;
    mCallIdRingIn[a2] = MEGACHAT_INVALID_HANDLE;
    mCallIdRequestSent[a1] = MEGACHAT_INVALID_HANDLE;
    flagPeerRinging = &mPeerIsRinging[a1]; *flagPeerRinging = false;
    mVideoLocal[a1] = false;
    megaChatApi[a1]->startChatCall(chatid, mVideoLocal[a1]);
    ASSERT_CHAT_TEST(waitForResponse(flagStartCall), "Timeout after start chat call " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to start chat call: " + std::to_string(lastErrorChat[a1]));

    ASSERT_CHAT_TEST(waitForResponse(callReceived), "Timeout expired for receiving a call");
    ASSERT_CHAT_TEST(mChatIdRingInCall[a2] == chatid, "Incorrect chat id at call receptor");
    ASSERT_CHAT_TEST(mCallIdRequestSent[a1] == mCallIdRingIn[a2], "Differents call id between caller and answer");
    call = megaChatApi[a2]->getChatCall(chatid);
    ASSERT_CHAT_TEST(mVideoLocal[a1] == call->hasVideoInitialCall(), "Video flags must have the same values at local account 1 and remote account 2");
    delete call;
    ASSERT_CHAT_TEST(waitForResponse(flagPeerRinging), "Remote Peer hasn't started to ring");

    sleep(5);

    megaChatApi[a1]->hangChatCall(chatid);
    ASSERT_CHAT_TEST(waitForResponse(flagHangUpCall), "Timeout after hang up chat call " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to hang up chat call: " + std::to_string(lastErrorChat[a1]));
    ASSERT_CHAT_TEST(waitForResponse(callDestroyed0), "The call has to be finished account 1");
    ASSERT_CHAT_TEST(waitForResponse(callDestroyed1), "The call has to be finished account 2");

    ASSERT_CHAT_TEST(*termCode0 == MegaChatCall::TERM_CODE_CALL_REQ_CANCEL && *termCode0 == *termCode1,
                     "Invalid Termination code. TermCode1: "
                     + std::to_string(*termCode0)
                     + "  TermCode2: "
                     + std::to_string(*termCode1));

    ASSERT_CHAT_TEST(*termLocal0 == true && *termLocal0 != *termLocal1, "Invalid Termination local");

    // A calls B(B is logged out), B logins, B receives the call and B hangs up the call
    logout(a2);
    flagStartCall = &requestFlagsChat[a1][MegaChatRequest::TYPE_START_CHAT_CALL]; *flagStartCall = false;
    callReceived = &mCallReceived[a2]; *callReceived = false;
    mChatIdRingInCall[a2] = MEGACHAT_INVALID_HANDLE;
    termLocal0 = &mTerminationLocal[a1]; *termLocal0 = false;
    termLocal1 = &mTerminationLocal[a2]; *termLocal1 = false;
    callDestroyed0 = &mCallDestroyed[a1]; *callDestroyed0 = false;
    callDestroyed1 = &mCallDestroyed[a2]; *callDestroyed1 = false;
    termCode0 = &mTerminationCode[a1]; *termCode0 = MegaChatCall::TERM_CODE_NOT_FINISHED;
    termCode1 = &mTerminationCode[a2]; *termCode1 = MegaChatCall::TERM_CODE_NOT_FINISHED;
    flagHangUpCall = &requestFlagsChat[a2][MegaChatRequest::TYPE_HANG_CHAT_CALL]; *flagHangUpCall = false;
    mCallIdRingIn[a2] = MEGACHAT_INVALID_HANDLE;
    mCallIdRequestSent[a1] = MEGACHAT_INVALID_HANDLE;
    flagPeerRinging = &mPeerIsRinging[a1]; *flagPeerRinging = false;
    mVideoLocal[a1] = true;
    megaChatApi[a1]->startChatCall(chatid, mVideoLocal[a1]);
    ASSERT_CHAT_TEST(waitForResponse(flagStartCall), "Timeout after start chat call " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to start chat call: " + std::to_string(lastErrorChat[a1]));

    char *secondarySession2 = login(a2, secondarySession);

    ASSERT_CHAT_TEST(waitForResponse(callReceived), "Timeout expired for receiving a call");
    ASSERT_CHAT_TEST(mChatIdRingInCall[a2] == chatid, "Incorrect chat id at call receptor");
    ASSERT_CHAT_TEST(mCallIdRequestSent[a1] == mCallIdRingIn[a2], "Differents call id between caller and answer");
    call = megaChatApi[a2]->getChatCall(chatid);
    ASSERT_CHAT_TEST(mVideoLocal[a1] == call->hasVideoInitialCall(), "Video flags must have the same values at local account 1 and remote account 2");
    delete call;
    ASSERT_CHAT_TEST(waitForResponse(flagPeerRinging), "Remote Peer hasn't started to ring");

    sleep(5);

    megaChatApi[a2]->hangChatCall(chatid);
    ASSERT_CHAT_TEST(waitForResponse(flagHangUpCall), "Timeout after hang up chat call " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(!lastErrorChat[a2], "Failed to hang up chat call: " + std::to_string(lastErrorChat[a2]));
    ASSERT_CHAT_TEST(waitForResponse(callDestroyed0), "The call has to be finished account 1");
    ASSERT_CHAT_TEST(waitForResponse(callDestroyed1), "The call has to be finished account 2");

    ASSERT_CHAT_TEST(*termCode0 == MegaChatCall::TERM_CODE_CALL_REJECT && *termCode0 == *termCode1,
                     "Invalid Termination code. TermCode1: "
                     + std::to_string(*termCode0)
                     + "  TermCode2: "
                     + std::to_string(*termCode1));

    ASSERT_CHAT_TEST(*termLocal0 == false && *termLocal0 != *termLocal1, "Invalid Termination local");

    // A calls B and B doesn't answer the call
    flagStartCall = &requestFlagsChat[a1][MegaChatRequest::TYPE_START_CHAT_CALL]; *flagStartCall = false;
    callReceived = &mCallReceived[a2]; *callReceived = false;
    mChatIdRingInCall[a2] = MEGACHAT_INVALID_HANDLE;
    termLocal0 = &mTerminationLocal[a1]; *termLocal0 = false;
    termLocal1 = &mTerminationLocal[a2]; *termLocal1 = false;
    callDestroyed0 = &mCallDestroyed[a1]; *callDestroyed0 = false;
    callDestroyed1 = &mCallDestroyed[a2]; *callDestroyed1 = false;
    termCode0 = &mTerminationCode[a1]; *termCode0 = MegaChatCall::TERM_CODE_NOT_FINISHED;
    termCode1 = &mTerminationCode[a2]; *termCode1 = MegaChatCall::TERM_CODE_NOT_FINISHED;
    mCallIdRingIn[a2] = MEGACHAT_INVALID_HANDLE;
    mCallIdRequestSent[a1] = MEGACHAT_INVALID_HANDLE;
    megaChatApi[a1]->startChatCall(chatid, true);
    ASSERT_CHAT_TEST(waitForResponse(flagStartCall), "Timeout after start chat call " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to start chat call: " + std::to_string(lastErrorChat[a1]));

    ASSERT_CHAT_TEST(waitForResponse(callReceived), "Timeout expired for receiving a call");
    ASSERT_CHAT_TEST(mChatIdRingInCall[a2] == chatid, "Incorrect chat id at call receptor");
    ASSERT_CHAT_TEST(mCallIdRequestSent[a1] == mCallIdRingIn[a2], "Differents call id between caller and answer");

    ASSERT_CHAT_TEST(waitForResponse(callDestroyed0), "The call has to be finished account 1");
    ASSERT_CHAT_TEST(waitForResponse(callDestroyed1), "The call has to be finished account 2");

    ASSERT_CHAT_TEST(*termCode0 == MegaChatCall::TERM_CODE_ANSWER_TIMEOUT && *termCode0 == *termCode1,
                     "Invalid Termination code. TermCode1: "
                     + std::to_string(*termCode0)
                     + "  TermCode2: "
                     + std::to_string(*termCode1));

    ASSERT_CHAT_TEST(*termLocal0 == true && *termLocal0 != *termLocal1, "Invalid Termination local");

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
 * @brief TEST_ManualCalls
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
void MegaChatApiTest::TEST_ManualCalls(unsigned int a1, unsigned int a2)
{
    char *primarySession = login(a1);
    char *secondarySession = login(a2);

    MegaUser *user = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    if (!user || user->getVisibility() != MegaUser::VISIBILITY_VISIBLE)
    {
        makeContact(a1, a2);
    }
    delete user;

    MegaChatHandle chatid = getPeerToPeerChatRoom(a1, a2);
    ASSERT_CHAT_TEST((megaChatApi[a1]->getChatConnectionState(chatid) == MegaChatApi::CHAT_CONNECTION_ONLINE),
                     "Not connected to chatd for account " + std::to_string(a1+1) + ": " + mAccounts[a1].getEmail());

    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);

    ASSERT_CHAT_TEST(megaChatApi[a1]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account 1");
    ASSERT_CHAT_TEST(megaChatApi[a2]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account 2");

    loadHistory(a1, chatid, chatroomListener);
    loadHistory(a2, chatid, chatroomListener);
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);
    logout(a2);

    bool *audioVideoDeviceListLoaded = &requestFlagsChat[a1][MegaChatRequest::TYPE_LOAD_AUDIO_VIDEO_DEVICES]; *audioVideoDeviceListLoaded = false;
    megaChatApi[a1]->loadAudioVideoDeviceList();
    ASSERT_CHAT_TEST(waitForResponse(audioVideoDeviceListLoaded), "Timeout expired for load audio video devices");

    ::mega::MegaStringList *videoInDevices = megaChatApi[a1]->getChatVideoInDevices();

    TestChatVideoListener localVideoListener;

    megaChatApi[a1]->addChatLocalVideoListener(chatid, &localVideoListener);

    // Manual Test
    // Emit call
    bool *flagRequest = &requestFlagsChat[a1][MegaChatRequest::TYPE_START_CHAT_CALL]; *flagRequest = false;
    std::cerr << "Start Call" << std::endl;
    megaChatApi[a1]->startChatCall(chatid, true);
    ASSERT_CHAT_TEST(waitForResponse(flagRequest), "Timeout after start chat call " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to start chat call: " + std::to_string(lastErrorChat[a1]));
    bool *callAnswered = &mCallAnswered[a1]; *callAnswered = false;
    ASSERT_CHAT_TEST(waitForResponse(callAnswered), "Timeout expired for receiving a call");
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
    ASSERT_CHAT_TEST(chatCall != NULL, "Invalid chat call at getChatCallByChatId");

    MegaChatCall *chatCall2 = megaChatApi[a1]->getChatCallByCallId(chatCall->getId());
    ASSERT_CHAT_TEST(chatCall2 != NULL, "Invalid chat call at getChatCall");


    bool *callDestroyed= &mCallDestroyed[a1]; *callDestroyed = false;
    sleep(5);
    std::cerr << "Finish Call" << std::endl;
    sleep(2);
    megaChatApi[a1]->hangChatCall(mChatIdInProgressCall[a1]);
    std::cout << "Call finished." << std::endl;

    ASSERT_CHAT_TEST(waitForResponse(callDestroyed), "The call has to be finished");
    megaChatApi[a1]->removeChatLocalVideoListener(chatid, &localVideoListener);

    // Receive call
    std::cout << "Ready to receive calls..." << std::endl;
    bool *callReceived = &mCallReceived[a1]; *callReceived = false;
    mChatIdRingInCall[a1] = MEGACHAT_INVALID_HANDLE;
    ASSERT_CHAT_TEST(waitForResponse(callReceived), "Timeout expired for receiving a call");
    ASSERT_CHAT_TEST(mChatIdRingInCall[a1] != MEGACHAT_INVALID_HANDLE, "Invalid Chatid from call emisor");
    megaChatApi[a1]->answerChatCall(mChatIdRingInCall[a1], true);
    megaChatApi[a1]->addChatLocalVideoListener(chatid, &localVideoListener);

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

    sleep(10);
    std::cerr << "Finish Call" << std::endl;
    sleep(2);
    megaChatApi[a1]->hangChatCall(mChatIdInProgressCall[a1]);
    std::cout << "Call finished." << std::endl;
    sleep(5);

    megaChatApi[a1]->removeChatLocalVideoListener(chatid, &localVideoListener);

    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);

    delete videoInDevices;
    videoInDevices = NULL;

    delete chatroomListener;
    chatroomListener = NULL;

    delete [] primarySession;
    primarySession = NULL;
    delete [] secondarySession;
    secondarySession = NULL;
}

/**
 * @brief TEST_RichLinkUserAttribute
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
void MegaChatApiTest::TEST_RichLinkUserAttribute(unsigned int a1)
{
   char *primarySession = login(a1);

   // Get rich link state
   TestMegaRequestListener requestListener(megaApi[a1], nullptr);
   megaApi[a1]->shouldShowRichLinkWarning(&requestListener);
   ASSERT_CHAT_TEST(requestListener.waitForResponse(), "Expired timeout for rich Link");
   int error = requestListener.getErrorCode();
   ASSERT_CHAT_TEST(!error || error == ::mega::API_ENOENT, "Should show richLink warning. Error: " + std::to_string(error));
   ASSERT_CHAT_TEST(requestListener.getMegaRequest()->getNumDetails() == 1, "Active at shouldShowRichLink");

   // Enable/disable rich link generation
   bool enableRichLink = !(requestListener.getMegaRequest()->getFlag());
   requestListener = TestMegaRequestListener(megaApi[a1], nullptr);
   megaApi[a1]->enableRichPreviews(enableRichLink, &requestListener);
   ASSERT_CHAT_TEST(requestListener.waitForResponse(), "User attribute retrieval not finished after timeout");
   ASSERT_CHAT_TEST(!requestListener.getErrorCode(), "Failed to enable rich preview. Error: " + std::to_string(requestListener.getErrorCode()));

   // Get rich link state
   requestListener = TestMegaRequestListener(megaApi[a1], nullptr);
   megaApi[a1]->shouldShowRichLinkWarning(&requestListener);
   ASSERT_CHAT_TEST(requestListener.waitForResponse(), "Expired timeout for rich Link");
   error = requestListener.getErrorCode();
   ASSERT_CHAT_TEST(!error || error == ::mega::API_ENOENT, "Should show richLink warning. Error: " + std::to_string(error));
   ASSERT_CHAT_TEST(requestListener.getMegaRequest()->getFlag() == false, "Rich link enable/disable has not worked, (Rich link warning hasn't to be shown)");

   // Change value for rich link counter
   int counter = 1;
   requestListener = TestMegaRequestListener(megaApi[a1], nullptr);
   megaApi[a1]->setRichLinkWarningCounterValue(counter, &requestListener);
   ASSERT_CHAT_TEST(requestListener.waitForResponse(), "User attribute retrieval not finished after timeout");
   ASSERT_CHAT_TEST(!requestListener.getErrorCode(), "Failed to set rich preview count. Error: " + std::to_string(requestListener.getErrorCode()));

   requestListener = TestMegaRequestListener(megaApi[a1], nullptr);
   megaApi[a1]->shouldShowRichLinkWarning(&requestListener);
   ASSERT_CHAT_TEST(requestListener.waitForResponse(), "Expired timeout for rich Link");
   error = requestListener.getErrorCode();
   ASSERT_CHAT_TEST(!error || error == ::mega::API_ENOENT, "Should show richLink warning. Error: " + std::to_string(error));
   ASSERT_CHAT_TEST(requestListener.getMegaRequest()->getNumDetails() == 1, "Active at shouldShowRichLink");
   ASSERT_CHAT_TEST(counter == requestListener.getMegaRequest()->getNumber(), "Rich link count has not taken the correct value - value: " + std::to_string(requestListener.getMegaRequest()->getNumber()) + " Desired value: " + std::to_string(counter));
   ASSERT_CHAT_TEST(requestListener.getMegaRequest()->getFlag() == true, "Rich link enable/disable has not worked, (Rich link warning has to be shown)");

   delete [] primarySession;
   primarySession = NULL;
}

/**
 * @brief TEST_Calls
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
void MegaChatApiTest::TEST_ManualGroupCalls(unsigned int a1, const std::string& chatRoomName)
{
    char *primarySession = login(a1);
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

    ASSERT_CHAT_TEST(chatid != MEGACHAT_INVALID_HANDLE, "Chat with title: " + chatRoomName + " not found.");
    ASSERT_CHAT_TEST((megaChatApi[a1]->getChatConnectionState(chatid) == MegaChatApi::CHAT_CONNECTION_ONLINE),
                     "Not connected to chatd for account " + std::to_string(a1+1) + ": " + mAccounts[a1].getEmail());

    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);

    ASSERT_CHAT_TEST(megaChatApi[a1]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account 1");

    loadHistory(a1, chatid, chatroomListener);

    bool *audioVideoDeviceListLoaded = &requestFlagsChat[a1][MegaChatRequest::TYPE_LOAD_AUDIO_VIDEO_DEVICES]; *audioVideoDeviceListLoaded = false;
    megaChatApi[a1]->loadAudioVideoDeviceList();
    ASSERT_CHAT_TEST(waitForResponse(audioVideoDeviceListLoaded), "Timeout expired for load audio video devices");

    ::mega::MegaStringList *videoInDevices = megaChatApi[a1]->getChatVideoInDevices();

    TestChatVideoListener localVideoListener;
    megaChatApi[a1]->addChatLocalVideoListener(chatid, &localVideoListener);

    // ---- MANUAL TEST ----

    // Start call

    bool *flagRequest = &requestFlagsChat[a1][MegaChatRequest::TYPE_START_CHAT_CALL]; *flagRequest = false;
    std::cerr << "Start Call" << std::endl;
    megaChatApi[a1]->startChatCall(chatid, true);
    ASSERT_CHAT_TEST(waitForResponse(flagRequest), "Timeout after start chat call " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to start chat call: " + std::to_string(lastErrorChat[a1]));
    bool *callAnswered = &mCallAnswered[a1]; *callAnswered = false;
    ASSERT_CHAT_TEST(waitForResponse(callAnswered), "Timeout expired for receiving a call");

    std::cout << "Waiting for the other peer to answer the call..." << std::endl;
    sleep(60);

    MegaChatCall *chatCall = megaChatApi[a1]->getChatCall(mChatIdInProgressCall[a1]);
    ASSERT_CHAT_TEST(chatCall != NULL, "Invalid chat call at getChatCall (by chatid)");

    MegaChatCall *chatCall2 = megaChatApi[a1]->getChatCallByCallId(chatCall->getId());
    ASSERT_CHAT_TEST(chatCall2 != NULL, "Invalid chat call at getChatCall (by callid)");

    delete chatCall;    chatCall = NULL;
    delete chatCall2;   chatCall2 = NULL;

    bool *callDestroyed= &mCallDestroyed[a1]; *callDestroyed = false;
    std::cerr << "Finish Call" << std::endl;
    megaChatApi[a1]->hangChatCall(mChatIdInProgressCall[a1]);
    std::cout << "Call finished." << std::endl;
    ASSERT_CHAT_TEST(waitForResponse(callDestroyed), "The call must be already finished and it is not");
    megaChatApi[a1]->removeChatLocalVideoListener(chatid, &localVideoListener);

    // Receive call

    std::cout << "Waiting for the other peer to start a call..." << std::endl;
    sleep(20);

    std::cout << "Ready to receive calls..." << std::endl;
    bool *callReceived = &mCallReceived[a1]; *callReceived = false;
    mChatIdRingInCall[a1] = MEGACHAT_INVALID_HANDLE;
    ASSERT_CHAT_TEST(waitForResponse(callReceived), "Timeout expired for receiving a call");
    ASSERT_CHAT_TEST(mChatIdRingInCall[a1] != MEGACHAT_INVALID_HANDLE, "Invalid Chatid from call emisor");
    megaChatApi[a1]->answerChatCall(mChatIdRingInCall[a1], true);
    megaChatApi[a1]->addChatLocalVideoListener(chatid, &localVideoListener);

    sleep(40);  // wait to receive some traffic
    megaChatApi[a1]->hangChatCall(mChatIdInProgressCall[a1]);
    std::cout << "Call finished." << std::endl;
    megaChatApi[a1]->removeChatLocalVideoListener(chatid, &localVideoListener);

    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);

    delete videoInDevices;
    videoInDevices = NULL;

    delete chatroomListener;
    chatroomListener = NULL;

    delete [] primarySession;
    primarySession = NULL;
}

#endif

/**
 * @brief TEST_SendRichLink
 *
 * This test does the following:
 *
 * - Enable rich links
 * - Send a message with a url
 * - Wait for rich link update
 * - Check if message has been updated with a rich link
 *
 */
void MegaChatApiTest::TEST_SendRichLink(unsigned int a1, unsigned int a2)
{
    char *primarySession = login(a1);
    char *secondarySession = login(a2);

    // Enable rich link
    bool enableRichLink = true;
    TestMegaRequestListener requestListener(megaApi[a1], nullptr);
    megaApi[a1]->enableRichPreviews(enableRichLink, &requestListener);
    ASSERT_CHAT_TEST(requestListener.waitForResponse(), "User attribute retrieval not finished after timeout");
    int error = requestListener.getErrorCode();
    ASSERT_CHAT_TEST(!error, "Failed to enable rich preview. Error: " + std::to_string(error));

    MegaUser *user = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    if (!user || (user->getVisibility() != MegaUser::VISIBILITY_VISIBLE))
    {
        makeContact(a1, a2);
    }
    delete user;
    user = NULL;

    MegaChatHandle chatid = getPeerToPeerChatRoom(a1, a2);

    TestChatRoomListener *chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_CHAT_TEST(megaChatApi[a1]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account " + std::to_string(a1+1));
    ASSERT_CHAT_TEST(megaChatApi[a2]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account " + std::to_string(a2+1));

    // Load some message to feed history
    loadHistory(a1, chatid, chatroomListener);
    loadHistory(a2, chatid, chatroomListener);

    // Send message with url
    std::string messageToSend = "http://mega.nz";
    bool *msgEdited1 = &chatroomListener->msgEdited[a1]; *msgEdited1 = false;
    bool *msgEdited2 = &chatroomListener->msgEdited[a2]; *msgEdited1 = false;
    MegaChatMessage *msgSent = sendTextMessageOrUpdate(a1, a2, chatid, messageToSend, chatroomListener);

    // Wait for update
    ASSERT_CHAT_TEST(waitForResponse(msgEdited1), "Message hasn't been updated with a richLink account" + std::to_string(a1+1) + " after timeout: " +  std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(waitForResponse(msgEdited2), "Message hasn't been updated with a richLink account" + std::to_string(a2+1) + " after timeout: " +  std::to_string(maxTimeout) + " seconds");

    // Check if message has been updated correctly
    MegaChatMessage *msgUpdated = megaChatApi[a1]->getMessage(chatid, msgSent->getMsgId());
    ASSERT_CHAT_TEST(msgUpdated->getType() == MegaChatMessage::TYPE_CONTAINS_META, "Invalid Message Type: " + std::to_string(msgUpdated->getType()) + "(account " + std::to_string(a1+1)+ ")");
    ASSERT_CHAT_TEST(msgUpdated->getContainsMeta() && msgUpdated->getContainsMeta()->getRichPreview(), "Rich link information has not been established (account " + std::to_string(a1+1)+ ")");
    const char *richLink = msgUpdated->getContainsMeta()->getRichPreview()->getText();
    ASSERT_CHAT_TEST(richLink == messageToSend , "Two strings have to have the same value (account " + std::to_string(a1+1)+ "): RichLink -> " + richLink + " Message sent: " + messageToSend);
    delete msgUpdated;
    msgUpdated = NULL;

    msgUpdated = megaChatApi[a2]->getMessage(chatid, msgSent->getMsgId());
    ASSERT_CHAT_TEST(msgUpdated->getType() == MegaChatMessage::TYPE_CONTAINS_META, "Invalid Message Type: " + std::to_string(msgUpdated->getType()) + "(account " + std::to_string(a2+1)+ ")");
    ASSERT_CHAT_TEST(msgUpdated->getContainsMeta() && msgUpdated->getContainsMeta()->getRichPreview(), "Rich link information has not been established (account " + std::to_string(a2+1)+ ")");
    richLink = msgUpdated->getContainsMeta()->getRichPreview()->getText();
    ASSERT_CHAT_TEST(richLink == messageToSend , "Two strings have to have the same value (account " + std::to_string(a2+1)+ "): RichLink -> " + richLink + " Message sent: " + messageToSend);
    delete msgUpdated;
    msgUpdated = NULL;

    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);

    delete msgSent;
    msgSent = NULL;

    delete chatroomListener;

    delete [] primarySession;
    primarySession = NULL;
    delete [] secondarySession;
    secondarySession = NULL;
}

/**
 * @brief TEST_SendGiphy
 *
 * This test does the following:
 *
 * - Send a message with a giphy
 * - Check if the receiver can get get the message correctly
 * - Check if the json can be parsed correctly
 */
void MegaChatApiTest::TEST_SendGiphy(unsigned int a1, unsigned int a2)
{
    TestChatRoomListener* chatroomListener = nullptr;
    MegaUser* user = nullptr;
    MegaChatHandle chatid = 0;
    char* primarySession = nullptr;
    char* secondarySession = nullptr;

    initChat(a1, a2, user, chatid, primarySession, secondarySession, chatroomListener);

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
    const char* giphyTitle = "TEST_SendGiphy";

    MegaChatMessage* msgSent = megaChatApi[a1]->sendGiphy(chatid, srcMp4, srcWebp, sizeMp4, sizeWebp, giphyWidth, giphyHeight, giphyTitle);

    // Wait for update
    ASSERT_CHAT_TEST(waitForResponse(flagConfirmed), "Timeout expired for receiving confirmation by server. Timeout: " +  std::to_string(maxTimeout) + " seconds");

    // Check if message has been received correctly
    MegaChatHandle msgId0 = chatroomListener->mConfirmedMessageHandle[a1];
    MegaChatMessage* msgReceived = megaChatApi[a2]->getMessage(chatid, msgId0);
    ASSERT_CHAT_TEST(msgReceived->getType() == MegaChatMessage::TYPE_CONTAINS_META, "Invalid Message Type: " + std::to_string(msgReceived->getType()) + "(account " + std::to_string(a1+1)+ ")");
    auto meta = msgReceived->getContainsMeta();
    ASSERT_CHAT_TEST(meta && meta->getGiphy(), "Giphy information has not been established (account " + std::to_string(a1+1)+ ")");
    auto giphy = meta->getGiphy();
    ASSERT_CHAT_TEST(!strcmp(giphy->getMp4Src(), srcMp4), "giphy mp4 src of message received doesn't match that of the message sent");
    ASSERT_CHAT_TEST(!strcmp(giphy->getWebpSrc(), srcWebp), "giphy webp src of message received doesn't match that of the message sent");
    ASSERT_CHAT_TEST(giphy->getMp4Size() == sizeMp4, "giphy mp4 size of message received doesn't match that of the message sent");
    ASSERT_CHAT_TEST(giphy->getWebpSize() == sizeWebp, "giphy webp size of message received doesn't match that of the message sent");
    ASSERT_CHAT_TEST(giphy->getWidth() == giphyWidth, "giphy width of message received doesn't match that of the message sent");
    ASSERT_CHAT_TEST(giphy->getHeight() == giphyHeight, "giphy height size of message received doesn't match that of the message sent");
    ASSERT_CHAT_TEST(!strcmp(giphy->getTitle(), giphyTitle), "giphy title of message received doesn't match that of the message sent");

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
    secondarySession = login(a2);

    user = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    if (!user || (user->getVisibility() != MegaUser::VISIBILITY_VISIBLE))
    {
        makeContact(a1, a2);
    }

    chatid = getPeerToPeerChatRoom(a1, a2);

    // 1. A sends a message to B while B has the chat opened.
    // --> check the confirmed in A, the received message in B, the delivered in A

    chatroomListener = new TestChatRoomListener(this, megaChatApi, chatid);
    ASSERT_CHAT_TEST(megaChatApi[a1]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account 1");
    ASSERT_CHAT_TEST(megaChatApi[a2]->openChatRoom(chatid, chatroomListener), "Can't open chatRoom account 2");

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
        ASSERT_CHAT_TEST(waitForResponse(flagChatdOnline), "Timeout expired for connecting to chatd");
        *flagChatdOnline = false;
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
        ASSERT_CHAT_TEST(waitForResponse(flagHistoryLoaded), "Timeout expired for loading history from chat: " + std::string(hstr));
        delete [] hstr;
        hstr = NULL;
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
    megaApi[a1]->inviteContact(mAccounts[a2].getEmail().c_str(),
                                                contactRequestMessage.c_str(), MegaContactRequest::INVITE_ACTION_ADD);

    ASSERT_CHAT_TEST(waitForResponse(flagRequestInviteContact), "Expired timeout for invite contact request");
    ASSERT_CHAT_TEST(!lastError[a1], "Error invite contact. Error: " + std::to_string(lastError[a1]));
    ASSERT_CHAT_TEST(waitForResponse(flagContactRequestUpdatedSecondary), "Expired timeout for receive contact request");

    getContactRequest(a2, false);

    bool *flagReplyContactRequest = &requestFlags[a2][MegaRequest::TYPE_REPLY_CONTACT_REQUEST];
    *flagReplyContactRequest = false;
    bool *flagContactRequestUpdatedPrimary = &mContactRequestUpdated[a1];
    *flagContactRequestUpdatedPrimary = false;
    megaApi[a2]->replyContactRequest(mContactRequest[a2], MegaContactRequest::REPLY_ACTION_ACCEPT);
    ASSERT_CHAT_TEST(waitForResponse(flagReplyContactRequest), "Expired timeout for reply contact request");
    ASSERT_CHAT_TEST(!lastError[a2], "Error reply contact request. Error: " + std::to_string(lastError[a2]));
    ASSERT_CHAT_TEST(waitForResponse(flagContactRequestUpdatedPrimary), "Expired timeout for receive contact request reply");

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
                                                 MegaChatPeerList *peers, bool create, bool publicChat, const char *title)
{
    MegaChatRoomList *chats = megaChatApi[a1]->getChatRooms();
    bool chatroomExist = false;
    MegaChatHandle targetChatid = MEGACHAT_INVALID_HANDLE;
    for (int i = 0; i < chats->size() && !chatroomExist; ++i)
    {
        const MegaChatRoom *chat = chats->get(i);
        if (!chat->isGroup() || !chat->isActive()
                || (chat->isPublic() != publicChat)
                || (chat->getPeerCount() != peers->size()))
        {
            continue;
        }

        for (int userIndex = 0; userIndex < chat->getPeerCount(); userIndex++)
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

                    // --> Ensure we are connected to chatd for the chatroom
                    ASSERT_CHAT_TEST((megaChatApi[a1]->getChatConnectionState(targetChatid) == MegaChatApi::CHAT_CONNECTION_ONLINE),
                                     "Not connected to chatd for account " + std::to_string(a1+1) + ": " + mAccounts[a1].getEmail());
                    if (a2LoggedIn)
                    {
                        ASSERT_CHAT_TEST((megaChatApi[a2]->getChatConnectionState(targetChatid) == MegaChatApi::CHAT_CONNECTION_ONLINE),
                                     "Not connected to chatd for account " + std::to_string(a2+1) + ": " + mAccounts[a2].getEmail());
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
        ASSERT_CHAT_TEST(waitForResponse(flagCreateChatRoom), "Expired timeout for creating groupchat");
        ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to create groupchat. Error: " + lastErrorMsgChat[a1] + " (" + std::to_string(lastErrorChat[a1]) + ")");
        targetChatid = chatid[a1];
        ASSERT_CHAT_TEST(targetChatid != MEGACHAT_INVALID_HANDLE, "Wrong chat id");
        ASSERT_CHAT_TEST(waitForResponse(chatItemPrimaryReceived), "Expired timeout for receiving the new chat list item");

        // wait for login into chatd for the new groupchat
        while (megaChatApi[a1]->getChatConnectionState(targetChatid) != MegaChatApi::CHAT_CONNECTION_ONLINE)
        {
            postLog("Waiting for connection to chatd for new chat before proceeding with test...");
            ASSERT_CHAT_TEST(waitForResponse(flagChatdOnline1), "Timeout expired for connecting to chatd after creation");
            *flagChatdOnline1 = false;
        }

        // since we may have multiple notifications for other chats, check we received the right one
        MegaChatListItem *chatItemSecondaryCreated = NULL;
        do
        {
            ASSERT_CHAT_TEST(waitForResponse(chatItemSecondaryReceived), "Expired timeout for receiving the new chat list item");
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
            ASSERT_CHAT_TEST(waitForResponse(flagChatdOnline2), "Timeout expired for connecting to chatd after creation");
            *flagChatdOnline2 = false;
        }
    }

    return targetChatid;
}

MegaChatHandle MegaChatApiTest::getPeerToPeerChatRoom(unsigned int a1, unsigned int a2)
{
    MegaUser *peerPrimary = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    MegaUser *peerSecondary = megaApi[a2]->getContact(mAccounts[a1].getEmail().c_str());
    ASSERT_CHAT_TEST(peerPrimary && peerSecondary, "Fail to get Peers");

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
        ASSERT_CHAT_TEST(waitForResponse(flag), "Expired timeout for create new chatroom request");
        ASSERT_CHAT_TEST(!lastErrorChat[a1], "Error create new chatroom request. Error: " + lastErrorMsgChat[a1] + " (" + std::to_string(lastErrorChat[a1]) + ")");
        ASSERT_CHAT_TEST(waitForResponse(chatCreated), "Expired timeout for  create new chatroom");
        ASSERT_CHAT_TEST(waitForResponse(chatReceived), "Expired timeout for create new chatroom");
        chatroom0 = megaChatApi[a1]->getChatRoomByUser(peerPrimary->getHandle());
        chatid0 = chatroom0->getChatId();
        ASSERT_CHAT_TEST(chatid0 != MEGACHAT_INVALID_HANDLE, "Invalid chatid");

        // Wait until both accounts are connected to chatd
        while (megaChatApi[a1]->getChatConnectionState(chatid0) != MegaChatApi::CHAT_CONNECTION_ONLINE)
        {
            postLog("Waiting for connection to chatd...");
            ASSERT_CHAT_TEST(waitForResponse(flagChatdOnline1), "Timeout expired for connecting to chatd, account " + std::to_string(a1+1));
            *flagChatdOnline1 = false;
        }
        while (megaChatApi[a2]->getChatConnectionState(chatid0) != MegaChatApi::CHAT_CONNECTION_ONLINE)
        {
            postLog("Waiting for connection to chatd...");
            ASSERT_CHAT_TEST(waitForResponse(flagChatdOnline2), "Timeout expired for connecting to chatd, account " + std::to_string(a2+1));
            *flagChatdOnline2 = false;
        }
    }
    else
    {
        // --> Ensure we are connected to chatd for the chatroom
        chatid0 = chatroom0->getChatId();
        ASSERT_CHAT_TEST(chatid0 != MEGACHAT_INVALID_HANDLE, "Invalid chatid");
        ASSERT_CHAT_TEST((megaChatApi[a1]->getChatConnectionState(chatid0) == MegaChatApi::CHAT_CONNECTION_ONLINE),
                         "Not connected to chatd for account " + std::to_string(a1+1) + ": " + mAccounts[a1].getEmail());
        ASSERT_CHAT_TEST((megaChatApi[a2]->getChatConnectionState(chatid0) == MegaChatApi::CHAT_CONNECTION_ONLINE),
                         "Not connected to chatd for account " + std::to_string(a2+1) + ": " + mAccounts[a2].getEmail());
    }

    delete chatroom0;
    chatroom0 = NULL;

    MegaChatRoom *chatroom1 = megaChatApi[a2]->getChatRoomByUser(peerSecondary->getHandle());
    MegaChatHandle chatid1 = chatroom1->getChatId();
    delete chatroom1;
    chatroom1 = NULL;
    ASSERT_CHAT_TEST(chatid0 == chatid1,
                     "Chat identificator is different for account0 and account1. chatid0: " + std::to_string(chatid0) +
                     " chatid1: " + std::to_string(chatid1));

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

    ASSERT_CHAT_TEST(messageSendEdit, "Failed to edit message");
    delete messageSendEdit;
    messageSendEdit = NULL;
    ASSERT_CHAT_TEST(waitForResponse(flagConfirmed), "Timeout expired for receiving confirmation by server");    // for confirmation, sendMessage() is synchronous
    MegaChatHandle msgPrimaryId = *msgidSendEdit;
    ASSERT_CHAT_TEST(msgPrimaryId != MEGACHAT_INVALID_HANDLE, "Wrong message id for sent message");
    MegaChatMessage *messageSent = megaChatApi[senderAccountIndex]->getMessage(chatid, msgPrimaryId);   // message should be already confirmed, so in RAM
    ASSERT_CHAT_TEST(messageSent, "Failed to find the confirmed message by msgid");
    ASSERT_CHAT_TEST(messageSent->getMsgId() == msgPrimaryId, "Failed to retrieve the message id");

    ASSERT_CHAT_TEST(waitForResponse(flagReceived), "Timeout expired for receiving message by target user");    // for reception
    ASSERT_CHAT_TEST(chatroomListener->hasArrivedMessage(receiverAccountIndex, msgPrimaryId), "Message id of sent message and received message don't match");
    MegaChatHandle msgSecondaryId = msgPrimaryId;
    MegaChatMessage *messageReceived = megaChatApi[receiverAccountIndex]->getMessage(chatid, msgSecondaryId);   // message should be already received, so in RAM
    ASSERT_CHAT_TEST(messageReceived, "Failed to retrieve the message at the receiver account");
    ASSERT_CHAT_TEST(!strcmp(textToSend.c_str(), messageReceived->getContent()), "Content of message received doesn't match the content of sent message");

    // Check if reception confirmation is active and, in this case, only 1on1 rooms have acknowledgement of receipt
    if (megaChatApi[senderAccountIndex]->isMessageReceptionConfirmationActive()
            && !megaChatApi[senderAccountIndex]->getChatRoom(chatid)->isGroup())
    {
        ASSERT_CHAT_TEST(waitForResponse(flagDelivered), "Timeout expired for receiving delivery notification");    // for delivery
    }

    // Update Message
    if (messageId != MEGACHAT_INVALID_HANDLE)
    {
        ASSERT_CHAT_TEST(messageReceived->isEdited(), "Edited messages is not reported as edition");
    }

    delete messageReceived;
    messageReceived = NULL;

    return messageSent;
}

void MegaChatApiTest::checkEmail(unsigned int indexAccount)
{
    char *myEmail = megaChatApi[indexAccount]->getMyEmail();
    ASSERT_CHAT_TEST(myEmail, "Incorrect email");
    ASSERT_CHAT_TEST(string(myEmail) == mAccounts[indexAccount].getEmail(), "");

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
    ASSERT_CHAT_TEST(waitForResponse(flagRequest), "Expired timeout for attaching node");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to attach node. Error: " + lastErrorMsgChat[a1] + " (" + std::to_string(lastErrorChat[a1]) + ")");
    delete megaNodeList;
    megaNodeList = NULL;

    ASSERT_CHAT_TEST(waitForResponse(flagConfirmed), "Timeout expired for receiving confirmation by server");
    MegaChatHandle msgId0 = chatroomListener->mConfirmedMessageHandle[a1];
    ASSERT_CHAT_TEST(msgId0 != MEGACHAT_INVALID_HANDLE, "Wrong message id for message sent");
    MegaChatMessage *msgSent = megaChatApi[a1]->getMessage(chatid, msgId0);   // message should be already confirmed, so in RAM

    ASSERT_CHAT_TEST(waitForResponse(flagReceived), "Timeout expired for receiving message by target user");    // for reception
    ASSERT_CHAT_TEST(chatroomListener->hasArrivedMessage(a2, msgId0), "Wrong message id at destination");
    MegaChatMessage *msgReceived = megaChatApi[a2]->getMessage(chatid, msgId0);   // message should be already received, so in RAM
    ASSERT_CHAT_TEST(msgReceived, "Failed to get messagbe by id");
    ASSERT_CHAT_TEST(msgReceived->getType() == MegaChatMessage::TYPE_NODE_ATTACHMENT, "Wrong type of message. Type: " + std::to_string(msgReceived->getType()));
    megaNodeList = msgReceived->getMegaNodeList();
    ASSERT_CHAT_TEST(megaNodeList, "Failed to get list of nodes attached");
    ASSERT_CHAT_TEST(megaNodeList->size() == 1, "Wrong size of list of nodes attached");
    ASSERT_CHAT_TEST(megaNodeList->get(0)->getHandle() == nodeToSend->getHandle(), "Handle of node from received message doesn't match the nodehandle attached");

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
    ASSERT_CHAT_TEST(waitForResponse(flagTruncateHistory), "Expired timeout for truncating history");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to truncate history. Error: " + lastErrorMsgChat[a1] + " (" + std::to_string(lastErrorChat[a1]) + ")");
    ASSERT_CHAT_TEST(waitForResponse(flagTruncatedPrimary), "Expired timeout for truncating history for primary account");
    ASSERT_CHAT_TEST(waitForResponse(flagTruncatedSecondary), "Expired timeout for truncating history for secondary account");
    ASSERT_CHAT_TEST(waitForResponse(chatItemUpdated0), "Expired timeout for receiving chat list item update for primary account");
    ASSERT_CHAT_TEST(waitForResponse(chatItemUpdated1), "Expired timeout for receiving chat list item update for secondary account");

    MegaChatListItem *itemPrimary = megaChatApi[a1]->getChatListItem(chatid);
    ASSERT_CHAT_TEST(itemPrimary->getUnreadCount() == 0, "Wrong unread count for chat list item after clear history. Count: " + std::to_string(itemPrimary->getUnreadCount()));
    ASSERT_CHAT_TEST(!strcmp(itemPrimary->getLastMessage(), ""), "Wrong content of last message for chat list item after clear history. Content: " + std::string(itemPrimary->getLastMessage()));
    ASSERT_CHAT_TEST(itemPrimary->getLastMessageType() == MegaChatMessage::TYPE_TRUNCATE, "Wrong type of last message after clear history. Type: " + std::to_string(itemPrimary->getLastMessageType()));
    ASSERT_CHAT_TEST(itemPrimary->getLastTimestamp() != 0, "Wrong last timestamp after clear history");
    delete itemPrimary; itemPrimary = NULL;
    MegaChatListItem *itemSecondary = megaChatApi[a2]->getChatListItem(chatid);
    ASSERT_CHAT_TEST(itemSecondary->getUnreadCount() == 0, "Wrong unread count for chat list item after clear history. Count: " + std::to_string(itemSecondary->getUnreadCount()));
    ASSERT_CHAT_TEST(!strcmp(itemSecondary->getLastMessage(), ""), "Wrong content of last message for chat list item after clear history. Content: " + std::string(itemSecondary->getLastMessage()));
    ASSERT_CHAT_TEST(itemSecondary->getLastMessageType() == MegaChatMessage::TYPE_TRUNCATE, "Wrong type of last message after clear history. Type: " + std::to_string(itemSecondary->getLastMessageType()));
    ASSERT_CHAT_TEST(itemSecondary->getLastTimestamp() != 0, "Wrong last timestamp after clear history");
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
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        if (api == megaChatApi[i])
        {
            apiIndex = i;
            break;
        }
    }

    if (apiIndex == -1)
    {
        ASSERT_CHAT_TEST(false, "Instance of MegaChatApi not recognized");
    }

    return apiIndex;
}

unsigned int MegaChatApiTest::getMegaApiIndex(MegaApi *api)
{
    int apiIndex = -1;
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        if (api == megaApi[i])
        {
            apiIndex = i;
            break;
        }
    }

    if (apiIndex == -1)
    {
        ASSERT_CHAT_TEST(false, "Instance of MegaChatApi not recognized");
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
    megaApi[accountIndex]->startUpload(filePath.c_str(), megaApi[accountIndex]->getNodeByPath(targetPath.c_str()), this);
    ASSERT_CHAT_TEST(waitForResponse(&isNotTransferRunning(accountIndex)), "Expired timeout for upload file");
    ASSERT_CHAT_TEST(!lastErrorTransfer[accountIndex],
                     "Error upload file. Error: " + std::to_string(lastErrorTransfer[accountIndex]) + ". Source: " + filePath + "  target: " + targetPath);

    ASSERT_CHAT_TEST(mNodeUploadHandle[accountIndex] != INVALID_HANDLE, "Upload node handle is invalid");

    MegaNode *node = megaApi[accountIndex]->getNodeByHandle(mNodeUploadHandle[accountIndex]);
    ASSERT_CHAT_TEST(node != NULL, "It is not possible recover upload node");

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
    struct stat st = {0};
    if (stat(DOWNLOAD_PATH.c_str(), &st) == -1)
    {
        mkdir(DOWNLOAD_PATH.c_str(), 0700);
    }

    addTransfer(accountIndex);
    megaApi[accountIndex]->startDownload(nodeToDownload, DOWNLOAD_PATH.c_str(), this);
    ASSERT_CHAT_TEST(waitForResponse(&isNotTransferRunning(accountIndex)), "Expired timeout for download file");
    return lastErrorTransfer[accountIndex] == API_OK;
}

bool MegaChatApiTest::importNode(int accountIndex, MegaNode *node, const string &targetName)
{
    bool *flagCopied = &requestFlags[accountIndex][MegaRequest::TYPE_COPY];
    *flagCopied = false;
    mNodeCopiedHandle[accountIndex] = INVALID_HANDLE;
    megaApi[accountIndex]->authorizeNode(node);
    MegaNode *parentNode = megaApi[accountIndex]->getRootNode();
    megaApi[accountIndex]->copyNode(node, parentNode, targetName.c_str(), this);
    ASSERT_CHAT_TEST(waitForResponse(flagCopied), "Expired timeout for copy node");
    delete parentNode;
    parentNode = NULL;

    return lastError[accountIndex] == API_OK;
}

void MegaChatApiTest::getContactRequest(unsigned int accountIndex, bool outgoing, int expectedSize)
{
    MegaContactRequestList *crl;

    if (outgoing)
    {
        crl = megaApi[accountIndex]->getOutgoingContactRequests();
        ASSERT_CHAT_TEST(expectedSize == crl->size(),
                         "Expected: " + std::to_string(expectedSize) + " and received: " + std::to_string(crl->size()));

        if (expectedSize)
        {
            mContactRequest[accountIndex] = crl->get(0)->copy();
        }
    }
    else
    {
        crl = megaApi[accountIndex]->getIncomingContactRequests();
        ASSERT_CHAT_TEST(expectedSize == crl->size(),
                         "Expected: " + std::to_string(expectedSize) + " and received: " + std::to_string(crl->size()));

        if (expectedSize)
        {
            mContactRequest[accountIndex] = crl->get(0)->copy();
        }
    }

    delete crl;
}

int MegaChatApiTest::purgeLocalTree(const std::string &path)
{
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

        bool *flagRemove = &requestFlags[accountIndex][MegaRequest::TYPE_REMOVE];
        *flagRemove = false;

        megaApi[accountIndex]->remove(childrenNode);
        TEST_LOG_ERROR(waitForResponse(flagRemove), "Expired timeout for remove node");
        TEST_LOG_ERROR(!lastError[accountIndex], "Failed to remove node. Error: " + std::to_string(lastError[accountIndex]));
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
        bool *flagRemoveContactRequest = &requestFlags[accountIndex][MegaRequest::TYPE_INVITE_CONTACT]; *flagRemoveContactRequest = false;
        megaApi[accountIndex]->inviteContact(contactRequest->getTargetEmail(), "Removing you", MegaContactRequest::INVITE_ACTION_DELETE);
        TEST_LOG_ERROR(waitForResponse(flagRemoveContactRequest), "Expired timeout for remove pending contact request");
        TEST_LOG_ERROR(!lastError[accountIndex], "Failed to remove peer. Error: " + std::to_string(lastError[accountIndex]));
    }

    delete contactRequests;
    contactRequests = NULL;
}

void MegaChatApiTest::changeLastName(unsigned int accountIndex, std::string lastName)
{
    bool *flagMyName = &requestFlags[accountIndex][MegaRequest::TYPE_SET_ATTR_USER]; *flagMyName = false;
    bool *flagMyNameReceived = &requestFlags[accountIndex][MegaRequest::TYPE_GET_ATTR_USER]; *flagMyNameReceived = false;
    megaApi[accountIndex]->setUserAttribute(MegaApi::USER_ATTR_LASTNAME, lastName.c_str());
    ASSERT_CHAT_TEST(waitForResponse(flagMyName), "User attribute retrieval not finished after " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(!lastError[accountIndex], "Failed SDK request to change lastname. Error: " + std::to_string(lastError[accountIndex]));
    ASSERT_CHAT_TEST(waitForResponse(flagMyNameReceived), "Expired timeout to get last name after own change (" + std::to_string(maxTimeout) + " seconds)");
    ASSERT_CHAT_TEST(!lastError[accountIndex], "Failed SDK to get lastname. Error: " + std::to_string(lastError[accountIndex]));

    // This sleep is necessary to allow execute the two listeners (MegaChatApi and MegaChatApiTest) for
    // MegaRequest::TYPE_GET_ATTR_USER before exit from this function.
    // In other case, we could ask for the name to MegaChatApi before this will be established
    // because MegachatApiTest listener is called before than MegaChatApi listener
    sleep(1);
}

void MegaChatApiTest::onRequestFinish(MegaApi *api, MegaRequest *request, MegaError *e)
{
    unsigned int apiIndex = getMegaApiIndex(api);

    lastError[apiIndex] = e->getErrorCode();
    if (!lastError[apiIndex])
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
    mChatsUpdated[apiIndex] = true;
    for (int i = 0; i < chats->size(); i++)
    {
         mChatListUpdated[apiIndex].emplace_back(chats->get(i)->getHandle());
    }
}

void MegaChatApiTest::onContactRequestsUpdate(MegaApi* api, MegaContactRequestList* /*requests*/)
{
    unsigned int apiIndex = getMegaApiIndex(api);

    mContactRequestUpdated[apiIndex] = true;
}

void MegaChatApiTest::onRequestFinish(MegaChatApi *api, MegaChatRequest *request, MegaChatError *e)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);

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
        }
    }

    requestFlagsChat[apiIndex][request->getType()] = true;
}

void MegaChatApiTest::onChatInitStateUpdate(MegaChatApi *api, int newState)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);

    initState[apiIndex] = newState;
    initStateChanged[apiIndex] = true;
}

void MegaChatApiTest::onChatListItemUpdate(MegaChatApi *api, MegaChatListItem *item)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);

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

void MegaChatApiTest::onChatOnlineStatusUpdate(MegaChatApi* api, MegaChatHandle userhandle, int status, bool inProgress)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);
    if (userhandle == megaChatApi[apiIndex]->getMyUserHandle())
    {
        mOnlineStatusUpdated[apiIndex] = true;
        mOnlineStatus[apiIndex] = status;
    }
}

void MegaChatApiTest::onChatPresenceConfigUpdate(MegaChatApi *api, MegaChatPresenceConfig */*config*/)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);
    mPresenceConfigUpdated[apiIndex] = true;
}

void MegaChatApiTest::onChatConnectionStateUpdate(MegaChatApi *api, MegaChatHandle chatid, int state)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);
    mChatConnectionOnline[apiIndex] = (state == MegaChatApi::CHAT_CONNECTION_ONLINE);
    mLoggedInAllChats[apiIndex] = (state == MegaChatApi::CHAT_CONNECTION_ONLINE) && (chatid == MEGACHAT_INVALID_HANDLE);
}

void MegaChatApiTest::onTransferStart(MegaApi */*api*/, MegaTransfer */*transfer*/)
{

}

void MegaChatApiTest::onTransferFinish(MegaApi *api, MegaTransfer *transfer, MegaError *error)
{
    unsigned int apiIndex = getMegaApiIndex(api);

    mNotTransferRunning[apiIndex] = true;
    mNodeUploadHandle[apiIndex] = transfer->getNodeHandle();
    lastErrorTransfer[apiIndex] = error->getErrorCode();
}

void MegaChatApiTest::onTransferUpdate(MegaApi */*api*/, MegaTransfer */*transfer*/)
{
}

void MegaChatApiTest::onTransferTemporaryError(MegaApi */*api*/, MegaTransfer */*transfer*/, MegaError */*error*/)
{
}

bool MegaChatApiTest::onTransferData(MegaApi */*api*/, MegaTransfer */*transfer*/, char */*buffer*/, size_t /*size*/)
{
}

#ifndef KARERE_DISABLE_WEBRTC

void MegaChatApiTest::onChatCallUpdate(MegaChatApi *api, MegaChatCall *call)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);

    if (call->hasChanged(MegaChatCall::CHANGE_TYPE_STATUS))
    {
        unsigned int apiIndex = getMegaChatApiIndex(api);
        switch (call->getStatus())
        {
        case MegaChatCall::CALL_STATUS_IN_PROGRESS:
            mCallAnswered[apiIndex] = true;
            mChatIdInProgressCall[apiIndex] = call->getChatid();
            break;

        case MegaChatCall::CALL_STATUS_RING_IN:
            if (api->getNumCalls() > 1)
            {
                // Hangup in progress call and answer the new call
//                api->hangChatCall(mCallId[apiIndex]);
//                api->answerChatCall(call->getChatid());

                // Hangup in coming call
                api->hangChatCall(call->getChatid());
            }

            mCallReceived[apiIndex] = true;
            mChatIdRingInCall[apiIndex] = call->getChatid();
            mCallIdRingIn[apiIndex] = call->getId();
            break;

        case MegaChatCall::CALL_STATUS_REQUEST_SENT:
            mCallIdRequestSent[apiIndex] = call->getId();
            break;

        case MegaChatCall::CALL_STATUS_TERMINATING_USER_PARTICIPATION:
            mTerminationCode[apiIndex] = call->getTermCode();
            mTerminationLocal[apiIndex] = (bool)call->isLocalTermCode();
            break;

        case MegaChatCall::CALL_STATUS_DESTROYED:
            mCallDestroyed[apiIndex] = true;
            break;
        default:
            break;
        }
    }

    if (call->hasChanged(MegaChatCall::CHANGE_TYPE_LOCAL_AVFLAGS))
    {
        mVideoLocal[apiIndex] = call->hasLocalVideo();
    }

    if (call->hasChanged(MegaChatCall::CHANGE_TYPE_RINGING_STATUS))
    {
        mPeerIsRinging[apiIndex] = call->isRinging();
    }

    LOG_debug << "On chat call change state ";
}

TestChatVideoListener::TestChatVideoListener()
{
}

TestChatVideoListener::~TestChatVideoListener()
{
}

void TestChatVideoListener::onChatVideoData(MegaChatApi *api, MegaChatHandle chatid, int width, int height, char *buffer, size_t size)
{
}

#endif

TestChatRoomListener::TestChatRoomListener(MegaChatApiTest *t, MegaChatApi **apis, MegaChatHandle chatid)
{
    this->t = t;
    this->megaChatApi = apis;
    this->chatid = chatid;
    this->message = NULL;

    for (int i = 0; i < NUM_ACCOUNTS; i++)
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
    for (int i = 0; i < msgId[apiIndex].size(); ++i)
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

void TestChatRoomListener::onReactionUpdate(MegaChatApi *api, MegaChatHandle msgid, const char *reaction, int count)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);
    reactionReceived[apiIndex] = true;
}

void TestChatRoomListener::onHistoryTruncatedByRetentionTime(MegaChatApi *api, MegaChatMessage *msg)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);
    mRetentionMessageHandle[apiIndex] = msg->getMsgId();
    retentionHistoryTruncated[apiIndex] = true;
}

void TestChatRoomListener::onMessageUpdate(MegaChatApi *api, MegaChatMessage *msg)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);

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
    checkUrls["http://??/"] = 0;
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
    for (auto testCase : checkUrls)
    {
        executedTests ++;
        if (chatd::Message::hasUrl(testCase.first, url) != testCase.second)
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

TestMegaRequestListener::TestMegaRequestListener(MegaApi *megaApi, MegaChatApi *megaChatApi)
    : RequestListener(megaApi, megaChatApi)
{
}

TestMegaRequestListener::~TestMegaRequestListener()
{
    delete mRequest;
    delete mError;
}

void TestMegaRequestListener::onRequestFinish(MegaApi *api, MegaRequest *request, MegaError *e)
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

void TestMegaChatRequestListener::onRequestFinish(MegaChatApi *api, MegaChatRequest *request, MegaChatError *e)
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
        usleep(pollingT);

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
