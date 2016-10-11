#include "sdk_test.h"

#include <megaapi.h>
#include "../../src/megachatapi.h"
#include "../../src/karereCommon.h" // for logging with karere facility

#include <signal.h>

void sigintHandler(int)
{
    printf("SIGINT Received\n");
    fflush(stdout);
}

int main(int argc, char **argv)
{
//    ::mega::MegaClient::APIURL = "https://staging.api.mega.co.nz/";

    MegaChatApiTest t;

    // Create a new session
    char *session = t.login(0);

    // Set online status to busy
    bool *flag = &t.requestFlagsChat[0][MegaChatRequest::TYPE_SET_ONLINE_STATUS]; *flag = false;
    t.megaChatApi[0]->setOnlineStatus(MegaChatApi::STATUS_BUSY);
    assert(t.waitForResponse(flag));

    // Print chats
    MegaChatRoomList *chats = t.megaChatApi[0]->getChatRooms();
    cout << chats->size() << " chat/s received: " << endl;
    for (int i = 0; i < chats->size(); i++)
    {
        t.printChatRoomInfo(chats->get(i));
    }

    // Open chats and print history
    for (int i = 0; i < chats->size(); i++)
    {
        // Open a chatroom
        MegaChatHandle chatid = chats->get(i)->getChatId();
        TestChatRoomListener *chatroomListener = new TestChatRoomListener(chatid);
        assert(t.megaChatApi[0]->openChatRoom(chatid, chatroomListener));

        // Load history
        flag = &chatroomListener->historyLoaded; *flag = false;
        t.megaChatApi[0]->loadMessages(chatid, 500);
        assert(t.waitForResponse(flag));

        // Close the chatroom
        t.megaChatApi[0]->closeChatRoom(chatid, chatroomListener);
        delete chatroomListener;
    }

    // Create a group chat
    /*  - Establish contact relationship: invite, accept
     *  - Create group chat
     *  - Invite the contact
     *  - Update permissions of peer
     *  - Remove the peer from chat
     *  - Set title
     *  - Send a message
     *  - Edit a message
     *  - Delete a message
     *  - Send 5 messages
     *  - Truncate history from 3rd message
     *  - Attach node
     *  - Attach contact
     */



//    // 8. Send a message and wait for confirmation from server
//    string msg = "HOLA - This is a testing message automatically sent";
//    flag = &chatroomListener->msgConfirmed; *flag = false;
//    chatroomListener->msgId = megachat::INVALID_HANDLE;   // will be set at confirmation
//    t.megaChatApi[0]->sendMessage(chatid, msg.c_str(), MegaChatMessage::TYPE_NORMAL);
//    assert(t.waitForResponse(flag));    // for confirmation, sendMessage() is synchronous

//    sleep(60);

//    // 9. Edit the sent message
//    MegaChatHandle msgId = chatroomListener->msgId;
//    msg = "Edited message: this is a test";
//    flag = &chatroomListener->msgConfirmed; *flag = false;
//    chatroomListener->msgId = megachat::INVALID_HANDLE;   // will be set at confirmation
//    t.megaChatApi[0]->editMessage(chatid, msgId, msg.c_str());
//    assert(t.waitForResponse(flag));

//    // 9.1. Delete the message
//    flag = &chatroomListener->msgConfirmed; *flag = false;
//    chatroomListener->msgId = megachat::INVALID_HANDLE;   // will be set at confirmation
//    t.megaChatApi[0]->deleteMessage(chatid, msgId);
//    assert(t.waitForResponse(flag));




    // Resume an existing session
    t.logout(0); // keeps session alive
    char *tmpSession = t.login(0, session);
    assert (!strcmp(session, tmpSession));
    delete [] tmpSession;   tmpSession = NULL;

    // Close session and create a new one
    t.logout(0, true);
    delete [] session; session = NULL;
    session = t.login(0);

    // TODO: log in the other account, check the message with msgId has arrived


    t.terminate();

    return 0;
}

