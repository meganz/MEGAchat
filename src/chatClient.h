#ifndef CHATCLIENT_H
#define CHATCLIENT_H

#include "karereCommon.h"
#include "sdkApi.h"
#include <memory>
#include <map>
#include <type_traits>
#include <retryHandler.h>
#include "userAttrCache.h"
#include <db.h>
#include "chatd.h"
#include "presenced.h"
#include "IGui.h"
#include <base/trackDelete.h>
#include "rtcModule/webrtc.h"

namespace mega { class MegaTextChat; class MegaTextChatList; }

namespace strongvelope { class ProtocolHandler; }

struct sqlite3;
class Buffer;

namespace karere
{
namespace rh { class IRetryController; }

/** @brief
 * Utulity function to create an application directory, suitable for desktop systems.
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

/** @brief An abstract class representing a chatd chatroom. It has two
 * descendants - \c PeerChatRoom, representing a 1on1 chatroom,
 * and \c GroupChatRoom, representing a group chat room. This class also
 * serves as a chat event handler for the chatroom, until the application creates
 * one via \c IApp::createChatHandler()
 */
class ChatRoom: public chatd::Listener, public DeleteTrackable
{
    //@cond PRIVATE
public:
    ChatRoomList& parent;
protected:
    IApp::IChatHandler* mAppChatHandler = nullptr;
    uint64_t mChatid;
    std::string mUrl;
    unsigned char mShardNo;
    bool mIsGroup;
    chatd::Priv mOwnPriv;
    chatd::Chat* mChat = nullptr;
    bool mIsInitializing = true;
    uint32_t mCreationTs;
    bool mIsArchived;
    std::string mTitleString;
    void notifyTitleChanged();
    void switchListenerToApp();
    void createChatdChat(const karere::SetOfIds& initialUsers); //We can't do the join in the ctor, as chatd may fire callbcks synchronously from join(), and the derived class will not be constructed at that point.
    void notifyExcludedFromChat();
    void notifyRejoinedChat();
    bool syncOwnPriv(chatd::Priv priv);
    bool syncArchive(bool aIsArchived);
    void onMessageTimestamp(uint32_t ts);
    ApiPromise requestGrantAccess(mega::MegaNode *node, mega::MegaHandle userHandle);
    ApiPromise requestRevokeAccess(mega::MegaNode *node, mega::MegaHandle userHandle);

public:
    virtual bool syncWithApi(const mega::MegaTextChat& chat) = 0;
    virtual IApp::IChatListItem* roomGui() = 0;
    /** @endcond PRIVATE */

    /** @brief The text that will be displayed on the chat list for that chat */
    virtual const char *titleString() const = 0;

    /** @brief Connects to the chatd chatroom */
    virtual void connect() = 0;

    ChatRoom(ChatRoomList& parent, const uint64_t& chatid, bool isGroup,
             unsigned char shard, chatd::Priv ownPriv, uint32_t ts, bool isArchived,
             const std::string& aTitle=std::string());

    virtual ~ChatRoom(){}

    /** @brief returns the chatd::Chat chat object associated with the room */
    chatd::Chat& chat() { return *mChat; }

    /** @brief returns the chatd::Chat chat object associated with the room */
    const chatd::Chat& chat() const { return *mChat; }

    /** @brief The chatid of the chatroom */
    const uint64_t& chatid() const { return mChatid; }

    /** @brief Whether this chatroom is a groupchat or 1on1 chat */
    bool isGroup() const { return mIsGroup; }

    /** @brief Whether this chatroom is archived or not */
    bool isArchived() const { return mIsArchived; }

    bool isCallActive() const;

    /** @brief The websocket url that is used to connect to chatd for that chatroom. Contains an authentication token */
    const std::string& url() const { return mUrl; }

    /** @brief The chatd shart number for that chatroom */
    unsigned char shardNo() const { return mShardNo; }

    /** @brief Our own privilege within this chat */
    chatd::Priv ownPriv() const { return mOwnPriv; }

    /** @brief Whether we are currently member of the chatroom (for group
      * chats), or we are contacts with the peer (for 1on1 chats)
      */
    bool isActive() const { return mIsGroup ? (mOwnPriv != chatd::PRIV_NOTPRESENT) : true; }

    /** @brief The online state reported by chatd for that chatroom */
    chatd::ChatState chatdOnlineState() const { return mChat->onlineState(); }

