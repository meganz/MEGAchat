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
#include "stringUtils.h"

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable: 4996) // rapidjson: The std::iterator class template (used as a base class to provide typedefs) is deprecated in C++17. (The <iterator> header is NOT deprecated.) 
#endif

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#ifdef _WIN32
#pragma warning(pop)
#endif

namespace mega { class MegaTextChat; class MegaTextChatList; }

namespace strongvelope { class ProtocolHandler; }

struct sqlite3;
class Buffer;

#define ID_CSTR(id) Id(id).toString().c_str()

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
typedef std::map<uint64_t, std::string> AliasesMap;
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
    unsigned char mShardNo;
    bool mIsGroup;
    chatd::Priv mOwnPriv;
    chatd::Chat* mChat = nullptr;
    bool mIsInitializing = true;
    int64_t mCreationTs;
    bool mIsArchived;
    std::string mTitleString;   // decrypted `ct` or title from member-names
    bool mHasTitle;             // only true if chat has custom topic (`ct`)
    void notifyTitleChanged();
    void notifyChatModeChanged();
    void switchListenerToApp();
    void createChatdChat(const karere::SetOfIds& initialUsers, bool isPublic = false,
            std::shared_ptr<std::string> unifiedKey = nullptr, int isUnifiedKeyEncrypted = false, const karere::Id = karere::Id::inval() ); //We can't do the join in the ctor, as chatd may fire callbcks synchronously from join(), and the derived class will not be constructed at that point.
    void notifyExcludedFromChat();
    void notifyRejoinedChat();
    bool syncOwnPriv(chatd::Priv priv);
    bool syncArchive(bool aIsArchived);
    void onMessageTimestamp(uint32_t ts);
    ApiPromise requestGrantAccess(mega::MegaNode *node, mega::MegaHandle userHandle);
    ApiPromise requestRevokeAccess(mega::MegaNode *node, mega::MegaHandle userHandle);
    bool isChatdChatInitialized();

public:
    virtual bool previewMode() const { return false; }
    virtual bool publicChat() const { return false; }
    virtual uint64_t getPublicHandle() const { return Id::inval(); }
    virtual unsigned int getNumPreviewers() const { return 0; }
    virtual bool syncWithApi(const mega::MegaTextChat& chat) = 0;
    virtual IApp::IChatListItem* roomGui() = 0;
    /** @endcond PRIVATE */

    /** @brief The text that will be displayed on the chat list for that chat */
    virtual const std::string &titleString() const  { return mTitleString; }

    /** @brief Returns whether the chatroom has a title set. If not, then
      * its title string will be composed from the first names of the room members.
      * This method will return false for PeerChatRoom, only GroupChatRoom are
      * capable to have a custom title.
      */
    virtual bool hasTitle() const { return mHasTitle; }

    /** @brief Connects to the chatd chatroom */
    virtual void connect() = 0;

    ChatRoom(ChatRoomList& parent, const uint64_t& chatid, bool isGroup,
             unsigned char shard, chatd::Priv ownPriv, int64_t ts, bool isArchived,
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
    virtual void onPreviewersUpdate();

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

    //Resume from cache
    PeerChatRoom(ChatRoomList& parent, const uint64_t& chatid,
            unsigned char shard, chatd::Priv ownPriv, const uint64_t& peer,
            chatd::Priv peerPriv, int64_t ts, bool aIsArchived);

    //Create chat or receive an invitation
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

    /** @brief The screen email address of the peer */
    virtual const std::string& email() const { return mEmail; }

    void initContact(const uint64_t& peer);
    void updateChatRoomTitle();

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

        promise::Promise<void> nameResolved() const;
        friend class GroupChatRoom;
    };
    /**
     * @brief A map that holds all the members of a group chat room, keyed by the userid */
    typedef std::map<uint64_t, Member*> MemberMap;

    /** @cond PRIVATE */