MegaChatApiTest::MegaChatApiTest()
{
    logger = new MegaLoggerSDK("SDK.log");
    MegaApi::setLoggerObject(logger);

    chatLogger = new MegaChatLoggerSDK("SDKchat.log");
    MegaChatApi::setLoggerObject(chatLogger);

    // do some initialization
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        // get credentials from environment variables
        std::string varName = "MEGA_EMAIL";
        varName += std::to_string(i);
        char *buf = getenv(varName.c_str());
        if (buf)
            email[i].assign(buf);
        if (!email[i].length())
        {
            cout << "TEST - Set your username at the environment variable $" << varName << endl;
            exit(-1);
        }

        varName.assign("MEGA_PWD");
        varName += std::to_string(i);
        buf = getenv(varName.c_str());
        if (buf)
            pwd[i].assign(buf);
        if (!pwd[i].length())
        {
            cout << "TEST - Set your password at the environment variable $" << varName << endl;
            exit(-1);
        }

        char path[1024];
        getcwd(path, sizeof path);
        megaApi[i] = new MegaApi(APP_KEY.c_str(), path, USER_AGENT.c_str());
        megaApi[i]->setLogLevel(MegaApi::LOG_LEVEL_DEBUG);
        megaApi[i]->addRequestListener(this);
        megaApi[i]->log(MegaApi::LOG_LEVEL_INFO, "___ Initializing tests for chat ___");

        megaChatApi[i] = new MegaChatApi(megaApi[i]);
        megaChatApi[i]->setLogLevel(MegaChatApi::LOG_LEVEL_DEBUG);
        megaChatApi[i]->addChatRequestListener(this);
        megaChatApi[i]->addChatListener(this);
        signal(SIGINT, sigintHandler);
        megaApi[i]->log(MegaChatApi::LOG_LEVEL_INFO, "___ Initializing tests for chat SDK___");
    }
}

char *MegaChatApiTest::login(int accountIndex, const char *session)
{
    // 1. login
    bool *flag = &requestFlags[accountIndex][MegaRequest::TYPE_LOGIN]; *flag = false;
    session ? megaApi[accountIndex]->fastLogin(session) : megaApi[accountIndex]->login(email[accountIndex].c_str(), pwd[accountIndex].c_str());
    assert(waitForResponse(flag));

    // 2. fetchnodes
    flag = &requestFlags[accountIndex][MegaRequest::TYPE_FETCH_NODES]; *flag = false;
    megaApi[accountIndex]->fetchNodes();
    assert(waitForResponse(flag));

    // 3. Initialize chat engine
    flag = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_INITIALIZE]; *flag = false;
    megaChatApi[accountIndex]->init(session);
    assert(waitForResponse(flag));

    // 4. Connect to chat servers
    flag = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_CONNECT]; *flag = false;
    megaChatApi[accountIndex]->connect();
    assert(waitForResponse(flag));

    return megaApi[accountIndex]->dumpSession();
}

void MegaChatApiTest::logout(int accountIndex, bool closeSession)
{
    bool *flag = &requestFlags[accountIndex][MegaRequest::TYPE_LOGOUT]; *flag = false;
    closeSession ? megaApi[accountIndex]->logout() : megaApi[accountIndex]->localLogout();
    assert(waitForResponse(flag));

    flag = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_LOGOUT]; *flag = false;
    closeSession ? megaChatApi[accountIndex]->logout() : megaChatApi[accountIndex]->localLogout();
    assert(waitForResponse(flag));
}

