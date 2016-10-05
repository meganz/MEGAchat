#include "test.h"

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

    MegaSdkTest t;

    // Create a new session
    char *session = t.login();

    // Resume an existing session
    t.logout(); // keeps session alive
    char *tmpSession = t.login(session);
    assert (!strcmp(session, tmpSession));
    delete [] tmpSession;   tmpSession = NULL;

    // Close session and create a new one
    t.logout(true);
    delete [] session; session = NULL;
    session = t.login();

    // Set online status to busy
    bool *flag = &t.requestFlagsChat[0][MegaChatRequest::TYPE_SET_ONLINE_STATUS]; *flag = false;
    t.megaChatApi[0]->setOnlineStatus(MegaChatApi::STATUS_BUSY);
    assert(t.waitForResponse(flag));


    // Print chats
    MegaChatRoomList *chats = t.megaChatApi[0]->getChatRooms();
    cout << chats->size() << " chat/s received: " << endl;
    for (int i = 0; i < chats->size(); i++)
    {
        cout << "\t" << i+1 << ". Chat id: " << chats->get(i)->getChatId() << endl;
    }

    // Open chats and print history
    for (int i = 0; i < chats->size(); i++)
    {
        // 6. Open a chatroom
        MegaChatHandle chatid = chats->get(i)->getChatId();
        TestChatRoomListener *chatroomListener = new TestChatRoomListener(chatid);
        assert(t.megaChatApi[0]->openChatRoom(chatid, chatroomListener));

        // 7. Load history
        flag = &chatroomListener->historyLoaded; *flag = false;
        t.megaChatApi[0]->getMessages(chatid, 500);
        assert(t.waitForResponse(flag));

        // 8. Send a message and wait for confirmation from server
        string msg = "HOLA - This is a testing message automatically sent";
        flag = &chatroomListener->msgConfirmed; *flag = false;
        chatroomListener->msgId = megachat::INVALID_HANDLE;   // will be set at confirmation
        t.megaChatApi[0]->sendMessage(chatid, msg.c_str(), msg.size(), MegaChatMessage::TYPE_NORMAL);
        assert(t.waitForResponse(flag));    // for confirmation, sendMessage() is synchronous

        sleep(60);


        // 9. Edit the sent message
        MegaChatHandle msgId = chatroomListener->msgId;
        msg = "Edited message: this is a test";
        flag = &chatroomListener->msgConfirmed; *flag = false;
        chatroomListener->msgId = megachat::INVALID_HANDLE;   // will be set at confirmation
        t.megaChatApi[0]->editMessage(chatid, msgId, msg.c_str(), msg.length());
        assert(t.waitForResponse(flag));

        // 9.1. Delete the message
        flag = &chatroomListener->msgConfirmed; *flag = false;
        chatroomListener->msgId = megachat::INVALID_HANDLE;   // will be set at confirmation
        t.megaChatApi[0]->deleteMessage(chatid, msgId);
        assert(t.waitForResponse(flag));

        // 10. Close the chatroom
        t.megaChatApi[0]->closeChatRoom(chatid, chatroomListener);
        delete chatroomListener;
    }


    // TODO: log in the other account, check the message with msgId has arrived


    t.terminate();

    return 0;
}

MegaSdkTest::MegaSdkTest()
{
    logger = new MegaLoggerSDK("SDK.log");
    MegaApi::setLoggerObject(logger);

    chatLogger = new MegaChatLoggerSDK("SDKchat.log");
    MegaChatApi::setLoggerObject(chatLogger);

    // do some initialization
    megaApi[0] = megaApi[1] = NULL;
    megaChatApi[0] = megaChatApi[1] = NULL;

    char *buf = getenv("MEGA_EMAIL");
    if (buf)
        email[0].assign(buf);
    if (!email[0].length())
    {
        cout << "TEST - Set your username at the environment variable $MEGA_EMAIL" << endl;
        exit(-1);
    }

    buf = getenv("MEGA_PWD");
    if (buf)
        pwd[0].assign(buf);
    if (!pwd[0].length())
    {
        cout << "TEST - Set your password at the environment variable $MEGA_PWD" << endl;
        exit(-1);
    }

    buf = getenv("MEGA_EMAIL_AUX");
    if (buf)
        email[1].assign(buf);

    buf = getenv("MEGA_PWD_AUX");
    if (buf)
        pwd[1].assign(buf);

    // 1. Create MegaApi instance
    if (!megaApi[0])
    {
        char path[1024];
        getcwd(path, sizeof path);
        megaApi[0] = new MegaApi(APP_KEY.c_str(), path, USER_AGENT.c_str());

        megaApi[0]->setLogLevel(MegaApi::LOG_LEVEL_DEBUG);
        megaApi[0]->addRequestListener(this);
        megaApi[0]->log(MegaApi::LOG_LEVEL_INFO, "___ Initializing tests for chat ___");
    }

    // 2. Create MegaChatApi instance
    if (!megaChatApi[0])
    {
        megaChatApi[0] = new MegaChatApi(megaApi[0]);

        megaChatApi[0]->setLogLevel(MegaChatApi::LOG_LEVEL_DEBUG);
        megaChatApi[0]->addChatRequestListener(this);
        megaChatApi[0]->addChatListener(this);
        signal(SIGINT, sigintHandler);
        megaApi[0]->log(MegaChatApi::LOG_LEVEL_INFO, "___ Initializing tests for chat SDK___");
    }
}

