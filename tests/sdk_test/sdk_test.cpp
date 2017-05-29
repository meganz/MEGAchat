#include "sdk_test.h"

#include <megaapi.h>
#include "../../src/megachatapi.h"
#include "../../src/karereCommon.h" // for logging with karere facility

#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace mega;
using namespace megachat;

const std::string MegaChatApiTest::DEFAULT_PATH = "../tests/sdk_test/";
const std::string MegaChatApiTest::FILE_IMAGE_NAME = "logo.png";
const std::string MegaChatApiTest::PATH_IMAGE = "PATH_IMAGE";

const std::string MegaChatApiTest::LOCAL_PATH = "."; // no ending slash
const std::string MegaChatApiTest::REMOTE_PATH = "/";
const std::string MegaChatApiTest::DOWNLOAD_PATH = LOCAL_PATH + "/download/";


void handlerSignalINT(int)
{
    printf("SIGINT Received\n");
    fflush(stdout);
}

int main(int argc, char **argv)
{
//    ::mega::MegaClient::APIURL = "https://staging.api.mega.co.nz/";
    MegaChatApiTest t;
    t.init();

    EXECUTE_TEST(t.TEST_ResumeSession(0), "TEST Resume session");
    EXECUTE_TEST(t.TEST_SetOnlineStatus(0), "TEST Online status");
    EXECUTE_TEST(t.TEST_GetChatRoomsAndMessages(0), "TEST Load chatrooms & messages");
    EXECUTE_TEST(t.TEST_SwitchAccounts(0, 1), "TEST Switch accounts");
    EXECUTE_TEST(t.TEST_ClearHistory(0, 1), "TEST Clear history");
    EXECUTE_TEST(t.TEST_EditAndDeleteMessages(0, 1), "TEST Edit & delete messages");
    EXECUTE_TEST(t.TEST_GroupChatManagement(0, 1), "TEST Groupchat management");
    EXECUTE_TEST(t.TEST_OfflineMode(0), "TEST Offline mode");
    EXECUTE_TEST(t.TEST_Attachment(0, 1), "TEST Attachments");
    EXECUTE_TEST(t.TEST_SendContact(0, 1), "TEST Sed contact");
    EXECUTE_TEST(t.TEST_LastMessage(0, 1), "TEST Last message");
    EXECUTE_TEST(t.TEST_GroupLastMessage(0, 1), "TEST Last message (group)");

    //EXECUTE_TEST(t.TEST_OfflineMode(0), "TEST Offline mode"); // This is a manual test. It is necesary stop intenet conection

    t.terminate();

    return t.mFailedTests;
}

ChatTestException::ChatTestException(const string &file, int line, const std::string &msg)
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
    : mNotTransferRunning(true)
{
    logger = new MegaLoggerSDK("SDK.log");
    MegaApi::setLoggerObject(logger);

    chatLogger = new MegaChatLoggerSDK("SDKchat.log");
    MegaChatApi::setLoggerObject(chatLogger);

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

MegaChatApiTest::~MegaChatApiTest()
{
}

void MegaChatApiTest::init()
{
    std::cout << "[==========] Global test environment initialization" << endl; \

    struct stat st = {0};
    if (stat(LOCAL_PATH.c_str(), &st) == -1)
    {
        mkdir(LOCAL_PATH.c_str(), 0700);
    }

    // do some initialization
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        megaApi[i] = NULL;
        megaChatApi[i] = NULL;
    }

    signal(SIGINT, handlerSignalINT);
    mOKTests = mFailedTests = 0;
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

    for (int i = 0; i < NUM_ACCOUNTS; ++i)
    {
        contactRequest[i] = NULL;
        chatroom[i] = NULL;
        chatListItem[i] = NULL;
        chatid[i] = MEGACHAT_INVALID_HANDLE;
    }


    // 1. Initialize chat engine
    bool *flagInit = &initStateChanged[accountIndex]; *flagInit = false;
    megaChatApi[accountIndex]->init(session);
    ASSERT_CHAT_TEST(waitForResponse(flagInit), "Initialization failed");
    if (!session)
    {
        ASSERT_CHAT_TEST(initState[accountIndex] == MegaChatApi::INIT_WAITING_NEW_SESSION, "Init state invalid");
    }
    else
    {
        ASSERT_CHAT_TEST(initState[accountIndex] == MegaChatApi::INIT_OFFLINE_SESSION, "Init state invalid");
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
    ASSERT_CHAT_TEST(waitForResponse(flagRequestFectchNodes), "");
    ASSERT_CHAT_TEST(!lastError[accountIndex], "");
    // after fetchnodes, karere should be ready for offline, at least
    ASSERT_CHAT_TEST(waitForResponse(flagInit), "");
    ASSERT_CHAT_TEST(initState[accountIndex] == MegaChatApi::INIT_ONLINE_SESSION, "");

    // 4. Connect to chat servers
    bool *flagRequestConnect = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_CONNECT]; *flagRequestConnect = false;
    megaChatApi[accountIndex]->connect();
    ASSERT_CHAT_TEST(waitForResponse(flagRequestConnect), "");
    ASSERT_CHAT_TEST(!lastError[accountIndex], "");

    return megaApi[accountIndex]->dumpSession();
}

void MegaChatApiTest::logout(unsigned int accountIndex, bool closeSession)
{
    bool *flagRequestLogout = &requestFlags[accountIndex][MegaRequest::TYPE_LOGOUT]; *flagRequestLogout = false;
    closeSession ? megaApi[accountIndex]->logout() : megaApi[accountIndex]->localLogout();
    ASSERT_CHAT_TEST(waitForResponse(flagRequestLogout), "");
    ASSERT_CHAT_TEST(!lastError[accountIndex], "");

    flagRequestLogout = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_LOGOUT]; *flagRequestLogout = false;
    closeSession ? megaChatApi[accountIndex]->logout() : megaChatApi[accountIndex]->localLogout();
    ASSERT_CHAT_TEST(waitForResponse(flagRequestLogout), "");
    ASSERT_CHAT_TEST(!lastErrorChat[accountIndex], "");
}

void MegaChatApiTest::terminate()
{
    std::cout << "[==========] Global test environment termination" << endl; \

    std::cout << "[ PASSED ] " << mOKTests << " test/s." << endl;
    if (mFailedTests)
    {
        std::cout << "[ FAILED ] " << mFailedTests << " test/s, see above." << endl;
    }
}

void MegaChatApiTest::SetUp()
{
    // TODO:
    // get credentials for each account
    // reset flags and values stored by the object MegaChatApiTest
    // instantiate MegaApi and MegaChatApi objects
    // set listeners, loggers, maybe log in the primary account directly

    // do some initialization
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        char path[1024];
        getcwd(path, sizeof path);
        megaApi[i] = new MegaApi(APPLICATION_KEY.c_str(), path, USER_AGENT_DESCRIPTION.c_str());
        megaApi[i]->setLogLevel(MegaApi::LOG_LEVEL_DEBUG);
        megaApi[i]->addListener(this);
        megaApi[i]->addRequestListener(this);
        megaApi[i]->log(MegaApi::LOG_LEVEL_INFO, "___ Initializing tests for chat ___");

        megaChatApi[i] = new MegaChatApi(megaApi[i]);
        megaChatApi[i]->setLogLevel(MegaChatApi::LOG_LEVEL_DEBUG);
        megaChatApi[i]->addChatRequestListener(this);
        megaChatApi[i]->addChatListener(this);
        megaApi[i]->log(MegaChatApi::LOG_LEVEL_INFO, "___ Initializing tests for chat SDK___");
    }
}

void MegaChatApiTest::TearDown()
{
    // TODO:
    // delete generated temporal local files
    // delete any node in the cloud
    // delete PCRs
    // leave any active groupchat
    // clear history of every active chat
    // logout from MegaApi and MegaChatApi (if logged in)
    // remove listeners and loggers
    // delete instances of MegaApi and MegaChatApi


    for (int i = 0; i < NUM_ACCOUNTS; i++)
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

            logout(i, true);

            megaApi[i]->removeRequestListener(this);
        }

        if (megaChatApi[i]->getInitState() == MegaChatApi::INIT_ONLINE_SESSION ||
                megaChatApi[i]->getInitState() == MegaChatApi::INIT_OFFLINE_SESSION )
        {
            megaChatApi[i]->removeChatRequestListener(this);
            megaChatApi[i]->removeChatListener(this);
        }

        delete megaChatApi[i];
        delete megaApi[i];

        megaApi[i] = NULL;
        megaChatApi[i] = NULL;
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

void MegaChatApiTest::printChatRoomInfo(const MegaChatRoom *chat)
{
    if (!chat)
    {
        return;
    }

    char hstr[sizeof(handle) * 4 / 3 + 4];
    MegaChatHandle chatid = chat->getChatId();
    Base64::btoa((const byte *)&chatid, sizeof(handle), hstr);

    cout << "Chat ID: " << hstr << " (" << chatid << ")" << endl;
    cout << "\tOwn privilege level: " << MegaChatRoom::privToString(chat->getOwnPrivilege()) << endl;
    if (chat->isActive())
    {
        cout << "\tActive: yes" << endl;
    }
    else
    {
        cout << "\tActive: no" << endl;
    }
    if (chat->isGroup())
    {
        cout << "\tGroup chat: yes" << endl;
    }
    else
    {
        cout << "\tGroup chat: no" << endl;
    }
    cout << "\tPeers:";

    if (chat->getPeerCount())
    {
        cout << "\t\t(userhandle)\t(privilege)\t(firstname)\t(lastname)\t(fullname)" << endl;
        for (unsigned i = 0; i < chat->getPeerCount(); i++)
        {
            MegaChatHandle uh = chat->getPeerHandle(i);
            Base64::btoa((const byte *)&uh, sizeof(handle), hstr);
            cout << "\t\t\t" << hstr;
            cout << "\t" << MegaChatRoom::privToString(chat->getPeerPrivilege(i));
            cout << "\t\t" << chat->getPeerFirstname(i);
            cout << "\t" << chat->getPeerLastname(i);
            cout << "\t" << chat->getPeerFullname(i) << endl;
        }
    }
    else
    {
        cout << " no peers (only you as participant)" << endl;
    }
    if (chat->getTitle())
    {
        cout << "\tTitle: " << chat->getTitle() << endl;
    }
    cout << "\tUnread count: " << chat->getUnreadCount() << " message/s" << endl;
    cout << "-------------------------------------------------" << endl;
    fflush(stdout);
}

void MegaChatApiTest::printMessageInfo(const MegaChatMessage *msg)
{
    if (!msg)
    {
        return;
    }

    const char *content = msg->getContent() ? msg->getContent() : "<empty>";

    cout << "id: " << msg->getMsgId() << ", content: " << content;
    cout << ", tempId: " << msg->getTempId() << ", index:" << msg->getMsgIndex();
    cout << ", status: " << msg->getStatus() << ", uh: " << msg->getUserHandle();
    cout << ", type: " << msg->getType() << ", edited: " << msg->isEdited();
    cout << ", deleted: " << msg->isDeleted() << ", changes: " << msg->getChanges();
    cout << ", ts: " << msg->getTimestamp() << endl;
    fflush(stdout);
}

void MegaChatApiTest::printChatListItemInfo(const MegaChatListItem *item)
{
    if (!item)
    {
        return;
    }

    const char *title = item->getTitle() ? item->getTitle() : "<empty>";

    cout << "id: " << item->getChatId() << ", title: " << title;
    cout << ", ownPriv: " << item->getOwnPrivilege();
    cout << ", unread: " << item->getUnreadCount() << ", changes: " << item->getChanges();
    cout << ", lastMsg: " << item->getLastMessage() << ", lastMsgType: " << item->getLastMessageType();
    cout << ", lastTs: " << item->getLastTimestamp() << endl;
    fflush(stdout);
}



