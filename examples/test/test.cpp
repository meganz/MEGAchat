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

    // 1. login (new session)
    bool *flag = &t.requestFlags[0][MegaRequest::TYPE_LOGIN]; *flag = false;
    t.megaApi[0]->login(t.email[0].c_str(), t.pwd[0].c_str());
    assert(t.waitForResponse(flag));

    // 2. fetchnodes
    flag = &t.requestFlags[0][MegaRequest::TYPE_FETCH_NODES]; *flag = false;
    t.megaApi[0]->fetchNodes();
    assert(t.waitForResponse(flag));

    // 3. Initialize chat engine
    flag = &t.requestFlagsChat[0][MegaChatRequest::TYPE_INITIALIZE]; *flag = false;
    t.megaChatApi[0]->init(false);
    assert(t.waitForResponse(flag));

    // 3.1. Print chats
    MegaChatRoomList *chats = t.megaChatApi[0]->getChatRooms();
    cout << chats->size() << " chat/s received: " << endl;
    for (int i = 0; i < chats->size(); i++)
    {
        cout << "\t" << i+1 << ". Chat id: " << chats->get(i)->getChatId() << endl;
    }

    // 4. Connect to chat servers
    flag = &t.requestFlagsChat[0][MegaChatRequest::TYPE_CONNECT]; *flag = false;
    t.megaChatApi[0]->connect();
    assert(t.waitForResponse(flag));

    delete chats;
    chats = t.megaChatApi[0]->getChatRooms();

    // 5. Set online status to away
    flag = &t.requestFlagsChat[0][MegaChatRequest::TYPE_SET_ONLINE_STATUS]; *flag = false;
    t.megaChatApi[0]->setOnlineStatus(MegaChatApi::STATUS_AWAY);
    assert(t.waitForResponse(flag));

    for (int i = 0; i < chats->size(); i++)
    {
        // 6. Open a chatroom
        TestChatRoomListener *chatroomListener = new TestChatRoomListener;
        MegaChatHandle chatid = megachat::INVALID_HANDLE;
        chatid = (chats->size() > 0) ? chats->get(i)->getChatId() : megachat::INVALID_HANDLE;
        t.megaChatApi[0]->openChatRoom(chatid, chatroomListener);

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

    // Local logout keeping the session
    string session(t.megaApi[0]->dumpSession());
    flag = &t.requestFlags[0][MegaRequest::TYPE_LOGOUT]; *flag = false;
    t.megaApi[0]->localLogout();
    assert(t.waitForResponse(flag));

    flag = &t.requestFlagsChat[0][MegaChatRequest::TYPE_LOGOUT]; *flag = false;
    t.megaChatApi[0]->localLogout();
    assert(t.waitForResponse(flag));

    // Login with existing session
    flag = &t.requestFlags[0][MegaRequest::TYPE_LOGIN]; *flag = false;
    t.megaApi[0]->fastLogin(session.c_str());
    assert(t.waitForResponse(flag));

    flag = &t.requestFlags[0][MegaRequest::TYPE_FETCH_NODES]; *flag = false;
    t.megaApi[0]->fetchNodes();
    assert(t.waitForResponse(flag));

    flag = &t.requestFlagsChat[0][MegaChatRequest::TYPE_INITIALIZE]; *flag = false;
    t.megaChatApi[0]->init(true);
    assert(t.waitForResponse(flag));

    // TODO: get chatrooms, check they're the same than before logging out.
    // TODO: connect()

    // Logout invalidating the session
    flag = &t.requestFlags[0][MegaRequest::TYPE_LOGOUT]; *flag = false;
    t.megaApi[0]->logout();
    assert(t.waitForResponse(flag));

    flag = &t.requestFlagsChat[0][MegaChatRequest::TYPE_LOGOUT]; *flag = false;
    t.megaChatApi[0]->logout();
    assert(t.waitForResponse(flag));

    // Login with a new session
    flag = &t.requestFlags[0][MegaRequest::TYPE_LOGIN]; *flag = false;
    t.megaApi[0]->login(t.email[0].c_str(), t.pwd[0].c_str());
    assert(t.waitForResponse(flag));

    flag = &t.requestFlags[0][MegaRequest::TYPE_FETCH_NODES]; *flag = false;
    t.megaApi[0]->fetchNodes();
    assert(t.waitForResponse(flag));

    flag = &t.requestFlagsChat[0][MegaChatRequest::TYPE_INITIALIZE]; *flag = false;
    t.megaChatApi[0]->init(false);
    assert(t.waitForResponse(flag));

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
        cout << "Set your username at the environment variable $MEGA_EMAIL" << endl;
        exit(-1);
    }

    buf = getenv("MEGA_PWD");
    if (buf)
        pwd[0].assign(buf);
    if (!pwd[0].length())
    {
        cout << "Set your password at the environment variable $MEGA_PWD" << endl;
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
        LOG_err << "Instance of megaChatApi not recognized";
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
    KR_LOG_DEBUG("Online status has been updated: %d", status);
}

void MegaSdkTest::onChatRoomUpdate(MegaChatApi *api, MegaChatRoom *chat)
{
    if (chat != NULL)
    {
        KR_LOG_DEBUG("Chat added or updated (handle: %d)", chat->getChatId());
    }
    else
    {
        KR_LOG_DEBUG("%d chat/s received", megaChatApi[0]->getChatRooms()->size());
    }
}

void MegaSdkTest::onChatListItemUpdate(MegaChatApi *api, MegaChatListItem *item)
{
    if (item)
    {
        KR_LOG_DEBUG("Chat list item added or updated");
    }
}

TestChatRoomListener::TestChatRoomListener()
{
    historyLoaded = false;
    msgConfirmed = false;
    msgId = megachat::INVALID_HANDLE;
}

void TestChatRoomListener::onMessageLoaded(MegaChatApi *api, MegaChatMessage *msg)
{
    if (msg)
    {
        KR_LOG_DEBUG("New message loaded: %s", msg->getContent());
    }
    else
    {
        historyLoaded = true;
        KR_LOG_DEBUG("Full history loaded. No more messages");
    }
}

void TestChatRoomListener::onMessageReceived(MegaChatApi *api, MegaChatMessage *msg)
{
        KR_LOG_DEBUG("New message loaded: %s", msg->getContent());
}

void TestChatRoomListener::onMessageUpdate(MegaChatApi *api, MegaChatMessage *msg)
{
    KR_LOG_DEBUG("Message updated: %s", msg->getContent());

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
        LOG_err << "Instance of MegaApi not recognized";
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
