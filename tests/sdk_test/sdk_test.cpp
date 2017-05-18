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

void sigintHandler(int)
{
    printf("SIGINT Received\n");
    fflush(stdout);
}

int main(int argc, char **argv)
{
//    ::mega::MegaClient::APIURL = "https://staging.api.mega.co.nz/";
    MegaChatApiTest t;
    t.init();

    t.TEST_ResumeSession(0);
    t.TEST_SetOnlineStatus(0);
    t.TEST_GetChatRoomsAndMessages(0);
    t.TEST_SwitchAccounts(0, 1);
    t.TEST_ClearHistory(0, 1);
    t.TEST_EditAndDeleteMessages(0, 1);
    t.TEST_GroupChatManagement(0, 1);
    t.TEST_Attachment(0, 1);
    t.TEST_SendContact(0, 1);
    t.TEST_LastMessage(0, 1);
    t.TEST_GroupLastMessage(0, 1);

    t.terminate();

    return 0;
}

void handlerSignalINT(int)
{
    printf("SIGINT Received\n");
    fflush(stdout);
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
    : mActiveDownload(0)
    , mNotDownloadRunning(true)
    , mTotalDownload(0)
{
    logger = new MegaLoggerSDK("SDK.log");
    MegaApi::setLoggerObject(logger);

    chatLogger = new MegaChatLoggerSDK("SDKchat.log");
    MegaChatApi::setLoggerObject(chatLogger);

    Account account1("ar+test6@mega.nz", "1A2b3C4d5E");
    mAccounts[0] = account1;

    Account account2("ar+test7@mega.nz", "1A2b3C4d5E");
    mAccounts[1] = account2;
}

MegaChatApiTest::~MegaChatApiTest()
{
}

void MegaChatApiTest::init()
{
    // do some initialization
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        char path[1024];
        getcwd(path, sizeof path);
        megaApi[i] = new MegaApi(APPLICATION_KEY.c_str(), path, USER_AGENT_DESCRIPTION.c_str());
        megaApi[i]->setLogLevel(MegaApi::LOG_LEVEL_DEBUG);
        megaApi[i]->addRequestListener(this);
        megaApi[i]->log(MegaApi::LOG_LEVEL_INFO, "___ Initializing tests for chat ___");

        megaChatApi[i] = new MegaChatApi(megaApi[i]);
        megaChatApi[i]->setLogLevel(MegaChatApi::LOG_LEVEL_DEBUG);
        megaChatApi[i]->addChatRequestListener(this);
        megaChatApi[i]->addChatListener(this);
        megaApi[i]->log(MegaChatApi::LOG_LEVEL_INFO, "___ Initializing tests for chat SDK___");

        signal(SIGINT, handlerSignalINT);
    }
}

char *MegaChatApiTest::login(int accountIndex, const char *session, const char *email, const char *password)
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
    assert(waitForResponse(flagInit));
    if (!session)
    {
        assert(initState[accountIndex] == MegaChatApi::INIT_WAITING_NEW_SESSION);
    }
    else
    {
        assert(initState[accountIndex] == MegaChatApi::INIT_OFFLINE_SESSION);
    }

    // 2. login
    bool *flagLogin = &requestFlags[accountIndex][MegaRequest::TYPE_LOGIN]; *flagLogin = false;
    session ? megaApi[accountIndex]->fastLogin(session) : megaApi[accountIndex]->login(mail.c_str(), pwd.c_str());
    assert(waitForResponse(flagLogin));
    assert(!lastError[accountIndex]);

    // 3. fetchnodes
    flagInit = &initStateChanged[accountIndex]; *flagInit = false;
    bool *flagRequestFectchNodes = &requestFlags[accountIndex][MegaRequest::TYPE_FETCH_NODES]; *flagRequestFectchNodes = false;
    megaApi[accountIndex]->fetchNodes();
    assert(waitForResponse(flagRequestFectchNodes));
    assert(!lastError[accountIndex]);
    // after fetchnodes, karere should be ready for offline, at least
    assert(waitForResponse(flagInit));
    assert(initState[accountIndex] == MegaChatApi::INIT_ONLINE_SESSION);

    // 4. Connect to chat servers
    bool *flagRequestConnect = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_CONNECT]; *flagRequestConnect = false;
    megaChatApi[accountIndex]->connect();
    assert(waitForResponse(flagRequestConnect));
    assert(!lastError[accountIndex]);

    return megaApi[accountIndex]->dumpSession();
}

void MegaChatApiTest::logout(int accountIndex, bool closeSession)
{
    bool *flagRequestLogout = &requestFlags[accountIndex][MegaRequest::TYPE_LOGOUT]; *flagRequestLogout = false;
    closeSession ? megaApi[accountIndex]->logout() : megaApi[accountIndex]->localLogout();
    assert(waitForResponse(flagRequestLogout));
    assert(!lastError[accountIndex]);

    flagRequestLogout = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_LOGOUT]; *flagRequestLogout = false;
    closeSession ? megaChatApi[accountIndex]->logout() : megaChatApi[accountIndex]->localLogout();
    assert(waitForResponse(flagRequestLogout));
    assert(!lastErrorChat[accountIndex]);
}

void MegaChatApiTest::terminate()
{
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        megaApi[i]->removeRequestListener(this);
        megaChatApi[i]->removeChatRequestListener(this);
        megaChatApi[i]->removeChatListener(this);

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
//        assert(waitForResponse(flag));
//        session = login(0);
//    }

    // ___ Resume an existing session ___
    logout(accountIndex, false); // keeps session alive
    char *tmpSession = login(accountIndex, session);
    assert (!strcmp(session, tmpSession));
    delete [] tmpSession;   tmpSession = NULL;

    checkEmail(accountIndex);

    // ___ Resume an existing session without karere cache ___
    // logout from SDK keeping cache
    bool *flagSdkLogout = &requestFlags[accountIndex][MegaRequest::TYPE_LOGOUT]; *flagSdkLogout = false;
    megaApi[accountIndex]->localLogout();
    assert(waitForResponse(flagSdkLogout));
    assert(!lastError[accountIndex]);
    // logout from Karere removing cache
    bool *flagChatLogout = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_LOGOUT]; *flagChatLogout = false;
    megaChatApi[accountIndex]->logout();
    assert(waitForResponse(flagChatLogout));
    assert(!lastErrorChat[accountIndex]);
    // try to initialize chat engine with cache --> should fail
    assert(megaChatApi[accountIndex]->init(session) == MegaChatApi::INIT_NO_CACHE);
    megaApi[accountIndex]->invalidateCache();


    // ___ Re-create Karere cache without login out from SDK___
    bool *flagInit = &initStateChanged[accountIndex]; *flagInit = false;
    // login in SDK
    bool *flagLogin = &requestFlags[accountIndex][MegaRequest::TYPE_LOGIN]; *flagLogin = false;
    session ? megaApi[accountIndex]->fastLogin(session) : megaApi[accountIndex]->login(mAccounts[accountIndex].getEmail().c_str(), mAccounts[accountIndex].getPassword().c_str());
    assert(waitForResponse(flagLogin));
    assert(!lastError[accountIndex]);
    // fetchnodes in SDK
    bool *flagFetchNodes = &requestFlags[accountIndex][MegaRequest::TYPE_FETCH_NODES]; *flagFetchNodes = false;
    megaApi[accountIndex]->fetchNodes();
    assert(waitForResponse(flagFetchNodes));
    assert(!lastError[accountIndex]);
    assert(waitForResponse(flagInit));
    assert(initState[accountIndex] == MegaChatApi::INIT_ONLINE_SESSION);
    // check there's a list of chats already available
    MegaChatListItemList *list = megaChatApi[accountIndex]->getChatListItems();
    assert(list->size());
    delete list; list = NULL;

    // ___ Close session ___
    logout(accountIndex, true);
    delete [] session; session = NULL;

    // ___ Login with chat enabled, transition to disabled and back to enabled
    session = login(accountIndex);
    assert(session);
    // fully disable chat: logout + remove logger + delete MegaChatApi instance
    flagChatLogout = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_LOGOUT]; *flagChatLogout = false;
    megaChatApi[accountIndex]->logout();
    assert(waitForResponse(flagChatLogout));
    assert(!lastErrorChat[accountIndex]);
    megaChatApi[accountIndex]->setLoggerObject(NULL);
    delete megaChatApi[accountIndex];
    // create a new MegaChatApi instance
    MegaChatApi::setLoggerObject(chatLogger);
    megaChatApi[accountIndex] = new MegaChatApi(megaApi[accountIndex]);
    megaChatApi[accountIndex]->setLogLevel(MegaChatApi::LOG_LEVEL_DEBUG);
    megaChatApi[accountIndex]->addChatRequestListener(this);
    megaChatApi[accountIndex]->addChatListener(this);
    // back to enabled: init + fetchnodes + connect
    assert(megaChatApi[accountIndex]->init(session) == MegaChatApi::INIT_NO_CACHE);
    flagInit = &initStateChanged[accountIndex]; *flagInit = false;
    flagFetchNodes = &requestFlags[accountIndex][MegaRequest::TYPE_FETCH_NODES]; *flagFetchNodes = false;
    megaApi[accountIndex]->fetchNodes();
    assert(waitForResponse(flagFetchNodes));
    assert(!lastError[accountIndex]);
    assert(waitForResponse(flagInit));
    assert(initState[accountIndex] == MegaChatApi::INIT_ONLINE_SESSION);
    bool *flagConnect = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_CONNECT]; *flagConnect = false;
    megaChatApi[accountIndex]->connect();
    assert(waitForResponse(flagConnect));
    assert(!lastErrorChat[accountIndex]);
    // check there's a list of chats already available
    list = megaChatApi[accountIndex]->getChatListItems();
    assert(list->size());
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
    assert(waitForResponse(flagLogin));
    assert(!lastError[accountIndex]);
    session = megaApi[accountIndex]->dumpSession();
    // fetchnodes in SDK
    flagFetchNodes = &requestFlags[accountIndex][MegaRequest::TYPE_FETCH_NODES]; *flagFetchNodes = false;
    megaApi[accountIndex]->fetchNodes();
    assert(waitForResponse(flagFetchNodes));
    assert(!lastError[accountIndex]);

    // init in Karere
    assert(megaChatApi[accountIndex]->init(session) == MegaChatApi::INIT_NO_CACHE);
    // full-fetchndoes in SDK to regenerate cache in Karere
    flagInit = &initStateChanged[accountIndex]; *flagInit = false;
    flagFetchNodes = &requestFlags[accountIndex][MegaRequest::TYPE_FETCH_NODES]; *flagFetchNodes = false;
    megaApi[accountIndex]->fetchNodes();
    assert(waitForResponse(flagFetchNodes));
    assert(!lastError[accountIndex]);
    assert(waitForResponse(flagInit));
    assert(initState[accountIndex] == MegaChatApi::INIT_ONLINE_SESSION);
    // connect in Karere
    flagConnect = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_CONNECT]; *flagConnect = false;
    megaChatApi[accountIndex]->connect();
    assert(waitForResponse(flagConnect));
    assert(!lastErrorChat[accountIndex]);
    // check there's a list of chats already available
    list = megaChatApi[accountIndex]->getChatListItems();
    assert(list->size());
    delete list;
    list = NULL;

    // ___ Disconnect from chat server and reconnect ___
    bool *flagDisconnect = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_DISCONNECT]; *flagDisconnect = false;
    megaChatApi[accountIndex]->disconnect();
    assert(waitForResponse(flagDisconnect));
    assert(!lastErrorChat[accountIndex]);
    // reconnect
    flagConnect = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_CONNECT]; *flagConnect = false;
    megaChatApi[accountIndex]->connect();
    assert(waitForResponse(flagConnect));
    assert(!lastErrorChat[accountIndex]);
    // check there's a list of chats already available
    list = megaChatApi[accountIndex]->getChatListItems();
    assert(list->size());
    delete list;
    list = NULL;

    logout(accountIndex, true);
    delete [] session; session = NULL;
}

