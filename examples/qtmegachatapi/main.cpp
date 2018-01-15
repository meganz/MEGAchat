#ifdef _WIN32
#include <winsock2.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <QApplication>
#include <QDir>
#include "mainwindow.h"
#include "chatWindow.h"
#include <base/gcm.h>
#include <base/services.h>
#include <chatClient.h>
#include <sdkApi.h>
#include <chatd.h>
#include <mega/megaclient.h>
#include <karereCommon.h>
#include <fstream>
#include <net/libwsIO.h>
#include "megachatapi.h"
#include "megachatapplication.h"
using namespace std;
using namespace promise;
using namespace mega;
using namespace megachat;
using namespace karere;

MegaChatApplication *mChatApplication=nullptr;

void createWindowAndClient(int &argc, char **argv)
{
    mChatApplication = new MegaChatApplication(argc,argv);
    mChatApplication->readSid();
    mChatApplication->init();
}

int main(int argc, char **argv)
{
    const char* customApiUrl = getenv("KR_API_URL");
    if (customApiUrl)
    {
        KR_LOG_WARNING("Using custom API server, due to KR_API_URL env variable");
        ::mega::MegaClient::APIURL = customApiUrl;
    }

    #ifdef __APPLE__
    //Set qt plugin dir for release builds
    #ifdef NDEBUG
        QDir dir(argv[0]);
        #ifdef __APPLE__
            dir.cdUp();
            dir.cdUp();
            dir.cd("Plugins");
        #else
            dir.cdUp();
            dir.cd("QtPlugins");
        #endif
        QApplication::setLibraryPaths(QStringList(dir.absolutePath()));
    #endif
    #endif

    createWindowAndClient(argc,argv);
    return mChatApplication->exec();
}
#include <main.moc>