    /** @brief send a notification to the chatroom that the user is typing. */
    virtual void sendTypingNotification() { mChat->sendTypingNotification(); }

    /** @brief send a notification to the chatroom that the user has stopped typing. */
    virtual void sendStopTypingNotification() { mChat->sendStopTypingNotification(); }

    void sendSync() { mChat->sendSync(); }

    /** @brief The application-side event handler that receives events from
     * the chatd chatroom and events about title, online status and unread
     * message count change.
     */
    IApp::IChatHandler* appChatHandler() { return mAppChatHandler; }

    /** @brief Attaches an app-provided event handler to the chatroom
     * The handler must forward some events to the chatroom in order to
     * have the chat list item still receive events. The events that need
     * to be forwarded are:
     * \c onUserJoin, \c onUserLeave, \c onUnreadChanged,
     * \c onOnlineStateChange, \c onRecvNewMessage, \c onRecvHistoryMessage.
     * @param handler The application-provided chat event handler.
     * The chatroom object does not take owhership of the handler,
     * so, on removal, the app should take care to free it if needed.
     */
    void setAppChatHandler(IApp::IChatHandler* handler);

    /** @brief Removes the application-supplied chat event handler from the
     * room. It is up to the aplication to destroy it if needed.
     */
    void removeAppChatHandler();

    /** @brief Whether the chatroom object is currently being
     * constructed.
     */
    bool isInitializing() const { return mIsInitializing; }

    bool hasChatHandler() const;

#ifndef KARERE_DISABLE_WEBRTC
    /** @brief Initiates a webrtc call in the chatroom
     *  @param av Whether to initially send video and/or audio
     */
    virtual rtcModule::ICall& mediaCall(AvFlags av, rtcModule::ICallHandler& handler);

    /** @brief Joins a webrtc call in the chatroom
     *  @param av Whether to initially send video and/or audio
     */
    virtual rtcModule::ICall& joinCall(AvFlags av, rtcModule::ICallHandler& handler, Id callid);
#endif

    //chatd::Listener implementation
    virtual void init(chatd::Chat& messages, chatd::DbInterface *&dbIntf);
    virtual void onLastTextMessageUpdated(const chatd::LastTextMsg& msg);
    virtual void onLastMessageTsUpdated(uint32_t ts);
    virtual void onExcludedFromRoom() {}
    virtual void onOnlineStateChange(chatd::ChatState state);
    virtual void onMsgOrderVerificationFail(const chatd::Message& msg, chatd::Idx idx, const std::string& errmsg);

    virtual void onRecvNewMessage(chatd::Idx idx, chatd::Message& msg, chatd::Message::Status status);
    virtual void onMessageEdited(const chatd::Message& msg, chatd::Idx idx);
    virtual void onMessageStatusChange(chatd::Idx idx, chatd::Message::Status newStatus, const chatd::Message& msg);
    virtual void onUnreadChanged();

    //IApp::IChatHandler implementation
    virtual void onArchivedChanged(bool archived);

    promise::Promise<void> truncateHistory(karere::Id msgId);
    promise::Promise<void> archiveChat(bool archive);

    virtual promise::Promise<void> requesGrantAccessToNodes(mega::MegaNodeList *nodes) = 0;
    virtual promise::Promise<void> requestRevokeAccessToNode(mega::MegaNode *node) = 0;
};
/** @brief Represents a 1on1 chatd chatroom */
class PeerChatRoom: public ChatRoom
{
protected:
    uint64_t mPeer;
    chatd::Priv mPeerPriv;
    std::string mEmail;
    Contact *mContact;
    // mRoomGui must be the last member, since when we initialize it,
    // we call into the app and pass our this pointer, so all other members
    // must be initialized
    IApp::IPeerChatListItem* mRoomGui;
    friend class ContactList;
    IApp::IPeerChatListItem* addAppItem();
    virtual bool syncWithApi(const mega::MegaTextChat& chat);
    bool syncPeerPriv(chatd::Priv priv);
    static uint64_t getSdkRoomPeer(const ::mega::MegaTextChat& chat);
    static chatd::Priv getSdkRoomPeerPriv(const ::mega::MegaTextChat& chat);
    void initWithChatd();
    virtual void connect();
    UserAttrCache::Handle mUsernameAttrCbId;
    void updateTitle(const std::string& title);
    friend class Contact;
    friend class ChatRoomList;
    PeerChatRoom(ChatRoomList& parent, const uint64_t& chatid,
            unsigned char shard, chatd::Priv ownPriv, const uint64_t& peer,
            chatd::Priv peerPriv, uint32_t ts, bool aIsArchived);
    PeerChatRoom(ChatRoomList& parent, const mega::MegaTextChat& room);
    ~PeerChatRoom();

public:
    virtual IApp::IChatListItem* roomGui() { return mRoomGui; }
    /** @brief The userid of the other person in the 1on1 chat */
    uint64_t peer() const { return mPeer; }
    chatd::Priv peerPrivilege() const { return mPeerPriv; }