void MegaChatApiTest::TEST_SetOnlineStatus(unsigned int accountIndex)
{
    char *sesion = login(accountIndex);

    bool *flag = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_SET_ONLINE_STATUS]; *flag = false;
    megaChatApi[accountIndex]->setOnlineStatus(MegaChatApi::STATUS_BUSY);
    assert(waitForResponse(flag));

    logout(accountIndex, true);
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
        assert(megaChatApi[accountIndex]->openChatRoom(chatid, chatroomListener));

        // Print chatroom information and peers' names
        printChatRoomInfo(chatroom);
        if (chatroom->getPeerCount())
        {
            for (unsigned i = 0; i < chatroom->getPeerCount(); i++)
            {
                MegaChatHandle uh = chatroom->getPeerHandle(i);

                bool *flagNameReceived = &chatNameReceived[accountIndex]; *flagNameReceived = false; mChatFirstname = "";
                megaChatApi[accountIndex]->getUserFirstname(uh);
                assert(waitForResponse(flagNameReceived));
                assert(!lastErrorChat[accountIndex]);
                cout << "Peer firstname (" << uh << "): " << mChatFirstname << " (len: " << mChatFirstname.length() << ")" << endl;

                flagNameReceived = &chatNameReceived[accountIndex]; *flagNameReceived = false; mChatLastname = "";
                megaChatApi[0]->getUserLastname(uh);
                assert(waitForResponse(flagNameReceived));
                assert(!lastErrorChat[accountIndex]);
                cout << "Peer lastname (" << uh << "): " << mChatLastname << " (len: " << mChatLastname.length() << ")" << endl;

                char *email = megaChatApi[accountIndex]->getContactEmail(uh);
                if (email)
                {
                    cout << "Contact email (" << uh << "): " << email << " (len: " << strlen(email) << ")" << endl;
                    delete [] email;
                }
                else
                {
                    flagNameReceived = &chatNameReceived[accountIndex]; *flagNameReceived = false; mChatEmail = "";
                    megaChatApi[accountIndex]->getUserEmail(uh);
                    assert(waitForResponse(flagNameReceived));
                    assert(!lastErrorChat[accountIndex]);
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
        assert(megaChatApi[accountIndex]->openChatRoom(chatid, chatroomListener));
        cout << "Loading messages locally for chat " << chatroom->getTitle() << " (id: " << chatroom->getChatId() << ")" << endl;
        loadHistory(accountIndex, chatid, chatroomListener);

        // Close the chatroom
        megaChatApi[accountIndex]->closeChatRoom(chatid, chatroomListener);
        delete chatroomListener;

        delete chatroom;
        chatroom = NULL;
    }

    logout(accountIndex, true);
    delete sesion;
    sesion = NULL;
}

void MegaChatApiTest::TEST_EditAndDeleteMessages(unsigned int primaryAccountIndex, unsigned int secondaryAccountIndex)
{
    char *primarySession = login(primaryAccountIndex);
    char *secondarySession = login(secondaryAccountIndex);

    MegaUser *peerPrimary = megaApi[primaryAccountIndex]->getContact(mAccounts[secondaryAccountIndex].getEmail().c_str());
    if (!peerPrimary)
    {
        makeContact(primaryAccountIndex, secondaryAccountIndex);
        peerPrimary = megaApi[primaryAccountIndex]->getContact(mAccounts[secondaryAccountIndex].getEmail().c_str());
    }

    MegaChatHandle chatid = getPeerToPeerChatRoom(primaryAccountIndex, secondaryAccountIndex);

    delete peerPrimary;
    peerPrimary = NULL;

    // 1. A sends a message to B while B has the chat opened.
    // --> check the confirmed in A, the received message in B, the delivered in A
    TestChatRoomListener *chatroomListener = new TestChatRoomListener(megaChatApi, chatid);
    assert(megaChatApi[primaryAccountIndex]->openChatRoom(chatid, chatroomListener));
    assert(megaChatApi[secondaryAccountIndex]->openChatRoom(chatid, chatroomListener));

    // Load some message to feed history
    loadHistory(primaryAccountIndex, chatid, chatroomListener);
    loadHistory(secondaryAccountIndex, chatid, chatroomListener);

    std::string messageToSend = "HOLA " + mAccounts[primaryAccountIndex].getEmail() + " - This is a testing message automatically sent to you";
    MegaChatMessage *msgSent = sendTextMessageOrUpdate(primaryAccountIndex, secondaryAccountIndex, chatid, messageToSend, chatroomListener);

    // edit the message
    std::string messageToUpdate = "This is an edited message to " + mAccounts[primaryAccountIndex].getEmail();
    MegaChatMessage *msgUpdated = sendTextMessageOrUpdate(primaryAccountIndex, secondaryAccountIndex, chatid, messageToUpdate, chatroomListener, msgSent->getMsgId());
    delete msgUpdated; msgUpdated = NULL;
    delete msgSent; msgSent = NULL;

    // finally, clear history
    clearHistory(primaryAccountIndex, secondaryAccountIndex, chatid, chatroomListener);

    delete chatroomListener;

    // 2. A sends a message to B while B doesn't have the chat opened.
    // Then, B opens the chat --> check the received message in B, the delivered in A

    logout(secondaryAccountIndex, true);
    logout(primaryAccountIndex, true);

    delete [] primarySession;
    primarySession = NULL;
    delete [] secondarySession;
    secondarySession = NULL;
}

void MegaChatApiTest::TEST_GroupChatManagement(unsigned int primaryAccountIndex, unsigned int secondaryAccountIndex)
{
    char *sessionPrimary = login(primaryAccountIndex);
    char *sessionSecondary = login(secondaryAccountIndex);

    // Prepare peers, privileges...
    MegaUser *peer = megaApi[primaryAccountIndex]->getContact(mAccounts[secondaryAccountIndex].getEmail().c_str());
    if (!peer)
    {
        makeContact(primaryAccountIndex, secondaryAccountIndex);
    }

    MegaChatPeerList *peers = MegaChatPeerList::createInstance();
    peers->addPeer(peer->getHandle(), MegaChatPeerList::PRIV_STANDARD);

    MegaChatHandle chatid = createGroupChatRoom(primaryAccountIndex, secondaryAccountIndex, peers);
    delete peers;
    peers = NULL;

    // --> Open chatroom
    TestChatRoomListener *chatroomListener = new TestChatRoomListener(megaChatApi, chatid);
    assert(megaChatApi[primaryAccountIndex]->openChatRoom(chatid, chatroomListener));
    assert(megaChatApi[secondaryAccountIndex]->openChatRoom(chatid, chatroomListener));

    // --> Remove from chat
    bool *flagRemoveFromChat = &requestFlagsChat[primaryAccountIndex][MegaChatRequest::TYPE_REMOVE_FROM_CHATROOM]; *flagRemoveFromChat = false;
    bool *chatItemLeft0 = &chatItemUpdated[primaryAccountIndex]; *chatItemLeft0 = false;
    bool *chatItemLeft1 = &chatItemUpdated[secondaryAccountIndex]; *chatItemLeft1 = false;
    bool *chatItemClosed1 = &chatItemClosed[secondaryAccountIndex]; *chatItemClosed1 = false;
    bool *chatLeft0 = &chatroomListener->chatUpdated[primaryAccountIndex]; *chatLeft0 = false;
    bool *chatLeft1 = &chatroomListener->chatUpdated[secondaryAccountIndex]; *chatLeft1 = false;
    bool *mngMsgRecv = &chatroomListener->msgReceived[primaryAccountIndex]; *mngMsgRecv = false;
    MegaChatHandle *uhAction = &chatroomListener->uhAction[primaryAccountIndex]; *uhAction = MEGACHAT_INVALID_HANDLE;
    int *priv = &chatroomListener->priv[primaryAccountIndex]; *priv = MegaChatRoom::PRIV_UNKNOWN;
    megaChatApi[primaryAccountIndex]->removeFromChat(chatid, peer->getHandle());
    assert(waitForResponse(flagRemoveFromChat));
    assert(!lastErrorChat[primaryAccountIndex]);
    assert(waitForResponse(mngMsgRecv));
    assert(*uhAction == peer->getHandle());
    assert(*priv == MegaChatRoom::PRIV_RM);

    MegaChatRoom *chatroom = megaChatApi[primaryAccountIndex]->getChatRoom(chatid);
    assert (chatroom);
    assert(chatroom->getPeerCount() == 0);
    delete chatroom;

    assert(waitForResponse(chatItemLeft0));
    assert(waitForResponse(chatItemLeft1));
    assert(waitForResponse(chatItemClosed1));
    assert(waitForResponse(chatLeft0));

    assert(waitForResponse(chatLeft1));
    chatroom = megaChatApi[primaryAccountIndex]->getChatRoom(chatid);
    assert (chatroom);
    assert(chatroom->getPeerCount() == 0);
    delete chatroom;

    // Close the chatroom, even if we've been removed from it
    megaChatApi[secondaryAccountIndex]->closeChatRoom(chatid, chatroomListener);

    // --> Invite to chat
    bool *flagInviteToChatRoom = &requestFlagsChat[primaryAccountIndex][MegaChatRequest::TYPE_INVITE_TO_CHATROOM]; *flagInviteToChatRoom = false;
    bool *chatItemJoined0 = &chatItemUpdated[primaryAccountIndex]; *chatItemJoined0 = false;
    bool *chatItemJoined1 = &chatItemUpdated[secondaryAccountIndex]; *chatItemJoined1 = false;
    bool *chatJoined0 = &chatroomListener->chatUpdated[primaryAccountIndex]; *chatJoined0 = false;
    bool *chatJoined1 = &chatroomListener->chatUpdated[secondaryAccountIndex]; *chatJoined1 = false;
    mngMsgRecv = &chatroomListener->msgReceived[primaryAccountIndex]; *mngMsgRecv = false;
    uhAction = &chatroomListener->uhAction[primaryAccountIndex]; *uhAction = MEGACHAT_INVALID_HANDLE;
    priv = &chatroomListener->priv[primaryAccountIndex]; *priv = MegaChatRoom::PRIV_UNKNOWN;
    megaChatApi[primaryAccountIndex]->inviteToChat(chatid, peer->getHandle(), MegaChatPeerList::PRIV_STANDARD);
    assert(waitForResponse(flagInviteToChatRoom));
    assert(!lastErrorChat[primaryAccountIndex]);
    assert(waitForResponse(chatItemJoined0));
    assert(waitForResponse(chatItemJoined1));
    assert(waitForResponse(chatJoined0));
//    assert(waitForResponse(chatJoined1)); --> account 1 haven't opened chat, won't receive callback
    assert(waitForResponse(mngMsgRecv));
    assert(*uhAction == peer->getHandle());
    assert(*priv == MegaChatRoom::PRIV_UNKNOWN);    // the message doesn't report the new priv

    chatroom = megaChatApi[primaryAccountIndex]->getChatRoom(chatid);
    assert (chatroom);
    assert(chatroom->getPeerCount() == 1);
    delete chatroom;

    // since we were expulsed from chatroom, we need to open it again
    assert(megaChatApi[secondaryAccountIndex]->openChatRoom(chatid, chatroomListener));

    // invite again --> error
    flagInviteToChatRoom = &requestFlagsChat[primaryAccountIndex][MegaChatRequest::TYPE_INVITE_TO_CHATROOM]; *flagInviteToChatRoom = false;
    megaChatApi[primaryAccountIndex]->inviteToChat(chatid, peer->getHandle(), MegaChatPeerList::PRIV_STANDARD);
    assert(waitForResponse(flagInviteToChatRoom));
    assert(lastErrorChat[primaryAccountIndex] == MegaChatError::ERROR_EXIST);

    // --> Set title
    string title = "My groupchat with title";
    bool *flagChatRoomName = &requestFlagsChat[primaryAccountIndex][MegaChatRequest::TYPE_EDIT_CHATROOM_NAME]; *flagChatRoomName = false;
    bool *titleItemChanged0 = &titleUpdated[primaryAccountIndex]; *titleItemChanged0 = false;
    bool *titleItemChanged1 = &titleUpdated[secondaryAccountIndex]; *titleItemChanged1 = false;
    bool *titleChanged0 = &chatroomListener->titleUpdated[primaryAccountIndex]; *titleChanged0 = false;
    bool *titleChanged1 = &chatroomListener->titleUpdated[secondaryAccountIndex]; *titleChanged1 = false;
    mngMsgRecv = &chatroomListener->msgReceived[primaryAccountIndex]; *mngMsgRecv = false;
    string *msgContent = &chatroomListener->content[primaryAccountIndex]; *msgContent = "";
    megaChatApi[primaryAccountIndex]->setChatTitle(chatid, title.c_str());
    assert(waitForResponse(flagChatRoomName));
    assert(!lastErrorChat[primaryAccountIndex]);
    assert(waitForResponse(titleItemChanged0));
    assert(waitForResponse(titleItemChanged1));
    assert(waitForResponse(titleChanged0));
    assert(waitForResponse(titleChanged1));
    assert(waitForResponse(mngMsgRecv));
    assert(!strcmp(title.c_str(), msgContent->c_str()));


    chatroom = megaChatApi[secondaryAccountIndex]->getChatRoom(chatid);
    assert (chatroom);
    assert(!strcmp(chatroom->getTitle(), title.c_str()));
    delete chatroom;

    // --> Change peer privileges to Moderator
    bool *flagUpdatePeerPermision = &requestFlagsChat[primaryAccountIndex][MegaChatRequest::TYPE_UPDATE_PEER_PERMISSIONS]; *flagUpdatePeerPermision = false;
    bool *peerUpdated0 = &peersUpdated[primaryAccountIndex]; *peerUpdated0 = false;
    bool *peerUpdated1 = &peersUpdated[secondaryAccountIndex]; *peerUpdated1 = false;
    mngMsgRecv = &chatroomListener->msgReceived[primaryAccountIndex]; *mngMsgRecv = false;
    uhAction = &chatroomListener->uhAction[primaryAccountIndex]; *uhAction = MEGACHAT_INVALID_HANDLE;
    priv = &chatroomListener->priv[primaryAccountIndex]; *priv = MegaChatRoom::PRIV_UNKNOWN;
    megaChatApi[primaryAccountIndex]->updateChatPermissions(chatid, peer->getHandle(), MegaChatRoom::PRIV_MODERATOR);
    assert(waitForResponse(flagUpdatePeerPermision));
    assert(!lastErrorChat[primaryAccountIndex]);
    assert(waitForResponse(peerUpdated0));
    assert(waitForResponse(peerUpdated1));
    assert(waitForResponse(mngMsgRecv));
    assert(*uhAction == peer->getHandle());
    assert(*priv == MegaChatRoom::PRIV_MODERATOR);


    // --> Change peer privileges to Read-only
    flagUpdatePeerPermision = &requestFlagsChat[primaryAccountIndex][MegaChatRequest::TYPE_UPDATE_PEER_PERMISSIONS]; *flagUpdatePeerPermision = false;
    peerUpdated0 = &peersUpdated[primaryAccountIndex]; *peerUpdated0 = false;
    peerUpdated1 = &peersUpdated[secondaryAccountIndex]; *peerUpdated1 = false;
    mngMsgRecv = &chatroomListener->msgReceived[primaryAccountIndex]; *mngMsgRecv = false;
    uhAction = &chatroomListener->uhAction[primaryAccountIndex]; *uhAction = MEGACHAT_INVALID_HANDLE;
    priv = &chatroomListener->priv[primaryAccountIndex]; *priv = MegaChatRoom::PRIV_UNKNOWN;
    megaChatApi[primaryAccountIndex]->updateChatPermissions(chatid, peer->getHandle(), MegaChatRoom::PRIV_RO);
    assert(waitForResponse(flagUpdatePeerPermision));
    assert(!lastErrorChat[primaryAccountIndex]);
    assert(waitForResponse(peerUpdated0));
    assert(waitForResponse(peerUpdated1));
    assert(waitForResponse(mngMsgRecv));
    assert(*uhAction == peer->getHandle());
    assert(*priv == MegaChatRoom::PRIV_RO);


    // --> Try to send a message without the right privilege
    string msg1 = "HOLA " + mAccounts[primaryAccountIndex].getEmail()+ " - This message can't be send because I'm read-only";
    bool *flagRejected = &chatroomListener->msgRejected[secondaryAccountIndex]; *flagRejected = false;
    chatroomListener->msgId[secondaryAccountIndex] = MEGACHAT_INVALID_HANDLE;   // will be set at reception
    MegaChatMessage *msgSent = megaChatApi[secondaryAccountIndex]->sendMessage(chatid, msg1.c_str());
    assert(msgSent);
    delete msgSent; msgSent = NULL;
    assert(waitForResponse(flagRejected));    // for confirmation, sendMessage() is synchronous
    MegaChatHandle msgId0 = chatroomListener->msgId[secondaryAccountIndex];
    assert (msgId0 == MEGACHAT_INVALID_HANDLE);

    // --> Load some message to feed history
    loadHistory(primaryAccountIndex, chatid, chatroomListener);
    loadHistory(secondaryAccountIndex, chatid, chatroomListener);

    // --> Send typing notification
    bool *flagTyping1 = &chatroomListener->userTyping[secondaryAccountIndex]; *flagTyping1 = false;
    uhAction = &chatroomListener->uhAction[secondaryAccountIndex]; *uhAction = MEGACHAT_INVALID_HANDLE;
    megaChatApi[primaryAccountIndex]->sendTypingNotification(chatid);
    assert(waitForResponse(flagTyping1));
    assert(*uhAction == megaChatApi[primaryAccountIndex]->getMyUserHandle());

    // --> Send a message and wait for reception by target user
    string msg0 = "HOLA " + mAccounts[primaryAccountIndex].getEmail() + " - Testing groupchats";
    bool *msgConfirmed = &chatroomListener->msgConfirmed[primaryAccountIndex]; *msgConfirmed = false;
    bool *msgReceived = &chatroomListener->msgReceived[secondaryAccountIndex]; *msgReceived = false;
    bool *msgDelivered = &chatroomListener->msgDelivered[primaryAccountIndex]; *msgDelivered = false;
    chatroomListener->msgId[primaryAccountIndex] = MEGACHAT_INVALID_HANDLE;   // will be set at confirmation
    chatroomListener->msgId[secondaryAccountIndex] = MEGACHAT_INVALID_HANDLE;   // will be set at reception
    megaChatApi[primaryAccountIndex]->sendMessage(chatid, msg0.c_str());
    assert(waitForResponse(msgConfirmed));    // for confirmation, sendMessage() is synchronous
    MegaChatHandle msgId = chatroomListener->msgId[primaryAccountIndex];
    assert (msgId != MEGACHAT_INVALID_HANDLE);
    assert(waitForResponse(msgReceived));    // for reception
    assert (msgId == chatroomListener->msgId[secondaryAccountIndex]);
    MegaChatMessage *msg = megaChatApi[secondaryAccountIndex]->getMessage(chatid, msgId);   // message should be already received, so in RAM
    assert(msg && !strcmp(msg0.c_str(), msg->getContent()));
    assert(waitForResponse(msgDelivered));    // for delivery

    // --> Close the chatroom
    megaChatApi[primaryAccountIndex]->closeChatRoom(chatid, chatroomListener);
    megaChatApi[secondaryAccountIndex]->closeChatRoom(chatid, chatroomListener);
    delete chatroomListener;

    // --> Remove peer from groupchat
    bool *flagRemoveFromChatRoom = &requestFlagsChat[primaryAccountIndex][MegaChatRequest::TYPE_REMOVE_FROM_CHATROOM]; *flagRemoveFromChatRoom = false;
    bool *chatClosed = &chatItemClosed[secondaryAccountIndex]; *chatClosed = false;
    megaChatApi[primaryAccountIndex]->removeFromChat(chatid, peer->getHandle());
    assert(waitForResponse(flagRemoveFromChatRoom));
    assert(!lastErrorChat[primaryAccountIndex]);
    assert(waitForResponse(chatClosed));
    chatroom = megaChatApi[secondaryAccountIndex]->getChatRoom(chatid);
    assert(chatroom);
    assert(!chatroom->isActive());
    delete chatroom;    chatroom = NULL;

    leaveChat(primaryAccountIndex, chatid);
    leaveChat(secondaryAccountIndex, chatid);

    logout(secondaryAccountIndex, true);
    logout(primaryAccountIndex, true);

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
        assert(megaChatApi[accountIndex]->openChatRoom(chatid, chatroomListener));

        // Load some message to feed history
        bool *flagHistoryLoaded = &chatroomListener->historyLoaded[accountIndex]; *flagHistoryLoaded = false;
        megaChatApi[accountIndex]->loadMessages(chatid, 16);
        assert(waitForResponse(flagHistoryLoaded));
        assert(!lastErrorChat[accountIndex]);

        cout << endl << endl << "Disconnect from the Internet now" << endl << endl;
//        system("pause");


        string msg0 = "This is a test message sent without Internet connection";
        chatroomListener->msgId[accountIndex] = MEGACHAT_INVALID_HANDLE;   // will be set at confirmation
        MegaChatMessage *msgSent = megaChatApi[accountIndex]->sendMessage(chatid, msg0.c_str());
        assert(msgSent);
        assert(msgSent->getStatus() == MegaChatMessage::STATUS_SENDING);

        megaChatApi[accountIndex]->closeChatRoom(chatid, chatroomListener);

        // close session and resume it while offline
        logout(accountIndex, false);
        bool *flagInit = &initStateChanged[accountIndex]; *flagInit = false;
        megaChatApi[accountIndex]->init(session);
        assert(waitForResponse(flagInit));
        assert(initState[accountIndex] == MegaChatApi::INIT_OFFLINE_SESSION);

        // check the unsent message is properly loaded
        flagHistoryLoaded = &chatroomListener->historyLoaded[accountIndex]; *flagHistoryLoaded = false;
        bool *msgUnsentLoaded = &chatroomListener->msgLoaded[accountIndex]; *msgUnsentLoaded = false;
        chatroomListener->msgId[accountIndex] = MEGACHAT_INVALID_HANDLE;
        assert(megaChatApi[accountIndex]->openChatRoom(chatid, chatroomListener));
        bool msgUnsentFound = false;
        do
        {
            assert(waitForResponse(msgUnsentLoaded));
            if (chatroomListener->msgId[accountIndex] == msgSent->getMsgId())
            {
                msgUnsentFound = true;
                break;
            }
            *msgUnsentLoaded = false;
        } while (*flagHistoryLoaded);
        assert(msgUnsentFound);


        cout << endl << endl << "Connect to the Internet now" << endl << endl;
//        system("pause");


        flagHistoryLoaded = &chatroomListener->historyLoaded[accountIndex]; *flagHistoryLoaded = false;
        bool *msgSentLoaded = &chatroomListener->msgLoaded[accountIndex]; *msgSentLoaded = false;
        chatroomListener->msgId[accountIndex] = MEGACHAT_INVALID_HANDLE;
        assert(megaChatApi[accountIndex]->openChatRoom(chatid, chatroomListener));
        bool msgSentFound = false;
        do
        {
            assert(waitForResponse(msgSentLoaded));
            if (chatroomListener->msgId[accountIndex] == msgSent->getMsgId())
            {
                msgSentFound = true;
                break;
            }
            *msgSentLoaded = false;
        } while (*flagHistoryLoaded);

        assert(msgSentFound);
        delete msgSent; msgSent = NULL;
        delete chatroomListener;
        chatroomListener = NULL;
    }

    logout(accountIndex, true);
    delete [] session;
}

