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

#ifndef CHATTEST_H
#define CHATTEST_H

#include <mega.h>
#include <megaapi.h>
#include "megachatapi.h"

#include <iostream>
#include <fstream>

static const std::string APPLICATION_KEY = "MBoVFSyZ";
static const std::string USER_AGENT_DESCRIPTION  = "Tests for Karere SDK functionality";

static const unsigned int maxTimeout = 300;
static const unsigned int pollingT = 500000;   // (microseconds) to check if response from server is received
static const unsigned int NUM_ACCOUNTS = 2;

class ChatTestException : public std::exception
{
public:
    ChatTestException(const std::string& file, int line);

    virtual const char *what() const throw();

private:
    int mLine;
    std::string mFile;
    std::string mExceptionText;
};

#define ASSERT_CHAT_TEST(a) \
    if (!(a)) \
    { \
        throw ChatTestException(__FILE__, __LINE__); \
    } \

#define EXECUTE_TEST(test) \
    try \
    { \
        test; \
    } \
    catch(ChatTestException e) \
    { \
        t.logoutAccounts(true); \
        MegaChatApiTest::mFailedTests ++; \
        std::cout << e.what() << std::endl; \
    } \

class TestChatRoomListener;

class MegaLoggerSDK : public mega::MegaLogger {

public:
    MegaLoggerSDK(const char *filename);
    ~MegaLoggerSDK();

private:
    std::ofstream sdklog;

protected:
    void log(const char *time, int loglevel, const char *source, const char *message);
};

class MegaChatLoggerSDK : public megachat::MegaChatLogger {

public:
    MegaChatLoggerSDK(const char *filename);
    ~MegaChatLoggerSDK();

private:
    std::ofstream sdklog;

protected:
    void log(int loglevel, const char *message);
};

class Account
{
public:
    Account();
    Account(const std::string &email, const std::string &password);

    std::string getEmail() const;
    std::string getPassword() const;
private:
    std::string mEmail;
    std::string mPassword;
};

class TestChatRoomListener;

