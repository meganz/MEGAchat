#ifndef CHATCLIENT_H
#define CHATCLIENT_H
#include "contactList.h"
#include "karereEventObjects.h"
#include "rtcModule/IRtcModule.h"
#include <memory>
#include <map>
#include <type_traits>
#include <retryHandler.h>
#include <serverListProviderForwards.h>
#include "sdkApi.h"
#include "userAttrCache.h"
#include "chatd.h"
#include "IGui.h"

namespace strophe { class Connection; }

namespace mega { namespace rh { class IRetryController; } }
namespace strongvelope { class ProtocolHandler; }

struct sqlite3;
class Buffer;

namespace karere
{
/** @brief
 * The application implementor must define this function to create the application
 * directory in case it does not exist, and return the path to it.
 * The reason this function is provided at link time (rather than programmatically
 * setting it during run time), is that it is needed by the logger, and the logger
 * is initialized before main() is entered.
 */

    KR_WEAKSYM(std::string getAppDir());
/** @brief
 * Builtin implementation of getAppDir() suoitable for desktop systems
 * It reads the env variable KRDIR for a path to the app dir, and if not present,
 * defaults to '~/.karere'.
 */
KARERE_IMPEXP const std::string& createAppDir(const char* dirname=".karere", const char* envVarName="KRDIR");

class TextModule;
class ChatRoom;
class GroupChatRoom;
class Contact;
class ContactList;

typedef std::map<Id, chatd::Priv> UserPrivMap;
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
    chatd::Priv mOwnPriv;
    chatd::Chat* mChat = nullptr;
    bool syncRoomPropertiesWithApi(const mega::MegaTextChat& chat);
    void switchListenerToChatWindow();
    void chatdJoin(const karere::SetOfIds& initialUsers); //We can't do the join in the ctor, as chatd may fire callbcks synchronously from join(), and the derived class will not be constructed at that point.
public:
    virtual bool syncWithApi(const mega::MegaTextChat& chat) = 0;
    virtual IGui::IContactGui& contactGui() = 0;
    virtual const std::string& titleString() const = 0;
    virtual Presence presence() const = 0;
    ChatRoom(ChatRoomList& parent, const uint64_t& chatid, bool isGroup, const std::string& url,
             unsigned char shard, chatd::Priv ownPriv);
    virtual ~ChatRoom(){}
    const uint64_t& chatid() const { return mChatid; }
    bool isGroup() const { return mIsGroup; }
    const std::string& url() const { return mUrl; }
    unsigned char shardNo() const { return mShardNo; }
    chatd::Priv ownPriv() const { return mOwnPriv; }
    chatd::ChatState chatdOnlineState() const { return mChat->onlineState(); }
    IGui::IChatWindow& chatWindow(); /// < creates the windows if not already created
    bool hasChatWindow() const { return mChatWindow != nullptr; }
    //chatd::Listener implementation
    virtual void init(chatd::Chat& messages, chatd::DbInterface *&dbIntf);
    virtual void onRecvNewMessage(chatd::Idx, chatd::Message&, chatd::Message::Status);
    virtual void onMessageStatusChange(chatd::Idx idx, chatd::Message::Status newStatus, const chatd::Message &msg);
//    virtual void onHistoryTruncated();
};
class PeerChatRoom: public ChatRoom
{
protected:
    uint64_t mPeer;
    chatd::Priv mPeerPriv;
    Contact* mContact = nullptr;
    void setContact(Contact& contact) { mContact = &contact; }
    friend class ContactList;
    inline Presence calculatePresence(Presence pres) const;
public:
    PeerChatRoom(ChatRoomList& parent, const uint64_t& chatid, const std::string& url,
            unsigned char shard, chatd::Priv ownPriv, const uint64_t& peer, chatd::Priv peerPriv);
    PeerChatRoom(ChatRoomList& parent, const mega::MegaTextChat& room);
    const uint64_t peer() const { return mPeer; }
    const Contact& contact() const { return *mContact; }
    bool syncOwnPriv(chatd::Priv priv);
    bool syncPeerPriv(chatd::Priv priv);
    virtual bool syncWithApi(const mega::MegaTextChat& chat);
    virtual IGui::IContactGui& contactGui();
    virtual const std::string& titleString() const;
    void updatePresence();
    virtual Presence presence() const;
    void join();
//chatd::Listener interface
    virtual void onUserJoin(Id userid, chatd::Priv priv);
    virtual void onUserLeave(Id userid);
    virtual void onOnlineStateChange(chatd::ChatState state);
    virtual void onUnreadChanged();
};