    /**
     * @brief The contact object representing the peer of the 1on1 chat.
     * @note Returns nullptr when the 1on1 chat is with a user who canceled the account
     */
    Contact *contact() const { return mContact; }

    /** @brief The screen name of the peer */
    virtual const char *titleString() const;

    /** @brief Returns a string <fistname length><fistname><lastname>. It has binary layout
      * First byte indicate first name length
      */
    const std::string& completeTitleString() const;


    /** @brief The screen email address of the peer */
    virtual const std::string& email() const { return mEmail; }

    void initContact(const uint64_t& peer);

/** @cond PRIVATE */
    //chatd::Listener interface
    virtual void onUserJoin(Id userid, chatd::Priv priv);
    virtual void onUserLeave(Id userid);
/** @endcond */

    virtual promise::Promise<void> requesGrantAccessToNodes(mega::MegaNodeList *nodes);
    virtual promise::Promise<void> requestRevokeAccessToNode(mega::MegaNode *node);
};

/** @brief Represents a chatd chatroom that is a groupchat */
class GroupChatRoom: public ChatRoom
{
public:
    /** @brief Represents a single chatroom member */
    class Member
    {
    protected:
        GroupChatRoom& mRoom;
        uint64_t mHandle;
        chatd::Priv mPriv;
        UserAttrCache::Handle mNameAttrCbHandle;
        UserAttrCache::Handle mEmailAttrCbHandle;
        std::string mName;
        std::string mEmail;
        Presence mPresence;
        void subscribeForNameChanges();
        promise::Promise<void> mNameResolved;
    public:
        Member(GroupChatRoom& aRoom, const uint64_t& user, chatd::Priv aPriv);
        ~Member();

        /** @brief The current display name of the member */
        const std::string& name() const { return mName; }

        /** @brief The user's email. This is obtainable even if the user is not
         * in our contactlist
         */
        const std::string& email() const { return mEmail; }

        /** @brief The current privilege of the member within the groupchat */
        chatd::Priv priv() const { return mPriv; }

        /** @brief The presence of the peer */
        Presence presence() const { return mPresence; }

        promise::Promise<void> nameResolved() const;

        friend class GroupChatRoom;
    };
    /**
     * @brief A map that holds all the members of a group chat room, keyed by the userid */
    typedef std::map<uint64_t, Member*> MemberMap;

    /** @cond PRIVATE */
    protected:
    MemberMap mPeers;
    bool mHasTitle;
    std::string mEncryptedTitle; //holds the encrypted title until we create the strongvelope module
    IApp::IGroupChatListItem* mRoomGui;
    promise::Promise<void> mMemberNamesResolved;
    bool syncMembers(const mega::MegaTextChat& chat);
    void loadTitleFromDb();
    promise::Promise<void> decryptTitle();
    void clearTitle();
    promise::Promise<void> addMember(uint64_t userid, chatd::Priv priv, bool saveToDb);
    bool removeMember(uint64_t userid);
    void updatePeerPresence(uint64_t peer, Presence pres);
    virtual bool syncWithApi(const mega::MegaTextChat &chat);
    IApp::IGroupChatListItem* addAppItem();
    virtual IApp::IChatListItem* roomGui() { return mRoomGui; }
    void deleteSelf(); ///< Deletes the room from db and then immediately destroys itself (i.e. delete this)
    void makeTitleFromMemberNames();
    void initWithChatd();
    void setRemoved();
    virtual void connect();
    promise::Promise<void> memberNamesResolved() const;