bool MegaChatApiTest::waitForResponse(bool *responseReceived, int timeout) const
{
    timeout *= 1000000; // convert to micro-seconds
    unsigned int tWaited = 0;    // microseconds
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
        }
    }

    return true;    // response is received
}



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
    ASSERT_CHAT_TEST(!strcmp(session, tmpSession), "");
    delete [] tmpSession;   tmpSession = NULL;

    checkEmail(accountIndex);

    // ___ Resume an existing session without karere cache ___
    // logout from SDK keeping cache
    bool *flagSdkLogout = &requestFlags[accountIndex][MegaRequest::TYPE_LOGOUT]; *flagSdkLogout = false;
    megaApi[accountIndex]->localLogout();
    ASSERT_CHAT_TEST(waitForResponse(flagSdkLogout), "");
    ASSERT_CHAT_TEST(!lastError[accountIndex], "");
    // logout from Karere removing cache
    bool *flagChatLogout = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_LOGOUT]; *flagChatLogout = false;
    megaChatApi[accountIndex]->logout();
    ASSERT_CHAT_TEST(waitForResponse(flagChatLogout), "");
    ASSERT_CHAT_TEST(!lastErrorChat[accountIndex], "");
    // try to initialize chat engine with cache --> should fail
    ASSERT_CHAT_TEST(megaChatApi[accountIndex]->init(session) == MegaChatApi::INIT_NO_CACHE, "");
    megaApi[accountIndex]->invalidateCache();


    // ___ Re-create Karere cache without login out from SDK___
    bool *flagInit = &initStateChanged[accountIndex]; *flagInit = false;
    // login in SDK
    bool *flagLogin = &requestFlags[accountIndex][MegaRequest::TYPE_LOGIN]; *flagLogin = false;
    session ? megaApi[accountIndex]->fastLogin(session) : megaApi[accountIndex]->login(mAccounts[accountIndex].getEmail().c_str(), mAccounts[accountIndex].getPassword().c_str());
    ASSERT_CHAT_TEST(waitForResponse(flagLogin), "");
    ASSERT_CHAT_TEST(!lastError[accountIndex], "");
    // fetchnodes in SDK
    bool *flagFetchNodes = &requestFlags[accountIndex][MegaRequest::TYPE_FETCH_NODES]; *flagFetchNodes = false;
    megaApi[accountIndex]->fetchNodes();
    ASSERT_CHAT_TEST(waitForResponse(flagFetchNodes), "");
    ASSERT_CHAT_TEST(!lastError[accountIndex], "");
    ASSERT_CHAT_TEST(waitForResponse(flagInit), "");
    ASSERT_CHAT_TEST(initState[accountIndex] == MegaChatApi::INIT_ONLINE_SESSION, "");
    // check there's a list of chats already available
    MegaChatListItemList *list = megaChatApi[accountIndex]->getChatListItems();
    ASSERT_CHAT_TEST(list->size(), "");
    delete list; list = NULL;

    // ___ Close session ___
    logout(accountIndex, true);
    delete [] session; session = NULL;

    // ___ Login with chat enabled, transition to disabled and back to enabled
    session = login(accountIndex);
    ASSERT_CHAT_TEST(session, "");
    // fully disable chat: logout + remove logger + delete MegaChatApi instance
    flagChatLogout = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_LOGOUT]; *flagChatLogout = false;
    megaChatApi[accountIndex]->logout();
    ASSERT_CHAT_TEST(waitForResponse(flagChatLogout), "");
    ASSERT_CHAT_TEST(!lastErrorChat[accountIndex], "");
    megaChatApi[accountIndex]->setLoggerObject(NULL);
    delete megaChatApi[accountIndex];
    // create a new MegaChatApi instance
    MegaChatApi::setLoggerObject(chatLogger);
    megaChatApi[accountIndex] = new MegaChatApi(megaApi[accountIndex]);
    megaChatApi[accountIndex]->setLogLevel(MegaChatApi::LOG_LEVEL_DEBUG);
    megaChatApi[accountIndex]->addChatRequestListener(this);
    megaChatApi[accountIndex]->addChatListener(this);
    // back to enabled: init + fetchnodes + connect
    ASSERT_CHAT_TEST(megaChatApi[accountIndex]->init(session) == MegaChatApi::INIT_NO_CACHE, "");
    flagInit = &initStateChanged[accountIndex]; *flagInit = false;
    flagFetchNodes = &requestFlags[accountIndex][MegaRequest::TYPE_FETCH_NODES]; *flagFetchNodes = false;
    megaApi[accountIndex]->fetchNodes();
    ASSERT_CHAT_TEST(waitForResponse(flagFetchNodes), "");
    ASSERT_CHAT_TEST(!lastError[accountIndex], "");
    ASSERT_CHAT_TEST(waitForResponse(flagInit), "");
    ASSERT_CHAT_TEST(initState[accountIndex] == MegaChatApi::INIT_ONLINE_SESSION, "");
    bool *flagConnect = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_CONNECT]; *flagConnect = false;
    megaChatApi[accountIndex]->connect();
    ASSERT_CHAT_TEST(waitForResponse(flagConnect), "");
    ASSERT_CHAT_TEST(!lastErrorChat[accountIndex], "");
    // check there's a list of chats already available
    list = megaChatApi[accountIndex]->getChatListItems();
    ASSERT_CHAT_TEST(list->size(), "");
    delete list; list = NULL;
    // close session and remove cache
    logout(accountIndex, true);
    delete [] session; session = NULL;

    // ___ Login with chat disabled, transition to enabled ___
    // fully disable chat: remove logger + delete MegaChatApi instance
    megaChatApi[accountIndex]->setLoggerObject(NULL);
    delete megaChatApi[accountIndex];
    // create a new MegaChatApi instance
    MegaChatApi::setLoggerObject(chatLogger);
    megaChatApi[accountIndex] = new MegaChatApi(megaApi[accountIndex]);
    megaChatApi[accountIndex]->setLogLevel(MegaChatApi::LOG_LEVEL_DEBUG);
    megaChatApi[accountIndex]->addChatRequestListener(this);
    megaChatApi[accountIndex]->addChatListener(this);
    // login in SDK
    flagLogin = &requestFlags[accountIndex][MegaRequest::TYPE_LOGIN]; *flagLogin = false;
    megaApi[accountIndex]->login(mAccounts[accountIndex].getEmail().c_str(), mAccounts[accountIndex].getPassword().c_str());
    ASSERT_CHAT_TEST(waitForResponse(flagLogin), "");
    ASSERT_CHAT_TEST(!lastError[accountIndex], "");
    session = megaApi[accountIndex]->dumpSession();
    // fetchnodes in SDK
    flagFetchNodes = &requestFlags[accountIndex][MegaRequest::TYPE_FETCH_NODES]; *flagFetchNodes = false;
    megaApi[accountIndex]->fetchNodes();
    ASSERT_CHAT_TEST(waitForResponse(flagFetchNodes), "");
    ASSERT_CHAT_TEST(!lastError[accountIndex], "");

    // init in Karere
    ASSERT_CHAT_TEST(megaChatApi[accountIndex]->init(session) == MegaChatApi::INIT_NO_CACHE, "");
    // full-fetchndoes in SDK to regenerate cache in Karere
    flagInit = &initStateChanged[accountIndex]; *flagInit = false;
    flagFetchNodes = &requestFlags[accountIndex][MegaRequest::TYPE_FETCH_NODES]; *flagFetchNodes = false;
    megaApi[accountIndex]->fetchNodes();
    ASSERT_CHAT_TEST(waitForResponse(flagFetchNodes), "");
    ASSERT_CHAT_TEST(!lastError[accountIndex], "");
    ASSERT_CHAT_TEST(waitForResponse(flagInit), "");
    ASSERT_CHAT_TEST(initState[accountIndex] == MegaChatApi::INIT_ONLINE_SESSION, "");
    // connect in Karere
    flagConnect = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_CONNECT]; *flagConnect = false;
    megaChatApi[accountIndex]->connect();
    ASSERT_CHAT_TEST(waitForResponse(flagConnect), "");
    ASSERT_CHAT_TEST(!lastErrorChat[accountIndex], "");
    // check there's a list of chats already available
    list = megaChatApi[accountIndex]->getChatListItems();
    ASSERT_CHAT_TEST(list->size(), "");
    delete list;
    list = NULL;

    // ___ Disconnect from chat server and reconnect ___
    bool *flagDisconnect = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_DISCONNECT]; *flagDisconnect = false;
    megaChatApi[accountIndex]->disconnect();
    ASSERT_CHAT_TEST(waitForResponse(flagDisconnect), "");
    ASSERT_CHAT_TEST(!lastErrorChat[accountIndex], "");
    // reconnect
    flagConnect = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_CONNECT]; *flagConnect = false;
    megaChatApi[accountIndex]->connect();
    ASSERT_CHAT_TEST(waitForResponse(flagConnect), "");
    ASSERT_CHAT_TEST(!lastErrorChat[accountIndex], "");
    // check there's a list of chats already available
    list = megaChatApi[accountIndex]->getChatListItems();
    ASSERT_CHAT_TEST(list->size(), "");
    delete list;
    list = NULL;

    logoutAccounts(true);
    delete [] session; session = NULL;
}

void MegaChatApiTest::TEST_SetOnlineStatus(unsigned int accountIndex)
{
    char *sesion = login(accountIndex);

    bool *flag = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_SET_ONLINE_STATUS]; *flag = false;
    megaChatApi[accountIndex]->setOnlineStatus(MegaChatApi::STATUS_BUSY);
    ASSERT_CHAT_TEST(waitForResponse(flag), "");

    logoutAccounts(true);
    delete sesion;
    sesion = NULL;
}