void MegaChatApiTest::TEST_ClearHistory(unsigned int primaryAccountIndex, unsigned int secondaryAccountIndex)
{
    char *sessionPrimary = login(primaryAccountIndex);
    char *sessionSecondary = login(secondaryAccountIndex);

    // Prepare peers, privileges...
    MegaUser *peer = megaApi[primaryAccountIndex]->getContact(mAccounts[secondaryAccountIndex].getEmail().c_str());
    if (!peer)
    {
        makeContact(primaryAccountIndex, secondaryAccountIndex);
    }

    MegaChatPeerList *peers = MegaChatPeerList::createInstance();
    peers->addPeer(peer->getHandle(), MegaChatPeerList::PRIV_STANDARD);

    MegaChatHandle chatid = createGroupChatRoom(primaryAccountIndex, secondaryAccountIndex, peers);
    delete peers;
    peers = NULL;

    // Open chatrooms
    TestChatRoomListener *chatroomListener = new TestChatRoomListener(megaChatApi, chatid);
    assert(megaChatApi[primaryAccountIndex]->openChatRoom(chatid, chatroomListener));
    assert(megaChatApi[secondaryAccountIndex]->openChatRoom(chatid, chatroomListener));

    // Send 5 messages to have some history
    for (int i = 0; i < 5; i++)
    {
        string msg0 = "HOLA " + mAccounts[primaryAccountIndex].getEmail() + " - Testing clearhistory. This messages is the number " + std::to_string(i);

        MegaChatMessage *message = sendTextMessageOrUpdate(primaryAccountIndex, secondaryAccountIndex, chatid, msg0, chatroomListener);

        delete message;
        message = NULL;
    }

    // Close the chatrooms
    megaChatApi[primaryAccountIndex]->closeChatRoom(chatid, chatroomListener);
    megaChatApi[secondaryAccountIndex]->closeChatRoom(chatid, chatroomListener);
    delete chatroomListener;

    // Open chatrooms
    chatroomListener = new TestChatRoomListener(megaChatApi, chatid);
    assert(megaChatApi[primaryAccountIndex]->openChatRoom(chatid, chatroomListener));
    assert(megaChatApi[secondaryAccountIndex]->openChatRoom(chatid, chatroomListener));

    // --> Load some message to feed history
    int count = loadHistory(primaryAccountIndex, chatid, chatroomListener);
    assert(count == 5);
    count = loadHistory(secondaryAccountIndex, chatid, chatroomListener);
    assert(count == 5);

    // Clear history
    clearHistory(primaryAccountIndex, secondaryAccountIndex, chatid, chatroomListener);

    // Close and re-open chatrooms
    megaChatApi[primaryAccountIndex]->closeChatRoom(chatid, chatroomListener);
    megaChatApi[secondaryAccountIndex]->closeChatRoom(chatid, chatroomListener);
    delete chatroomListener;
    chatroomListener = new TestChatRoomListener(megaChatApi, chatid);
    assert(megaChatApi[primaryAccountIndex]->openChatRoom(chatid, chatroomListener));
    assert(megaChatApi[secondaryAccountIndex]->openChatRoom(chatid, chatroomListener));

    // --> Check history is been truncated
    count = loadHistory(primaryAccountIndex, chatid, chatroomListener);
    assert(count == 1);
    count = loadHistory(secondaryAccountIndex, chatid, chatroomListener);
    assert(count == 1);

    // Close the chatrooms
    megaChatApi[primaryAccountIndex]->closeChatRoom(chatid, chatroomListener);
    megaChatApi[secondaryAccountIndex]->closeChatRoom(chatid, chatroomListener);
    delete chatroomListener;

    leaveChat(primaryAccountIndex, chatid);
    leaveChat(secondaryAccountIndex, chatid);

    logout(primaryAccountIndex, true);
    logout(secondaryAccountIndex, true);

    delete [] sessionPrimary;
    sessionPrimary = NULL;
    delete [] sessionSecondary;
    sessionSecondary = NULL;
}

