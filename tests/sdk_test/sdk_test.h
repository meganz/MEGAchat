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

static const unsigned int pollingT      = 500000;   // (microseconds) to check if response from server is received
static const unsigned int maxTimeout    = 300;      // Maximum time (seconds) to wait for response from server
static const unsigned int NUM_ACCOUNTS  = 2;

class MegaLoggerSDK : public MegaLogger {

public:
    MegaLoggerSDK(const char *filename);
    ~MegaLoggerSDK();

private:
    ofstream sdklog;

protected:
    void log(const char *time, int loglevel, const char *source, const char *message);
};

class MegaChatLoggerSDK : public MegaChatLogger {

public:
    MegaChatLoggerSDK(const char *filename);
    ~MegaChatLoggerSDK();

private:
    ofstream sdklog;

protected:
    void log(int loglevel, const char *message);
};

class MegaChatApiTest : public MegaRequestListener, MegaChatRequestListener, MegaChatListener
{
public:
    MegaChatApiTest();
    void init();
    char *login(int accountIndex, const char *session = NULL);
    void logout(int accountIndex, bool closeSession = false);
    void terminate();

    static void printChatRoomInfo(const MegaChatRoom *);
    static void printMessageInfo(const MegaChatMessage *);

    bool waitForResponse(bool *responseReceived, int timeout = maxTimeout);

    MegaApi* megaApi[NUM_ACCOUNTS];
    MegaChatApi* megaChatApi[NUM_ACCOUNTS];

    // flags to monitor the completion of requests/transfers
    bool requestFlags[NUM_ACCOUNTS][MegaRequest::TYPE_CHAT_SET_TITLE];
    bool requestFlagsChat[NUM_ACCOUNTS][MegaChatRequest::TOTAL_OF_REQUEST_TYPES];

    void TEST_resumeSession();
    void TEST_setOnlineStatus();
    void TEST_getChatRoomsAndMessages();
    void TEST_groupChatManagement();

private:
    std::string email[NUM_ACCOUNTS];
    std::string pwd[NUM_ACCOUNTS];

    int lastError[NUM_ACCOUNTS];

    MegaChatHandle chatid;  // chatroom created...
    bool chatUpdated[NUM_ACCOUNTS];

//    MegaContactRequest* cr[2];

    // flags to monitor the updates of nodes/users/PCRs due to actionpackets
//    bool nodeUpdated[2];
//    bool userUpdated[2];
//    bool contactRequestUpdated[2];

//    MegaChatRoomList *chats;   //  runtime cache of fetched/updated chats
//    MegaHandle chatid;         // opened chatroom

    MegaLoggerSDK *logger;
    MegaChatLoggerSDK *chatLogger;

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
    void onOnlineStatusUpdate(MegaChatApi* api, int status);
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

class TestChatRoomListener : public MegaChatRoomListener
{
public:
    TestChatRoomListener(MegaChatApi *api, MegaChatHandle chatid);

    MegaChatApi *api;
    MegaChatHandle chatid;

    bool historyLoaded;   // when, after loadMessage(X), X messages have been loaded
    bool msgLoaded;
    bool msgConfirmed;
    bool msgReceived;

    MegaChatHandle msgId;
    bool chatUpdated;

    // implementation for MegaChatRoomListener
    virtual void onChatRoomUpdate(MegaChatApi* api, MegaChatRoom *chat);
    virtual void onMessageLoaded(MegaChatApi* api, MegaChatMessage *msg);   // loaded by getMessages()
    virtual void onMessageReceived(MegaChatApi* api, MegaChatMessage *msg);
    virtual void onMessageUpdate(MegaChatApi* api, MegaChatMessage *msg);   // new or updated
};