class MegaChatApiTest :
        public mega::MegaListener,
        public mega::MegaRequestListener,
        public mega::MegaTransferListener,
        public mega::MegaLogger,
        public megachat::MegaChatRequestListener,
        public megachat::MegaChatListener
{
public:
    MegaChatApiTest();
    ~MegaChatApiTest();

    void init();

    // email and password parameter is used if you don't want to use default values for accountIndex
    char *login(unsigned int accountIndex, const char *session = NULL, const char *email = NULL, const char *password = NULL);
    void logout(unsigned int accountIndex, bool closeSession = false);
    void terminate();
    void logoutAccounts(bool closeSession = false);

    static void printChatRoomInfo(const megachat::MegaChatRoom *);
    static void printMessageInfo(const megachat::MegaChatMessage *);
    static void printChatListItemInfo(const megachat::MegaChatListItem *);

    bool waitForResponse(bool *responseReceived, int timeout = maxTimeout) const;

    bool TEST_ResumeSession(unsigned int accountIndex);
    void TEST_SetOnlineStatus(unsigned int accountIndex);
    void TEST_GetChatRoomsAndMessages(unsigned int accountIndex);
    void TEST_EditAndDeleteMessages(unsigned int primaryAccountIndex, unsigned int secondaryAccountIndex);
    void TEST_GroupChatManagement(unsigned int primaryAccountIndex, unsigned int secondaryAccountIndex);
    void TEST_OfflineMode(unsigned int accountIndex);
    void TEST_ClearHistory(unsigned int primaryAccountIndex, unsigned int secondaryAccountIndex);
    void TEST_SwitchAccounts(unsigned int primaryAccountIndex, unsigned int secondaryAccountIndex);
    void TEST_SendContact(unsigned int primaryAccountIndex, unsigned int secondaryAccountIndex);
    void TEST_Attachment(unsigned int primaryAccountIndex, unsigned int secondaryAccountIndex);
    void TEST_attachmentPNG(unsigned int primaryAccountIndex, unsigned int secondaryAccountIndex);
    void TEST_LastMessage(unsigned int primaryAccountIndex, unsigned int secondaryAccountIndex);
    void TEST_GroupLastMessage(unsigned int primaryAccountIndex, unsigned int secondaryAccountIndex);

    static int mFailedTests;

private:
    int loadHistory(unsigned int accountIndex, megachat::MegaChatHandle chatid, TestChatRoomListener *chatroomListener);
    void makeContact(unsigned int primaryAccountIndex, unsigned int secondaryAccountIndex);
    megachat::MegaChatHandle getGroupChatRoom(unsigned int primaryAccountIndex, unsigned int secondaryAccountIndex,
                                              megachat::MegaChatPeerList *peers);

    megachat::MegaChatHandle getPeerToPeerChatRoom(unsigned int primaryAccountIndex, unsigned int secondaryAccountIndex);

    megachat::MegaChatMessage *sendTextMessageOrUpdate(unsigned int senderAccountIndex, unsigned int receiverAccountIndex,
                                               megachat::MegaChatHandle chatid, const std::string& textToSend,
                                               TestChatRoomListener *chatroomListener, megachat::MegaChatHandle messageId = megachat::MEGACHAT_INVALID_HANDLE);

    void checkEmail(unsigned int indexAccount);
    std::string dateToString();
    mega::MegaNode *attachNode(unsigned int primaryAccountIndex, unsigned int secondaryAccountIndex, megachat::MegaChatHandle chatid,
                                    mega::MegaNode *nodeToSend, TestChatRoomListener* chatroomListener);

    void clearHistory(unsigned int primaryAccountIndex, unsigned int secondaryAccountIndex, megachat::MegaChatHandle chatid, TestChatRoomListener *chatroomListener);
    void leaveChat(unsigned int accountIndex, megachat::MegaChatHandle chatid);

    unsigned int getMegaChatApiIndex(megachat::MegaChatApi *api);
    unsigned int getMegaApiIndex(mega::MegaApi *api);

    void createFile(const std::string &fileName, const std::string &originPath, const std::string &contain);
    mega::MegaNode *uploadFile(int accountIndex, const std::string &fileName, const std::string &originPath, const std::string &destinationPath);
    void addDownload();
    bool &isNotDownloadRunning();


    bool downloadNode(int accountIndex, mega::MegaNode *nodeToDownload);
    void importNode(int accountIndex, mega::MegaNode* node, const std::string& destinationName);

    void getContactRequest(unsigned int accountIndex, bool outgoing, int expectedSize = 1);

    Account mAccounts[NUM_ACCOUNTS];

    mega::MegaApi* megaApi[NUM_ACCOUNTS];
    megachat::MegaChatApi* megaChatApi[NUM_ACCOUNTS];

    // flags
    bool requestFlags[NUM_ACCOUNTS][mega::MegaRequest::TYPE_CHAT_SET_TITLE];
    bool requestFlagsChat[NUM_ACCOUNTS][megachat::MegaChatRequest::TOTAL_OF_REQUEST_TYPES];
    bool initStateChanged[NUM_ACCOUNTS];
    int initState[NUM_ACCOUNTS];
    int lastError[NUM_ACCOUNTS];
    int lastErrorChat[NUM_ACCOUNTS];
    int lastErrorTransfer[NUM_ACCOUNTS];

    megachat::MegaChatHandle chatid[NUM_ACCOUNTS];  // chatroom id from request
    megachat::MegaChatRoom *chatroom[NUM_ACCOUNTS];
    megachat::MegaChatListItem *chatListItem[NUM_ACCOUNTS];
    bool chatUpdated[NUM_ACCOUNTS];
    bool chatItemUpdated[NUM_ACCOUNTS];
    bool chatItemClosed[NUM_ACCOUNTS];
    bool peersUpdated[NUM_ACCOUNTS];
    bool titleUpdated[NUM_ACCOUNTS];

    std::string mFirstname;
    std::string mLastname;
    std::string mEmail;
    bool nameReceived[NUM_ACCOUNTS];

    std::string mChatFirstname;
    std::string mChatLastname;
    std::string mChatEmail;
    bool chatNameReceived[NUM_ACCOUNTS];

    mega::MegaHandle mNodeCopiedHandle;

    mega::MegaNodeList *mAttachmentNodeList;
    megachat::MegaChatHandle mAttachmentRevokeNode;

    MegaLoggerSDK *logger;
    MegaChatLoggerSDK *chatLogger;

    bool mNotDownloadRunning;

    bool attachNodeSend[NUM_ACCOUNTS];
    bool revokeNodeSend[NUM_ACCOUNTS];

    mega::MegaContactRequest* contactRequest[NUM_ACCOUNTS];
    bool contactRequestUpdated[2];

    static const std::string DEFAULT_PATH;
    static const std::string PATH_IMAGE;
    static const std::string FILE_IMAGE_NAME;

    static const std::string LOCAL_PATH;
    static const std::string REMOTE_PATH;
    static const std::string DOWNLOAD_PATH;

public:
    // implementation for MegaRequestListener
    virtual void onRequestStart(mega::MegaApi *api, mega::MegaRequest *request) {}
    virtual void onRequestUpdate(mega::MegaApi*api, mega::MegaRequest *request) {}
    virtual void onRequestFinish(mega::MegaApi *api, mega::MegaRequest *request, mega::MegaError *e);
    virtual void onRequestTemporaryError(mega::MegaApi *api, mega::MegaRequest *request, mega::MegaError* error) {}

    // implementation for MegaListener
    virtual void onContactRequestsUpdate(mega::MegaApi* api, mega::MegaContactRequestList* requests);

    // implementation for MegaChatRequestListener
    virtual void onRequestStart(megachat::MegaChatApi* api, megachat::MegaChatRequest *request) {}
    virtual void onRequestFinish(megachat::MegaChatApi* api, megachat::MegaChatRequest *request, megachat::MegaChatError* e);
    virtual void onRequestUpdate(megachat::MegaChatApi*api, megachat::MegaChatRequest *request) {}
    virtual void onRequestTemporaryError(megachat::MegaChatApi *api, megachat::MegaChatRequest *request, megachat::MegaChatError* error) {}

    // implementation for MegaChatListener
    virtual void onChatInitStateUpdate(megachat::MegaChatApi *api, int newState);
    virtual void onChatListItemUpdate(megachat::MegaChatApi* api, megachat::MegaChatListItem *item);
    virtual void onChatOnlineStatusUpdate(megachat::MegaChatApi* api, int status);

    virtual void onTransferStart(mega::MegaApi *api, mega::MegaTransfer *transfer);
    virtual void onTransferFinish(mega::MegaApi* api, mega::MegaTransfer *transfer, mega::MegaError* error);
    virtual void onTransferUpdate(mega::MegaApi *api, mega::MegaTransfer *transfer);
    virtual void onTransferTemporaryError(mega::MegaApi *api, mega::MegaTransfer *transfer, mega::MegaError* error);
    virtual bool onTransferData(mega::MegaApi *api, mega::MegaTransfer *transfer, char *buffer, size_t size);
};

