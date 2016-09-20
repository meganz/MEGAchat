/**
 * @file tests/sdk_test.cpp
 * @brief Mega SDK test file
 *
 * (c) 2016 by Mega Limited, Wellsford, New Zealand
 *
 * This file is part of the MEGA SDK - Client Access Engine.
 *
 * Applications using the MEGA API must present a valid application key
 * and comply with the the rules set forth in the Terms of Service.
 *
 * The MEGA SDK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @copyright Simplified (2-clause) BSD License.
 *
 * You should have received a copy of the license along with this
 * program.
 */

#include <mega.h>
#include <megaapi.h>
#include "../../src/megachatapi.h"

#include <iostream>
#include <fstream>

using namespace mega;
using namespace megachat;

static const string APP_KEY     = "MBoVFSyZ";
static const string USER_AGENT  = "Tests for Karere SDK functionality";

class MegaLoggerSDK : public MegaLogger {

public:
    MegaLoggerSDK(const char *filename);
    ~MegaLoggerSDK();

private:
    ofstream sdklog;

protected:
    void log(const char *time, int loglevel, const char *source, const char *message);
};

class MegaSdkTest : public MegaRequestListener, MegaChatRequestListener, MegaChatListener
{
public:
    MegaSdkTest();
    void start();
    void terminate();

private:
    MegaApi* megaApi[2];
    MegaChatApi* megaChatApi[2];

    string email[2];
    string pwd[2];

    int lastError[2];

    // flags to monitor the completion of requests/transfers
    bool requestFlags[2][MegaRequest::TYPE_CHAT_SET_TITLE];

    MegaContactRequest* cr[2];

    // flags to monitor the updates of nodes/users/PCRs due to actionpackets
    bool nodeUpdated[2];
    bool userUpdated[2];
    bool contactRequestUpdated[2];

#ifdef ENABLE_CHAT
    bool chatUpdated[2];                // flags to monitor the updates of chats due to actionpackets
    map<handle, MegaTextChat*> chats;   //  runtime cache of fetched/updated chats
    MegaHandle chatid;                  // last chat added
#endif

    MegaLoggerSDK *logger;

public:
    // implementation for MegaRequestListener
    virtual void onRequestStart(MegaApi *api, MegaRequest *request) {}
    virtual void onRequestUpdate(MegaApi*api, MegaRequest *request) {}
    virtual void onRequestFinish(MegaApi *api, MegaRequest *request, MegaError *e);
    virtual void onRequestTemporaryError(MegaApi *api, MegaRequest *request, MegaError* error) {}

    // implementation for MegaChatRequestListener
    virtual void onRequestStart(MegaChatApi* api, MegaChatRequest *request) {}
    virtual void onRequestFinish(MegaChatApi* api, MegaChatRequest *request, MegaChatError* e);
    virtual void onRequestUpdate(MegaChatApi*api, MegaChatRequest *request) {}
    virtual void onRequestTemporaryError(MegaChatApi *api, MegaChatRequest *request, MegaChatError* error) {}

    // implementation for MegaChatListener
    void onOnlineStatusUpdate(MegaChatApi* api, MegaChatApi::Status status);
    virtual void onChatRoomUpdate(MegaChatApi* api, MegaChatRoom *chat);
    virtual void onChatListItemUpdate(MegaChatApi* api, MegaChatListItem *item);



//    void onUsersUpdate(MegaApi* api, MegaUserList *users);
//    void onNodesUpdate(MegaApi* api, MegaNodeList *nodes);
//    void onAccountUpdate(MegaApi *api) {}
//    void onContactRequestsUpdate(MegaApi* api, MegaContactRequestList* requests);
//    void onReloadNeeded(MegaApi *api) {}
//#ifdef ENABLE_SYNC
//    void onSyncFileStateChanged(MegaApi *api, MegaSync *sync, const char *filePath, int newState) {}
//    void onSyncEvent(MegaApi *api, MegaSync *sync,  MegaSyncEvent *event) {}
//    void onSyncStateChanged(MegaApi *api,  MegaSync *sync) {}
//    void onGlobalSyncStateChanged(MegaApi* api) {}
//#endif
//#ifdef ENABLE_CHAT
//    void onChatsUpdate(MegaApi *api, MegaTextChatList *chats);
//#endif
};