void MegaChatApiTest::TEST_SwitchAccounts(unsigned int primaryAccountIndex, unsigned int secondaryAccountIndex)
{
    char *session = login(primaryAccountIndex);

    MegaChatListItemList *items = megaChatApi[primaryAccountIndex]->getChatListItems();
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
        MegaChatListItem *itemUpdated = megaChatApi[primaryAccountIndex]->getChatListItem(chatid);

        printChatListItemInfo(itemUpdated);

        delete itemUpdated;
        itemUpdated = NULL;

        continue;
    }

    delete items;
    items = NULL;

    logout(primaryAccountIndex, true);    // terminate() and destroy Client

    delete [] session;
    session = NULL;

    // LOgin over same index account but with other user
    session = login(primaryAccountIndex, NULL, mAccounts[secondaryAccountIndex].getEmail().c_str(), mAccounts[secondaryAccountIndex].getPassword().c_str());

    logout(primaryAccountIndex, true);

    delete [] session;
    session = NULL;
}

void MegaChatApiTest::TEST_Attachment(unsigned int primaryAccountIndex, unsigned int secondaryAccountIndex)
{
    char *primarySession = login(primaryAccountIndex);
    char *secondarySession = login(secondaryAccountIndex);

    MegaUser *peerPrimary = megaApi[primaryAccountIndex]->getContact(mAccounts[secondaryAccountIndex].getEmail().c_str());
    if (!peerPrimary)
    {
        makeContact(primaryAccountIndex, secondaryAccountIndex);
        peerPrimary = megaApi[primaryAccountIndex]->getContact(mAccounts[secondaryAccountIndex].getEmail().c_str());
    }

    delete peerPrimary;
    peerPrimary = NULL;

    MegaChatHandle chatid = getPeerToPeerChatRoom(primaryAccountIndex, secondaryAccountIndex);

    // 1. A sends a message to B while B has the chat opened.
    // --> check the confirmed in A, the received message in B, the delivered in A

    TestChatRoomListener *chatroomListener = new TestChatRoomListener(megaChatApi, chatid);
    assert(megaChatApi[primaryAccountIndex]->openChatRoom(chatid, chatroomListener));
    assert(megaChatApi[secondaryAccountIndex]->openChatRoom(chatid, chatroomListener));

    // Load some message to feed history
    loadHistory(primaryAccountIndex, chatid, chatroomListener);
    loadHistory(secondaryAccountIndex, chatid, chatroomListener);

    chatroomListener->msgId[primaryAccountIndex] = MEGACHAT_INVALID_HANDLE;   // will be set at confirmation
    chatroomListener->msgId[secondaryAccountIndex] = MEGACHAT_INVALID_HANDLE;   // will be set at reception

    struct stat st = {0};

    std::string downloadPath = "/tmp/download/";
    if (stat(downloadPath.c_str(), &st) == -1)
    {
        mkdir(downloadPath.c_str(), 0700);
    }

    std::string formatDate = dateToString();
    MegaHandle nodeSentHandle = createAndSendFile(primaryAccountIndex, secondaryAccountIndex, chatid, formatDate, formatDate, chatroomListener);

    MegaChatMessage* msgReceived = megaChatApi[secondaryAccountIndex]->getMessage(chatid, chatroomListener->msgId[secondaryAccountIndex]);

    // Download File
    assert(msgReceived);
    assert(msgReceived->getType() == MegaChatMessage::TYPE_NODE_ATTACHMENT);
    mega::MegaNodeList *nodeList = msgReceived->getMegaNodeList();
    MegaNode* node1 = nodeList->get(0);
    addDownload();
    megaApi[secondaryAccountIndex]->startDownload(node1, downloadPath.c_str(), this);
    assert(waitForResponse(&isNotDownloadRunning()));
    assert(lastErrorTransfer[secondaryAccountIndex] == API_OK);

    // Import node
    MegaNode *parentNode = megaApi[secondaryAccountIndex]->getNodeByPath("/");
    assert(parentNode);
    bool *flagNodeCopied = &requestFlags[secondaryAccountIndex][mega::MegaRequest::TYPE_COPY]; *flagNodeCopied = false;
    megaApi[secondaryAccountIndex]->copyNode(node1, parentNode, formatDate.c_str(), this);
    delete parentNode;
    assert(waitForResponse(flagNodeCopied));
    assert(!lastError[secondaryAccountIndex]);
    MegaNode *nodeCopied = megaApi[secondaryAccountIndex]->getNodeByHandle(mNodeCopiedHandle);
    assert(nodeCopied);
    delete nodeCopied;

    // Revoke node
    bool *flagConfirmed = &revokeNodeSend[primaryAccountIndex]; *flagConfirmed = false;
    bool *flagReceived = &chatroomListener->msgReceived[secondaryAccountIndex]; *flagReceived = false;
    chatroomListener->msgId[primaryAccountIndex] = MEGACHAT_INVALID_HANDLE;   // will be set at confirmation
    chatroomListener->msgId[secondaryAccountIndex] = MEGACHAT_INVALID_HANDLE;   // will be set at reception
    megachat::MegaChatHandle revokeAttachmentNode = nodeSentHandle;
    megaChatApi[primaryAccountIndex]->revokeAttachment(chatid, revokeAttachmentNode, this);
    assert(waitForResponse(flagConfirmed));
    MegaChatHandle msgId0 = chatroomListener->msgId[primaryAccountIndex];
    assert (msgId0 != MEGACHAT_INVALID_HANDLE);

    assert(waitForResponse(flagReceived));    // for reception
    MegaChatHandle msgId1 = chatroomListener->msgId[1];
    assert (msgId0 == msgId1);
    msgReceived = megaChatApi[secondaryAccountIndex]->getMessage(chatid, msgId0);   // message should be already received, so in RAM
    assert(msgReceived);
    assert(msgReceived->getType() == MegaChatMessage::TYPE_REVOKE_NODE_ATTACHMENT);

    // Remove file downloaded to try to download after revoke
    std::string filePath = downloadPath + std::string(formatDate);
    std::string secondaryFilePath = downloadPath + std::string("remove");
    rename(filePath.c_str(), secondaryFilePath.c_str());

    // Download File
    mega::MegaHandle nodeHandle = msgReceived->getHandleOfAction();
    assert(nodeHandle == node1->getHandle());

    addDownload();
    megaApi[secondaryAccountIndex]->startDownload(node1, downloadPath.c_str(), this);
    assert(waitForResponse(&isNotDownloadRunning()));
    assert(lastErrorTransfer[secondaryAccountIndex] != API_OK);

    clearHistory(primaryAccountIndex, secondaryAccountIndex, chatid, chatroomListener);

    logout(primaryAccountIndex, true);
    logout(secondaryAccountIndex, true);

    delete [] primarySession;
    primarySession = NULL;
    delete [] secondarySession;
    secondarySession = NULL;
}