    friend class ChatRoomList;
    friend class Member;
    friend class Client;
    GroupChatRoom(ChatRoomList& parent, const mega::MegaTextChat& chat);
    GroupChatRoom(ChatRoomList& parent, const uint64_t& chatid,
                  unsigned char aShard, chatd::Priv aOwnPriv, uint32_t ts,
                  bool aIsArchived, const std::string& title);
    ~GroupChatRoom();
public:
//chatd::Listener
    virtual void onUserJoin(Id userid, chatd::Priv priv);
    virtual void onUserLeave(Id userid);
//====
    /** @endcond PRIVATE */

    /** @brief Returns the map of the users in the chatroom, except our own user */
    const MemberMap& peers() const { return mPeers; }

    /** @brief Returns whether the group chatroom has a title set. If not, then
      * its title string will be composed from the first names of the room members
      */
    bool hasTitle() const { return mHasTitle; }

    /** @brief The title of the chatroom */
    virtual const char *titleString() const { return mTitleString.c_str(); }

    /** @brief Removes the specifid user from the chatroom. You must have
     * operator privileges to do that.
     * @note Do not use this method to exclude yourself. Instead, call leave()
     * @param user The handle of the user to remove from the chatroom.
     * @returns A void promise, which will fail if the MegaApi request fails.
     */
    promise::Promise<void> excludeMember(uint64_t user);

    /**
     * @brief Removes yourself from the chatroom.
     * @returns A void promise, which will fail if the MegaApi request fails.
     */
    promise::Promise<void> leave();

    /** TODO
     * @brief setTitle
     * @param title
     * @returns A void promise, which will fail if the MegaApi request fails.
     */
    promise::Promise<void> setTitle(const std::string& title);

    /** TODO
     * @brief invite
     * @param userid
     * @param priv
     * @returns A void promise, which will fail if the MegaApi request fails.
     */
    promise::Promise<void> invite(uint64_t userid, chatd::Priv priv);

    /** TODO
     * @brief setPrivilege
     * @param userid
     * @param priv
     * @returns A void promise, which will fail if the MegaApi request fails.
     */
    promise::Promise<void> setPrivilege(karere::Id userid, chatd::Priv priv);

    virtual promise::Promise<void> requesGrantAccessToNodes(mega::MegaNodeList *nodes);
    virtual promise::Promise<void> requestRevokeAccessToNode(mega::MegaNode *node);
};

/** @brief Represents all chatd chatrooms that we are members of at the moment,
 * keyed by the chatid of the chatroom. This object can be obtained
 * via \c Client::chats
 */
class ChatRoomList: public std::map<uint64_t, ChatRoom*> //don't use shared_ptr here as we want to be able to immediately delete a chatroom once the API tells us it's deleted
{
/** @cond PRIVATE */
public:
    Client& mKarereClient;
    void addMissingRoomsFromApi(const mega::MegaTextChatList& rooms, karere::SetOfIds& chatids);
    ChatRoom* addRoom(const mega::MegaTextChat &room);
    void removeRoom(GroupChatRoom& room);
    ChatRoomList(Client& aClient);
    ~ChatRoomList();
    void loadFromDb();
    void onChatsUpdate(mega::MegaTextChatList& chats);
/** @endcond PRIVATE */
};

/** @brief Represents a karere contact. Also handles presence change events. */
class Contact: public karere::DeleteTrackable
{
    friend class ContactList;
    friend class PeerChatRoom;
/** @cond PRIVATE */
protected:
    ContactList& mClist;
    Presence mPresence;
    uint64_t mUserid;
    PeerChatRoom* mChatRoom;
    UserAttrCache::Handle mUsernameAttrCbId;
    std::string mEmail;
    int64_t mSince;
    std::string mTitleString;
    int mVisibility;
    bool mIsInitializing = true;
    void updateTitle(const std::string& str);
    void notifyTitleChanged();
    void setChatRoom(PeerChatRoom& room);
    void attachChatRoom(PeerChatRoom& room);
    void updatePresence(Presence pres);
public:
    Contact(ContactList& clist, const uint64_t& userid, const std::string& email,
            int visibility, int64_t since, PeerChatRoom* room = nullptr);
    ~Contact();
/** @endcond PRIVATE */

    /** @brief The contactlist object which this contact is member of */
    ContactList& contactList() const { return mClist; }

    /** @brief Returns the 1on1 chatroom with this contact, if one exists.
     * Otherwise returns NULL
     */
    PeerChatRoom* chatRoom() { return mChatRoom; }