protected:
    MemberMap mPeers;
    std::string mEncryptedTitle; //holds the last encrypted title (the "ct" from API)
    IApp::IGroupChatListItem* mRoomGui;
    promise::Promise<void> mMemberNamesResolved;

    void setChatPrivateMode();
    bool syncMembers(const mega::MegaTextChat& chat);
    void loadTitleFromDb();
    promise::Promise<void> decryptTitle();
    void clearTitle();
    promise::Promise<void> addMember(uint64_t userid, chatd::Priv priv, bool saveToDb);
    bool removeMember(uint64_t userid);
    virtual bool syncWithApi(const mega::MegaTextChat &chat);
    IApp::IGroupChatListItem* addAppItem();
    virtual IApp::IChatListItem* roomGui() { return mRoomGui; }
    void deleteSelf(); ///< Deletes the room from db and then immediately destroys itself (i.e. delete this)
    void makeTitleFromMemberNames();
    void updateTitleInDb(const std::string &title, int isEncrypted);
    void initWithChatd(bool isPublic, std::shared_ptr<std::string> unifiedKey, int isUnifiedKeyEncrypted, Id ph = Id::inval());
    void notifyPreviewClosed();
    void setRemoved();
    virtual void connect();
    promise::Promise<void> memberNamesResolved() const;
    void initChatTitle(const std::string &title, int isTitleEncrypted, bool saveToDb = false);

    friend class ChatRoomList;
    friend class Member;
    friend class Client;

    //Create chat or receive an invitation
    GroupChatRoom(ChatRoomList& parent, const mega::MegaTextChat& chat);

    //Resume from cache
    GroupChatRoom(ChatRoomList& parent, const uint64_t& chatid,
                unsigned char aShard, chatd::Priv aOwnPriv, int64_t ts,
                bool aIsArchived, const std::string& title, int isTitleEncrypted, bool publicChat, std::shared_ptr<std::string> unifiedKey, int isUnifiedKeyEncrypted);

    //Load chatLink
    GroupChatRoom(ChatRoomList& parent, const uint64_t& chatid,
                unsigned char aShard, chatd::Priv aOwnPriv, int64_t ts,
                bool aIsArchived, const std::string& title,
                const uint64_t publicHandle, std::shared_ptr<std::string> unifiedKey);

    ~GroupChatRoom();

public:
//chatd::Listener
    virtual void onUserJoin(Id userid, chatd::Priv priv);
    virtual void onUserLeave(Id userid);
//====
    /** @endcond PRIVATE */

    /** @brief Returns the map of the users in the chatroom, except our own user */
    const MemberMap& peers() const { return mPeers; }


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

    /** TODO
     *
     */
    promise::Promise<void> autojoinPublicChat(uint64_t ph);

    virtual promise::Promise<void> requesGrantAccessToNodes(mega::MegaNodeList *nodes);
    virtual promise::Promise<void> requestRevokeAccessToNode(mega::MegaNode *node);
    virtual void enablePreview(uint64_t ph);
    virtual bool publicChat() const;
    virtual uint64_t getPublicHandle() const;
    virtual unsigned int getNumPreviewers() const;

    virtual bool previewMode() const;

    promise::Promise<std::shared_ptr<std::string>> unifiedKey();

    void handleTitleChange(const std::string &title, bool saveToDb = false);
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
    void removeRoomPreview(Id chatid);
    ChatRoomList(Client& aClient);
    ~ChatRoomList();
    void loadFromDb();
    void previewCleanup(karere::Id chatid);
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
    uint64_t mUserid;
    PeerChatRoom* mChatRoom;
    UserAttrCache::Handle mUsernameAttrCbId;
    UserAttrCache::Handle mEmailAttrCbId;
    std::string mEmail;
    int64_t mSince;
    std::string mTitleString;
    std::string mName;
    int mVisibility;
    bool mIsInitializing = true;
    void notifyTitleChanged();
    void setChatRoom(PeerChatRoom& room);
    void attachChatRoom(PeerChatRoom& room);
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

    bool isInitializing() const { return mIsInitializing; }
    /** @cond PRIVATE */
    void onVisibilityChanged(int newVisibility);

    /** @brief Set the full name of this contact */
    void setContactName(std::string name);

    /** @brief Set the title of this contact */
    void updateTitle(const std::string& str);

    /** @brief Returns the full name of this contact */
    std::string getContactName();
};