void MegaChatApiTest::TEST_LastMessage(unsigned int primaryAccountIndex, unsigned int secondaryAccountIndex)
{
    char *sessionPrimary = login(primaryAccountIndex);
    char *sessionSecondary = login(secondaryAccountIndex);

    MegaUser *peerPrimary = megaApi[primaryAccountIndex]->getContact(mAccounts[secondaryAccountIndex].getEmail().c_str());
    if (!peerPrimary)
    {
        makeContact(primaryAccountIndex, secondaryAccountIndex);
        peerPrimary = megaApi[primaryAccountIndex]->getContact(mAccounts[secondaryAccountIndex].getEmail().c_str());
    }

    delete peerPrimary;
    peerPrimary = NULL;

    MegaChatHandle chatid = getPeerToPeerChatRoom(primaryAccountIndex, secondaryAccountIndex);

    TestChatRoomListener *chatroomListener = new TestChatRoomListener(megaChatApi, chatid);
    assert(megaChatApi[primaryAccountIndex]->openChatRoom(chatid, chatroomListener));
    assert(megaChatApi[secondaryAccountIndex]->openChatRoom(chatid, chatroomListener));

    // Load some message to feed history
    loadHistory(primaryAccountIndex, chatid, chatroomListener);
    loadHistory(secondaryAccountIndex, chatid, chatroomListener);

    std::string formatDate = dateToString();

    sendTextMessageOrUpdate(primaryAccountIndex, secondaryAccountIndex, chatid, formatDate, chatroomListener);

    MegaChatHandle msgId1 = chatroomListener->msgId[secondaryAccountIndex];
    assert (msgId1 != MEGACHAT_INVALID_HANDLE);

    MegaChatListItem *item = megaChatApi[primaryAccountIndex]->getChatListItem(chatid);
    assert(strcmp(formatDate.c_str(), item->getLastMessage()) == 0);
    delete item;
    item = NULL;

    clearHistory(primaryAccountIndex, secondaryAccountIndex, chatid, chatroomListener);

    formatDate = dateToString();
    createAndSendFile(primaryAccountIndex, secondaryAccountIndex, chatid, formatDate, formatDate, chatroomListener);
    msgId1 = chatroomListener->msgId[secondaryAccountIndex];
    assert (msgId1 != MEGACHAT_INVALID_HANDLE);

    item = megaChatApi[primaryAccountIndex]->getChatListItem(chatid);
    assert(strcmp(formatDate.c_str(), item->getLastMessage()) == 0);
    delete item;
    item = NULL;

    clearHistory(primaryAccountIndex, secondaryAccountIndex, chatid, chatroomListener);

    logout(primaryAccountIndex, true);
    logout(secondaryAccountIndex, true);

    delete [] sessionPrimary;
    sessionPrimary = NULL;
    delete [] sessionSecondary;
    sessionSecondary = NULL;
}