    /** @brief Creates a 1on1 chatroom with this contact, if one does not exist,
     * otherwise returns the existing one.
     * @returns a promise containing the 1on1 chatroom object
     */
    promise::Promise<ChatRoom *> createChatRoom();

    /** @brief Returns the current screen name of this contact. It is not pure std::string. It has binary layout
      * First byte indicate first name length
      */
    const std::string& titleString() const { return mTitleString; }

    /** @brief Returns the userid of this contact */
    uint64_t userId() const { return mUserid; }

    /** @brief Returns the email of this contact */
    const std::string& email() const { return mEmail; }

    /** @brief Returns the time since this contact was added */
    int64_t since() const { return mSince; }

    /** @brief The visibility of this contact, as returned by
     * mega::MegaUser::getVisibility(). If it is \c MegaUser::VISIBILITY_HIDDEN,
     * then this contact is not a contact anymore, but kept in the contactlist
     * to be able to access archived chat history etc.
     */
    int visibility() const { return mVisibility; }

    /** @brief The presence of the contact */
    Presence presence() const { return mPresence; }
    bool isInitializing() const { return mIsInitializing; }
    /** @cond PRIVATE */
    void onVisibilityChanged(int newVisibility);
};

/** @brief This is the karere contactlist class. It maps user ids
 * to Contact objects
 */
class ContactList: public std::map<uint64_t, Contact*>
{
    friend class Client;
protected:
    void removeUser(iterator it);
    void onPresenceChanged(Id userid, Presence pres);
    void setAllOffline();
public:
    /** @brief The Client object that this contactlist belongs to */
    Client& client;

    /** @brief Returns the contact object from the specified XMPP jid if one exists,
     * otherwise returns NULL
     */
    Contact* contactFromJid(const std::string& jid) const;
    Contact* contactFromUserId(uint64_t userid) const;
    Contact* contactFromEmail(const std::string& email) const;