void MegaChatApiTest::TEST_GetChatRoomsAndMessages(unsigned int accountIndex)
{
    char *sesion = login(accountIndex);

    MegaChatRoomList *chats = megaChatApi[accountIndex]->getChatRooms();
    cout << chats->size() << " chat/s received: " << endl;

    // Open chats and print history
    for (int i = 0; i < chats->size(); i++)
    {
        // Open a chatroom
        const MegaChatRoom *chatroom = chats->get(i);
        MegaChatHandle chatid = chatroom->getChatId();
        TestChatRoomListener *chatroomListener = new TestChatRoomListener(megaChatApi, chatid);
        ASSERT_CHAT_TEST(megaChatApi[accountIndex]->openChatRoom(chatid, chatroomListener), "");

        // Print chatroom information and peers' names
        printChatRoomInfo(chatroom);
        if (chatroom->getPeerCount())
        {
            for (unsigned i = 0; i < chatroom->getPeerCount(); i++)
            {
                MegaChatHandle uh = chatroom->getPeerHandle(i);

                bool *flagNameReceived = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_GET_FIRSTNAME]; *flagNameReceived = false; mChatFirstname = "";
                megaChatApi[accountIndex]->getUserFirstname(uh);
                ASSERT_CHAT_TEST(waitForResponse(flagNameReceived), "");
                ASSERT_CHAT_TEST(!lastErrorChat[accountIndex], "");
                cout << "Peer firstname (" << uh << "): " << mChatFirstname << " (len: " << mChatFirstname.length() << ")" << endl;

                flagNameReceived = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_GET_LASTNAME]; *flagNameReceived = false; mChatLastname = "";
                megaChatApi[0]->getUserLastname(uh);
                ASSERT_CHAT_TEST(waitForResponse(flagNameReceived), "");
                ASSERT_CHAT_TEST(!lastErrorChat[accountIndex], "");
                cout << "Peer lastname (" << uh << "): " << mChatLastname << " (len: " << mChatLastname.length() << ")" << endl;

                char *email = megaChatApi[accountIndex]->getContactEmail(uh);
                if (email)
                {
                    cout << "Contact email (" << uh << "): " << email << " (len: " << strlen(email) << ")" << endl;
                    delete [] email;
                }
                else
                {
                    flagNameReceived = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_GET_EMAIL]; *flagNameReceived = false; mChatEmail = "";
                    megaChatApi[accountIndex]->getUserEmail(uh);
                    ASSERT_CHAT_TEST(waitForResponse(flagNameReceived), "");
                    ASSERT_CHAT_TEST(!lastErrorChat[accountIndex], "");
                    cout << "Peer email (" << uh << "): " << mChatEmail << " (len: " << mChatEmail.length() << ")" << endl;
                }
            }
        }

        // TODO: remove the block below (currently cannot load history from inactive chats.
        // Redmine ticket: #5721
        if (!chatroom->isActive())
        {
            continue;
        }

        // Load history
        cout << "Loading messages for chat " << chatroom->getTitle() << " (id: " << chatroom->getChatId() << ")" << endl;
        loadHistory(accountIndex, chatid, chatroomListener);

        // Close the chatroom
        megaChatApi[accountIndex]->closeChatRoom(chatid, chatroomListener);
        delete chatroomListener;

        // Now, load history locally (it should be cached by now)
        chatroomListener = new TestChatRoomListener(megaChatApi, chatid);
        ASSERT_CHAT_TEST(megaChatApi[accountIndex]->openChatRoom(chatid, chatroomListener), "");
        cout << "Loading messages locally for chat " << chatroom->getTitle() << " (id: " << chatroom->getChatId() << ")" << endl;
        loadHistory(accountIndex, chatid, chatroomListener);

        // Close the chatroom
        megaChatApi[accountIndex]->closeChatRoom(chatid, chatroomListener);
        delete chatroomListener;

        delete chatroom;
        chatroom = NULL;
    }

    logoutAccounts(true);
    delete sesion;
    sesion = NULL;
}

void MegaChatApiTest::TEST_EditAndDeleteMessages(unsigned int a1, unsigned int a2)
{
    char *primarySession = login(a1);
    char *secondarySession = login(a2);

    MegaUser *peer = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    if (!peer)
    {
        makeContact(a1, a2);
        peer = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    }

    MegaChatHandle chatid = getPeerToPeerChatRoom(a1, a2);

    delete peer;
    peer = NULL;

    // 1. A sends a message to B while B has the chat opened.
    // --> check the confirmed in A, the received message in B, the delivered in A
    TestChatRoomListener *chatroomListener = new TestChatRoomListener(megaChatApi, chatid);
    ASSERT_CHAT_TEST(megaChatApi[a1]->openChatRoom(chatid, chatroomListener), "");
    ASSERT_CHAT_TEST(megaChatApi[a2]->openChatRoom(chatid, chatroomListener), "");

    // Load some message to feed history
    loadHistory(a1, chatid, chatroomListener);
    loadHistory(a2, chatid, chatroomListener);

    std::string messageToSend = "HOLA " + mAccounts[a1].getEmail() + " - This is a testing message automatically sent to you";
    MegaChatMessage *msgSent = sendTextMessageOrUpdate(a1, a2, chatid, messageToSend, chatroomListener);

    // edit the message
    std::string messageToUpdate = "This is an edited message to " + mAccounts[a1].getEmail();
    MegaChatMessage *msgUpdated = sendTextMessageOrUpdate(a1, a2, chatid, messageToUpdate, chatroomListener, msgSent->getMsgId());
    delete msgUpdated; msgUpdated = NULL;
    delete msgSent; msgSent = NULL;

    // finally, clear history
    clearHistory(a1, a2, chatid, chatroomListener);

    delete chatroomListener;

    // 2. A sends a message to B while B doesn't have the chat opened.
    // Then, B opens the chat --> check the received message in B, the delivered in A

    logoutAccounts(true);

    delete [] primarySession;
    primarySession = NULL;
    delete [] secondarySession;
    secondarySession = NULL;
}

void MegaChatApiTest::TEST_GroupChatManagement(unsigned int a1, unsigned int a2)
{
    char *sessionPrimary = login(a1);
    char *sessionSecondary = login(a2);

    // Prepare peers, privileges...
    MegaUser *peer = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    if (!peer)
    {
        makeContact(a1, a2);
        peer = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    }

    MegaChatPeerList *peers = MegaChatPeerList::createInstance();
    peers->addPeer(peer->getHandle(), MegaChatPeerList::PRIV_STANDARD);

    MegaChatHandle chatid = getGroupChatRoom(a1, a2, peers);
    delete peers;
    peers = NULL;

    // --> Open chatroom
    TestChatRoomListener *chatroomListener = new TestChatRoomListener(megaChatApi, chatid);
    ASSERT_CHAT_TEST(megaChatApi[a1]->openChatRoom(chatid, chatroomListener), "");
    ASSERT_CHAT_TEST(megaChatApi[a2]->openChatRoom(chatid, chatroomListener), "");

    // --> Remove from chat
    bool *flagRemoveFromChat = &requestFlagsChat[a1][MegaChatRequest::TYPE_REMOVE_FROM_CHATROOM]; *flagRemoveFromChat = false;
    bool *chatItemLeft0 = &chatItemUpdated[a1]; *chatItemLeft0 = false;
    bool *chatItemLeft1 = &chatItemUpdated[a2]; *chatItemLeft1 = false;
    bool *chatItemClosed1 = &chatItemClosed[a2]; *chatItemClosed1 = false;
    bool *chatLeft0 = &chatroomListener->chatUpdated[a1]; *chatLeft0 = false;
    bool *chatLeft1 = &chatroomListener->chatUpdated[a2]; *chatLeft1 = false;
    bool *mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
    MegaChatHandle *uhAction = &chatroomListener->uhAction[a1]; *uhAction = MEGACHAT_INVALID_HANDLE;
    int *priv = &chatroomListener->priv[a1]; *priv = MegaChatRoom::PRIV_UNKNOWN;
    megaChatApi[a1]->removeFromChat(chatid, peer->getHandle());
    ASSERT_CHAT_TEST(waitForResponse(flagRemoveFromChat), "");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "");
    ASSERT_CHAT_TEST(waitForResponse(mngMsgRecv), "");
    ASSERT_CHAT_TEST(*uhAction == peer->getHandle(), "");
    ASSERT_CHAT_TEST(*priv == MegaChatRoom::PRIV_RM, "");

    MegaChatRoom *chatroom = megaChatApi[a1]->getChatRoom(chatid);
    ASSERT_CHAT_TEST(chatroom, "");
    ASSERT_CHAT_TEST(chatroom->getPeerCount() == 0, "");
    delete chatroom;

    ASSERT_CHAT_TEST(waitForResponse(chatItemLeft0), "");
    ASSERT_CHAT_TEST(waitForResponse(chatItemLeft1), "");
    ASSERT_CHAT_TEST(waitForResponse(chatItemClosed1), "");
    ASSERT_CHAT_TEST(waitForResponse(chatLeft0), "");

    ASSERT_CHAT_TEST(waitForResponse(chatLeft1), "");
    chatroom = megaChatApi[a1]->getChatRoom(chatid);
    ASSERT_CHAT_TEST(chatroom, "");
    ASSERT_CHAT_TEST(chatroom->getPeerCount() == 0, "");
    delete chatroom;

    // Close the chatroom, even if we've been removed from it
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);

    // --> Invite to chat
    bool *flagInviteToChatRoom = &requestFlagsChat[a1][MegaChatRequest::TYPE_INVITE_TO_CHATROOM]; *flagInviteToChatRoom = false;
    bool *chatItemJoined0 = &chatItemUpdated[a1]; *chatItemJoined0 = false;
    bool *chatItemJoined1 = &chatItemUpdated[a2]; *chatItemJoined1 = false;
    bool *chatJoined0 = &chatroomListener->chatUpdated[a1]; *chatJoined0 = false;
    bool *chatJoined1 = &chatroomListener->chatUpdated[a2]; *chatJoined1 = false;
    mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
    uhAction = &chatroomListener->uhAction[a1]; *uhAction = MEGACHAT_INVALID_HANDLE;
    priv = &chatroomListener->priv[a1]; *priv = MegaChatRoom::PRIV_UNKNOWN;
    megaChatApi[a1]->inviteToChat(chatid, peer->getHandle(), MegaChatPeerList::PRIV_STANDARD);
    ASSERT_CHAT_TEST(waitForResponse(flagInviteToChatRoom), "");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "");
    ASSERT_CHAT_TEST(waitForResponse(chatItemJoined0), "");
    ASSERT_CHAT_TEST(waitForResponse(chatItemJoined1), "");
    ASSERT_CHAT_TEST(waitForResponse(chatJoined0), "");