void MegaChatApiTest::TEST_SendContact(unsigned int primaryAccountIndex, unsigned int secondaryAccountIndex)
{
    char *primarySession = login(primaryAccountIndex);
    char *secondarySession = login(secondaryAccountIndex);

    MegaUser *peerPrimary = megaApi[primaryAccountIndex]->getContact(mAccounts[secondaryAccountIndex].getEmail().c_str());
    if (!peerPrimary)
    {
        makeContact(primaryAccountIndex, secondaryAccountIndex);
        peerPrimary = megaApi[primaryAccountIndex]->getContact(mAccounts[secondaryAccountIndex].getEmail().c_str());
    }

    delete peerPrimary;
    peerPrimary = NULL;

    MegaChatHandle chatid = getPeerToPeerChatRoom(primaryAccountIndex, secondaryAccountIndex);

    // 1. A sends a message to B while B has the chat opened.
    // --> check the confirmed in A, the received message in B, the delivered in A

    TestChatRoomListener *chatroomListener = new TestChatRoomListener(megaChatApi, chatid);
    assert(megaChatApi[primaryAccountIndex]->openChatRoom(chatid, chatroomListener));
    assert(megaChatApi[secondaryAccountIndex]->openChatRoom(chatid, chatroomListener));

    loadHistory(primaryAccountIndex, chatid, chatroomListener);
    loadHistory(secondaryAccountIndex, chatid, chatroomListener);

    bool *flagConfirmed = &chatroomListener->msgConfirmed[primaryAccountIndex]; *flagConfirmed = false;
    bool *flagReceived = &chatroomListener->msgContactReceived[secondaryAccountIndex]; *flagReceived = false;
    bool *flagDelivered = &chatroomListener->msgDelivered[primaryAccountIndex]; *flagDelivered = false;
    chatroomListener->msgId[primaryAccountIndex] = MEGACHAT_INVALID_HANDLE;   // will be set at confirmation
    chatroomListener->msgId[secondaryAccountIndex] = MEGACHAT_INVALID_HANDLE;   // will be set at reception

    MegaUser* user = megaApi[primaryAccountIndex]->getContact(mAccounts[secondaryAccountIndex].getEmail().c_str());
    assert(user);
    MegaChatHandle handle = user->getHandle();
    delete user;
    user = NULL;
    megaChatApi[primaryAccountIndex]->attachContacts(chatid, 1, &handle);
    assert(waitForResponse(flagConfirmed));
    MegaChatHandle msgId0 = chatroomListener->msgId[primaryAccountIndex];
    assert (msgId0 != MEGACHAT_INVALID_HANDLE);

    assert(waitForResponse(flagReceived));    // for reception
    MegaChatHandle msgId1 = chatroomListener->msgId[secondaryAccountIndex];
    assert (msgId0 == msgId1);
    MegaChatMessage *msgReceived = megaChatApi[secondaryAccountIndex]->getMessage(chatid, msgId0);   // message should be already received, so in RAM
    assert(msgReceived);

    assert(msgReceived->getType() == MegaChatMessage::TYPE_CONTACT_ATTACHMENT);
    assert(msgReceived->getUsersCount() > 0);

    assert(strcmp(msgReceived->getUserEmail(0), mAccounts[secondaryAccountIndex].getEmail().c_str()) == 0);

    delete msgReceived;
    msgReceived = NULL;

    clearHistory(primaryAccountIndex, secondaryAccountIndex, chatid, chatroomListener);

    logout(primaryAccountIndex, true);
    logout(secondaryAccountIndex, true);

    delete [] primarySession;
    primarySession = NULL;
    delete [] secondarySession;
    secondarySession = NULL;
}

void MegaChatApiTest::TEST_GroupLastMessage(unsigned int primaryAccountIndex, unsigned int secondaryAccountIndex)
{
    char *session0 = login(primaryAccountIndex);
    char *session1 = login(secondaryAccountIndex);

    // Prepare peers, privileges...
    MegaUser *peer = megaApi[primaryAccountIndex]->getContact(mAccounts[secondaryAccountIndex].getEmail().c_str());
    if (!peer)
    {
        makeContact(primaryAccountIndex, secondaryAccountIndex);
    }

    MegaChatPeerList *peers = MegaChatPeerList::createInstance();
    peers->addPeer(peer->getHandle(), MegaChatPeerList::PRIV_STANDARD);

    MegaChatHandle chatid = createGroupChatRoom(primaryAccountIndex, secondaryAccountIndex, peers);
    delete peers;
    peers = NULL;

    // --> Open chatroom
    TestChatRoomListener *chatroomListener = new TestChatRoomListener(megaChatApi, chatid);
    assert(megaChatApi[primaryAccountIndex]->openChatRoom(chatid, chatroomListener));
    assert(megaChatApi[1]->openChatRoom(chatid, chatroomListener));

    std::string textToSend = "Last Message";
    sendTextMessageOrUpdate(primaryAccountIndex, secondaryAccountIndex, chatid, textToSend, chatroomListener);

    // --> Set title
    string title = "My groupchat with title 2";
    bool *flagChatRoomName = &requestFlagsChat[primaryAccountIndex][MegaChatRequest::TYPE_EDIT_CHATROOM_NAME]; *flagChatRoomName = false;
    bool *titleItemChanged0 = &titleUpdated[primaryAccountIndex]; *titleItemChanged0 = false;
    bool *titleItemChanged1 = &titleUpdated[secondaryAccountIndex]; *titleItemChanged1 = false;
    bool *titleChanged0 = &chatroomListener->titleUpdated[primaryAccountIndex]; *titleChanged0 = false;
    bool *titleChanged1 = &chatroomListener->titleUpdated[secondaryAccountIndex]; *titleChanged1 = false;
    bool *mngMsgRecv = &chatroomListener->msgReceived[primaryAccountIndex]; *mngMsgRecv = false;
    string *msgContent = &chatroomListener->content[primaryAccountIndex]; *msgContent = "";
    megaChatApi[primaryAccountIndex]->setChatTitle(chatid, title.c_str());
    assert(waitForResponse(flagChatRoomName));
    assert(!lastErrorChat[primaryAccountIndex]);
    assert(waitForResponse(titleItemChanged0));
    assert(waitForResponse(titleItemChanged1));
    assert(waitForResponse(titleChanged0));
    assert(waitForResponse(titleChanged1));
    assert(waitForResponse(mngMsgRecv));
    assert(!strcmp(title.c_str(), msgContent->c_str()));

    MegaChatListItem *item = megaChatApi[primaryAccountIndex]->getChatListItem(chatid);
    assert(strcmp(textToSend.c_str(), item->getLastMessage()) == 0);
    delete item;
    item = NULL;

    clearHistory(primaryAccountIndex, secondaryAccountIndex, chatid, chatroomListener);

    leaveChat(primaryAccountIndex, chatid);
    leaveChat(secondaryAccountIndex, chatid);

    logout(secondaryAccountIndex, true);
    logout(primaryAccountIndex, true);

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
        assert(waitForResponse(flagHistoryLoaded));
        assert(!lastErrorChat[accountIndex]);
    }

    return chatroomListener->msgCount[accountIndex];
}

