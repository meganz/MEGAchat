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

    MegaSdkTest test;
    test.start();

    sleep(60);

    test.terminate();

    sleep(15);

    return 0;
}

MegaSdkTest::MegaSdkTest()
{
    logger = new MegaLoggerSDK("SDK.log");
    MegaApi::setLoggerObject(logger);

    // do some initialization
    megaApi[0] = megaApi[1] = NULL;

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
}

/**
 * This test currently chains several actions in a row. When a request finishes with success, it
 * calls the next one. The list of actions tested are:
 *
 * - Create a MegaApi
 * - Create a MegaChatApi
 * - MegaApi::login(user, pwd)
 * - MegaApi::fetchNodes()
 * - MegaChatApi::init()
 * - MegaChatApi::connect()
 * - MegaChatApi::setOnlineStatus(away)
 * - MegaChatApi::getChatRooms() - sync call
 * - MegaApi::getContacts() - sync call
 */
void MegaSdkTest::start()
{
    // 1. Create MegaApi instance
    char path[1024];
    getcwd(path, sizeof path);
    megaApi[0] = new MegaApi(APP_KEY.c_str(), path, USER_AGENT.c_str());

    megaApi[0]->setLogLevel(MegaApi::LOG_LEVEL_DEBUG);
    megaApi[0]->addRequestListener(this);
    megaApi[0]->log(MegaApi::LOG_LEVEL_INFO, "___ Initializing tests for chat ___");

    // 2. Create MegaChatApi instance
    megaChatApi[0] = new MegaChatApi(megaApi[0]);
    megaChatApi[0]->addChatRequestListener(this);
    megaChatApi[0]->addChatListener(this);
    signal(SIGINT, sigintHandler);

    // 3. Login into the user account and fetchnodes (launched in the login callback)
    megaApi[0]->login(email[0].c_str(), pwd[0].c_str());
}

void MegaSdkTest::terminate()
{
    delete megaChatApi[0];
    delete megaApi[0];
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

    requestFlags[apiIndex][request->getType()] = true;
    lastError[apiIndex] = e->getErrorCode();

    switch(request->getType())
    {
    case MegaChatRequest::TYPE_INITIALIZE:
        if (e->getErrorCode() == API_OK)
        {
            KR_LOG_DEBUG("Initialization of local cache successfully.");

            MegaChatRoomList *chats = megaChatApi[apiIndex]->getChatRooms();
            KR_LOG_DEBUG("Chats: ");
            for (int i = 0; i < chats->size(); i++)
            {
                KR_LOG_DEBUG("Chat %d", chats->get(i)->getChatId());
            }

            megaChatApi[apiIndex]->connect();
        }
        else
        {
            KR_LOG_ERROR("Initialization of local cache failed. Error: %s (%d)", e->getErrorString(), e->getErrorCode());
        }
        break;

    case MegaChatRequest::TYPE_CONNECT:
        if (e->getErrorCode() == API_OK)
        {
            KR_LOG_DEBUG("Connection to chat servers established!");

            megaChatApi[apiIndex]->setOnlineStatus(MegaChatApi::STATUS_AWAY);
        }
        else
        {
            KR_LOG_ERROR("Connection to chat servers error: %s (%d)", e->getErrorString(), e->getErrorCode());
        }
        break;

    case MegaChatRequest::TYPE_SET_ONLINE_STATUS:
        if (e->getErrorCode() == API_OK)
        {
            KR_LOG_DEBUG("Online status changed successfully.");
        }
        else
        {
            KR_LOG_ERROR("Online status change error: %s (%d)", e->getErrorString(), e->getErrorCode());
        }
        break;
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
        MegaChatRoomList *chats = megaChatApi[0]->getChatRooms();
        KR_LOG_DEBUG("%d chat/s received", chats->size());
    }
}

void MegaSdkTest::onChatListItemUpdate(MegaChatApi *api, MegaChatListItem *item)
{
    if (item)
    {
        KR_LOG_DEBUG("Chat list item added or updated");
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
    case MegaRequest::TYPE_LOGIN:
        if (e->getErrorCode() == API_OK)
        {
            megaApi[apiIndex]->fetchNodes();
        }
        break;

    case MegaRequest::TYPE_FETCH_NODES:
        if (e->getErrorCode() == API_OK)
        {
            megaChatApi[apiIndex]->init();
        }
        break;
    }
}







