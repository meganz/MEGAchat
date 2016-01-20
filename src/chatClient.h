#ifndef CHATCLIENT_H
#define CHATCLIENT_H
#include "contactList.h"
#include "ITypes.h" //for IPtr
#include "karereEventObjects.h"
#include "rtcModule/IRtcModule.h"
#include <memory>
#include <map>
#include <type_traits>
#include <retryHandler.h>
#include <busConstants.h>
#include <serverListProviderForwards.h>
#include "sdkApi.h"
#include "chatd.h"

namespace strophe { class Connection; }
//namespace chatd { class Client; class UserPrivMap; class Listener; }

namespace rtcModule
{
    class IRtcModule;
    class IEventHandler;
}
namespace mega { namespace rh { class IRetryController; } }
struct sqlite3;
class Buffer;

namespace karere
{
class TextModule;
class ChatRoom;
class GroupChatRoom;
class Contact;
class ContactList;
class IGui
{
public:
    class ITitleDisplay
    {
    public:
        virtual void updateTitle(const std::string& title) {}
        virtual void updateOverlayCount(int count) {}
        virtual void updateOnlineIndication(Presence state) {}
    };
    class ICallGui{};
    class IChatWindow: public chatd::Listener, public ITitleDisplay
    {
    public:
        virtual ICallGui* getCallGui() = 0;
        virtual void show() = 0;
        virtual void hide() = 0;
    };
    class ILoginDialog
    {
    public:
        enum LoginStage { kAuthenticating, kBadCredentials, kLoggingIn, kFetchingNodes, kLoginComplete, kLast=kLoginComplete};
        virtual promise::Promise<std::pair<std::string, std::string>> requestCredentials() = 0;
        virtual void setState(LoginStage state) {}
        virtual ~ILoginDialog() {}
    };
    virtual ILoginDialog* createLoginDialog() = 0;