/** @brief This is the karere contactlist class. It maps user ids
 * to Contact objects
 */
class ContactList: public std::map<uint64_t, Contact*>
{
    friend class Client;
public:
    /** @brief The Client object that this contactlist belongs to */
    Client& client;

    Contact* contactFromUserId(uint64_t userid) const;
    Contact* contactFromEmail(const std::string& email) const;

    /** @cond PRIVATE */
    ContactList(Client& aClient);
    ~ContactList();
    void loadFromDb();
    void syncWithApi(mega::MegaUserList& users);
    const std::string* getUserEmail(uint64_t userid) const;
    /** @endcond */
};

/** @brief Class to manage init stats of Karere.
 * This class will measure the initialization times of every stage
 * in order to improve the performance.
 *
 * The main stages included in this class are:
 *      Init
 *      Login
 *      Fetch nodes
 *      Post fetch nodes (From the end of fetch nodes until start of connect)
 *      Connection
 *
 * The Connection stage is subdivided in the following stages with stats per shard:
 *      GetChatUrl
 *      QueryDns
 *      Connect to chatd
 *      All chats logged in
 *
 * To obtain a string with the stats in JSON you have to call statsToString. The structure of the JSON is:
 * [
 * {
 *  "nn":14,		// Number of nodes
 *  "ncn":2,		// Number of contacts
 *  "nch":17,		// Number of chats
 *  "sid":1,		// Init state {InitNewSession = 0, InitResumeSession = 1, InitInvalidCache = 2, InitAnonymous =3}
 *  "telap":1240,	// Total elapsed time
 *  "stgs":			// Array with main stages
 *  [
 *  	{
 *  	"stg":0,		// Stage number
 *  	"tag":"Init",	// Stage tag
 *  	"elap":16		// Stage elapsed time
 *  	},
 *  	{
 *  	...
 *  	}
 *  ]
 *  "shstgs":		// Array with stages divided by shard
 *  [
 *  	{
 *  	"stg":0,				// Stage number
 *  	"tag":"Fetch chat url",	// Stage tag
 *  	"sa":					// Sub array with stats
 *  		[
 *  			{
 *  			"sh":0,             // Shard number
 *  			"elap":222,         // Shard elapsed time
 *  			"max":222,          // Shard max elapsed time
 *  			"ret":0             // Number of retries
 *  			}
 *  			{
 *  			...
 *  			}
 *  		]
 *  	},
 *  	{
 *  	...
 *  	}
 *  ]
 *  }
 *  ]
 *
**/
class InitStats
{
    public:
        /** @brief MEGAchat init stats version :
         * - Version 1: Initial version
         * - Version 2: Fix errors and discard atypical values
         * - Version 3: Implement DNS, Chatd and Presenced Ip/Url cache
         */
        const uint32_t INITSTATSVERSION = 3;

        /** @brief Init states in init stats */
        enum
        {
            kInitNewSession      = 0,
            kInitResumeSession   = 1,
            kInitInvalidCache    = 2,
            kInitAnonymous       = 3
        };

        /** @brief Main stages */
        enum
        {
            kStatsInit              = 0,
            kStatsLogin             = 1,
            kStatsFetchNodes        = 2,
            kStatsPostFetchNodes    = 3,
            kStatsConnection        = 4
        };


        /** @brief Stages per shard */
        enum
        {
            kStatsFetchChatUrl      = 0,
            kStatsQueryDns          = 1,
            kStatsConnect           = 2,
            kStatsLoginChatd        = 3
        };

        std::string onCompleted(long long numNodes, size_t numChats, size_t numContacts);
        bool isCompleted() const;
        void onCanceled();

        /*  Stages Methods */

        /** @brief Obtain initial ts for a stage */
        void stageStart(uint8_t stage);

        /** @brief Obtain end ts for a stage */
        void stageEnd(uint8_t stage);

        /** @brief Set the init state */
        void setInitState(uint8_t state);


        /*  Shard Stages Methods */