//    ASSERT_CHAT_TEST(waitForResponse(chatJoined1), ""); --> account 1 haven't opened chat, won't receive callback
    ASSERT_CHAT_TEST(waitForResponse(mngMsgRecv), "");
    ASSERT_CHAT_TEST(*uhAction == peer->getHandle(), "");
    ASSERT_CHAT_TEST(*priv == MegaChatRoom::PRIV_UNKNOWN, "");    // the message doesn't report the new priv

    chatroom = megaChatApi[a1]->getChatRoom(chatid);
    ASSERT_CHAT_TEST(chatroom, "");
    ASSERT_CHAT_TEST(chatroom->getPeerCount() == 1, "");
    delete chatroom;

    // since we were expulsed from chatroom, we need to open it again
    ASSERT_CHAT_TEST(megaChatApi[a2]->openChatRoom(chatid, chatroomListener), "");

    // invite again --> error
    flagInviteToChatRoom = &requestFlagsChat[a1][MegaChatRequest::TYPE_INVITE_TO_CHATROOM]; *flagInviteToChatRoom = false;
    megaChatApi[a1]->inviteToChat(chatid, peer->getHandle(), MegaChatPeerList::PRIV_STANDARD);
    ASSERT_CHAT_TEST(waitForResponse(flagInviteToChatRoom), "");
    ASSERT_CHAT_TEST(lastErrorChat[a1] == MegaChatError::ERROR_EXIST, "");

    // --> Set title
    string title = "My groupchat with title";
    bool *flagChatRoomName = &requestFlagsChat[a1][MegaChatRequest::TYPE_EDIT_CHATROOM_NAME]; *flagChatRoomName = false;
    bool *titleItemChanged0 = &titleUpdated[a1]; *titleItemChanged0 = false;
    bool *titleItemChanged1 = &titleUpdated[a2]; *titleItemChanged1 = false;
    bool *titleChanged0 = &chatroomListener->titleUpdated[a1]; *titleChanged0 = false;
    bool *titleChanged1 = &chatroomListener->titleUpdated[a2]; *titleChanged1 = false;
    mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
    string *msgContent = &chatroomListener->content[a1]; *msgContent = "";
    megaChatApi[a1]->setChatTitle(chatid, title.c_str());
    ASSERT_CHAT_TEST(waitForResponse(flagChatRoomName), "");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "");
    ASSERT_CHAT_TEST(waitForResponse(titleItemChanged0), "");
    ASSERT_CHAT_TEST(waitForResponse(titleItemChanged1), "");
    ASSERT_CHAT_TEST(waitForResponse(titleChanged0), "");
    ASSERT_CHAT_TEST(waitForResponse(titleChanged1), "");
    ASSERT_CHAT_TEST(waitForResponse(mngMsgRecv), "");
    ASSERT_CHAT_TEST(!strcmp(title.c_str(), msgContent->c_str()), "");

    chatroom = megaChatApi[a2]->getChatRoom(chatid);
    ASSERT_CHAT_TEST(chatroom, "");
    ASSERT_CHAT_TEST(!strcmp(chatroom->getTitle(), title.c_str()), "");
    delete chatroom;

    // --> Change peer privileges to Moderator
    bool *flagUpdatePeerPermision = &requestFlagsChat[a1][MegaChatRequest::TYPE_UPDATE_PEER_PERMISSIONS]; *flagUpdatePeerPermision = false;
    bool *peerUpdated0 = &peersUpdated[a1]; *peerUpdated0 = false;
    bool *peerUpdated1 = &peersUpdated[a2]; *peerUpdated1 = false;
    mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
    uhAction = &chatroomListener->uhAction[a1]; *uhAction = MEGACHAT_INVALID_HANDLE;
    priv = &chatroomListener->priv[a1]; *priv = MegaChatRoom::PRIV_UNKNOWN;
    megaChatApi[a1]->updateChatPermissions(chatid, peer->getHandle(), MegaChatRoom::PRIV_MODERATOR);
    ASSERT_CHAT_TEST(waitForResponse(flagUpdatePeerPermision), "");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "");
    ASSERT_CHAT_TEST(waitForResponse(peerUpdated0), "");
    ASSERT_CHAT_TEST(waitForResponse(peerUpdated1), "");
    ASSERT_CHAT_TEST(waitForResponse(mngMsgRecv), "");
    ASSERT_CHAT_TEST(*uhAction == peer->getHandle(), "");
    ASSERT_CHAT_TEST(*priv == MegaChatRoom::PRIV_MODERATOR, "");

    // --> Change peer privileges to Read-only
    flagUpdatePeerPermision = &requestFlagsChat[a1][MegaChatRequest::TYPE_UPDATE_PEER_PERMISSIONS]; *flagUpdatePeerPermision = false;
    peerUpdated0 = &peersUpdated[a1]; *peerUpdated0 = false;
    peerUpdated1 = &peersUpdated[a2]; *peerUpdated1 = false;
    mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
    uhAction = &chatroomListener->uhAction[a1]; *uhAction = MEGACHAT_INVALID_HANDLE;
    priv = &chatroomListener->priv[a1]; *priv = MegaChatRoom::PRIV_UNKNOWN;
    megaChatApi[a1]->updateChatPermissions(chatid, peer->getHandle(), MegaChatRoom::PRIV_RO);
    ASSERT_CHAT_TEST(waitForResponse(flagUpdatePeerPermision), "");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "");
    ASSERT_CHAT_TEST(waitForResponse(peerUpdated0), "");
    ASSERT_CHAT_TEST(waitForResponse(peerUpdated1), "");
    ASSERT_CHAT_TEST(waitForResponse(mngMsgRecv), "");
    ASSERT_CHAT_TEST(*uhAction == peer->getHandle(), "");
    ASSERT_CHAT_TEST(*priv == MegaChatRoom::PRIV_RO, "");

    // --> Try to send a message without the right privilege
    string msg1 = "HOLA " + mAccounts[a1].getEmail()+ " - This message can't be send because I'm read-only";
    bool *flagRejected = &chatroomListener->msgRejected[a2]; *flagRejected = false;
    chatroomListener->msgId[a2] = MEGACHAT_INVALID_HANDLE;   // will be set at reception
    MegaChatMessage *msgSent = megaChatApi[a2]->sendMessage(chatid, msg1.c_str());
    ASSERT_CHAT_TEST(msgSent, "");
    delete msgSent; msgSent = NULL;
    ASSERT_CHAT_TEST(waitForResponse(flagRejected), "");    // for confirmation, sendMessage() is synchronous
    MegaChatHandle msgId0 = chatroomListener->msgId[a2];
    ASSERT_CHAT_TEST(msgId0 == MEGACHAT_INVALID_HANDLE, "");

    // --> Load some message to feed history
    loadHistory(a1, chatid, chatroomListener);
    loadHistory(a2, chatid, chatroomListener);

    // --> Send typing notification
    bool *flagTyping1 = &chatroomListener->userTyping[a2]; *flagTyping1 = false;
    uhAction = &chatroomListener->uhAction[a2]; *uhAction = MEGACHAT_INVALID_HANDLE;
    megaChatApi[a1]->sendTypingNotification(chatid);
    ASSERT_CHAT_TEST(waitForResponse(flagTyping1), "");
    ASSERT_CHAT_TEST(*uhAction == megaChatApi[a1]->getMyUserHandle(), "");

    // --> Send a message and wait for reception by target user
    string msg0 = "HOLA " + mAccounts[a1].getEmail() + " - Testing groupchats";
    bool *msgConfirmed = &chatroomListener->msgConfirmed[a1]; *msgConfirmed = false;
    bool *msgReceived = &chatroomListener->msgReceived[a2]; *msgReceived = false;
    bool *msgDelivered = &chatroomListener->msgDelivered[a1]; *msgDelivered = false;
    chatroomListener->msgId[a1] = MEGACHAT_INVALID_HANDLE;   // will be set at confirmation
    chatroomListener->msgId[a2] = MEGACHAT_INVALID_HANDLE;   // will be set at reception
    megaChatApi[a1]->sendMessage(chatid, msg0.c_str());
    ASSERT_CHAT_TEST(waitForResponse(msgConfirmed), "");    // for confirmation, sendMessage() is synchronous
    MegaChatHandle msgId = chatroomListener->msgId[a1];
    ASSERT_CHAT_TEST(msgId != MEGACHAT_INVALID_HANDLE, "");
    ASSERT_CHAT_TEST(waitForResponse(msgReceived), "");    // for reception
    ASSERT_CHAT_TEST(msgId == chatroomListener->msgId[a2], "");
    MegaChatMessage *msg = megaChatApi[a2]->getMessage(chatid, msgId);   // message should be already received, so in RAM
    ASSERT_CHAT_TEST(msg && !strcmp(msg0.c_str(), msg->getContent()), "");
    ASSERT_CHAT_TEST(waitForResponse(msgDelivered), "");    // for delivery

    // --> Close the chatroom
    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);
    delete chatroomListener;

    // --> Remove peer from groupchat
    bool *flagRemoveFromChatRoom = &requestFlagsChat[a1][MegaChatRequest::TYPE_REMOVE_FROM_CHATROOM]; *flagRemoveFromChatRoom = false;
    bool *chatClosed = &chatItemClosed[a2]; *chatClosed = false;
    megaChatApi[a1]->removeFromChat(chatid, peer->getHandle());
    ASSERT_CHAT_TEST(waitForResponse(flagRemoveFromChatRoom), "");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "");
    ASSERT_CHAT_TEST(waitForResponse(chatClosed), "");
    chatroom = megaChatApi[a2]->getChatRoom(chatid);
    ASSERT_CHAT_TEST(chatroom, "");
    ASSERT_CHAT_TEST(!chatroom->isActive(), "");
    delete chatroom;    chatroom = NULL;

    leaveChat(a1, chatid);
    leaveChat(a2, chatid);

    logoutAccounts(true);

    delete [] sessionPrimary;
    sessionPrimary = NULL;
    delete [] sessionSecondary;
    sessionSecondary = NULL;
}

void MegaChatApiTest::TEST_OfflineMode(unsigned int accountIndex)
{
    char *session = login(accountIndex);

    MegaChatRoomList *chats = megaChatApi[accountIndex]->getChatRooms();
    cout << chats->size() << " chat/s received: " << endl;

    // Redmine ticket: #5721 (history from inactive chats is not retrievable)
    const MegaChatRoom *chatroom = NULL;
    for (int i = 0; i < chats->size(); i++)
    {
        if (chats->get(i)->isActive())
        {
            chatroom = chats->get(i);
            break;
        }
    }

    if (chatroom)
    {
        // Open a chatroom
        MegaChatHandle chatid = chatroom->getChatId();

        printChatRoomInfo(chatroom);

        TestChatRoomListener *chatroomListener = new TestChatRoomListener(megaChatApi, chatid);
        ASSERT_CHAT_TEST(megaChatApi[accountIndex]->openChatRoom(chatid, chatroomListener), "");

        // Load some message to feed history
        bool *flagHistoryLoaded = &chatroomListener->historyLoaded[accountIndex]; *flagHistoryLoaded = false;
        megaChatApi[accountIndex]->loadMessages(chatid, 16);
        ASSERT_CHAT_TEST(waitForResponse(flagHistoryLoaded), "");
        ASSERT_CHAT_TEST(!lastErrorChat[accountIndex], "");

        cout << endl << endl << "Disconnect from the Internet now" << endl << endl;
//        system("pause");


        string msg0 = "This is a test message sent without Internet connection";
        chatroomListener->msgId[accountIndex] = MEGACHAT_INVALID_HANDLE;   // will be set at confirmation
        MegaChatMessage *msgSent = megaChatApi[accountIndex]->sendMessage(chatid, msg0.c_str());
        ASSERT_CHAT_TEST(msgSent, "");
        ASSERT_CHAT_TEST(msgSent->getStatus() == MegaChatMessage::STATUS_SENDING, "");

        megaChatApi[accountIndex]->closeChatRoom(chatid, chatroomListener);

        // close session and resume it while offline
        logout(accountIndex, false);
        bool *flagInit = &initStateChanged[accountIndex]; *flagInit = false;
        megaChatApi[accountIndex]->init(session);
        ASSERT_CHAT_TEST(waitForResponse(flagInit), "");
        ASSERT_CHAT_TEST(initState[accountIndex] == MegaChatApi::INIT_OFFLINE_SESSION, "");

        // check the unsent message is properly loaded
        flagHistoryLoaded = &chatroomListener->historyLoaded[accountIndex]; *flagHistoryLoaded = false;
        bool *msgUnsentLoaded = &chatroomListener->msgLoaded[accountIndex]; *msgUnsentLoaded = false;
        chatroomListener->msgId[accountIndex] = MEGACHAT_INVALID_HANDLE;
        ASSERT_CHAT_TEST(megaChatApi[accountIndex]->openChatRoom(chatid, chatroomListener), "");
        bool msgUnsentFound = false;
        do
        {
            ASSERT_CHAT_TEST(waitForResponse(msgUnsentLoaded), "");
            if (chatroomListener->msgId[accountIndex] == msgSent->getMsgId())
            {
                msgUnsentFound = true;
                break;
            }
            *msgUnsentLoaded = false;
        } while (*flagHistoryLoaded);
        ASSERT_CHAT_TEST(msgUnsentFound, "");


        cout << endl << endl << "Connect to the Internet now" << endl << endl;
//        system("pause");


        flagHistoryLoaded = &chatroomListener->historyLoaded[accountIndex]; *flagHistoryLoaded = false;
        bool *msgSentLoaded = &chatroomListener->msgLoaded[accountIndex]; *msgSentLoaded = false;
        chatroomListener->msgId[accountIndex] = MEGACHAT_INVALID_HANDLE;
        ASSERT_CHAT_TEST(megaChatApi[accountIndex]->openChatRoom(chatid, chatroomListener), "");
        bool msgSentFound = false;
        do
        {
            ASSERT_CHAT_TEST(waitForResponse(msgSentLoaded), "");
            if (chatroomListener->msgId[accountIndex] == msgSent->getMsgId())
            {
                msgSentFound = true;
                break;
            }
            *msgSentLoaded = false;
        } while (*flagHistoryLoaded);

        ASSERT_CHAT_TEST(msgSentFound, "");
        delete msgSent; msgSent = NULL;
        delete chatroomListener;
        chatroomListener = NULL;
    }

    delete chats;
    chats = NULL;

    logoutAccounts(true);
    delete [] session;
}