char *MegaSdkTest::login(const char *session)
{
    // 1. login
    bool *flag = &requestFlags[0][MegaRequest::TYPE_LOGIN]; *flag = false;
    session ? megaApi[0]->fastLogin(session) : megaApi[0]->login(email[0].c_str(), pwd[0].c_str());
    assert(waitForResponse(flag));

    // 2. fetchnodes
    flag = &requestFlags[0][MegaRequest::TYPE_FETCH_NODES]; *flag = false;
    megaApi[0]->fetchNodes();
    assert(waitForResponse(flag));

    // 3. Initialize chat engine
    flag = &requestFlagsChat[0][MegaChatRequest::TYPE_INITIALIZE]; *flag = false;
    megaChatApi[0]->init(session);
    assert(waitForResponse(flag));

    // 4. Connect to chat servers
    flag = &requestFlagsChat[0][MegaChatRequest::TYPE_CONNECT]; *flag = false;
    megaChatApi[0]->connect();
    assert(waitForResponse(flag));

    return megaApi[0]->dumpSession();
}

void MegaSdkTest::logout(bool closeSession)
{
    bool *flag = &requestFlags[0][MegaRequest::TYPE_LOGOUT]; *flag = false;
    closeSession ? megaApi[0]->logout() : megaApi[0]->localLogout();
    assert(waitForResponse(flag));

    flag = &requestFlagsChat[0][MegaChatRequest::TYPE_LOGOUT]; *flag = false;
    closeSession ? megaChatApi[0]->logout() : megaChatApi[0]->localLogout();
    assert(waitForResponse(flag));
}

void MegaSdkTest::terminate()
{
    delete megaChatApi[0];
    delete megaApi[0];

    megaApi[0] = NULL;
    megaChatApi[0] = NULL;
}

bool MegaSdkTest::waitForResponse(bool *responseReceived, int timeout)
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

void MegaSdkTest::onRequestFinish(MegaChatApi *api, MegaChatRequest *request, MegaChatError *e)
{
    unsigned int apiIndex;
    if (api == megaChatApi[0])
    {
        apiIndex = 0;
    }
    else if (api == megaChatApi[1])
    {
        apiIndex = 1;
    }
    else
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

void MegaSdkTest::onOnlineStatusUpdate(MegaChatApi *api, MegaChatApi::Status status)
{
    cout << "TEST - Online status has been updated: " << status << endl;
}

void MegaSdkTest::onChatRoomUpdate(MegaChatApi *api, MegaChatRoom *chat)
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

void MegaSdkTest::onChatListItemUpdate(MegaChatApi *api, MegaChatListItem *item)
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
    this->msgId = megachat::INVALID_HANDLE;
}

void TestChatRoomListener::onMessageLoaded(MegaChatApi *api, MegaChatMessage *msg)
{
    if (msg)
    {
        cout << "TEST - New message loaded from chat " << chatid << ": " << msg->getContent() << endl;
    }
    else
    {
        historyLoaded = true;
        cout << "TEST - Full history loaded. No more messages in chat " << chatid << endl;
    }
}

void TestChatRoomListener::onMessageReceived(MegaChatApi *api, MegaChatMessage *msg)
{
        cout << "TEST - New message received from chat " << chatid << ": " << msg->getContent() << endl;
}

void TestChatRoomListener::onMessageUpdate(MegaChatApi *api, MegaChatMessage *msg)
{
    cout << "TEST - Message updated from chat " << chatid << ": " << msg->getContent() << endl;

    if (msg->getStatus() == MegaChatMessage::STATUS_SERVER_RECEIVED ||
            msg->getStatus() == MegaChatMessage::STATUS_DELIVERED)
    {
        msgConfirmed = true;
        msgId = msg->getMsgId();
    }
}

void MegaSdkTest::onRequestFinish(MegaApi *api, MegaRequest *request, MegaError *e)
{
    unsigned int apiIndex;
    if (api == megaApi[0])
    {
        apiIndex = 0;
    }
    else if (api == megaApi[1])
    {
        apiIndex = 1;
    }
    else
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