        /** @brief Obtain initial ts for a shard */
        void shardStart(uint8_t stage, uint8_t shard);

        /** @brief Obtain end ts for a shard */
        void shardEnd(uint8_t stage, uint8_t shard);

        /** @brief Increments the number of retries for a shard */
        void incrementRetries(uint8_t stage, uint8_t shard);

        /** @brief This function handle the shard stats according to connections states transitions, getting
         *  the start or end ts for a shard in a stage or increments the number of retries in case of error in the stage
         *
         *  @note In kStatsQueryDns the figures are recorded when DNS are resolved successfully and they are stored in DNS cache.
        */
        void handleShardStats(chatd::Connection::State oldState, chatd::Connection::State newState, uint8_t shard);

private:

    struct ShardStats
    {
        /** @brief Elapsed time */
        mega::dstime elapsed = 0;

        /** @brief Max elapsed time */
        mega::dstime maxElapsed = 0;

        /** @brief Starting time */
        mega::dstime tsStart = 0;

        /** @brief Number of retries */
        unsigned int mRetries = 0;
    };

    typedef std::map<uint8_t, mega::dstime> StageMap;   // maps stage to elapsed time (first it stores tsStart)
    typedef std::map<uint8_t, ShardStats> ShardMap;
    typedef std::map<uint8_t, std::map<uint8_t, ShardStats>> StageShardMap;


    /** @brief Maps stages to statistics */
    StageMap mStageStats;

    /** @brief Maps sharded stages to statistics */
    StageShardMap mStageShardStats;

    /** @brief Number of nodes in the account */
    long long int mNumNodes = 0;

    /** @brief Number of chats in the account */
    long int mNumChats = 0;

    /** @brief Number of contacts in the account */
    long int mNumContacts = 0;

    /** @brief Flag that indicates whether the stats have already been sent */
    bool mCompleted = false;

    /** @brief Indicates the init state with cache */
    uint8_t mInitState = kInitNewSession;


    /* Auxiliar methods */

    /** @brief Returns the current time of the clock in milliseconds */
    static mega::dstime currentTime();

    /** @brief  Returns a string with the associated tag to the stage **/
    std::string stageToString(uint8_t stage);

    /** @brief  Returns a string with the associated tag to the stage **/
    std::string shardStageToString(uint8_t stage);

    /** @brief Returns a string that contains init stats in JSON format */
    std::string toJson();

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

public:
    enum { kInitErrorType = 0x9e9a1417 }; //should resemble 'megainit'

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

        /** \c Karere has sucessfully initialized in anonymous mode */
        kInitAnonymousMode,

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

    std::string mAppDir;
    WebsocketsIO *websocketIO;  // network-layer interface
    void *appCtx;               // app's context
    MyMegaApi api;              // MegaApi's instance
    IApp& app;                  // app's interface
    SqliteDb db;                // db-layer interface
    DNScache mDnsCache;         // dns cache

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
    std::unique_ptr<ContactList> mContactList;

    /** @brief The list of chats that we are member of */
    std::unique_ptr<ChatRoomList> chats;

    // timer for receiving acknowledge of SYNCs
    megaHandle mSyncTimer = 0;

    // to track if all chats returned SYNC
    int mSyncCount = -1;

    // resolved only when up to date
    promise::Promise<void> mSyncPromise;

protected:
    Id mMyHandle = Id::inval(); //mega::UNDEF
    std::string mMyName = std::string("\0", 1);
    std::string mMyEmail;
    uint64_t mMyIdentity = 0; // seed for CLIENTID
    std::unique_ptr<UserAttrCache> mUserAttrCache;
    UserAttrCache::Handle mOwnNameAttrHandle;
    UserAttrCache::Handle mAliasAttrHandle;

    std::string mSid;
    std::string mLastScsn;
    InitState mInitState = kInitCreated;
    ConnState mConnState = kDisconnected;

    // resolved when fetchnodes is completed
    promise::Promise<void> mSessionReadyPromise;

    // resolved when connection to presenced is established
    promise::Promise<void> mConnectPromise;

    presenced::Client mPresencedClient;
    std::string mPresencedUrl;