void MegaChatApiTest::TEST_ClearHistory(unsigned int a1, unsigned int a2)
{
    char *sessionPrimary = login(a1);
    char *sessionSecondary = login(a2);

    // Prepare peers, privileges...
    MegaUser *peer = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    if (!peer)
    {
        makeContact(a1, a2);
        peer = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    }

    MegaChatPeerList *peers = MegaChatPeerList::createInstance();
    peers->addPeer(peer->getHandle(), MegaChatPeerList::PRIV_STANDARD);

    MegaChatHandle chatid = getGroupChatRoom(a1, a2, peers);
    delete peers;
    peers = NULL;

    // Open chatrooms
    TestChatRoomListener *chatroomListener = new TestChatRoomListener(megaChatApi, chatid);
    ASSERT_CHAT_TEST(megaChatApi[a1]->openChatRoom(chatid, chatroomListener), "");
    ASSERT_CHAT_TEST(megaChatApi[a2]->openChatRoom(chatid, chatroomListener), "");

    // Send 5 messages to have some history
    for (int i = 0; i < 5; i++)
    {
        string msg0 = "HOLA " + mAccounts[a1].getEmail() + " - Testing clearhistory. This messages is the number " + std::to_string(i);

        MegaChatMessage *message = sendTextMessageOrUpdate(a1, a2, chatid, msg0, chatroomListener);

        delete message;
        message = NULL;
    }

    // Close the chatrooms
    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);
    delete chatroomListener;

    // Open chatrooms
    chatroomListener = new TestChatRoomListener(megaChatApi, chatid);
    ASSERT_CHAT_TEST(megaChatApi[a1]->openChatRoom(chatid, chatroomListener), "");
    ASSERT_CHAT_TEST(megaChatApi[a2]->openChatRoom(chatid, chatroomListener), "");

    // --> Load some message to feed history
    int count = loadHistory(a1, chatid, chatroomListener);
    ASSERT_CHAT_TEST(count == 5, "");
    count = loadHistory(a2, chatid, chatroomListener);
    ASSERT_CHAT_TEST(count == 5, "");

    // Clear history
    clearHistory(a1, a2, chatid, chatroomListener);
    // TODO: in this case, it's not just to clear the history, but
    // to also check the other user received the corresponding message.

    // Close and re-open chatrooms
    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);
    delete chatroomListener;
    chatroomListener = new TestChatRoomListener(megaChatApi, chatid);
    ASSERT_CHAT_TEST(megaChatApi[a1]->openChatRoom(chatid, chatroomListener), "");
    ASSERT_CHAT_TEST(megaChatApi[a2]->openChatRoom(chatid, chatroomListener), "");

    // --> Check history is been truncated
    count = loadHistory(a1, chatid, chatroomListener);
    ASSERT_CHAT_TEST(count == 1, "");
    count = loadHistory(a2, chatid, chatroomListener);
    ASSERT_CHAT_TEST(count == 1, "");

    // Close the chatrooms
    megaChatApi[a1]->closeChatRoom(chatid, chatroomListener);
    megaChatApi[a2]->closeChatRoom(chatid, chatroomListener);
    delete chatroomListener;

    leaveChat(a1, chatid);
    leaveChat(a2, chatid);

    logoutAccounts(true);

    delete [] sessionPrimary;
    sessionPrimary = NULL;
    delete [] sessionSecondary;
    sessionSecondary = NULL;
}

void MegaChatApiTest::TEST_SwitchAccounts(unsigned int a1, unsigned int a2)
{
    char *session = login(a1);

    MegaChatListItemList *items = megaChatApi[a1]->getChatListItems();
    for (int i = 0; i < items->size(); i++)
    {
        const MegaChatListItem *item = items->get(i);
        if (!item->isActive())
        {
            continue;
        }

        printChatListItemInfo(item);

        sleep(3);

        MegaChatHandle chatid = item->getChatId();
        MegaChatListItem *itemUpdated = megaChatApi[a1]->getChatListItem(chatid);

        printChatListItemInfo(itemUpdated);

        delete itemUpdated;
        itemUpdated = NULL;

        continue;
    }

    delete items;
    items = NULL;

    logout(a1, true);    // terminate() and destroy Client

    delete [] session;
    session = NULL;

    // LOgin over same index account but with other user
    session = login(a1, NULL, mAccounts[a2].getEmail().c_str(), mAccounts[a2].getPassword().c_str());

    logoutAccounts(true);

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
    MegaUser *peer = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    if (!peer)
    {
        makeContact(a1, a2);
        peer = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    }

    delete peer;
    peer = NULL;

    MegaChatHandle chatid = getPeerToPeerChatRoom(a1, a2);

    TestChatRoomListener *chatroomListener = new TestChatRoomListener(megaChatApi, chatid);
    ASSERT_CHAT_TEST(megaChatApi[a1]->openChatRoom(chatid, chatroomListener), "Cannot open chatroom");
    ASSERT_CHAT_TEST(megaChatApi[a2]->openChatRoom(chatid, chatroomListener), "Cannot open chatroom");

    // Load some messages to feed history
    loadHistory(a1, chatid, chatroomListener);
    loadHistory(a2, chatid, chatroomListener);

    chatroomListener->msgId[a1] = MEGACHAT_INVALID_HANDLE;   // will be set at confirmation
    chatroomListener->msgId[a2] = MEGACHAT_INVALID_HANDLE;   // will be set at reception

    std::string formatDate = dateToString();

    // A uploads a new file
    createFile(formatDate, LOCAL_PATH, formatDate);
    MegaNode* nodeSent = uploadFile(a1, formatDate, LOCAL_PATH, REMOTE_PATH);

    // A sends the file as attachment to the chatroom
    MegaNode *nodeReceived = attachNode(a1, a2, chatid, nodeSent, chatroomListener);

    // B downloads the node
    ASSERT_CHAT_TEST(downloadNode(a2, nodeReceived), "Cannot download node attached to message");

    // B imports the node
    ASSERT_CHAT_TEST(importNode(a2, nodeReceived, FILE_IMAGE_NAME), "Cannot import node attached to message");

    // A revokes access to node
    bool *flagRequest = &requestFlagsChat[a1][MegaChatRequest::TYPE_REVOKE_NODE_MESSAGE]; *flagRequest = false;
    bool *flagConfirmed = &chatroomListener->msgConfirmed[a1]; *flagConfirmed = false;
    bool *flagReceived = &chatroomListener->msgReceived[a2]; *flagReceived = false;
    chatroomListener->msgId[a1] = MEGACHAT_INVALID_HANDLE;   // will be set at confirmation
    chatroomListener->msgId[a2] = MEGACHAT_INVALID_HANDLE;   // will be set at reception
    megachat::MegaChatHandle revokeAttachmentNode = nodeSent->getHandle();
    megaChatApi[a1]->revokeAttachment(chatid, revokeAttachmentNode, this);
    ASSERT_CHAT_TEST(waitForResponse(flagRequest), "Failed to revoke access to node after " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "Failed to revoke access: " + std::to_string(lastErrorChat[a1]));
    MegaChatHandle msgId0 = chatroomListener->msgId[a1];
    ASSERT_CHAT_TEST(msgId0 != MEGACHAT_INVALID_HANDLE, "");
    ASSERT_CHAT_TEST(waitForResponse(flagConfirmed), "");    // for reception by server

    ASSERT_CHAT_TEST(waitForResponse(flagReceived), "");    // for reception by target user
    MegaChatHandle msgId1 = chatroomListener->msgId[a2];
    ASSERT_CHAT_TEST(msgId0 == msgId1, "");
    MegaChatMessage *msgReceived = megaChatApi[a2]->getMessage(chatid, msgId0);   // message should be already received, so in RAM
    ASSERT_CHAT_TEST(msgReceived, "");
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

    delete nodeSent;
    nodeSent = NULL;

    // A uploads an image to check previews / thumbnails
    std::string path = DEFAULT_PATH;
    if (getenv(PATH_IMAGE.c_str()) != NULL)
    {
        path = getenv(PATH_IMAGE.c_str());
    }
    nodeSent = uploadFile(a1, FILE_IMAGE_NAME, path, REMOTE_PATH);
    nodeReceived = attachNode(a1, a2, chatid, nodeSent, chatroomListener);

    // A gets the thumbnail of the uploaded image
    bool *flagRequestThumbnail0 = &requestFlags[a1][MegaRequest::TYPE_GET_ATTR_FILE]; *flagRequestThumbnail0 = false;
    std::string thumbnailPath = LOCAL_PATH + "/thumbnail0.jpg";
    megaApi[a1]->getThumbnail(nodeSent, thumbnailPath.c_str(), this);
    ASSERT_CHAT_TEST(waitForResponse(flagRequestThumbnail0), "Failed to get own thumbnail after " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(!lastError[a1], "Failed to get thumbnail" + std::to_string(lastError[a1]));

    // B gets the thumbnail of the attached image
    bool *flagRequestThumbnail1 = &requestFlags[a2][MegaRequest::TYPE_GET_ATTR_FILE]; *flagRequestThumbnail1 = false;
    thumbnailPath = LOCAL_PATH + "/thumbnail1.jpg";
    megaApi[a2]->getThumbnail(nodeReceived, thumbnailPath.c_str(), this);
    ASSERT_CHAT_TEST(waitForResponse(flagRequestThumbnail1), "Failed to get thumbnail after " + std::to_string(maxTimeout) + " seconds");
    ASSERT_CHAT_TEST(!lastError[a2], "Failed to get thumbnail" + std::to_string(lastError[a2]));


    // Clean chatroom history
    clearHistory(a1, a2, chatid, chatroomListener);
    logoutAccounts(true);

    delete msgReceived;
    msgReceived = NULL;

    delete nodeReceived;
    nodeReceived = NULL;

    delete nodeSent;
    nodeSent = NULL;

    delete [] primarySession;
    primarySession = NULL;
    delete [] secondarySession;
    secondarySession = NULL;
}

void MegaChatApiTest::TEST_LastMessage(unsigned int a1, unsigned int a2)
{
    char *sessionPrimary = login(a1);
    char *sessionSecondary = login(a2);

    MegaUser *peer = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    if (!peer)
    {
        makeContact(a1, a2);
        peer = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    }

    delete peer;
    peer = NULL;

    MegaChatHandle chatid = getPeerToPeerChatRoom(a1, a2);

    TestChatRoomListener *chatroomListener = new TestChatRoomListener(megaChatApi, chatid);
    ASSERT_CHAT_TEST(megaChatApi[a1]->openChatRoom(chatid, chatroomListener), "");
    ASSERT_CHAT_TEST(megaChatApi[a2]->openChatRoom(chatid, chatroomListener), "");

    // Load some message to feed history
    loadHistory(a1, chatid, chatroomListener);
    loadHistory(a2, chatid, chatroomListener);

    std::string formatDate = dateToString();

    sendTextMessageOrUpdate(a1, a2, chatid, formatDate, chatroomListener);

    MegaChatHandle msgId1 = chatroomListener->msgId[a2];
    ASSERT_CHAT_TEST(msgId1 != MEGACHAT_INVALID_HANDLE, "");

    MegaChatListItem *item = megaChatApi[a1]->getChatListItem(chatid);
    ASSERT_CHAT_TEST(strcmp(formatDate.c_str(), item->getLastMessage()) == 0, "");
    delete item;
    item = NULL;

    clearHistory(a1, a2, chatid, chatroomListener);

    formatDate = dateToString();
    createFile(formatDate, LOCAL_PATH, formatDate);
    MegaNode* nodeSent = uploadFile(a1, formatDate, LOCAL_PATH, REMOTE_PATH);
    MegaNode* nodeReceived = attachNode(a1, a2, chatid, nodeSent, chatroomListener);
    msgId1 = chatroomListener->msgId[a2];
    ASSERT_CHAT_TEST(msgId1 != MEGACHAT_INVALID_HANDLE, "");

    item = megaChatApi[a1]->getChatListItem(chatid);
    ASSERT_CHAT_TEST(strcmp(formatDate.c_str(), item->getLastMessage()) == 0, "");
    delete item;
    item = NULL;

    clearHistory(a1, a2, chatid, chatroomListener);

    logoutAccounts(true);

    delete nodeReceived;
    nodeReceived = NULL;

    delete nodeSent;
    nodeSent = NULL;


    delete [] sessionPrimary;
    sessionPrimary = NULL;
    delete [] sessionSecondary;
    sessionSecondary = NULL;
}

void MegaChatApiTest::TEST_SendContact(unsigned int a1, unsigned int a2)
{
    char *primarySession = login(a1);
    char *secondarySession = login(a2);

    MegaUser *peer = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    if (!peer)
    {
        makeContact(a1, a2);
        peer = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    }

    delete peer;
    peer = NULL;

    MegaChatHandle chatid = getPeerToPeerChatRoom(a1, a2);

    // 1. A sends a message to B while B has the chat opened.
    // --> check the confirmed in A, the received message in B, the delivered in A

    TestChatRoomListener *chatroomListener = new TestChatRoomListener(megaChatApi, chatid);
    ASSERT_CHAT_TEST(megaChatApi[a1]->openChatRoom(chatid, chatroomListener), "");
    ASSERT_CHAT_TEST(megaChatApi[a2]->openChatRoom(chatid, chatroomListener), "");

    loadHistory(a1, chatid, chatroomListener);
    loadHistory(a2, chatid, chatroomListener);

    bool *flagConfirmed = &chatroomListener->msgConfirmed[a1]; *flagConfirmed = false;
    bool *flagReceived = &chatroomListener->msgContactReceived[a2]; *flagReceived = false;
    bool *flagDelivered = &chatroomListener->msgDelivered[a1]; *flagDelivered = false;
    chatroomListener->msgId[a1] = MEGACHAT_INVALID_HANDLE;   // will be set at confirmation
    chatroomListener->msgId[a2] = MEGACHAT_INVALID_HANDLE;   // will be set at reception

    MegaUser* user = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    ASSERT_CHAT_TEST(user, "");
    MegaChatHandle handle = user->getHandle();
    delete user;
    user = NULL;
    megaChatApi[a1]->attachContacts(chatid, 1, &handle);
    ASSERT_CHAT_TEST(waitForResponse(flagConfirmed), "");
    MegaChatHandle msgId0 = chatroomListener->msgId[a1];
    ASSERT_CHAT_TEST(msgId0 != MEGACHAT_INVALID_HANDLE, "");

    ASSERT_CHAT_TEST(waitForResponse(flagReceived), "");    // for reception
    MegaChatHandle msgId1 = chatroomListener->msgId[a2];
    ASSERT_CHAT_TEST(msgId0 == msgId1, "");
    MegaChatMessage *msgReceived = megaChatApi[a2]->getMessage(chatid, msgId0);   // message should be already received, so in RAM
    ASSERT_CHAT_TEST(msgReceived, "");

    ASSERT_CHAT_TEST(msgReceived->getType() == MegaChatMessage::TYPE_CONTACT_ATTACHMENT, "");
    ASSERT_CHAT_TEST(msgReceived->getUsersCount() > 0, "");

    ASSERT_CHAT_TEST(strcmp(msgReceived->getUserEmail(0), mAccounts[a2].getEmail().c_str()) == 0, "");

    delete msgReceived;
    msgReceived = NULL;

    clearHistory(a1, a2, chatid, chatroomListener);

    logoutAccounts(true);

    delete [] primarySession;
    primarySession = NULL;
    delete [] secondarySession;
    secondarySession = NULL;
}

void MegaChatApiTest::TEST_GroupLastMessage(unsigned int a1, unsigned int a2)
{
    char *session0 = login(a1);
    char *session1 = login(a2);

    // Prepare peers, privileges...
    MegaUser *peer = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    if (!peer)
    {
        makeContact(a1, a2);
        peer = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    }

    MegaChatPeerList *peers = MegaChatPeerList::createInstance();
    peers->addPeer(peer->getHandle(), MegaChatPeerList::PRIV_STANDARD);

    MegaChatHandle chatid = getGroupChatRoom(a1, a2, peers);
    delete peers;
    peers = NULL;

    // --> Open chatroom
    TestChatRoomListener *chatroomListener = new TestChatRoomListener(megaChatApi, chatid);
    ASSERT_CHAT_TEST(megaChatApi[a1]->openChatRoom(chatid, chatroomListener), "");
    ASSERT_CHAT_TEST(megaChatApi[1]->openChatRoom(chatid, chatroomListener), "");

    std::string textToSend = "Last Message";
    sendTextMessageOrUpdate(a1, a2, chatid, textToSend, chatroomListener);

    // --> Set title
    string title = "My groupchat with title 2";
    bool *flagChatRoomName = &requestFlagsChat[a1][MegaChatRequest::TYPE_EDIT_CHATROOM_NAME]; *flagChatRoomName = false;
    bool *titleItemChanged0 = &titleUpdated[a1]; *titleItemChanged0 = false;
    bool *titleItemChanged1 = &titleUpdated[a2]; *titleItemChanged1 = false;
    bool *titleChanged0 = &chatroomListener->titleUpdated[a1]; *titleChanged0 = false;
    bool *titleChanged1 = &chatroomListener->titleUpdated[a2]; *titleChanged1 = false;
    bool *mngMsgRecv = &chatroomListener->msgReceived[a1]; *mngMsgRecv = false;
    string *msgContent = &chatroomListener->content[a1]; *msgContent = "";
    megaChatApi[a1]->setChatTitle(chatid, title.c_str());
    ASSERT_CHAT_TEST(waitForResponse(flagChatRoomName), "");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "");
    ASSERT_CHAT_TEST(waitForResponse(titleItemChanged0), "");
    ASSERT_CHAT_TEST(waitForResponse(titleItemChanged1), "");
    ASSERT_CHAT_TEST(waitForResponse(titleChanged0), "");
    ASSERT_CHAT_TEST(waitForResponse(titleChanged1), "");
    ASSERT_CHAT_TEST(waitForResponse(mngMsgRecv), "");
    ASSERT_CHAT_TEST(!strcmp(title.c_str(), msgContent->c_str()), "");

    MegaChatListItem *item = megaChatApi[a1]->getChatListItem(chatid);
    ASSERT_CHAT_TEST(strcmp(textToSend.c_str(), item->getLastMessage()) == 0, "");
    delete item;
    item = NULL;

    clearHistory(a1, a2, chatid, chatroomListener);

    leaveChat(a1, chatid);
    leaveChat(a2, chatid);

    logoutAccounts(true);

    delete [] session0;
    session0 = NULL;
    delete [] session1;
    session1 = NULL;
}