    virtual IChatWindow* createChatWindow(ChatRoom& room) = 0;
    class IContactList
    {
    public:
        virtual ITitleDisplay* createContactItem(Contact& contact) = 0;
        virtual ITitleDisplay* createGroupChatItem(GroupChatRoom& room) = 0;
        virtual void removeContactItem(ITitleDisplay* item) = 0;
        virtual void removeGroupChatItem(ITitleDisplay* item) = 0;
        virtual IChatWindow& chatWindowForPeer(uint64_t handle) = 0;
    };
    virtual IContactList& contactList() = 0;
    virtual rtcModule::IEventHandler*
        createCallAnswerGui(const std::shared_ptr<rtcModule::ICallAnswer>& ans) = 0;
    virtual void notifyInvited(const ChatRoom& room) {}
    virtual void show() = 0;
    virtual bool visible() const = 0;
    virtual void onTerminate() {}
    virtual ~IGui() {}
};

enum { USER_ATTR_RSA_PUBKEY = 128}; //virtual user attribute type, to be used with the common attr cache table

struct UserAttrDesc
{
    Buffer*(*getData)(const mega::MegaRequest&);
    int changeMask;
};

extern UserAttrDesc attrDesc[];

struct UserAttrPair
{
    uint64_t user;
    unsigned attrType;
    bool operator<(const UserAttrPair& other) const
    {
        if (user == other.user)
            return attrType < other.attrType;
        else
            return user < other.user;
    }
    UserAttrPair(uint64_t aUser, unsigned aType): user(aUser), attrType(aType)
    {
        if (attrType > mega::MegaApi::USER_ATTR_LAST_INTERACTION)
            throw std::runtime_error("UserAttrPair: Invalid user attribute id specified");
    }
};
typedef void(*UserAttrReqCbFunc)(Buffer*, void*);
struct UserAttrReqCb
{
    UserAttrReqCbFunc cb;
    void* userp;
    UserAttrReqCb(UserAttrReqCbFunc aCb, void* aUserp): cb(aCb), userp(aUserp){}
};

enum { kCacheFetchNotPending=0, kCacheFetchUpdatePending=1, kCacheFetchNewPending=2};
struct UserAttrCacheItem
{
    Buffer* data;
    std::list<UserAttrReqCb> cbs;
    unsigned char pending;
    UserAttrCacheItem(Buffer* buf, bool aPending): data(buf), pending(aPending){}
    ~UserAttrCacheItem();
    void notify();
};

class UserAttrCache: public std::map<UserAttrPair, std::shared_ptr<UserAttrCacheItem>>, public mega::MegaGlobalListener
{
protected:
    struct CbRefItem
    {
        iterator itemit;
        std::list<UserAttrReqCb>::iterator cbit;
        CbRefItem(iterator aItemIt, std::list<UserAttrReqCb>::iterator aCbIt)
            :itemit(aItemIt), cbit(aCbIt){}
    };
    Client& mClient;
    uint64_t mCbId = 0;
    std::map<uint64_t, CbRefItem> mCallbacks;
    void dbWrite(const UserAttrPair& key, const Buffer& data);
    void dbInvalidateItem(const UserAttrPair& item);
    uint64_t addCb(iterator itemit, UserAttrReqCbFunc cb, void* userp);
    void fetchAttr(const UserAttrPair& key, std::shared_ptr<UserAttrCacheItem>& item);
    void onUserAttrChange(mega::MegaUserList& users);
    void onLogin();
    friend class Client;
public:
    UserAttrCache(Client& aClient);
    ~UserAttrCache();
    uint64_t getAttr(const uint64_t& user, unsigned attrType, void* userp,
                             UserAttrReqCbFunc cb);
    promise::Promise<Buffer*> getAttr(const uint64_t &user, unsigned attrType);
    bool removeCb(const uint64_t &cbid);
};
class ChatRoomList;
class ChatRoom: public chatd::Listener
{
public:
    ChatRoomList& parent;
protected:
    IGui::IChatWindow* mChatWindow = nullptr;
    uint64_t mChatid;
    std::string mUrl;
    unsigned char mShardNo;
    bool mIsGroup;
    char mOwnPriv;
    chatd::Messages* mMessages = nullptr;
    void syncRoomPropertiesWithApi(const mega::MegaTextChat& chat);
    void switchListenerToChatWindow();
    void join(); //We can't do the join in the ctor, as chatd may fire callbcks synchronously from join(), and the derived class will not be constructed at that point.
public:
    virtual void syncWithApi(const mega::MegaTextChat& chat) = 0;
    virtual IGui::ITitleDisplay& titleDisplay() = 0;
    virtual const std::string& titleString() const = 0;
    virtual Presence presence() const = 0;
    ChatRoom(ChatRoomList& parent, const uint64_t& chatid, bool isGroup, const std::string& url,
             unsigned char shard, char ownPriv);
    virtual ~ChatRoom(){}
    const uint64_t& chatid() const { return mChatid; }
    bool isGroup() const { return mIsGroup; }
    const std::string& url() const { return mUrl; }
    unsigned char shardNo() const { return mShardNo; }
    char ownPriv() const { return mOwnPriv; }
    chatd::ChatState chatdOnlineState() const { return mMessages->onlineState(); }
    IGui::IChatWindow& chatWindow(); /// < creates the windows if not already created
    bool hasChatWindow() const { return mChatWindow != nullptr; }
    //chatd::Listener implementation
    void init(chatd::Messages *messages, chatd::DbInterface *&dbIntf);
    void onRecvNewMessage(chatd::Idx, chatd::Message&, chatd::Message::Status);
    void onMessageStatusChange(chatd::Idx idx, chatd::Message::Status newStatus, const chatd::Message &msg);
};
class PeerChatRoom: public ChatRoom
{
protected:
    uint64_t mPeer;
    char mPeerPriv;
    Contact* mContact = nullptr;
    void setContact(Contact& contact) { mContact = &contact; }
    friend class ContactList;
public:
    PeerChatRoom(ChatRoomList& parent, const uint64_t& chatid, const std::string& url,
            unsigned char shard, char ownPriv, const uint64_t& peer, char peerPriv);
    PeerChatRoom(ChatRoomList& parent, const mega::MegaTextChat& room);
    const uint64_t peer() const { return mPeer; }
    const Contact& contact() const { return *mContact; }
    void syncOwnPriv(char priv);
    void syncPeerPriv(char priv);
    virtual void syncWithApi(const mega::MegaTextChat& chat);
    virtual IGui::ITitleDisplay& titleDisplay();
    virtual const std::string& titleString() const;
    virtual Presence presence() const;
//chatd::Listener interface
    virtual void onUserJoined(const chatd::Id& userid, char priv);
    virtual void onUserLeft(const chatd::Id& userid);
    virtual void onOnlineStateChange(chatd::ChatState state);
};

class GroupChatRoom: public ChatRoom
{
protected:
    class Member
    {
        GroupChatRoom& mRoom;
        std::string mName;
        char mPriv;
        uint64_t mNameAttrCbHandle;
    public:
        Member(GroupChatRoom& aRoom, const uint64_t& user, char aPriv);
        ~Member();
        friend class GroupChatRoom;
    };
    std::map<uint64_t, Member*> mPeers;
    IGui::ITitleDisplay* mTitleDisplay = nullptr;
    std::string mTitleString;
    bool mHasUserTitle = false;
    void syncRoomPropertiesWithApi(const mega::MegaTextChat &chat);
    void syncMembers(const chatd::UserPrivMap& users);
    static chatd::UserPrivMap& apiMembersToMap(const mega::MegaTextChat& chat, chatd::UserPrivMap& membs);
    void loadUserTitle();
    void updateAllOnlineDisplays(Presence pres);
    friend class Member;
public:
    GroupChatRoom(ChatRoomList& parent, const mega::MegaTextChat& chat, const std::string& userTitle);
    GroupChatRoom(ChatRoomList& parent, const uint64_t& chatid, const std::string& aUrl,
                  unsigned char aShard, char aOwnPriv, const std::string& title);
    ~GroupChatRoom();
    void addMember(const uint64_t& userid, char priv, bool saveToDb);
    bool removeMember(const uint64_t& userid);
    void setUserTitle(const std::string& title);
    void deleteSelf(); //<Deletes the room from db and then immediately destroys itself (i.e. delete this)
    promise::Promise<void> leave();
    promise::Promise<void> invite(uint64_t userid, char priv);
    virtual void syncWithApi(const mega::MegaTextChat &chat);
    virtual IGui::ITitleDisplay& titleDisplay() { return *mTitleDisplay; }
    virtual const std::string& titleString() const { return mTitleString; }
    virtual Presence presence() const
    {
        return (mMessages->onlineState() == chatd::kChatStateOnline)? Presence::kOnline:Presence::kOffline;
    }
    void updateTitle()
    {
        if (mHasUserTitle)
            return;
        mTitleString.clear();
        for (auto& m: mPeers)
        {
            auto& name = m.second->mName;
            if (name.size() <= 1)
                mTitleString.append("...,");
            else
                mTitleString.append(name.c_str()+1, name.size()-1).append(", ");
        }
        if (!mTitleString.empty())
            mTitleString.resize(mTitleString.size()-2); //truncate last ", "

        if (mTitleDisplay) //doesn't exist during construction
            mTitleDisplay->updateTitle(mTitleString);
        if(mChatWindow)
            mChatWindow->updateTitle(mTitleString);
    }
//chatd::Listener
    void onUserJoined(const chatd::Id& userid, char priv);
    void onUserLeft(const chatd::Id& userid);
    void onOnlineStateChange(chatd::ChatState);

};
class ChatRoomList: public std::map<uint64_t, ChatRoom*> //don't use shared_ptr here as we want to be able to immediately delete a chatroom once the API tells us it's deleted
{
protected:
    void loadFromDb();
public:
    Client& client;
    void syncRoomsWithApi(const mega::MegaTextChatList& rooms);
    ChatRoom& addRoom(const mega::MegaTextChat &room, const std::string& groupRoomTitle="");
    bool removeRoom(const uint64_t& chatid);
    ChatRoomList(Client& aClient);
    ~ChatRoomList();
    void onChatsUpdate(const std::shared_ptr<mega::MegaTextChatList>& chats);
};

class Contact: public IPresenceListener
{
protected:
    ContactList& mClist;
    uint64_t mUserid;
    PeerChatRoom* mChatRoom;
    uint64_t mUsernameAttrCbId;
    std::string mEmail;
    std::string mTitleString;
    IGui::ITitleDisplay* mDisplay; //must be after mTitleString because it will read it
    std::shared_ptr<XmppContact> mXmppContact; //after constructor returns, we are guaranteed to have this set to a vaild instance
    void updateTitle(const std::string& str);
    void setChatRoom(PeerChatRoom& room);
public:
    Contact(ContactList& clist, const uint64_t& userid, const std::string& email,
            PeerChatRoom* room = nullptr);
    ~Contact();
    ContactList& contactList() { return mClist; }
    XmppContact& xmppContact() { return *mXmppContact; }
    PeerChatRoom* chatRoom() { return mChatRoom; }
    promise::Promise<ChatRoom *> createChatRoom();
    const std::string& titleString() const { return mTitleString; }
    IGui::ITitleDisplay& titleDisplay() { return *mDisplay; }
    uint64_t userId() const { return mUserid; }
    virtual void onPresence(Presence pres)
    {
        mDisplay->updateOnlineIndication(pres);
    }
    friend class ContactList;
};

class ContactList: public std::map<uint64_t, Contact*>
{
protected:
    void removeUser(iterator it);
public:
    Client& client;
    ContactList(Client& aClient);
    ~ContactList();
    bool addUserFromApi(mega::MegaUser& user);
    void removeUser(const uint64_t& userid);
    void syncWithApi(mega::MegaUserList& users);
    IGui::ITitleDisplay* attachRoomToContact(const uint64_t& userid, PeerChatRoom &room);
    Contact* contactFromJid(const std::string& jid) const;
    void onContactOnlineState(const std::string& jid);
};

class Client: public rtcModule::IGlobalEventHandler, mega::MegaGlobalListener
{
protected:
    std::string mAppDir;
public:
    sqlite3* db = nullptr;
    std::shared_ptr<strophe::Connection> conn;
    std::unique_ptr<chatd::Client> chatd;
    std::unique_ptr<MyMegaApi> api;
    //we use IPtr smart pointers instead of std::unique_ptr because we want to delete not via the
    //destructor, but via a destroy() method. This is to support cross-DLL loading of plugins,
    //where operator delete would try to deallocate memory via the memory manager/runtime of the caller,
    //which is often not the one that allocated that memory (usually the DLL allocates the object).
    //Calling a function defined in the DLL that in turn calls the destructor ensures that operator
    //delete is called from code inside the DLL, i.e. in the runtime where the class is implemented,
    //operates and was allocated
    rtcModule::IRtcModule* rtc = nullptr;
    TextModule* mTextModule = nullptr;
//    bool mHadSid = false;
    bool isTerminating = false;
    unsigned mReconnectConnStateHandler = 0;
    std::function<void()> onChatdReady;
    UserAttrCache userAttrCache;
    IGui& gui;
    std::unique_ptr<ContactList> contactList;
    std::unique_ptr<ChatRoomList> chats;
    bool isLoggedIn() const { return mIsLoggedIn; }
    const chatd::Id myHandle() const { return mMyHandle; }
    const std::string& myName() const { return mMyName; }
    static uint64_t useridFromJid(const std::string& jid);
    std::string getUsername() const
    {
        return strophe::getNodeFromJid(conn->fullOrBareJid());
    }
    std::string getResource() const /// < Get resource of current connection.
    {
        return strophe::getResourceFromJid(conn->fullJid());
    }
    /**
    * @Get a unique nickname based on current connection.
    * @returns {string} nickname based on current connection.
    */
    std::string getNickname() const { return getUsername() + "__" + getResource(); }