    megaHandle mHeartbeatTimer = 0;
    InitStats mInitStats;

    // Maps uhBin to user alias encoded in B64
    AliasesMap mAliasesMap;
    bool mIsInBackground = false;

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

    presenced::Client& presenced() { return mPresencedClient; }

    /**
     * @brief Performs karere-only login, assuming the Mega SDK is already logged in
     * with an existing session.
     */
    void initWithDbSession(const char* sid);

    InitState initWithAnonymousSession();

    /**
     * @brief Performs karere-only login, assuming the Mega SDK is already logged
     * in with a new session
     */
    promise::Promise<void> initWithNewSession(const char* sid, const std::string& scsn,
        const std::shared_ptr<::mega::MegaUserList>& contactList,
        const std::shared_ptr<::mega::MegaTextChatList>& chatList);

    /**
     * @brief This function returns basic information about a public chat, to be able to open it in preview mode.
     * The information returned by this function includes: the chatid, the connection url, the encrypted title,
     * and the number of participants.
     *
     * @return The chatid, the connection url, the encrypted title, and the number of participants.
     */
    promise::Promise<ReqResult> openChatPreview(uint64_t publicHandle);

    /**
     * @brief This function allows to create a public chat room. This function should be called after call openChatPreview with createChat flag set to true
     * to avoid that openChatPreview creates the chat room
     */
    void createPublicChatRoom(uint64_t chatId, uint64_t ph, int shard, const std::string &decryptedTitle, std::shared_ptr<std::string> unifiedKey, const std::string &url, uint32_t ts);

    /**
     * @brief This function returns the decrypted title of a chat. We must provide the decrypt key.
     * @return The decrypted title of the chat
     */
    promise::Promise<std::string> decryptChatTitle(uint64_t chatId, const std::string &key, const std::string &encTitle, Id ph = Id::inval());

    /** @brief This function invalidates the current public handle and set the chat mode to private
     */
    promise::Promise<void> setPublicChatToPrivate(karere::Id chatid);

    /** @brief This function creates a public handle if not exists
     */
    promise::Promise<uint64_t> getPublicHandle(karere::Id chatid, bool createifmissing);

    /** @brief This function invalidates the current public handle
     */
    promise::Promise<uint64_t> deleteChatLink(karere::Id chatid);

    /**
     * @brief Initializes karere, opening or creating the local db cache
     *
     * @param sid - an optional session id to restore from. If one is specified
     * (sid is not \c NULL), the client will try to initialize for offline work
     * from an existing karere cache for that sid. If loading the local cache was
     * successful, the client will transition to \c kInitHasOfflineSession.
     * If a karere cache for that sid does not exist or is not valid, the client
     * will signal its init state as \c kInitErrNoCache and then continue
     * to \c kInitWaitingNewSession, effectively behaving as if a new session
     * is being created by the SDK (see the \c NULL-sid case below).
     * If \c sid is \c NULL, then the client will transition to \c kInitWaitingForNewSession,
     * and wait for a fetchnodes completion from the SDK. Then it will initialize
     * its state and cache from scratch, from information provided by the SDK.
     * In both cases, when fetchnodes completes, the client will transition to
     * \c kInitHasOnlineSession.
     *
     * @param waitFetchnodesToConnect - if false, the connection request will not
     * wait for the completion of the fetchnodes. Note that it may result on the
     * DB cache becoming out of sync with the state of the account, since the app
     * could connect to chatd and send/receive messages even without being logged
     * in to the API. In consequence, this option should be used with care.
     *
     * @note In any case, if there is no existing karere session cache,
     * offline operation is not possible.
     */
    InitState init(const char* sid, bool waitForFetchnodesToConnect);
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
     * @param isInBackground In case the app requests to connect from a service in
     * background, it should not send KEEPALIVE, but KEEPALIVEAWAY to chatd. Hence, it will
     * avoid to tell chatd that the client is active. Also, the presenced client will
     * prevent to send USERACTIVE 1 in background, since the user is not active.
     */
    promise::Promise<void> connect(bool isInBackground = false);

