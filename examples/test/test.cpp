#include "test.h"

#include "../include/megaapi.h"
#include "../../src/megachatapi.h"

void sigintHandler(int)
{
    printf("SIGINT Received\n");
    fflush(stdout);
}

int main(int argc, char **argv)
{
    ::mega::MegaClient::APIURL = "https://staging.api.mega.co.nz/";

    MegaSdkTest test;
    test.start();

    sleep(3000000);
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
        log("Set your username at the environment variable $MEGA_EMAIL");

    buf = getenv("MEGA_PWD");
    if (buf)
        pwd[0].assign(buf);
    if (!pwd[0].length())
        log("Set your password at the environment variable $MEGA_PWD");
}

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
    megaChatApi[0]->addRequestListener(this);
    signal(SIGINT, sigintHandler);

    // 3. Login into the user account and fetchnodes (launched in the login callback)
    megaApi[0]->login(email[0].c_str(), pwd[0].c_str());
}

void MegaSdkTest::onRequestFinish(MegaChatApi *api, MegaChatRequest *request, MegaChatError *e)
{

}

void SdkTest::onRequestFinish(MegaApi *api, MegaRequest *request, MegaError *e)
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
            megaApi[apiIndex]->fetchnodes();
        }
        break;

    case MegaRequest::TYPE_FETCHNODES:
        if (e->getErrorCode() == API_OK)
        {
            megaChatApi[apiIndex]->init();
            MegaChatRoomList *chats = megaChatApi[apiIndex]->getChatRooms();
            megaChatApi[apiIndex]->connect();
        }
        break;
    }
}