void MegaChatApiTest::makeContact(unsigned int primaryAccountIndex, unsigned int secondaryAccountIndex)
{
    bool *flagRequestInviteContact = &requestFlags[primaryAccountIndex][MegaRequest::TYPE_INVITE_CONTACT];
    *flagRequestInviteContact = false;
    bool *flagContactRequestUpdatedSecondary = &contactRequestUpdated[secondaryAccountIndex];
    *flagContactRequestUpdatedSecondary = false;
    std::string contactRequestMessage = "Contact Request Message";
    megaApi[primaryAccountIndex]->inviteContact(mAccounts[secondaryAccountIndex].getEmail().c_str(),
                                                contactRequestMessage.c_str(), MegaContactRequest::INVITE_ACTION_ADD, this);

    assert(waitForResponse(flagRequestInviteContact));
    assert(!lastError[primaryAccountIndex]);
    assert(waitForResponse(flagContactRequestUpdatedSecondary));

    getContactRequest(secondaryAccountIndex, false);

    bool *flagReplyContactRequest = &requestFlags[secondaryAccountIndex][MegaRequest::TYPE_REPLY_CONTACT_REQUEST];
    *flagReplyContactRequest = false;
    bool *flagContactRequestUpdatedPrimary = &contactRequestUpdated[primaryAccountIndex];
    *flagContactRequestUpdatedPrimary = false;
    megaApi[secondaryAccountIndex]->replyContactRequest(contactRequest[secondaryAccountIndex], MegaContactRequest::REPLY_ACTION_ACCEPT, this);
    assert(waitForResponse(flagReplyContactRequest));
    assert(!lastError[secondaryAccountIndex]);
    assert(waitForResponse(flagContactRequestUpdatedPrimary));

    delete contactRequest[secondaryAccountIndex];
    contactRequest[secondaryAccountIndex] = NULL;
}