    /**
     * @brief Retry pending connections to chatd and presenced
     * @return A promise to track the result of the action.
     */
    void retryPendingConnections(bool disconnect, bool refreshURL = false);

    /**
     * @brief A convenience method that logs in the Mega SDK and then inits
     * karere. This can be used when building a standalone chat app where there
     * is no app code that logs in the Mega SDK.
     * @param sid - The mega session id with which to log in the SDK and init
     * karere. If it is NULL, then a new SDK session is created.
     */
    promise::Promise<void> loginSdkAndInit(const char* sid);

    /** @brief Call this when the app changes the status: foreground <-> background,
     * so that PUSH notifications are triggered correctly (enabled in background,
     * disabled in foreground).
     */
    promise::Promise<void> notifyUserStatus(bool background);

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
    createGroupChat(std::vector<std::pair<uint64_t, chatd::Priv>> peers, bool publicchat, const char *title = NULL);
    void setCommitMode(bool commitEach);
    void saveDb();  // forces a commit

    /** @brief There is a call active in the chatroom*/
    bool isCallActive(karere::Id chatid = karere::Id::inval()) const;

    /** @brief There is a call in state in-progress in the chatroom and the client is participating*/
    bool isCallInProgress(karere::Id chatid = karere::Id::inval()) const;

    /** @brief Catch up with API for pending actionpackets*/
    promise::Promise<void> pushReceived(Id chatid);
    void onSyncReceived(karere::Id chatid); // called upon SYNC reception

    void dumpChatrooms(::mega::MegaTextChatList& chatRooms);
    void dumpContactList(::mega::MegaUserList& clist);
    bool anonymousMode() const;
    bool isChatRoomOpened(Id chatid);
    void updateAndNotifyLastGreen(Id userid);
    InitStats &initStats();
    void sendStats();
    void resetMyIdentity();
    uint64_t initMyIdentity();

    bool isInBackground() const;
    void updateAliases(Buffer *data);

    /** @brief Returns a string that contains the user alias in UTF-8 if exists, otherwise returns an empty string*/
    std::string getUserAlias(uint64_t userId);
    void setMyEmail(const std::string &email);
    const std::string& getMyEmail() const;

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

    strongvelope::ProtocolHandler* newStrongvelope(karere::Id chatid, bool isPublic,
            std::shared_ptr<std::string> unifiedKey, int isUnifiedKeyEncrypted, karere::Id ph);

    // connection-related methods
    void connectToChatd();
    promise::Promise<void> connectToPresenced(Presence pres);
    promise::Promise<int> initializeContactList();

    bool checkSyncWithSdkDb(const std::string& scsn, ::mega::MegaUserList& aContactList, ::mega::MegaTextChatList& chats);
    void commit(const std::string& scsn);

    /** @brief Does the actual connect, once the SDK is online.
     * connect() waits for the mCanConnect promise to be resolved and then calls
     * this method
     */
    promise::Promise<void> doConnect();
    void setConnState(ConnState newState);

    // mega::MegaGlobalListener interface, called by worker thread
    virtual void onChatsUpdate(mega::MegaApi*, mega::MegaTextChatList* rooms);
    virtual void onUsersUpdate(mega::MegaApi*, mega::MegaUserList* users);
    virtual void onEvent(::mega::MegaApi* api, ::mega::MegaEvent* event);

    // MegaRequestListener interface
    virtual void onRequestStart(::mega::MegaApi* apiObj, ::mega::MegaRequest *request);
    virtual void onRequestFinish(::mega::MegaApi* apiObj, ::mega::MegaRequest *request, ::mega::MegaError* e);

    // presenced listener interface
    virtual void onConnStateChange(presenced::Client::ConnState state);
    virtual void onPresenceChange(Id userid, Presence pres, bool inProgress = false);
    virtual void onPresenceConfigChanged(const presenced::Config& state, bool pending);
    virtual void onPresenceLastGreenUpdated(karere::Id userid);

    //==
    friend class ChatRoom;
    friend class ChatRoomList;
};
}
#endif // CHATCLIENT_H