int MegaChatApiTest::loadHistory(unsigned int accountIndex, MegaChatHandle chatid, TestChatRoomListener *chatroomListener)
{
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
        ASSERT_CHAT_TEST(waitForResponse(flagHistoryLoaded), "");
    }

    return chatroomListener->msgCount[accountIndex];
}

void MegaChatApiTest::makeContact(unsigned int a1, unsigned int a2)
{
    bool *flagRequestInviteContact = &requestFlags[a1][MegaRequest::TYPE_INVITE_CONTACT];
    *flagRequestInviteContact = false;
    bool *flagContactRequestUpdatedSecondary = &contactRequestUpdated[a2];
    *flagContactRequestUpdatedSecondary = false;
    std::string contactRequestMessage = "Contact Request Message";
    megaApi[a1]->inviteContact(mAccounts[a2].getEmail().c_str(),
                                                contactRequestMessage.c_str(), MegaContactRequest::INVITE_ACTION_ADD);

    ASSERT_CHAT_TEST(waitForResponse(flagRequestInviteContact), "");
    ASSERT_CHAT_TEST(!lastError[a1], "");
    ASSERT_CHAT_TEST(waitForResponse(flagContactRequestUpdatedSecondary), "");

    getContactRequest(a2, false);

    bool *flagReplyContactRequest = &requestFlags[a2][MegaRequest::TYPE_REPLY_CONTACT_REQUEST];
    *flagReplyContactRequest = false;
    bool *flagContactRequestUpdatedPrimary = &contactRequestUpdated[a1];
    *flagContactRequestUpdatedPrimary = false;
    megaApi[a2]->replyContactRequest(contactRequest[a2], MegaContactRequest::REPLY_ACTION_ACCEPT);
    ASSERT_CHAT_TEST(waitForResponse(flagReplyContactRequest), "");
    ASSERT_CHAT_TEST(!lastError[a2], "");
    ASSERT_CHAT_TEST(waitForResponse(flagContactRequestUpdatedPrimary), "");

    delete contactRequest[a2];
    contactRequest[a2] = NULL;
}