void MegaChatApiTest::terminate()
{    
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        delete megaChatApi[i];
        delete megaApi[i];

        megaApi[i] = NULL;
        megaChatApi[i] = NULL;
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

    cout << "Chat ID: " << hstr << endl;
    cout << "\tOwn privilege level: " << MegaChatRoom::privToString(chat->getOwnPrivilege()) << endl;
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
        cout << "\t\t(userhandle)\t(privilege level)" << endl;
        for (unsigned i = 0; i < chat->getPeerCount(); i++)
        {
            MegaChatHandle uh = chat->getPeerHandle(i);
            Base64::btoa((const byte *)&uh, sizeof(handle), hstr);
            cout << "\t\t\t" << hstr;
            cout << "\t" << MegaChatRoom::privToString(chat->getPeerPrivilege(i)) << endl;
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
    cout << "\tOnline state: " << MegaChatRoom::statusToString(chat->getOnlineStatus()) << endl;
    cout << "\tUnread count: " << chat->getUnreadCount() << " message/s" << endl;
    cout << "-------------------------------------------------" << endl;
}

bool MegaChatApiTest::waitForResponse(bool *responseReceived, int timeout)
{
    timeout *= 1000000; // convert to micro-seconds
    int tWaited = 0;    // microseconds
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

//    bool errorLevel = ((loglevel == logError) && !testingInvalidArgs);
//    ASSERT_FALSE(errorLevel) << "Test aborted due to an SDK error.";
}

void MegaChatApiTest::onRequestFinish(MegaChatApi *api, MegaChatRequest *request, MegaChatError *e)
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
        return;
    }

    requestFlagsChat[apiIndex][request->getType()] = true;
    lastError[apiIndex] = e->getErrorCode();

    switch(request->getType())
    {

    }
}

void MegaChatApiTest::onOnlineStatusUpdate(MegaChatApi *api, MegaChatApi::Status status)
{
    cout << "TEST - Online status has been updated: " << status << endl;
}

void MegaChatApiTest::onChatRoomUpdate(MegaChatApi *api, MegaChatRoom *chat)
{
    if (chat != NULL)
    {
        cout << "TEST - Chat added or updated (" << chat->getChatId() << ")" << endl;
    }
    else
    {
        cout << "TEST - " <<  megaChatApi[0]->getChatRooms()->size() << " chat/s received" << endl;
    }
}

void MegaChatApiTest::onChatListItemUpdate(MegaChatApi *api, MegaChatListItem *item)
{
    if (item)
    {
        cout << "TEST - Chat list item added or updated (" << item->getChatId() << ")" << endl;
    }
}

TestChatRoomListener::TestChatRoomListener(MegaChatHandle chatid)
{
    this->chatid = chatid;
    this->historyLoaded = false;
    this->msgConfirmed = false;
    this->msgId = megachat::MEGACHAT_INVALID_HANDLE;
}

void TestChatRoomListener::onMessageLoaded(MegaChatApi *api, MegaChatMessage *msg)
{
    if (msg)
    {
        cout << "TEST - New message loaded from chat " << chatid << ": " << msg->getContent() << " (" << msg->getMsgId() << ")" << endl;
    }
    else
    {
        historyLoaded = true;
        cout << "TEST - Full history loaded. No more messages in chat " << chatid << endl;
    }
}

void TestChatRoomListener::onMessageReceived(MegaChatApi *api, MegaChatMessage *msg)
{
        cout << "TEST - New message received from chat " << chatid << ": " << msg->getContent() << " (" << msg->getMsgId() << ")" << endl;
}

void TestChatRoomListener::onMessageUpdate(MegaChatApi *api, MegaChatMessage *msg)
{
    cout << "TEST - Message updated from chat " << chatid << ": " << msg->getContent()  << " (" << msg->getMsgId() << ")" << endl;

    if (msg->getStatus() == MegaChatMessage::STATUS_SERVER_RECEIVED ||
            msg->getStatus() == MegaChatMessage::STATUS_DELIVERED)
    {
        msgConfirmed = true;
        msgId = msg->getMsgId();
    }
}

void MegaChatApiTest::onRequestFinish(MegaApi *api, MegaRequest *request, MegaError *e)
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
        return;
    }

    requestFlags[apiIndex][request->getType()] = true;
    lastError[apiIndex] = e->getErrorCode();

    switch(request->getType())
    {
    }
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