class TestChatRoomListener : public megachat::MegaChatRoomListener
{
public:
    TestChatRoomListener(megachat::MegaChatApi **apis, megachat::MegaChatHandle chatid);

    megachat::MegaChatApi **megaChatApi;
    megachat::MegaChatHandle chatid;

    bool historyLoaded[NUM_ACCOUNTS];   // when, after loadMessage(X), X messages have been loaded
    bool historyTruncated[NUM_ACCOUNTS];
    bool msgLoaded[NUM_ACCOUNTS];
    bool msgConfirmed[NUM_ACCOUNTS];
    bool msgDelivered[NUM_ACCOUNTS];
    bool msgReceived[NUM_ACCOUNTS];
    bool msgEdited[NUM_ACCOUNTS];
    bool msgRejected[NUM_ACCOUNTS];
    bool msgAttachmentReceived[NUM_ACCOUNTS];
    bool msgContactReceived[NUM_ACCOUNTS];
    bool msgRevokeAttachmentReceived[NUM_ACCOUNTS];

    megachat::MegaChatMessage *message;
    megachat::MegaChatHandle msgId[NUM_ACCOUNTS];
    int msgCount[NUM_ACCOUNTS];
    megachat::MegaChatHandle uhAction[NUM_ACCOUNTS];
    int priv[NUM_ACCOUNTS];
    std::string content[NUM_ACCOUNTS];
    bool chatUpdated[NUM_ACCOUNTS];
    bool userTyping[NUM_ACCOUNTS];
    bool titleUpdated[NUM_ACCOUNTS];

    // implementation for MegaChatRoomListener
    virtual void onChatRoomUpdate(megachat::MegaChatApi* megaChatApi, megachat::MegaChatRoom *chat);
    virtual void onMessageLoaded(megachat::MegaChatApi* megaChatApi, megachat::MegaChatMessage *msg);   // loaded by getMessages()
    virtual void onMessageReceived(megachat::MegaChatApi* megaChatApi, megachat::MegaChatMessage *msg);
    virtual void onMessageUpdate(megachat::MegaChatApi* megaChatApi, megachat::MegaChatMessage *msg);   // new or updated

private:
    unsigned int getMegaChatApiIndex(megachat::MegaChatApi *api);
};


#endif // CHATTEST_H