MegaChatHandle MegaChatApiTest::getGroupChatRoom(unsigned int a1, unsigned int a2,
                                                 MegaChatPeerList *peers)
{
    // Get chatroom name with peer firstname and lastname
    bool *flagAttributeUser = &requestFlags[a1][MegaRequest::TYPE_GET_ATTR_USER]; *flagAttributeUser = false;
    bool *nameReceivedFlag = &nameReceived[a1]; *nameReceivedFlag = false; mFirstname = "";
    megaApi[a1]->getUserAttribute(MegaApi::USER_ATTR_FIRSTNAME);
    ASSERT_CHAT_TEST(waitForResponse(flagAttributeUser), "");
    ASSERT_CHAT_TEST(!lastError[a1], "");
    ASSERT_CHAT_TEST(waitForResponse(nameReceivedFlag), "");
    std::string peerFirstname = mFirstname;
    flagAttributeUser = &requestFlags[a1][MegaRequest::TYPE_GET_ATTR_USER]; *flagAttributeUser = false;
    nameReceivedFlag = &nameReceived[a1]; *nameReceivedFlag = false; mLastname = "";
    megaApi[a1]->getUserAttribute(MegaApi::USER_ATTR_LASTNAME);
    ASSERT_CHAT_TEST(waitForResponse(flagAttributeUser), "");
    ASSERT_CHAT_TEST(!lastError[a1], "");
    ASSERT_CHAT_TEST(waitForResponse(nameReceivedFlag), "");
    std::string peerLastname = mLastname;
    std::string peerFullname = peerFirstname + " " + peerLastname;

    MegaChatRoomList *chats = megaChatApi[a1]->getChatRooms();

    bool chatroomExist = false;
    MegaChatHandle chatid = MEGACHAT_INVALID_HANDLE;
    for (int i = 0; i < chats->size(); ++i)
    {
        if (strcmp(chats->get(i)->getTitle(), peerFullname.c_str()) == 0)
        {
            chatroomExist = true;
            chatid = chats->get(i)->getChatId();
        }
    }

    delete chats;
    chats = NULL;

    if (!chatroomExist)
    {
        bool *flagCreateChatRoom = &requestFlagsChat[a1][MegaChatRequest::TYPE_CREATE_CHATROOM]; *flagCreateChatRoom = false;
        bool *chatItemPrimaryReceived = &chatItemUpdated[a1]; *chatItemPrimaryReceived = false;
        bool *chatItemSecondaryReceived = &chatItemUpdated[a2]; *chatItemSecondaryReceived = false;
        chatListItem[a1] = NULL;
        chatListItem[a2] = NULL;
        this->chatid[a1] = MEGACHAT_INVALID_HANDLE;

        megaChatApi[a1]->createChat(true, peers, this);
        ASSERT_CHAT_TEST(waitForResponse(flagCreateChatRoom), "");
        ASSERT_CHAT_TEST(!lastErrorChat[a1], "");
        chatid = this->chatid[a1];
        ASSERT_CHAT_TEST(chatid != MEGACHAT_INVALID_HANDLE, "");
        ASSERT_CHAT_TEST(waitForResponse(chatItemPrimaryReceived), "");

        MegaChatListItem *chatItemPrimaryCreated = chatListItem[a1];   chatListItem[a1] = NULL;
        ASSERT_CHAT_TEST(chatItemPrimaryCreated, "");
        delete chatItemPrimaryCreated;    chatItemPrimaryCreated = NULL;

        ASSERT_CHAT_TEST(waitForResponse(chatItemSecondaryReceived), "");
        MegaChatListItem *chatItemSecondaryCreated = chatListItem[a2];   chatListItem[a2] = NULL;

        // FIXME: find a safe way to control when the auxiliar account receives the
        // new chatroom, since we may have multiple notifications for other chats
        while (!chatItemSecondaryCreated)
        {
            ASSERT_CHAT_TEST(waitForResponse(chatItemSecondaryReceived), "");
            ASSERT_CHAT_TEST(chatItemSecondaryCreated, "");
            if (chatItemSecondaryCreated->getChatId() == chatid)
            {
                break;
            }
            else
            {
                delete chatItemSecondaryCreated;    chatItemSecondaryCreated = NULL;
                *chatItemSecondaryReceived = false;
            }
        }

        MegaChatRoom *chatroom = megaChatApi[a2]->getChatRoom(chatid);
        ASSERT_CHAT_TEST(chatroom, "");
        delete chatroom;    chatroom = NULL;

        ASSERT_CHAT_TEST(!strcmp(chatItemSecondaryCreated->getTitle(), peerFullname.c_str()), ""); // ERROR: we get empty title
        delete chatItemPrimaryCreated;    chatItemPrimaryCreated = NULL;
        delete chatItemSecondaryCreated;    chatItemSecondaryCreated = NULL;
    }

    ASSERT_CHAT_TEST(chatid != MEGACHAT_INVALID_HANDLE, "");

    return chatid;
}

MegaChatHandle MegaChatApiTest::getPeerToPeerChatRoom(unsigned int a1, unsigned int a2)
{
    MegaUser *peerPrimary = megaApi[a1]->getContact(mAccounts[a2].getEmail().c_str());
    MegaUser *peerSecondary = megaApi[a2]->getContact(mAccounts[a1].getEmail().c_str());
    ASSERT_CHAT_TEST(peerPrimary && peerSecondary, "");

    MegaChatRoom *chatroom0 = megaChatApi[a1]->getChatRoomByUser(peerPrimary->getHandle());
    if (!chatroom0) // chat 1on1 doesn't exist yet --> create it
    {
        MegaChatPeerList *peers = MegaChatPeerList::createInstance();
        peers->addPeer(peerPrimary->getHandle(), MegaChatPeerList::PRIV_STANDARD);

        bool *flag = &requestFlagsChat[a1][MegaChatRequest::TYPE_CREATE_CHATROOM]; *flag = false;
        bool *chatCreated = &chatItemUpdated[a1]; *chatCreated = false;
        bool *chatReceived = &chatItemUpdated[a2]; *chatReceived = false;
        megaChatApi[a1]->createChat(false, peers, this);
        ASSERT_CHAT_TEST(waitForResponse(flag), "");
        ASSERT_CHAT_TEST(!lastErrorChat[a1], "");
        ASSERT_CHAT_TEST(waitForResponse(chatCreated), "");
        ASSERT_CHAT_TEST(waitForResponse(chatReceived), "");

        chatroom0 = megaChatApi[a1]->getChatRoomByUser(peerPrimary->getHandle());
    }

    MegaChatHandle chatid0 = chatroom0->getChatId();
    ASSERT_CHAT_TEST(chatid0 != MEGACHAT_INVALID_HANDLE, "");
    delete chatroom0;
    chatroom0 = NULL;

    MegaChatRoom *chatroom1 = megaChatApi[a2]->getChatRoomByUser(peerSecondary->getHandle());
    MegaChatHandle chatid1 = chatroom1->getChatId();
    delete chatroom1;
    chatroom1 = NULL;
    ASSERT_CHAT_TEST(chatid0 == chatid1, "");

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
    chatroomListener->msgId[senderAccountIndex] = MEGACHAT_INVALID_HANDLE;   // will be set at confirmation
    chatroomListener->msgId[receiverAccountIndex] = MEGACHAT_INVALID_HANDLE;   // will be set at reception

    MegaChatMessage *messageSendEdit = NULL;
    if (messageId == MEGACHAT_INVALID_HANDLE)
    {
        flagConfirmed = &chatroomListener->msgConfirmed[senderAccountIndex]; *flagConfirmed = false;
        flagReceived = &chatroomListener->msgReceived[receiverAccountIndex]; *flagReceived = false;

        messageSendEdit = megaChatApi[senderAccountIndex]->sendMessage(chatid, textToSend.c_str());
    }
    else  // Update Message
    {
        flagConfirmed = &chatroomListener->msgEdited[senderAccountIndex]; *flagConfirmed = false;
        flagReceived = &chatroomListener->msgEdited[receiverAccountIndex]; *flagReceived = false;

        messageSendEdit = megaChatApi[senderAccountIndex]->editMessage(chatid, messageId, textToSend.c_str());
    }

    ASSERT_CHAT_TEST(messageSendEdit, "");
    ASSERT_CHAT_TEST(waitForResponse(flagConfirmed), "");    // for confirmation, sendMessage() is synchronous
    MegaChatHandle msgPrimaryId = chatroomListener->msgId[senderAccountIndex];
    ASSERT_CHAT_TEST(msgPrimaryId != MEGACHAT_INVALID_HANDLE, "");

    ASSERT_CHAT_TEST(waitForResponse(flagReceived), "");    // for reception
    MegaChatHandle msgSecondaryId = chatroomListener->msgId[senderAccountIndex];
    ASSERT_CHAT_TEST(msgPrimaryId == msgSecondaryId, "");
    MegaChatMessage *messageReceived = megaChatApi[receiverAccountIndex]->getMessage(chatid, msgSecondaryId);   // message should be already received, so in RAM
    ASSERT_CHAT_TEST(messageReceived && !strcmp(textToSend.c_str(), messageReceived->getContent()), "");
    ASSERT_CHAT_TEST(waitForResponse(flagDelivered), "");    // for delivery

    // Update Message
    if (messageId != MEGACHAT_INVALID_HANDLE)
    {
        ASSERT_CHAT_TEST(messageReceived->isEdited(), "");
    }

    delete messageReceived;
    messageReceived = NULL;

    return messageSendEdit;
}

void MegaChatApiTest::checkEmail(unsigned int indexAccount)
{
    char *myEmail = megaChatApi[indexAccount]->getMyEmail();
    ASSERT_CHAT_TEST(myEmail, "");
    ASSERT_CHAT_TEST(string(myEmail) == mAccounts[indexAccount].getEmail(), "");
    cout << "My email is: " << myEmail << endl;
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

MegaNode *MegaChatApiTest::attachNode(unsigned int a1, unsigned int a2, MegaChatHandle chatid,
                                        MegaNode* nodeToSend, TestChatRoomListener* chatroomListener)
{
    MegaNodeList *megaNodeList = MegaNodeList::createInstance();
    megaNodeList->addNode(nodeToSend);

    bool *flagRequest = &requestFlagsChat[a1][MegaChatRequest::TYPE_ATTACH_NODE_MESSAGE]; *flagRequest = false;
    bool *flagConfirmed = &chatroomListener->msgConfirmed[a1]; *flagConfirmed = false;
    bool *flagReceived = &chatroomListener->msgReceived[a2]; *flagReceived = false;

    megaChatApi[a1]->attachNodes(chatid, megaNodeList, this);
    ASSERT_CHAT_TEST(waitForResponse(flagRequest), "");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "");
    delete megaNodeList;
    megaNodeList = NULL;

    ASSERT_CHAT_TEST(waitForResponse(flagConfirmed), "");    // for confirmation, sendMessage() is synchronous
    MegaChatHandle msgId0 = chatroomListener->msgId[a1];
    ASSERT_CHAT_TEST(msgId0 != MEGACHAT_INVALID_HANDLE, "");

    ASSERT_CHAT_TEST(waitForResponse(flagReceived), "");    // for reception
    MegaChatHandle msgId1 = chatroomListener->msgId[a2];
    ASSERT_CHAT_TEST(msgId0 == msgId1, "");
    MegaChatMessage *msgReceived = megaChatApi[a2]->getMessage(chatid, msgId0);   // message should be already received, so in RAM
    ASSERT_CHAT_TEST(msgReceived, "");
    ASSERT_CHAT_TEST(msgReceived->getType() == MegaChatMessage::TYPE_NODE_ATTACHMENT, "");
    megaNodeList = msgReceived->getMegaNodeList();
    ASSERT_CHAT_TEST(megaNodeList, "");
    MegaNode *nodeReceived = megaNodeList->get(0)->copy();
    ASSERT_CHAT_TEST(nodeReceived->getHandle() == nodeToSend->getHandle(), "");

    delete msgReceived;
    msgReceived = NULL;

    return nodeReceived;
}

void MegaChatApiTest::clearHistory(unsigned int a1, unsigned int a2, MegaChatHandle chatid, TestChatRoomListener *chatroomListener)
{
    bool *flagTruncateHistory = &requestFlagsChat[a1][MegaChatRequest::TYPE_TRUNCATE_HISTORY]; *flagTruncateHistory = false;
    bool *flagTruncatedPrimary = &chatroomListener->historyTruncated[a1]; *flagTruncatedPrimary = false;
    bool *flagTruncatedSecondary = &chatroomListener->historyTruncated[a2]; *flagTruncatedSecondary = false;
    bool *chatItemUpdated0 = &chatItemUpdated[a1]; *chatItemUpdated0 = false;
    bool *chatItemUpdated1 = &chatItemUpdated[a2]; *chatItemUpdated1 = false;
    megaChatApi[a1]->clearChatHistory(chatid);
    ASSERT_CHAT_TEST(waitForResponse(flagTruncateHistory), "");
    ASSERT_CHAT_TEST(!lastErrorChat[a1], "");
    ASSERT_CHAT_TEST(waitForResponse(flagTruncatedPrimary), "");
    ASSERT_CHAT_TEST(waitForResponse(flagTruncatedSecondary), "");
    ASSERT_CHAT_TEST(waitForResponse(chatItemUpdated0), "");
    ASSERT_CHAT_TEST(waitForResponse(chatItemUpdated1), "");

    MegaChatListItem *itemPrimary = megaChatApi[a1]->getChatListItem(chatid);
    ASSERT_CHAT_TEST(itemPrimary->getUnreadCount() == 0, "");
    ASSERT_CHAT_TEST(!strcmp(itemPrimary->getLastMessage(), ""), "");
    ASSERT_CHAT_TEST(itemPrimary->getLastMessageType() == 0, "");
    ASSERT_CHAT_TEST(itemPrimary->getLastTimestamp() != 0, "");
    delete itemPrimary; itemPrimary = NULL;
    MegaChatListItem *itemSecondary = megaChatApi[a2]->getChatListItem(chatid);
    ASSERT_CHAT_TEST(itemSecondary->getUnreadCount() == 1, "");
    ASSERT_CHAT_TEST(!strcmp(itemSecondary->getLastMessage(), ""), "");
    ASSERT_CHAT_TEST(itemSecondary->getLastMessageType() == 0, "");
    ASSERT_CHAT_TEST(itemSecondary->getLastTimestamp() != 0, "");
    delete itemSecondary; itemSecondary = NULL;
}