class GroupChatRoom: public ChatRoom
{
protected:
    class Member
    {
        GroupChatRoom& mRoom;
        std::string mName;
        chatd::Priv mPriv;
        uint64_t mNameAttrCbHandle;
    public:
        Member(GroupChatRoom& aRoom, const uint64_t& user, chatd::Priv aPriv);
        ~Member();
        const std::string& name() const { return mName; }
        chatd::Priv priv() const { return mPriv; }
        friend class GroupChatRoom;
    };
    typedef std::map<uint64_t, Member*> MemberMap;
    MemberMap mPeers;
    IGui::IContactGui* mContactGui = nullptr;
    std::string mTitleString;
    bool mHasUserTitle = false;
    void syncRoomPropertiesWithApi(const mega::MegaTextChat &chat);
    bool syncMembers(const UserPrivMap& users);
    static UserPrivMap& apiMembersToMap(const mega::MegaTextChat& chat, UserPrivMap& membs);
    void loadUserTitle();
    void updateAllOnlineDisplays(Presence pres);
    friend class Member;
public:
    GroupChatRoom(ChatRoomList& parent, const mega::MegaTextChat& chat, const std::string& userTitle);
    GroupChatRoom(ChatRoomList& parent, const uint64_t& chatid, const std::string& aUrl,
                  unsigned char aShard, chatd::Priv aOwnPriv, const std::string& title);
    ~GroupChatRoom();
    const MemberMap& peers() const { return mPeers; }
    void addMember(const uint64_t& userid, chatd::Priv priv, bool saveToDb);
    bool removeMember(const uint64_t& userid);
    void setUserTitle(const std::string& title);
    void deleteSelf(); //<Deletes the room from db and then immediately destroys itself (i.e. delete this)
    void leave();
    promise::Promise<void> invite(uint64_t userid, chatd::Priv priv);
    virtual bool syncWithApi(const mega::MegaTextChat &chat);
    virtual IGui::IContactGui& contactGui() { return *mContactGui; }
    virtual const std::string& titleString() const { return mTitleString; }
    virtual Presence presence() const
    {
        return (mChat->onlineState() == chatd::kChatStateOnline)? Presence::kOnline:Presence::kOffline;
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

        if (mContactGui) //doesn't exist during construction
            mContactGui->updateTitle(mTitleString);
        if(mChatWindow)
            mChatWindow->updateTitle(mTitleString);
    }
    void join();
//chatd::Listener
    void onUserJoin(Id userid, chatd::Priv priv);
    void onUserLeave(Id userid);
    void onOnlineStateChange(chatd::ChatState);

};
class ChatRoomList: public std::map<uint64_t, ChatRoom*> //don't use shared_ptr here as we want to be able to immediately delete a chatroom once the API tells us it's deleted
{
protected:
public:
    Client& client;
    void syncRoomsWithApi(const mega::MegaTextChatList& rooms);
    ChatRoom& addRoom(const mega::MegaTextChat &room, const std::string& groupRoomTitle="");
    bool removeRoom(const uint64_t& chatid);
    ChatRoomList(Client& aClient);
    ~ChatRoomList();
    void loadFromDb();
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
    int64_t mSince;
    std::string mTitleString;
    int mVisibility;
    IGui::IContactGui* mDisplay; //must be after mTitleString because it will read it
    std::shared_ptr<XmppContact> mXmppContact; //after constructor returns, we are guaranteed to have this set to a vaild instance
    void updateTitle(const std::string& str);
    void setChatRoom(PeerChatRoom& room);
public:
    Contact(ContactList& clist, const uint64_t& userid, const std::string& email,
            int visibility, int64_t since, PeerChatRoom* room = nullptr);
    ~Contact();
    ContactList& contactList() { return mClist; }
    XmppContact& xmppContact() { return *mXmppContact; }
    PeerChatRoom* chatRoom() { return mChatRoom; }
    promise::Promise<ChatRoom *> createChatRoom();
    const std::string& titleString() const { return mTitleString; }
    IGui::IContactGui& gui() { return *mDisplay; }
    uint64_t userId() const { return mUserid; }
    const std::string& email() const { return mEmail; }
    const std::string& jid() const { return mXmppContact->bareJid(); }
    int64_t since() const { return mSince; }
    virtual void onPresence(Presence pres)
    {
        if (mChatRoom && (mChatRoom->chatdOnlineState() != chatd::kChatStateOnline))
            pres = Presence::kOffline;
        updateAllOnlineDisplays(pres);
    }
    void onVisibilityChanged(int newVisibility)
    {
        mVisibility = newVisibility;
        mDisplay->onVisibilityChanged(newVisibility);
    }
    void updateAllOnlineDisplays(Presence pres)
    {
            mDisplay->updateOnlineIndication(pres);
            if (mChatRoom)
                mChatRoom->updatePresence();
    }
    int visibility() const { return mVisibility; }
    friend class ContactList;
};