    /**
     * @brief Initialize the contact list.
     *
     * This performs a request to xmpp roster server and fetch the contact list.
     * Contact list also registers a contact presence handler to update the list itself based on received presence messages.
     */
    Client(IGui& gui, const char *homedir=nullptr);
    virtual ~Client();
    void registerRtcHandler(rtcModule::IEventHandler* rtcHandler);
    promise::Promise<int> init();
    bool loginDialogDisplayed() const { return mLoginDlg.operator bool(); }
    /** @brief Notifies the client that internet connection is again available */
    void notifyNetworkOffline();
    /** @brief Notifies the client that network connection is down */
    void notifyNetworkOnline();
    void startKeepalivePings();
    promise::Promise<void> terminate();
    /**
     * @brief Ping a target peer to check whether he/she is alive
     * @param [peerJid] {const char*} peer's Jid. If NULL, then no 'to'
     * attribute will be included in the stanza, effectively sending the ping to the server
     * @param [intervalSec] {int} optional with default value as 100, interval in seconds to do ping.
     *
     * This performs a xmpp ping request to xmpp server and check whether the target user is alive or not.
     */
    strophe::StanzaPromise pingPeer(const char* peerJid);
    /**
    * @brief set user's chat presence.
    * set user's presence state, which can be one of online, busy, away, online
    */
    void setPresence(const Presence pres, const int delay = 0);
    XmppContactList& xmppContactList()
    {
        return mXmppContactList;
    }
protected:
    chatd::Id mMyHandle = (uint64_t)-1;
    std::string mMyName;
    std::unique_ptr<IGui::ILoginDialog> mLoginDlg;
    bool mIsLoggedIn = false;
    /** our own email address */
    std::string mEmail;
    /** our password */
    std::string mPassword;
    /** client's contact list */
    XmppContactList mXmppContactList;
    typedef FallbackServerProvider<HostPortServerInfo> XmppServerProvider;
    std::unique_ptr<XmppServerProvider> mXmppServerProvider;
    std::unique_ptr<mega::rh::IRetryController> mReconnectController;
    xmpp_ts mLastPingTs = 0;
    std::string checkAppDir(const char* dir);
    sqlite3* openDb();
    void setupReconnectHandler();
    promise::Promise<message_bus::SharedMessage<M_MESS_PARAMS>>
        getOtherUserInfo(std::string &emailAddress);
    promise::Promise<message_bus::SharedMessage<M_MESS_PARAMS>>
        getThisUserInfo();
    void setupHandlers();
    promise::Promise<int> initializeContactList();
    /**
     * @brief send response to ping request.
     *
     * This performs an xmpp response to the received xmpp ping request.
     */
    void sendPong(const std::string& peerJid, const std::string& messageId);
    //rtcModule::IGlobalEventHandler interface
    virtual rtcModule::IEventHandler* onIncomingCallRequest(
            const std::shared_ptr<rtcModule::ICallAnswer> &call);
    virtual void discoAddFeature(const char *feature);
    //mega::MegaGlobalListener interface, called by worker thread
    virtual void onChatsUpdate(mega::MegaApi*, mega::MegaTextChatList* rooms);
    virtual void onUsersUpdate(mega::MegaApi*, mega::MegaUserList* users);
};
}
#endif // CHATCLIENT_H