void MegaChatApiTest::leaveChat(unsigned int accountIndex, MegaChatHandle chatid)
{
    bool *flagRemoveFromchatRoom = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_REMOVE_FROM_CHATROOM]; *flagRemoveFromchatRoom = false;
    bool *chatClosed = &chatItemClosed[accountIndex]; *chatClosed = false;
    megaChatApi[accountIndex]->leaveChat(chatid);
    ASSERT_CHAT_TEST(waitForResponse(flagRemoveFromchatRoom), "");
    ASSERT_CHAT_TEST(!lastErrorChat[accountIndex], "");
    ASSERT_CHAT_TEST(waitForResponse(chatClosed), "");
    MegaChatRoom *chatroom = megaChatApi[accountIndex]->getChatRoom(chatid);
    ASSERT_CHAT_TEST(!chatroom->isActive(), "");
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
        cout << "TEST - Instance of MegaChatApi not recognized" << endl;
        ASSERT_CHAT_TEST(false, "");
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
        cout << "TEST - Instance of MegaApi not recognized" << endl;
        ASSERT_CHAT_TEST(false, "");
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
    addTransfer();
    std::string filePath = sourcePath + "/" + fileName;
    megaApi[accountIndex]->startUpload(filePath.c_str(), megaApi[accountIndex]->getNodeByPath(targetPath.c_str()), this);
    ASSERT_CHAT_TEST(waitForResponse(&isNotTransferRunning()), "");
    ASSERT_CHAT_TEST(!lastErrorTransfer[accountIndex], "");

    std::string pathComplete = targetPath + fileName;
    MegaNode *node = megaApi[accountIndex]->getNodeByPath(pathComplete.c_str());
    ASSERT_CHAT_TEST(node != NULL, "");

    return node;
}

void MegaChatApiTest::addTransfer()
{
    mNotTransferRunning = false;
}

bool &MegaChatApiTest::isNotTransferRunning()
{
    return mNotTransferRunning;
}

bool MegaChatApiTest::downloadNode(int accountIndex, MegaNode *nodeToDownload)
{
    struct stat st = {0};
    if (stat(DOWNLOAD_PATH.c_str(), &st) == -1)
    {
        mkdir(DOWNLOAD_PATH.c_str(), 0700);
    }

    addTransfer();
    megaApi[accountIndex]->startDownload(nodeToDownload, DOWNLOAD_PATH.c_str(), this);
    ASSERT_CHAT_TEST(waitForResponse(&isNotTransferRunning()), "");
    return lastErrorTransfer[accountIndex] == API_OK;
}

bool MegaChatApiTest::importNode(int accountIndex, MegaNode *node, const string &targetName)
{
    bool *flagCopied = &requestFlags[accountIndex][MegaRequest::TYPE_COPY];
    *flagCopied = false;
    megaApi[accountIndex]->authorizeNode(node);
    MegaNode *parentNode = megaApi[accountIndex]->getNodeByPath("/");
    megaApi[accountIndex]->copyNode(node, parentNode, targetName.c_str(), this);
    ASSERT_CHAT_TEST(waitForResponse(flagCopied), "");
    return lastError[accountIndex] == API_OK;
}

void MegaChatApiTest::getContactRequest(unsigned int accountIndex, bool outgoing, int expectedSize)
{
    MegaContactRequestList *crl;

    if (outgoing)
    {
        crl = megaApi[accountIndex]->getOutgoingContactRequests();
        ASSERT_CHAT_TEST(expectedSize == crl->size(), "");
        if (expectedSize)
        {
            contactRequest[accountIndex] = crl->get(0)->copy();
        }
    }
    else
    {
        crl = megaApi[accountIndex]->getIncomingContactRequests();
        ASSERT_CHAT_TEST(expectedSize == crl->size(), "");
        if (expectedSize)
        {
            contactRequest[accountIndex] = crl->get(0)->copy();
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

        ASSERT_CHAT_TEST(waitForResponse(flagRemove), "");
        ASSERT_CHAT_TEST(!lastError[accountIndex], "");
    }

    delete children;
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
                }
                else if (request->getParamType() == MegaApi::USER_ATTR_LASTNAME)
                {
                    mLastname = request->getText() ? request->getText() : "";
                }
                nameReceived[apiIndex] = true;
                break;

            case MegaRequest::TYPE_COPY:
                mNodeCopiedHandle = request->getNodeHandle();
                break;
        }
    }

    requestFlags[apiIndex][request->getType()] = true;
}

void MegaChatApiTest::onContactRequestsUpdate(MegaApi* api, MegaContactRequestList* requests)
{
    unsigned int apiIndex = getMegaApiIndex(api);

    contactRequestUpdated[apiIndex] = true;
}

void MegaChatApiTest::onRequestFinish(MegaChatApi *api, MegaChatRequest *request, MegaChatError *e)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);

    lastErrorChat[apiIndex] = e->getErrorCode();
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
        cout << "[api: " << apiIndex << "] Chat list item added or updated - ";
        chatListItem[apiIndex] = item->copy();
        printChatListItemInfo(item);

        if (item->hasChanged(MegaChatListItem::CHANGE_TYPE_CLOSED))
        {
            chatItemClosed[apiIndex] = true;
        }
        if (item->hasChanged(MegaChatListItem::CHANGE_TYPE_PARTICIPANTS))
        {
            peersUpdated[apiIndex] = true;
        }
        if (item->hasChanged(MegaChatListItem::CHANGE_TYPE_TITLE))
        {
            titleUpdated[apiIndex] = true;
        }

        chatItemUpdated[apiIndex] = true;
    }
}

void MegaChatApiTest::onChatOnlineStatusUpdate(MegaChatApi *api, int status)
{

}

void MegaChatApiTest::onTransferStart(MegaApi *api, MegaTransfer *transfer)
{

}

void MegaChatApiTest::onTransferFinish(MegaApi *api, MegaTransfer *transfer, MegaError *error)
{
    unsigned int apiIndex = getMegaApiIndex(api);

    mNotTransferRunning = true;

    lastErrorTransfer[apiIndex] = error->getErrorCode();
}

void MegaChatApiTest::onTransferUpdate(MegaApi *api, MegaTransfer *transfer)
{

}

void MegaChatApiTest::onTransferTemporaryError(MegaApi *api, MegaTransfer *transfer, MegaError *error)
{

}

bool MegaChatApiTest::onTransferData(MegaApi *api, MegaTransfer *transfer, char *buffer, size_t size)
{

}

TestChatRoomListener::TestChatRoomListener(MegaChatApi **apis, MegaChatHandle chatid)
{
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
        this->msgId[i] = MEGACHAT_INVALID_HANDLE;
        this->chatUpdated[i] = false;
        this->userTyping[i] = false;
        this->titleUpdated[i] = false;
        this->msgAttachmentReceived[i] = false;
        this->msgContactReceived[i] = false;
        this->msgRevokeAttachmentReceived[i] = false;
    }
}

void TestChatRoomListener::onChatRoomUpdate(MegaChatApi *api, MegaChatRoom *chat)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);

    if (!chat)
    {
        cout << "[api: " << apiIndex << "] Initialization completed!" << endl;
        return;
    }
    if (chat)
    {
        if (chat->hasChanged(MegaChatRoom::CHANGE_TYPE_USER_TYPING))
        {
            uhAction[apiIndex] = chat->getUserTyping();
            userTyping[apiIndex] = true;
        }
        else if (chat->hasChanged(MegaChatRoom::CHANGE_TYPE_TITLE))
        {
            titleUpdated[apiIndex] = true;
        }
    }

    cout << "[api: " << apiIndex << "] Chat updated - ";
    MegaChatApiTest::printChatRoomInfo(chat);
    chatUpdated[apiIndex] = chat->getChatId();
}

void TestChatRoomListener::onMessageLoaded(MegaChatApi *api, MegaChatMessage *msg)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);

    if (msg)
    {
        cout << endl << "[api: " << apiIndex << "] Message loaded - ";
        MegaChatApiTest::printMessageInfo(msg);

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

        msgCount[apiIndex]++;
        msgId[apiIndex] = msg->getMsgId();
        msgLoaded[apiIndex] = true;
    }
    else
    {
        historyLoaded[apiIndex] = true;
        cout << "[api: " << apiIndex << "] Loading of messages completed" << endl;
    }
}

void TestChatRoomListener::onMessageReceived(MegaChatApi *api, MegaChatMessage *msg)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);
    cout << "[api: " << apiIndex << "] Message received - ";
    MegaChatApiTest::printMessageInfo(msg);

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

    msgId[apiIndex] = msg->getMsgId();
    msgReceived[apiIndex] = true;
}

void TestChatRoomListener::onMessageUpdate(MegaChatApi *api, MegaChatMessage *msg)
{
    unsigned int apiIndex = getMegaChatApiIndex(api);
    cout << "[api: " << apiIndex << "] Message updated - ";
    MegaChatApiTest::printMessageInfo(msg);

    msgId[apiIndex] = msg->getMsgId();

    if (msg->getStatus() == MegaChatMessage::STATUS_SERVER_RECEIVED)
    {
        msgConfirmed[apiIndex] = true;
    }
    else if (msg->getStatus() == MegaChatMessage::STATUS_DELIVERED)
    {
        msgDelivered[apiIndex] = true;
    }

    if (msg->isEdited())
    {
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
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        if (api == this->megaChatApi[i])
        {
            apiIndex = i;
            break;
        }
    }

    if (apiIndex == -1)
    {
        cout << "TEST - Instance of MegaChatApi not recognized" << endl;
        ASSERT_CHAT_TEST(false, "");
    }
    return apiIndex;
}

MegaLoggerSDK::MegaLoggerSDK(const char *filename)
{
    sdklog.open(filename, ios::out | ios::app);
}

MegaLoggerSDK::~MegaLoggerSDK()
{
    sdklog.close();
}

void MegaLoggerSDK::log(const char *time, int loglevel, const char *source, const char *message)
{
    sdklog << "[" << time << "] " << SimpleLogger::toStr((LogLevel)loglevel) << ": ";
    sdklog << message << " (" << source << ")" << endl;
}

MegaChatLoggerSDK::MegaChatLoggerSDK(const char *filename)
{
    sdklog.open(filename, ios::out | ios::app);
}

MegaChatLoggerSDK::~MegaChatLoggerSDK()
{
    sdklog.close();
}

void MegaChatLoggerSDK::log(int loglevel, const char *message)
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
    sdklog  << message;
}