    /** @cond PRIVATE */
    ContactList(Client& aClient);
    ~ContactList();
    void loadFromDb();
    bool addUserFromApi(mega::MegaUser& user);
    void onUserAddRemove(mega::MegaUser& user); //called for actionpackets
    promise::Promise<void> removeContactFromServer(uint64_t userid);
    void syncWithApi(mega::MegaUserList& users);
    void onContactOnlineState(const std::string& jid);
    const std::string* getUserEmail(uint64_t userid) const;
    bool isExContact(karere::Id userid);
    /** @endcond */
};

/** @brief The karere Client object. Create an instance to use Karere.
 *
 *  A sequence of how the client has to be initialized:
 *  1. create a new MegaApi instance of the Mega SDK
 *  2. create a new karere::Client instance and pass it the Mega SDK instance
 *  3. Call MegaApi::login() and wait for completion
 *  4. Call MegaApi::fetchnodes() and wait for completion
 *     [at this stage, cloud storage apps show the main GUI, but apps with
 *      with chat enabled are not ready to be shown yet]
 *  5. Call karere::Client::init() to initialize the chat engine.
 *     [at this stage, a chat-enabled app can load chatrooms and history
 *      from the local karere cache db, and can operate in offline mode]
 *  6. Call karere::Client::connect() and wait for completion
 *  7. The app is ready to operate
 */
class Client: public ::mega::MegaGlobalListener,
              public ::mega::MegaRequestListener,
              public presenced::Listener,
              public karere::DeleteTrackable
{
public:
    enum ConnState { kDisconnected = 0, kConnecting, kConnected };
    enum InitState: uint8_t
    {
        /** The client has just been created. \c init() has not been called yet */
        kInitCreated = 0,

         /** \c init() has been called with no \c sid. The client is waiting
         * for the completion of a full fetchnodes from the SDK on a new session.
         */
        kInitWaitingNewSession,

        /** \c init() has been called with a \c sid, there is a valid cache for that
         * \sid, and the client is successfully initialized for offline operation */
        kInitHasOfflineSession,

        /** Karere has sucessfully initialized and the SDK/API is online.
         * Note that the karere client itself (chat, presence) is not online.
         * It has to be explicitly connected via \c connect()
         */
        kInitHasOnlineSession,

        /** Client has disconnected and terminated */
        kInitTerminated,

        /** The first init state error code. All values equal or greater than this
         * represent error states
         */
        kInitErrFirst,

        /** Unspecified init error */
        kInitErrGeneric = kInitErrFirst,

        /** There was not valid database for the sid specified to \c init().
         * This error is recoverable - the client will signal it, but then will
         * behave like it starts with a new session. However, the application must
         * be aware of this condition in order to tell the SDK to do a full fetchnodes,
         * and receive all actionpackets again, so that karere can have a chance
         * to initialize its state from scratch
         */
        kInitErrNoCache,

        /** A problem was fund while initializing from a seemingly valid karere cache.
         * This error is not recoverable. The client is probably in a bogus state,
         * and the client instance should be destroyed. A possible recovery scenario
         * is to delete the karere cache file, create and init a new client instance
         * with the same sid. Then, login the SDK with full fetchnodes.
         * In that case, the recoverable kInitErrNoCache will occur but
         * karere will continue by creating the cache from scratch.
         */
        kInitErrCorruptCache,

        /** The session given to init() was different than the session with which
         * the SDK was initialized
         */
        kInitErrSidMismatch,

        /** init() has already been called on that client instance */
        kInitErrAlready,

        /** The session has expired or has been closed. */
        kInitErrSidInvalid
    };

    enum
    {
        kHeartbeatTimeout = 10000     /// Timeout for heartbeats (ms)
    };

    /** @brief Convenience aliases for the \c force flag in \c setPresence() */
    enum: bool { kSetPresOverride = true, kSetPresDynamic = false };

    std::string mAppDir;        // must be before db member because it's used during the init of db member
    WebsocketsIO *websocketIO;  // network-layer interface
    void *appCtx;               // app's context
    MyMegaApi api;              // MegaApi's instance
    IApp& app;                  // app's interface
    SqliteDb db;                // db-layer interface

    std::unique_ptr<chatd::Client> mChatdClient;

#ifndef KARERE_DISABLE_WEBRTC
    std::unique_ptr<rtcModule::IRtcModule> rtc;
#endif

    char mMyPrivCu25519[32] = {0};
    char mMyPrivEd25519[32] = {0};
    char mMyPrivRsa[1024] = {0};
    unsigned short mMyPrivRsaLen = 0;
    char mMyPubRsa[512] = {0};
    unsigned short mMyPubRsaLen = 0;

    /** @brief The contact list of the client */
    std::unique_ptr<ContactList> contactList;

    /** @brief The list of chats that we are member of */
    std::unique_ptr<ChatRoomList> chats;

    // timer for receiving acknowledge of SYNCs
    megaHandle mSyncTimer = 0;

    // to track if all chats returned SYNC
    int mSyncCount = -1;

    // resolved only when up to date
    promise::Promise<void> mSyncPromise;

protected:

    Id mMyHandle = Id::null(); //mega::UNDEF
    std::string mMyName = std::string("\0", 1);
    std::string mMyEmail;
    uint64_t mMyIdentity = 0; // seed for CLIENTID
    std::unique_ptr<UserAttrCache> mUserAttrCache;
    UserAttrCache::Handle mOwnNameAttrHandle;

    std::string mSid;
    std::string mLastScsn;
    InitState mInitState = kInitCreated;
    ConnState mConnState = kDisconnected;
    bool mContactsLoaded = false;

    // resolved when fetchnodes is completed
    promise::Promise<void> mSessionReadyPromise;

    // resolved when connection to presenced is established
    promise::Promise<void> mConnectPromise;

    Presence mOwnPresence = Presence::kInvalid;
    presenced::Client mPresencedClient;
    std::string mPresencedUrl;

    megaHandle mHeartbeatTimer = 0;
    bool mGroupCallsEnabled = false;

public:

    /**
     * @brief Creates a karere Client.
     *
     * @param sdk The Mega SDK instance that this client will use.
     * @param app The IApp interface that the client will use to access
     * services/objects implemented by the application.
     * @param pres The initial presence that will be set when we log in.
     * @param existingCache Whether the karere cache db exists and karere should
     * try to use it, or not. If \c true is specified but the db is unreadable or
     * inconsistent, karere will behave as if \c false was specified - will
     * delete the karere.db file and re-create it from scratch.
     */
    Client(::mega::MegaApi& sdk, WebsocketsIO *websocketsIO, IApp& app, const std::string& appDir,
           uint8_t caps, void *ctx = NULL);

    virtual ~Client();

    const Id myHandle() const { return mMyHandle; }
    const std::string& myName() const { return mMyName; }
    const std::string& myEmail() const { return mMyEmail; }
    uint64_t myIdentity() const { return mMyIdentity; }
    UserAttrCache& userAttrCache() const { return *mUserAttrCache; }

    ConnState connState() const { return mConnState; }
    bool connected() const { return mConnState == kConnected; }
    bool contactsLoaded() const { return mContactsLoaded; }

    Presence ownPresence() const { return mOwnPresence; }
    presenced::Client& presenced() { return mPresencedClient; }

    /**
     * @brief Performs karere-only login, assuming the Mega SDK is already logged in
     * with an existing session.
     */
    void initWithDbSession(const char* sid);

    /**
     * @brief Performs karere-only login, assuming the Mega SDK is already logged
     * in with a new session
     */
    promise::Promise<void> initWithNewSession(const char* sid, const std::string& scsn,
        const std::shared_ptr<::mega::MegaUserList>& contactList,
        const std::shared_ptr<::mega::MegaTextChatList>& chatList);

    /**
     * @brief Initializes karere, opening or creating the local db cache
     * @param sid - an optional session id to restore from. If one is specified
     * (sid is not \c NULL), the client will try to initialize for offline work
     * from an existing karere cache for that sid. If loading the local cache was
     * successful, the client will transition to \c kInitHasOfflineSession.
     * If a karere cache for that sid does not exist or is not valid, the client
     * will signal its init state as \c kInitErrNoCache and then continue
     * to \c kInitWaitingNewSession, effectively behaving as if a new session
     * is being created by the SDK (see the \c NULL-sid case below).
     * The app, upon seeing \c kInitErrNoCache must do the complete version of the
     * \c fetchnodes operation, and not the fast one that fetches only newly queued
     * actionpackets. This is in order to replay all actionpackets and associated
     * events, so that karere can rebuild its cache from scratch, as if this was
     * a new session.
     * If \c sid is \c NULL, then the client will transition to \c kInitWaitingForNewSession,
     * and wait for a fetchnodes completion from the SDK. Then it will initialize
     * its state and cache from scratch, from information provided by the SDK.
     * In both cases, when fetchnodes completes, the client will transition to
     * \c kInitHasOnlineSession.
     * @note In any case, if there is no existing karere session cache,
     * offline operation is not possible.
     */
    InitState init(const char* sid);
    InitState initState() const { return mInitState; }
    bool hasInitError() const { return mInitState >= kInitErrFirst; }
    bool isTerminated() const { return mInitState == kInitTerminated; }
    const char* initStateStr() const { return initStateToStr(mInitState); }
    static const char* initStateToStr(unsigned char state);
    const char* connStateStr() const { return connStateToStr(mConnState); }
    static const char* connStateToStr(ConnState state);

    /** @brief Does the actual connection to chatd and presenced. Assumes the
     * Mega SDK is already logged in. This must be called after
     * \c initNewSession() or \c initExistingSession() completes
     * @param pres The presence which should be set. This is a forced presence,
     * i.e. it will be preserved even if the client disconnects. To disable
     * setting such a forced presence and assume whatever presence was last used,
     * and/or use only dynamic presence, set this param to \c Presence::kClear
     * @param isInBackground In case the app requests to connect from a service in
     * background, it should not send KEEPALIVE, but KEEPALIVEAWAY. Hence, it will
     * avoid to tell chatd that the client is active.
     */
    promise::Promise<void> connect(Presence pres=Presence::kClear, bool isInBackground = false);

    /**
     * @brief Retry pending connections to chatd and presenced
     * @return A promise to track the result of the action.
     */
    void retryPendingConnections(bool disconnect);

    /**
     * @brief A convenience method that logs in the Mega SDK and then inits
     * karere. This can be used when building a standalone chat app where there
     * is no app code that logs in the Mega SDK.
     * @param sid - The mega session id with which to log in the SDK and init
     * karere. If it is NULL, then a new SDK session is created.
     */
    promise::Promise<void> loginSdkAndInit(const char* sid);

    /** @brief Notifies the client that network connection is down */
    void notifyNetworkOffline();

    /** @brief Notifies the client that internet connection is again available */
    void notifyNetworkOnline();
    /** @brief Call this when the app goes into background, so that it notifies
     * the servers to enable PUSH notifications
     */
    void notifyUserIdle();
    /** @brief Call this when the app goes into foreground, so that push notifications
     * are disabled
     */
    void notifyUserActive();

    void startKeepalivePings();

    /** Terminates the karere client, logging it out, hanging up calls,
     * and cleaning up state
     * @param deleteDb - if set to \c true, deletes the local cache db.
     */
    void terminate(bool deleteDb=false);

    /**
    * @brief set user's chat presence.
    * Set user's presence state
    *
    * @param force Forces re-setting the presence, even if the current presence
    * is the same. Normally is \c false
    */
    promise::Promise<void> setPresence(const Presence pres);

    /** @brief Creates a group chatroom with the specified peers, privileges
     * and title.
     * @param peers A vector of userhandle and privilege pairs for each of
     * the peer users. Our own handle is implicitly added to the groupchat
     * with operator privilega
     * @param title The title of the group chat. If set to an empty string,
     * no title will be set and the room will be shown with the names of
     * the participants.
     */
    promise::Promise<karere::Id>
    createGroupChat(std::vector<std::pair<uint64_t, chatd::Priv>> peers);
    void setCommitMode(bool commitEach);
    void saveDb();  // forces a commit

    /** @brief There is a call active in the chatroom*/
    bool isCallActive(karere::Id chatid = karere::Id::inval()) const;

    /** @brief There is a call in state in-progress in the chatroom and the client is participating*/
    bool isCallInProgress(karere::Id chatid = karere::Id::inval()) const;

    promise::Promise<void> pushReceived(Id chatid);
    void onSyncReceived(karere::Id chatid); // called upon SYNC reception

    void dumpChatrooms(::mega::MegaTextChatList& chatRooms);
    void dumpContactList(::mega::MegaUserList& clist);

    bool isChatRoomOpened(Id chatid);
    bool areGroupCallsEnabled();
    void enableGroupCalls(bool enable);
    void updateAndNotifyLastGreen(Id userid);

protected:
    void heartbeat();
    void setInitState(InitState newState);

    // db-related methods
    std::string dbPath(const std::string& sid) const;
    bool openDb(const std::string& sid);
    void createDb();
    void wipeDb(const std::string& sid);
    void createDbSchema();

    // initialization of own handle/email/identity/keys/contacts...
    karere::Id getMyHandleFromDb();
    karere::Id getMyHandleFromSdk();
    std::string getMyEmailFromDb();
    std::string getMyEmailFromSdk();
    uint64_t getMyIdentityFromDb();
    promise::Promise<void> loadOwnKeysFromApi();
    void loadOwnKeysFromDb();
    void loadContactListFromApi();
    void loadContactListFromApi(::mega::MegaUserList& contactList);

    strongvelope::ProtocolHandler* newStrongvelope(karere::Id chatid);

    // connection-related methods
    void connectToChatd(bool isInBackground);
    promise::Promise<void> connectToPresenced(Presence pres);
    promise::Promise<void> connectToPresencedWithUrl(const std::string& url, Presence forcedPres);
    promise::Promise<int> initializeContactList();

    bool checkSyncWithSdkDb(const std::string& scsn, ::mega::MegaUserList& clist, ::mega::MegaTextChatList& chats);
    void commit(const std::string& scsn);

    /** @brief Does the actual connect, once the SDK is online.
     * connect() waits for the mCanConnect promise to be resolved and then calls
     * this method
     */
    promise::Promise<void> doConnect(Presence pres, bool isInBackground);
    void setConnState(ConnState newState);

    // mega::MegaGlobalListener interface, called by worker thread
    virtual void onChatsUpdate(mega::MegaApi*, mega::MegaTextChatList* rooms);
    virtual void onUsersUpdate(mega::MegaApi*, mega::MegaUserList* users);
    virtual void onEvent(::mega::MegaApi* api, ::mega::MegaEvent* event);

    // MegaRequestListener interface
    virtual void onRequestFinish(::mega::MegaApi* apiObj, ::mega::MegaRequest *request, ::mega::MegaError* e);

    // presenced listener interface
    virtual void onConnStateChange(presenced::Client::ConnState state);
    virtual void onPresenceChange(Id userid, Presence pres);
    virtual void onPresenceConfigChanged(const presenced::Config& state, bool pending);
    virtual void onPresenceLastGreenUpdated(karere::Id userid);

    //==
    friend class ChatRoom;
    friend class ChatRoomList;
};

}
#endif // CHATCLIENT_H