class ContactList: public std::map<uint64_t, Contact*>
{
protected:
    void removeUser(iterator it);
    void removeUser(uint64_t userid);
public:
    Client& client;
    ContactList(Client& aClient);
    ~ContactList();
    bool addUserFromApi(mega::MegaUser& user);
    void onUserAddRemove(mega::MegaUser& user); //called for actionpackets
    promise::Promise<void> removeContactFromServer(uint64_t userid);
    void syncWithApi(mega::MegaUserList& users);
    IGui::IContactGui* attachRoomToContact(const uint64_t& userid, PeerChatRoom &room);
    Contact* contactFromJid(const std::string& jid) const;
    void onContactOnlineState(const std::string& jid);
    const std::string* getUserEmail(uint64_t userid) const;
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
//    TextModule* mTextModule = nullptr;
//    bool mHadSid = false;
    bool isTerminating = false;
    unsigned mReconnectConnStateHandler = 0;
    std::function<void()> onChatdReady;
    UserAttrCache userAttrCache;
    IGui& gui;
    std::unique_ptr<ContactList> contactList;
    std::unique_ptr<ChatRoomList> chats;
    char mMyPrivCu25519[32] = {0};
    char mMyPrivEd25519[32] = {0};
    char mMyPrivRsa[1024] = {0};
    unsigned short mMyPrivRsaLen = 0;
    char mMyPubRsa[512] = {0};
    unsigned short mMyPubRsaLen = 0;

    bool isLoggedIn() const { return mIsLoggedIn; }
    const Id myHandle() const { return mMyHandle; }
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
    Client(IGui& gui, Presence pres);
    virtual ~Client();
    void registerRtcHandler(rtcModule::IEventHandler* rtcHandler);
    promise::Promise<void> init();
    strongvelope::ProtocolHandler* newStrongvelope(karere::Id chatid);
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
    promise::Promise<void> setPresence(const Presence pres, bool always = false);
    XmppContactList& xmppContactList()
    {
        return mXmppContactList;
    }
protected:
    Id mMyHandle = mega::UNDEF;
    std::string mSid;
    std::string mMyName;

    Presence mOwnPresence;
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
    sqlite3* openDb();
    promise::Promise<void> loginNewSession();
    promise::Promise<void> loginExistingSession();
    void loadOwnUserHandle();
    promise::Promise<void> loadOwnKeysFromApi();
    void loadOwnKeysFromDb();
    void setupXmppReconnectHandler();
    promise::Promise<void> connectXmpp(const std::shared_ptr<HostPortServerInfo>& server);
    void setupXmppHandlers();
    promise::Promise<int> initializeContactList();
    /**
     * @brief send response to ping request.
     *
     * This performs an xmpp response to the received xmpp ping request.
     */
    void sendPong(const std::string& peerJid, const std::string& messageId);
    void dumpChatrooms(::mega::MegaTextChatList& chatRooms);
    void dumpContactList(::mega::MegaUserList& clist);
    //rtcModule::IGlobalEventHandler interface
    virtual rtcModule::IEventHandler* onIncomingCallRequest(
            const std::shared_ptr<rtcModule::ICallAnswer> &call);
    virtual void discoAddFeature(const char *feature);
    //mega::MegaGlobalListener interface, called by worker thread
    virtual void onChatsUpdate(mega::MegaApi*, mega::MegaTextChatList* rooms);
    virtual void onUsersUpdate(mega::MegaApi*, mega::MegaUserList* users);
    virtual void onContactRequestsUpdate(mega::MegaApi*, mega::MegaContactRequestList* reqs);
};

inline Presence PeerChatRoom::calculatePresence(Presence pres) const
{
    if (mChat && mChat->onlineState() != chatd::kChatStateOnline)
        return Presence::kOffline;
    return pres;
}

}
#endif // CHATCLIENT_H