MegaChatHandle MegaChatApiTest::createGroupChatRoom(unsigned int primaryAccountIndex, unsigned int secondaryAccountIndex, MegaChatPeerList *peers)
{
    bool *flagCreateChatRoom = &requestFlagsChat[primaryAccountIndex][MegaChatRequest::TYPE_CREATE_CHATROOM]; *flagCreateChatRoom = false;
    bool *chatItemPrimaryReceived = &chatItemUpdated[primaryAccountIndex]; *chatItemPrimaryReceived = false;
    bool *chatItemSecondaryReceived = &chatItemUpdated[secondaryAccountIndex]; *chatItemSecondaryReceived = false;
    chatListItem[primaryAccountIndex] = NULL;
    chatListItem[secondaryAccountIndex] = NULL;
    this->chatid[primaryAccountIndex] = MEGACHAT_INVALID_HANDLE;

    megaChatApi[primaryAccountIndex]->createChat(true, peers, this);
    assert(waitForResponse(flagCreateChatRoom));
    assert(!lastErrorChat[primaryAccountIndex]);
    MegaChatHandle chatid = this->chatid[primaryAccountIndex];
    assert (chatid != MEGACHAT_INVALID_HANDLE);
    assert(waitForResponse(chatItemPrimaryReceived));

    MegaChatListItem *chatItemPrimaryCreated = chatListItem[primaryAccountIndex];   chatListItem[primaryAccountIndex] = NULL;
    assert(chatItemPrimaryCreated);
    delete chatItemPrimaryCreated;    chatItemPrimaryCreated = NULL;

    assert(waitForResponse(chatItemSecondaryReceived));
    MegaChatListItem *chatItemSecondaryCreated = chatListItem[secondaryAccountIndex];   chatListItem[secondaryAccountIndex] = NULL;

    // FIXME: find a safe way to control when the auxiliar account receives the
    // new chatroom, since we may have multiple notifications for other chats
    while (!chatItemSecondaryCreated)
    {
        assert(waitForResponse(chatItemSecondaryReceived));
        assert(chatItemSecondaryCreated);
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

    MegaChatRoom *chatroom = megaChatApi[secondaryAccountIndex]->getChatRoom(chatid);
    assert (chatroom);
    delete chatroom;    chatroom = NULL;

    bool *flagAttributeUser = &requestFlags[primaryAccountIndex][MegaRequest::TYPE_GET_ATTR_USER]; *flagAttributeUser = false;
    bool *nameReceivedFlag = &nameReceived[primaryAccountIndex]; *nameReceivedFlag = false; mFirstname = "";
    megaApi[primaryAccountIndex]->getUserAttribute(MegaApi::USER_ATTR_FIRSTNAME);
    assert(waitForResponse(flagAttributeUser));
    assert(!lastError[primaryAccountIndex]);
    assert(waitForResponse(nameReceivedFlag));
    string peerFirstname = mFirstname;
    flagAttributeUser = &requestFlags[primaryAccountIndex][MegaRequest::TYPE_GET_ATTR_USER]; *flagAttributeUser = false;
    nameReceivedFlag = &nameReceived[primaryAccountIndex]; *nameReceivedFlag = false; mLastname = "";
    megaApi[primaryAccountIndex]->getUserAttribute(MegaApi::USER_ATTR_LASTNAME);
    assert(waitForResponse(flagAttributeUser));
    assert(!lastError[primaryAccountIndex]);
    assert(waitForResponse(nameReceivedFlag));
    string peerLastname = mLastname;
    string peerFullname = peerFirstname + " " + peerLastname;

    assert(!strcmp(chatItemSecondaryCreated->getTitle(), peerFullname.c_str())); // ERROR: we get empty title
    delete chatItemPrimaryCreated;    chatItemPrimaryCreated = NULL;
    delete chatItemSecondaryCreated;    chatItemSecondaryCreated = NULL;

    return chatid;
}

MegaChatHandle MegaChatApiTest::getPeerToPeerChatRoom(unsigned int primaryAccountIndex, unsigned int secondaryAccountIndex)
{
    MegaUser *peerPrimary = megaApi[primaryAccountIndex]->getContact(mAccounts[secondaryAccountIndex].getEmail().c_str());
    MegaUser *peerSecondary = megaApi[secondaryAccountIndex]->getContact(mAccounts[primaryAccountIndex].getEmail().c_str());
    assert(peerPrimary && peerSecondary);

    MegaChatRoom *chatroom0 = megaChatApi[primaryAccountIndex]->getChatRoomByUser(peerPrimary->getHandle());
    if (!chatroom0) // chat 1on1 doesn't exist yet --> create it
    {
        MegaUser *peer = megaApi[primaryAccountIndex]->getContact(mAccounts[secondaryAccountIndex].getEmail().c_str());
        assert(peer);

        MegaChatPeerList *peers = MegaChatPeerList::createInstance();
        peers->addPeer(peer->getHandle(), MegaChatPeerList::PRIV_STANDARD);

        bool *flag = &requestFlagsChat[primaryAccountIndex][MegaChatRequest::TYPE_CREATE_CHATROOM]; *flag = false;
        bool *chatCreated = &chatItemUpdated[primaryAccountIndex]; *chatCreated = false;
        bool *chatReceived = &chatItemUpdated[secondaryAccountIndex]; *chatReceived = false;
        megaChatApi[primaryAccountIndex]->createChat(false, peers, this);
        assert(!lastErrorChat[primaryAccountIndex]);
        assert(waitForResponse(chatCreated));
        assert(waitForResponse(chatReceived));

        chatroom0 = megaChatApi[primaryAccountIndex]->getChatRoomByUser(peer->getHandle());
    }

    MegaChatHandle chatid0 = chatroom0->getChatId();
    assert (chatid0 != MEGACHAT_INVALID_HANDLE);
    delete chatroom0;
    chatroom0 = NULL;

    MegaChatRoom *chatroom1 = megaChatApi[secondaryAccountIndex]->getChatRoomByUser(peerSecondary->getHandle());
    MegaChatHandle chatid1 = chatroom1->getChatId();
    delete chatroom1;
    chatroom1 = NULL;
    assert (chatid0 == chatid1);

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

    assert(messageSendEdit);
    assert(waitForResponse(flagConfirmed));    // for confirmation, sendMessage() is synchronous
    MegaChatHandle msgPrimaryId = chatroomListener->msgId[senderAccountIndex];
    assert(msgPrimaryId != MEGACHAT_INVALID_HANDLE);

    assert(waitForResponse(flagReceived));    // for reception
    MegaChatHandle msgSecondaryId = chatroomListener->msgId[senderAccountIndex];
    assert(msgPrimaryId == msgSecondaryId);
    MegaChatMessage *messageReceived = megaChatApi[receiverAccountIndex]->getMessage(chatid, msgSecondaryId);   // message should be already received, so in RAM
    assert(messageReceived && !strcmp(textToSend.c_str(), messageReceived->getContent()));
    assert(waitForResponse(flagDelivered));    // for delivery

    // Update Message
    if (messageId != MEGACHAT_INVALID_HANDLE)
    {
        assert(messageReceived->isEdited());
    }

    delete messageReceived;
    messageReceived = NULL;

    return messageSendEdit;
}

void MegaChatApiTest::checkEmail(unsigned int indexAccount)
{
    char *myEmail = megaChatApi[indexAccount]->getMyEmail();
    assert(myEmail);
    assert(string(myEmail) == mAccounts[indexAccount].getEmail());
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

MegaHandle MegaChatApiTest::createAndSendFile(unsigned int primaryAccountIndex, unsigned int secondaryAccountIndex, MegaChatHandle chatid,
                                        std::string fileName, std::string contain, TestChatRoomListener* chatroomListener)
{
    std::string fileDestination = uploadFile(primaryAccountIndex, fileName, "/tmp/", contain, "/");

    MegaNode* node0 = megaApi[primaryAccountIndex]->getNodeByPath(fileDestination.c_str());
    assert(node0 != NULL);

    MegaNodeList *megaNodeList = MegaNodeList::createInstance();
    megaNodeList->addNode(node0);

    bool *flagConfirmed = &chatroomListener->msgConfirmed[primaryAccountIndex]; *flagConfirmed = false;
    bool *flagReceived = &chatroomListener->msgReceived[secondaryAccountIndex]; *flagReceived = false;
    bool *flagDelivered = &chatroomListener->msgDelivered[primaryAccountIndex]; *flagDelivered = false;

    megaChatApi[primaryAccountIndex]->attachNodes(chatid, megaNodeList, this);
    delete megaNodeList;
    megaNodeList = NULL;

    assert(waitForResponse(flagConfirmed));
    assert(waitForResponse(flagConfirmed));    // for confirmation, sendMessage() is synchronous
    MegaChatHandle msgId0 = chatroomListener->msgId[primaryAccountIndex];
    assert (msgId0 != MEGACHAT_INVALID_HANDLE);

    assert(waitForResponse(flagReceived));    // for reception
    MegaChatHandle msgId1 = chatroomListener->msgId[secondaryAccountIndex];
    assert (msgId0 == msgId1);
    MegaChatMessage *msgReceived = megaChatApi[secondaryAccountIndex]->getMessage(chatid, msgId0);   // message should be already received, so in RAM
    assert(msgReceived);

    MegaHandle nodeSentHandle = node0->getHandle();

    delete node0;
    return nodeSentHandle;
}

void MegaChatApiTest::clearHistory(unsigned int primaryAccountIndex, unsigned int secondaryAccountIndex, MegaChatHandle chatid, TestChatRoomListener *chatroomListener)
{
    bool *flagTruncateHistory = &requestFlagsChat[primaryAccountIndex][MegaChatRequest::TYPE_TRUNCATE_HISTORY]; *flagTruncateHistory = false;
    bool *flagTruncatedPrimary = &chatroomListener->historyTruncated[primaryAccountIndex]; *flagTruncatedPrimary = false;
    bool *flagTruncatedSecondary = &chatroomListener->historyTruncated[secondaryAccountIndex]; *flagTruncatedSecondary = false;
    bool *chatItemUpdated0 = &chatItemUpdated[primaryAccountIndex]; *chatItemUpdated0 = false;
    bool *chatItemUpdated1 = &chatItemUpdated[secondaryAccountIndex]; *chatItemUpdated1 = false;
    megaChatApi[primaryAccountIndex]->clearChatHistory(chatid);
    assert(waitForResponse(flagTruncateHistory));
    assert(!lastErrorChat[primaryAccountIndex]);
    assert(waitForResponse(flagTruncatedPrimary));
    assert(waitForResponse(flagTruncatedSecondary));
    assert(waitForResponse(chatItemUpdated0));
    assert(waitForResponse(chatItemUpdated1));

    MegaChatListItem *itemPrimary = megaChatApi[primaryAccountIndex]->getChatListItem(chatid);
    assert(itemPrimary->getUnreadCount() == 0);
    assert(!strcmp(itemPrimary->getLastMessage(), ""));
    assert(itemPrimary->getLastMessageType() == 0);
    assert(itemPrimary->getLastTimestamp() != 0);
    delete itemPrimary; itemPrimary = NULL;
    MegaChatListItem *itemSecondary = megaChatApi[secondaryAccountIndex]->getChatListItem(chatid);
    assert(itemSecondary->getUnreadCount() == 1);
    assert(!strcmp(itemSecondary->getLastMessage(), ""));
    assert(itemSecondary->getLastMessageType() == 0);
    assert(itemSecondary->getLastTimestamp() != 0);
    delete itemSecondary; itemSecondary = NULL;
}

void MegaChatApiTest::leaveChat(unsigned int accountIndex, MegaChatHandle chatid)
{
    bool *flagRemoveFromchatRoom = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_REMOVE_FROM_CHATROOM]; *flagRemoveFromchatRoom = false;
    bool *chatClosed = &chatItemClosed[accountIndex]; *chatClosed = false;
    megaChatApi[accountIndex]->leaveChat(chatid);
    assert(waitForResponse(flagRemoveFromchatRoom));
    assert(!lastErrorChat[accountIndex]);
    assert(waitForResponse(chatClosed));
    MegaChatRoom *chatroom = megaChatApi[accountIndex]->getChatRoom(chatid);
    assert(!chatroom->isActive());
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
        assert(false);
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
        assert(false);
    }

    return apiIndex;
}

std::string MegaChatApiTest::uploadFile(int accountIndex, const std::string& fileName, const std::string& originPath, const std::string& contain, const std::string& destinationPath)
{
    std::string filePath = originPath + fileName;
    FILE* fileDescriptor = fopen(filePath.c_str(), "w");
    fprintf(fileDescriptor, "%s", contain.c_str());
    fclose(fileDescriptor);

    addDownload();
    megaApi[accountIndex]->startUpload(filePath.c_str(), megaApi[accountIndex]->getNodeByPath(destinationPath.c_str()), this);
    assert(waitForResponse(&isNotDownloadRunning()));
    assert(!lastError[accountIndex]);

    return destinationPath + fileName;
}

void MegaChatApiTest::addDownload()
{
    ++mActiveDownload;
    ++mTotalDownload;
    mNotDownloadRunning = false;
}

bool &MegaChatApiTest::isNotDownloadRunning()
{
    return mNotDownloadRunning;
}

int MegaChatApiTest::getTotalDownload() const
{
    return mTotalDownload;
}

void MegaChatApiTest::getContactRequest(unsigned int accountIndex, bool outgoing, int expectedSize)
{
    MegaContactRequestList *crl;

    if (outgoing)
    {
        crl = megaApi[accountIndex]->getOutgoingContactRequests();
        assert(expectedSize == crl->size());
        if (expectedSize)
        {
            contactRequest[accountIndex] = crl->get(0)->copy();
        }
    }
    else
    {
        crl = megaApi[accountIndex]->getIncomingContactRequests();
        assert(expectedSize == crl->size());
        if (expectedSize)
        {
            contactRequest[accountIndex] = crl->get(0)->copy();
        }
    }

    delete crl;
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

void MegaChatApiTest::onContactRequestsUpdate(MegaApi *api, MegaContactRequestList *requests)
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
                chatNameReceived[apiIndex] = true;
                break;

            case MegaChatRequest::TYPE_GET_LASTNAME:
                mChatLastname = request->getText() ? request->getText() : "";
                chatNameReceived[apiIndex] = true;
                break;

            case MegaChatRequest::TYPE_GET_EMAIL:
                mChatEmail = request->getText() ? request->getText() : "";
                chatNameReceived[apiIndex] = true;
                break;

            case MegaChatRequest::TYPE_ATTACH_NODE_MESSAGE:
                attachNodeSend[apiIndex] = true;
                break;

            case MegaChatRequest::TYPE_REVOKE_NODE_MESSAGE:
                revokeNodeSend[apiIndex] = true;
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

    --mActiveDownload;
    if (mActiveDownload == 0)
    {
        mNotDownloadRunning = true;
    }

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
        assert(false);
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

