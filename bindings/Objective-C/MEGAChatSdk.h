#import <Foundation/Foundation.h>

#import "MEGAChatRequest.h"
#import "MEGAChatError.h"
#import "MEGAChatRoom.h"
#import "MEGAChatRoomList.h"
#import "MEGAChatPeerList.h"
#import "MEGAChatListItemList.h"
#import "MEGAChatPresenceConfig.h"
#import "MEGAHandleList.h"
#import "MEGAChatRequestDelegate.h"
#import "MEGAChatLoggerDelegate.h"
#import "MEGAChatRoomDelegate.h"
#import "MEGAChatDelegate.h"
#import "MEGAChatCallDelegate.h"
#import "MEGAChatVideoDelegate.h"
#import "MEGAChatNotificationDelegate.h"
#import "MEGAChatNodeHistoryDelegate.h"
#import "MEGAChatLogLevel.h"
#import "MEGAChatScheduledMeetingDelegate.h"
#import "ListenerDispatch.h"

#import "MEGASdk.h"

typedef NS_ENUM (NSInteger, MEGAChatStatus) {
    MEGAChatStatusOffline = 1,
    MEGAChatStatusAway    = 2,
    MEGAChatStatusOnline  = 3,
    MEGAChatStatusBusy    = 4,
    MEGAChatStatusInvalid = 15
};

typedef NS_ENUM (NSInteger, MEGAChatSource) {
    MEGAChatSourceError = -1,
    MEGAChatSourceNone  = 0,
    MEGAChatSourceLocal,
    MEGAChatSourceRemote
};

typedef NS_ENUM (NSInteger, MEGAChatInit) {
    MEGAChatInitError             = -1,
    MEGAChatInitNotDone           = 0,
    MEGAChatInitWaitingNewSession = 1,
    MEGAChatInitOfflineSession    = 2,
    MEGAChatInitOnlineSession     = 3,
    MEGAChatInitAnonymous         = 4,
    MEGAChatInitNoCache           = 7
};

typedef NS_ENUM (NSInteger, MEGAChatConnection) {
    MEGAChatConnectionOffline    = 0,
    MEGAChatConnectionInProgress = 1,
    MEGAChatConnectionLogging    = 2,
    MEGAChatConnectionOnline     = 3
};

typedef NS_ENUM (NSInteger, MEGAChatType) {
    MEGAChatTypeAll             = 0,
    MEGAChatTypeIndividual      = 1,
    MEGAChatTypeGroup           = 2,
    MEGAChatTypeGroupPrivate    = 3,
    MEGAChatTypeGroupPublic     = 4,
    MEGAChatTypeMeeting         = 5,
    MEGAChatTypeNonMeeting      = 6,
};

typedef NS_ENUM (NSUInteger, MEGAChatOption) {
    MEGAChatOptionEmpty             = 0,
    MEGAChatOptionSpeakRequest      = 1 << 0,
    MEGAChatOptionWaitingRoom       = 1 << 1,
    MEGAChatOptionOpenInvite        = 1 << 2
};


NS_ASSUME_NONNULL_BEGIN

@interface MEGAChatSdk : NSObject

@property (nonatomic, assign) uint64_t myUserHandle;
@property (nonatomic, readonly, nullable) NSString *myFirstname;
@property (nonatomic, readonly, nullable) NSString *myLastname;
@property (nonatomic, readonly, nullable) NSString *myFullname;
@property (nonatomic, readonly, nullable) NSString *myEmail;
@property (nonatomic, readonly, nullable) MEGAChatRoomList *chatRooms;
@property (nonatomic, readonly, nullable) MEGAChatListItemList *chatListItems;
@property (nonatomic, readonly) NSInteger unreadChats;
@property (nonatomic, readonly, nullable) MEGAChatListItemList *activeChatListItems;
@property (nonatomic, readonly, nullable) MEGAChatListItemList *inactiveChatListItems;
@property (nonatomic, readonly, nullable) MEGAChatListItemList *archivedChatListItems;
@property (nonatomic, readonly, getter=areAllChatsLoggedIn) BOOL allChatsLoggedIn;
@property (nonatomic, readonly, getter=isOnlineStatusPending) BOOL onlineStatusPending;
@property (nonatomic, readonly) NSInteger numCalls;

#pragma mark - Init

- (instancetype)init:(MEGASdk *)megaSDK;

- (MEGAChatInit)initKarereWithSid:(nullable NSString *)sid;
- (MEGAChatInit)initKarereLeanModeWithSid:(NSString *)sid;
- (void)importMessagesFromPath:(NSString *)externalDbPath delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)importMessagesFromPath:(NSString *)externalDbPath;
- (MEGAChatInit)initAnonymous;
- (void)resetClientId;
- (void)deleteMegaChatApi;

- (MEGAChatInit)initState;

- (MEGAChatConnection)chatConnectionState:(uint64_t)chatId;
- (void)retryPendingConnections;
- (void)reconnect;
- (void)refreshUrls;

#pragma mark - Logout

- (void)logoutWithDelegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)logout;
- (void)localLogoutWithDelegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)localLogout;

#pragma mark - Presence

- (void)setOnlineStatus:(MEGAChatStatus)status delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)setOnlineStatus:(MEGAChatStatus)status;
- (MEGAChatStatus)onlineStatus;

- (void)setPresenceAutoaway:(BOOL)enable timeout:(NSInteger)timeout;
- (void)setPresencePersist:(BOOL)enable;
- (void)setLastGreenVisible:(BOOL)enable delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)setLastGreenVisible:(BOOL)enable;
- (void)requestLastGreen:(uint64_t)userHandle delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)requestLastGreen:(uint64_t)userHandle;
- (BOOL)isSignalActivityRequired;
- (void)signalPresenceActivity;
- (MEGAChatPresenceConfig *)presenceConfig;

- (MEGAChatStatus)userOnlineStatus:(uint64_t)userHandle;
- (void)setBackgroundStatus:(BOOL)status delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)setBackgroundStatus:(BOOL)status;

#pragma mark - Add and remove delegates

- (void)addChatRoomDelegate:(uint64_t)chatId delegate:(id<MEGAChatRoomDelegate>)delegate;
- (void)removeChatRoomDelegate:(uint64_t)chatId delegate:(id<MEGAChatRoomDelegate>)delegate;

- (void)addChatRequestDelegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)removeChatRequestDelegate:(id<MEGAChatRequestDelegate>)delegate;

- (void)addChatDelegate:(id<MEGAChatDelegate>)delegate;
- (void)addChatDelegate:(id<MEGAChatDelegate>)delegate queueType:(ListenerQueueType)queueType;
- (void)removeChatDelegate:(id<MEGAChatDelegate>)delegate;

- (void)addChatNotificationDelegate:(id<MEGAChatNotificationDelegate>)delegate;
- (void)removeChatNotificationDelegate:(id<MEGAChatNotificationDelegate>)delegate;

- (void)addChatScheduledMeetingDelegate:(id<MEGAChatScheduledMeetingDelegate>)delegate;
- (void)addChatScheduledMeetingDelegate:(id<MEGAChatScheduledMeetingDelegate>)delegate queueType:(ListenerQueueType)queueType;
- (void)removeChatScheduledMeetingDelegate:(id<MEGAChatScheduledMeetingDelegate>)delegate;

#ifndef KARERE_DISABLE_WEBRTC

- (void)addChatCallDelegate:(id<MEGAChatCallDelegate>)delegate;
- (void)removeChatCallDelegate:(id<MEGAChatCallDelegate>)delegate;

- (void)addChatLocalVideo:(uint64_t)chatId delegate:(id<MEGAChatVideoDelegate>)delegate;
- (void)removeChatLocalVideo:(uint64_t)chatId delegate:(id<MEGAChatVideoDelegate>)delegate;

- (void)addChatRemoteVideo:(uint64_t)chatId cliendId:(uint64_t)clientId hiRes:(BOOL)hiRes delegate:(id<MEGAChatVideoDelegate>)delegate;
- (void)removeChatRemoteVideo:(uint64_t)chatId cliendId:(uint64_t)clientId hiRes:(BOOL)hiRes delegate:(id<MEGAChatVideoDelegate>)delegate;

#endif

#pragma mark - Chat rooms and chat list items

- (nullable MEGAChatRoom *)chatRoomForChatId:(uint64_t)chatId;
- (nullable MEGAChatRoom *)chatRoomByUser:(uint64_t)userHandle;
- (nullable MEGAChatRoomList *)chatRoomsByType:(MEGAChatType)type;
- (nullable MEGAChatListItemList *)chatListItemsByType:(MEGAChatType)type;

- (nullable MEGAChatListItem *)chatListItemForChatId:(uint64_t)chatId;

- (uint64_t)chatIdByUserHandle:(uint64_t)userHandle;

#pragma mark - Users attributes

- (void)userEmailByUserHandle:(uint64_t)userHandle delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)userEmailByUserHandle:(uint64_t)userHandle;
- (void)userFirstnameByUserHandle:(uint64_t)userHandle authorizationToken:(NSString *)authorizationToken delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)userFirstnameByUserHandle:(uint64_t)userHandle authorizationToken:(NSString *)authorizationToken;
- (void)userLastnameByUserHandle:(uint64_t)userHandle authorizationToken:(NSString *)authorizationToken delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)userLastnameByUserHandle:(uint64_t)userHandle authorizationToken:(NSString *)authorizationToken;

- (nullable NSString *)userEmailFromCacheByUserHandle:(uint64_t)userHandle;
- (nullable NSString *)userFirstnameFromCacheByUserHandle:(uint64_t)userHandle;
- (nullable NSString *)userLastnameFromCacheByUserHandle:(uint64_t)userHandle;
- (nullable NSString *)userFullnameFromCacheByUserHandle:(uint64_t)userHandle;

- (nullable NSString *)contactEmailByHandle:(uint64_t)userHandle;
- (uint64_t)userHandleByEmail:(NSString *)email;

- (void)loadUserAttributesForChatId:(uint64_t)chatId usersHandles:(NSArray<NSNumber *> *)usersHandles delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)loadUserAttributesForChatId:(uint64_t)chatId
                       usersHandles:(NSArray<NSNumber *> *)usersHandles
                           delegate:(id<MEGAChatRequestDelegate>)delegate
                          queueType:(ListenerQueueType)queueType;
- (void)loadUserAttributesForChatId:(uint64_t)chatId usersHandles:(NSArray<NSNumber *> *)usersHandles;

#pragma mark - Chat management

- (void)createChatGroup:(BOOL)group peers:(MEGAChatPeerList *)peers delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)createChatGroup:(BOOL)group peers:(MEGAChatPeerList *)peers;
- (void)createChatGroup:(BOOL)group peers:(MEGAChatPeerList *)peers title:(nullable NSString *)title delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)createChatGroup:(BOOL)group peers:(MEGAChatPeerList *)peers title:(nullable NSString *)title;

/**
 * @brief Creates a group chat for one or more participants, allowing you to specify their permissions and creation chat options
 *
 * The creator of the chat will have moderator level privilege and should not be included in the
 * list of peers.
 *
 * Valid data in the MegaChatRequest object received on callbacks:
 * - [MegaChatRequest flag] - Returns if the new chat is a group chat or permanent chat
 * - [MegaChatRequest privilege] - Returns zero (private mode)
 * - [MegaChatRequest megaChatPeerList] - List of participants and their privilege level
 * - [MegaChatRequest text] - Returns the title of the chat.
 * - [MegaChatRequest paramType] - Returns the values of params speakRequest, waitingRoom, openInvite in a bitmask.
 *  + To check if speakRequest was true you need to call [MEGAChatSdk hasChatOptionEnabledForChatOption:chatOptionsBitMask:] with option as MEGAChatOptionSpeakRequest
 *  + To check if waitingRoom was true you need to call [MEGAChatSdk hasChatOptionEnabledForChatOption:chatOptionsBitMask:] with option as MEGAChatOptionWaitingRoom
 *  + To check if openInvite was true you need to call [MEGAChatSdk hasChatOptionEnabledForChatOption:chatOptionsBitMask:] with option as MEGAChatOptionOpenInvite
 *
 * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
 * is MEGAChatErrorTypeOk:
 * - [MegaChatRequest chatHandle] - Returns the handle of the new chatroom
 *
 * On the onRequestFinish error, the error code associated to the MegaChatError can be:
 * - MEGAChatErrorTypeNoEnt  - If the target user is the same user as caller
 * - MEGAChatErrorTypeAccess - If the target is not actually contact of the user.
 *
 * @note If you are trying to create a chat with more than 1 other person, then it will be forced
 * to be a group chat.
 *
 * @note If peers list contains only one person, group chat is not set and a permament chat already
 * exists with that person, then this call will return the information for the existing chat, rather
 * than a new chat.
 *
 * @param peers MegaChatPeerList including other users and their privilege level
 * @param title Null-terminated string with the chat title. If the title
 * is longer than 30 characters, it will be truncated to that maximum length.
 * @param speakRequest YES to set that during calls non moderator users, must request permission to speak
 * @param waitingRoom YES to set that during calls, non moderator members will be placed into a waiting room.
 * A moderator user must grant each user access to the call.
 * @param openInvite to set that users with MEGAChatRoomPrivilegeStandard privilege, can invite other users into the chat
 * @param delegate MEGAChatRequestDelegate to track this request
 */
- (void)createChatGroupWithPeers:(MEGAChatPeerList *)peers
                           title:(nullable NSString *)title
                    speakRequest:(BOOL)speakRequest
                     waitingRoom:(BOOL)waitingRoom
                      openInvite:(BOOL)openInvite
                        delegate:(id<MEGAChatRequestDelegate>)delegate;

/**
 * @brief Creates an public chatroom for multiple participants (groupchat) allowing you to specify creation chat options
 *
 * This function allows to create public chats, where the moderator can create chat links to share
 * the access to the chatroom via a URL (chat-link). In order to create a public chat-link, the
 * moderator can create/get a public handle for the chatroom and generate a URL by using
 * \c [MEGAChatSDK createChatLink]. The chat-link can be deleted at any time by any moderator
 * by using \c [MEGAChatSDK removeChatLink].
 *
 * The chatroom remains in the public mode until a moderator calls \c [MEGAChatSDK setPublicChatToPrivate].
 *
 * Any user can preview the chatroom thanks to the chat-link by using \c [MEGAChatSDK openChatPreview].
 * Any user can join the chatroom thanks to the chat-link by using \c [MEGAChatSDK autojoinPublicChat].
 *
 * Valid data in the MegaChatRequest object received on callbacks:
 * - [MegaChatRequest flag] - Returns if the new chat is a group chat or permanent chat
 * - [MegaChatRequest privilege] - Returns zero (private mode)
 * - [MegaChatRequest megaChatPeerList] - List of participants and their privilege level
 * - [MegaChatRequest text] - Returns the title of the chat.
 * - [MegaChatRequest paramType] - Returns the values of params speakRequest, waitingRoom, openInvite in a bitmask.
 *  + To check if speakRequest was true you need to call [MEGAChatSdk hasChatOptionEnabledForChatOption:chatOptionsBitMask:] with option as MEGAChatOptionSpeakRequest
 *  + To check if waitingRoom was true you need to call [MEGAChatSdk hasChatOptionEnabledForChatOption:chatOptionsBitMask:] with option as MEGAChatOptionWaitingRoom
 *  + To check if openInvite was true you need to call [MEGAChatSdk hasChatOptionEnabledForChatOption:chatOptionsBitMask:] with option as MEGAChatOptionOpenInvite
 *
 * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
 * is MEGAChatErrorTypeOk:
 * - [MegaChatRequest chatHandle] - Returns the handle of the new chatroom
 *
 * On the onRequestFinish error, the error code associated to the MegaChatError can be:
 * - MEGAChatErrorTypeNoEnt  - If the target user is the same user as caller
 * - MEGAChatErrorTypeAccess - If the target is not actually contact of the user.
 *
 * @param peers MegaChatPeerList including other users and their privilege level
 * @param title Null-terminated character string with the chat title. If the title
 * is longer than 30 characters, it will be truncated to that maximum length.
 * @param speakRequest True to set that during calls non moderator users, must request permission to speak
 * @param waitingRoom True to set that during calls, non moderator members will be placed into a waiting room.
 * A moderator user must grant each user access to the call.
 * @param openInvite to set that users with MEGAChatRoomPrivilegeStandard privilege, can invite other users into the chat
 * @param delegate MEGAChatRequestDelegate to track this request
 */
- (void)createPublicChatWithPeers:(MEGAChatPeerList *)peers
                            title:(nullable NSString *)title
                     speakRequest:(BOOL)speakRequest
                      waitingRoom:(BOOL)waitingRoom
                       openInvite:(BOOL)openInvite
                         delegate:(id<MEGAChatRequestDelegate>)delegate;

- (void)createPublicChatWithPeers:(MEGAChatPeerList *)peers title:(nullable NSString *)title delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)createPublicChatWithPeers:(MEGAChatPeerList *)peers title:(nullable NSString *)title;
- (void)createMeetingWithTitle:(NSString *)title delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)createMeetingWithTitle:(NSString *)title;

/**
 * @brief Creates a meeting
 *
 * * This function allows to create public chats, where the moderator can create chat links to share
 * the access to the chatroom via a URL (chat-link). In order to create a public chat-link, the
 * moderator can create/get a public handle for the chatroom and generate a URL by using
 * \c [MEGAChatSDK createChatLink]. The chat-link can be deleted at any time by any moderator
 * by using \c [MEGAChatSDK removeChatLink].
 *
 *
 * The chatroom remains in the public mode until a moderator calls \c [MEGAChatSDK setPublicChatToPrivate].
 *
 * Any user can preview the chatroom thanks to the chat-link by using \c [MEGAChatSDK openChatPreview].
 * Any user can join the chatroom thanks to the chat-link by using \c [MEGAChatSDK autojoinPublicChat].
 *
 * Valid data in the MegaChatRequest object received on callbacks:
 * - [MegaChatRequest flag] - Returns if the new chat is a group chat or permanent chat
 * - [MegaChatRequest privilege] - Returns zero (private mode)
 * - [MegaChatRequest megaChatPeerList] - List of participants and their privilege level
 * - [MegaChatRequest text] - Returns the title of the chat.
 * - [MegaChatRequest paramType] - Returns the values of params speakRequest, waitingRoom, openInvite in a bitmask.
 *  + To check if speakRequest was true you need to call [MEGAChatSdk hasChatOptionEnabledForChatOption:chatOptionsBitMask:] with option as MEGAChatOptionSpeakRequest
 *  + To check if waitingRoom was true you need to call [MEGAChatSdk hasChatOptionEnabledForChatOption:chatOptionsBitMask:] with option as MEGAChatOptionWaitingRoom
 *  + To check if openInvite was true you need to call [MEGAChatSdk hasChatOptionEnabledForChatOption:chatOptionsBitMask:] with option as MEGAChatOptionOpenInvite
 *
 * Valid data in the MegaChatRequest object received in onRequestFinish when the error code
 * is MEGAChatErrorTypeOk:
 * - [MegaChatRequest chatHandle] - Returns the handle of the new chatroom
 *
 * On the onRequestFinish error, the error code associated to the MegaChatError can be:
 * - MEGAChatErrorTypeNoEnt  - If the target user is the same user as caller
 * - MEGAChatErrorTypeAccess - If the target is not actually contact of the user.
 *
 * @param title Null-terminated character string with the chat title. If the title
 * is longer than 30 characters, it will be truncated to that maximum length.
 * @param speakRequest True to set that during calls non moderator users, must request permission to speak
 * @param waitingRoom True to set that during calls, non moderator members will be placed into a waiting room.
 * A moderator user must grant each user access to the call.
 * @param openInvite to set that users with MEGAChatRoomPrivilegeStandard privilege, can invite other users into the chat
 * @param delegate MEGAChatRequestDelegate to track this request
 */
- (void)createMeetingWithTitle:(NSString *)title
                  speakRequest:(BOOL)speakRequest
                   waitingRoom:(BOOL)waitingRoom
                    openInvite:(BOOL)openInvite
                      delegate:(id<MEGAChatRequestDelegate>)delegate;

- (void)queryChatLink:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)queryChatLink:(uint64_t)chatId;
- (void)createChatLink:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)createChatLink:(uint64_t)chatId;

- (void)inviteToChat:(uint64_t)chatId user:(uint64_t)userHandle privilege:(NSInteger)privilege
            delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)inviteToChat:(uint64_t)chatId user:(uint64_t)userHandle privilege:(NSInteger)privilege;

- (void)autojoinPublicChat:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)autojoinPublicChat:(uint64_t)chatId;
- (void)autorejoinPublicChat:(uint64_t)chatId publicHandle:(uint64_t)publicHandle delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)autorejoinPublicChat:(uint64_t)chatId publicHandle:(uint64_t)publicHandle;

- (void)removeFromChat:(uint64_t)chatId userHandle:(uint64_t)userHandle
              delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)removeFromChat:(uint64_t)chatId userHandle:(uint64_t)userHandle;

- (void)leaveChat:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)leaveChat:(uint64_t)chatId;

- (void)updateChatPermissions:(uint64_t)chatId userHandle:(uint64_t)userHandle privilege:(NSInteger)privilege
                     delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)updateChatPermissions:(uint64_t)chatId userHandle:(uint64_t)userHandle privilege:(NSInteger)privilege;

- (void)truncateChat:(uint64_t)chatId messageId:(uint64_t)messageId delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)truncateChat:(uint64_t)chatId messageId:(uint64_t)messageId;

- (void)clearChatHistory:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)clearChatHistory:(uint64_t)chatId;

- (void)setChatTitle:(uint64_t)chatId title:(NSString *)title delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)setChatTitle:(uint64_t)chatId title:(NSString *)title;

- (void)openChatPreview:(NSURL *)link delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)openChatPreview:(NSURL *)link;

- (void)checkChatLink:(NSURL *)link delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)checkChatLink:(NSURL *)link;

- (void)setPublicChatToPrivate:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)setPublicChatToPrivate:(uint64_t)chatId;

- (void)removeChatLink:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)removeChatLink:(uint64_t)chatId;

- (void)archiveChat:(uint64_t)chatId archive:(BOOL)archive delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)archiveChat:(uint64_t)chatId archive:(BOOL)archive;

- (void)setChatRetentionTime:(uint64_t)chatID period:(NSUInteger)period delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)setChatRetentionTime:(uint64_t)chatID period:(NSUInteger)period;

- (BOOL)openChatRoom:(uint64_t)chatId delegate:(id<MEGAChatRoomDelegate>)delegate;

- (void)closeChatRoom:(uint64_t)chatId delegate:(id<MEGAChatRoomDelegate>)delegate;

- (void)closeChatPreview:(uint64_t)chatId;

- (MEGAChatSource)loadMessagesForChat:(uint64_t)chatId count:(NSInteger)count;
- (BOOL)isFullHistoryLoadedForChat:(uint64_t)chatId;

- (nullable MEGAChatMessage *)messageForChat:(uint64_t)chatId messageId:(uint64_t)messageId;
- (nullable MEGAChatMessage *)messageFromNodeHistoryForChat:(uint64_t)chatId messageId:(uint64_t)messageId;
- (nullable MEGAChatMessage *)sendMessageToChat:(uint64_t)chatId message:(NSString *)message;
- (nullable MEGAChatMessage *)attachContactsToChat:(uint64_t)chatId contacts:(NSArray *)contacts;
- (nullable MEGAChatMessage *)forwardContactFromChat:(uint64_t)sourceChatId messageId:(uint64_t)messageId targetChatId:(uint64_t)targetChatId;
- (void)attachNodesToChat:(uint64_t)chatId nodes:(NSArray *)nodesArray delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)attachNodesToChat:(uint64_t)chatId nodes:(NSArray *)nodesArray;
- (nullable MEGAChatMessage *)sendGiphyToChat:(uint64_t)chatId srcMp4:(NSString *)srcMp4 srcWebp:(NSString *)srcWebp sizeMp4:(uint64_t)sizeMp4 sizeWebp:(uint64_t)sizeWebp  width:(int)width height:(int)height title:(NSString *)title;
- (nullable MEGAChatMessage *)sendGeolocationToChat:(uint64_t)chatId longitude:(float)longitude latitude:(float)latitude image:(nullable NSString *)image;
- (nullable MEGAChatMessage *)editGeolocationForChat:(uint64_t)chatId messageId:(uint64_t)messageId longitude:(float)longitude latitude:(float)latitude image:(nullable NSString *)image;
- (void)revokeAttachmentToChat:(uint64_t)chatId node:(uint64_t)nodeHandle delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)revokeAttachmentToChat:(uint64_t)chatId node:(uint64_t)nodeHandle;
- (void)attachNodeToChat:(uint64_t)chatId node:(uint64_t)nodeHandle delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)attachNodeToChat:(uint64_t)chatId node:(uint64_t)nodeHandle;
- (nullable MEGAChatMessage *)revokeAttachmentMessageForChat:(uint64_t)chatId messageId:(uint64_t)messageId;
- (BOOL)isRevokedNode:(uint64_t)nodeHandle inChat:(uint64_t)chatId;
- (void)attachVoiceMessageToChat:(uint64_t)chatId node:(uint64_t)nodeHandle delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)attachVoiceMessageToChat:(uint64_t)chatId node:(uint64_t)nodeHandle;
- (nullable MEGAChatMessage *)editMessageForChat:(uint64_t)chatId messageId:(uint64_t)messageId message:(NSString *)message;
- (nullable MEGAChatMessage *)deleteMessageForChat:(uint64_t)chatId messageId:(uint64_t)messageId;
- (nullable MEGAChatMessage *)removeRichLinkForChat:(uint64_t)chatId messageId:(uint64_t)messageId;
- (BOOL)setMessageSeenForChat:(uint64_t)chatId messageId:(uint64_t)messageId;
- (nullable MEGAChatMessage *)lastChatMessageSeenForChat:(uint64_t)chatId;
- (void)removeUnsentMessageForChat:(uint64_t)chatId rowId:(uint64_t)rowId;

- (void)addReactionForChat:(uint64_t)chatId messageId:(uint64_t)messageId reaction:(NSString *)reaction;
- (void)deleteReactionForChat:(uint64_t)chatId messageId:(uint64_t)messageId reaction:(NSString *)reaction;
- (NSInteger)messageReactionCountForChat:(uint64_t)chatId messageId:(uint64_t)messageId reaction:(NSString *)reaction;
- (nullable MEGAStringList *)messageReactionsForChat:(uint64_t)chatId messageId:(uint64_t)messageId;
- (nullable MEGAHandleList *)reactionUsersForChat:(uint64_t)chatId messageId:(uint64_t)messageId reaction:(NSString *)reaction;
- (void)setPublicKeyPinning:(BOOL)enable;

- (void)sendTypingNotificationForChat:(uint64_t)chatId;
- (void)sendStopTypingNotificationForChat:(uint64_t)chatId;
- (void)saveCurrentState;
- (void)pushReceivedWithBeep:(BOOL)beep delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)pushReceivedWithBeep:(BOOL)beep;
- (void)pushReceivedWithBeep:(BOOL)beep chatId:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)pushReceivedWithBeep:(BOOL)beep chatId:(uint64_t)chatId;

/**
 * @brief Allows to enable/disable the open invite option for a chat room
 *
 * The open invite option allows users with MEGAChatRoomPrivilegeStandard privilege, to invite other users into the chat
 *
 * Valid data in the MegaChatRequest object received on callbacks:
 * - [MegaChatRequest chatHandle] - Returns the handle of the chatroom
 * - [MegaChatRequest privilege] - Returns MEGAChatOptionOpenInvite
 * - [MegaChatRequest flag] - Returns YES if enabled was set YES, otherwise it will return false
 *
 * On the onRequestFinish error, the error code associated to the MegaChatError can be:
 * - MEGAChatErrorTypeNoEnt - If the chatroom does not exists or the chatid is invalid.
 * - MEGAChatErrorTypeArgs - If the chatroom is a 1on1 chat
 * - MEGAChatErrorTypeAccess - If the caller is not an operator.
 * - MegaChatErrorTypeExist - If the value of enabled is the same as open invite option
 *
 * @param chatId MegaChatHandle that identifies the chat room
 * @param enabled YES if we want to enable open invite option, otherwise false.
 */
- (void)openInvite:(BOOL)enabled chatId:(uint64_t)chatId;

/**
 * @brief Allows to enable/disable the open invite option for a chat room
 *
 * The open invite option allows users with MEGAChatRoomPrivilegeStandard privilege, to invite other users into the chat
 *
 * Valid data in the MegaChatRequest object received on callbacks:
 * - [MegaChatRequest chatHandle] - Returns the handle of the chatroom
 * - [MegaChatRequest privilege] - Returns MEGAChatOptionOpenInvite
 * - [MegaChatRequest flag] - Returns YES if enabled was set YES, otherwise it will return false
 *
 * On the onRequestFinish error, the error code associated to the MegaChatError can be:
 * - MEGAChatErrorTypeNoEnt - If the chatroom does not exists or the chatid is invalid.
 * - MEGAChatErrorTypeArgs - If the chatroom is a 1on1 chat
 * - MEGAChatErrorTypeAccess - If the caller is not an operator.
 * - MegaChatErrorTypeExist - If the value of enabled is the same as open invite option
 *
 * @param chatId MegaChatHandle that identifies the chat room
 * @param enabled YES if we want to enable open invite option, otherwise false.
 * @param delegate MegaChatRequestListener to track this request
 */
- (void)openInvite:(BOOL)enabled chatId:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate;

/**
 * @brief Checks if a chat option is enabled in a bitmask
 *
 * Valid values for option are:
 * - MEGAChatOptionSpeakRequest
 * - MEGAChatOptionWaitingRoom
 * - MEGAChatOptionOpenInvite
 *
 * @param option Option to check if it's enabled in a bitmask
 * @param chatOptionsBitMask Bitmask that represents a set of chat options
 * @return YES if specified option is enabled in the bitmask
 */
- (BOOL)hasChatOptionEnabledForChatOption:(MEGAChatOption)option chatOptionsBitMask:(NSInteger)chatOptionsBitMask;

#pragma mark - Scheduled meetings

- (void)createChatAndScheduledMeeting:(BOOL)isMeeting publicChat:(BOOL)publicChat speakRequest:(BOOL)speakRequest waitingRoom:(BOOL)waitingRoom openInvite:(BOOL)openInvite timezone:(NSString *)timezone startDate:(uint64_t)startDate endDate:(uint64_t)endDate title:(NSString *)title description:(NSString *)description emailsDisabled:(BOOL)emailsDisabled frequency:(int)frequency attributes:(NSString *)attributes;

- (void)createChatAndScheduledMeeting:(BOOL)isMeeting publicChat:(BOOL)publicChat speakRequest:(BOOL)speakRequest waitingRoom:(BOOL)waitingRoom openInvite:(BOOL)openInvite timezone:(NSString *)timezone startDate:(uint64_t)startDate endDate:(uint64_t)endDate title:(NSString *)title description:(NSString *)description emailsDisabled:(BOOL)emailsDisabled frequency:(int)frequency attributes:(NSString *)attributes delegate:(id<MEGAChatRequestDelegate>)delegate;

- (void)updateScheduledMeeting:(uint64_t)chatId scheduledId:(uint64_t)scheduledId timezone:(NSString *)timezone startDate:(uint64_t)startDate endDate:(uint64_t)endDate title:(NSString *)title description:(NSString *)description emailsDisabled:(BOOL)emailsDisabled frequency:(int)frequency attributes:(NSString *)attributes;
- (void)updateScheduledMeeting:(uint64_t)chatId scheduledId:(uint64_t)scheduledId timezone:(NSString *)timezone startDate:(uint64_t)startDate endDate:(uint64_t)endDate title:(NSString *)title description:(NSString *)description emailsDisabled:(BOOL)emailsDisabled frequency:(int)frequency attributes:(NSString *)attributes delegate:(id<MEGAChatRequestDelegate>)delegate;

- (void)updateScheduledMeetingOccurrence:(uint64_t)chatId  scheduledId:(uint64_t)scheduledId overrides:(uint64_t)overrides newStartDate:(uint64_t)newStartDate newEndDate:(uint64_t)newEndDate newCancelled:(BOOL)newCancelled;

- (void)updateScheduledMeetingOccurrence:(uint64_t)chatId  scheduledId:(uint64_t)scheduledId overrides:(uint64_t)overrides newStartDate:(uint64_t)newStartDate newEndDate:(uint64_t)newEndDate newCancelled:(BOOL)newCancelled delegate:(id<MEGAChatRequestDelegate>)delegate;

- (void)removeScheduledMeeting:(uint64_t)chatId scheduledId:(uint64_t)scheduledId;

- (void)removeScheduledMeeting:(uint64_t)chatId scheduledId:(uint64_t)scheduledId delegate:(id<MEGAChatRequestDelegate>)delegate;

- (NSArray<MEGAChatScheduledMeeting *> *)scheduledMeetingsByChat:(uint64_t)chatId;

- (MEGAChatScheduledMeeting *)scheduledMeeting:(uint64_t)chatId scheduledId:(uint64_t)scheduledId;

- (NSArray<MEGAChatScheduledMeeting *> *)getAllScheduledMeetings;

- (void)fetchScheduledMeetingOccurrencesByChat:(uint64_t)chatId;

- (void)fetchScheduledMeetingOccurrencesByChat:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate;

#pragma mark - Audio and video calls

#ifndef KARERE_DISABLE_WEBRTC

- (MEGAStringList *)chatVideoInDevices;
- (void)setChatVideoInDevices:(NSString *)devices;
- (void)setChatVideoInDevices:(NSString *)devices delegate:(id<MEGAChatRequestDelegate>)delegate;
- (NSString *)videoDeviceSelected;
- (void)startChatCall:(uint64_t)chatId enableVideo:(BOOL)enableVideo enableAudio:(BOOL)enableAudio delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)startChatCall:(uint64_t)chatId enableVideo:(BOOL)enableVideo enableAudio:(BOOL)enableAudio;
- (void)startChatCallNoRinging:(uint64_t)chatId scheduledId:(uint64_t)scheduledId enableVideo:(BOOL)enableVideo enableAudio:(BOOL)enableAudio delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)startChatCallNoRinging:(uint64_t)chatId scheduledId:(uint64_t)scheduledId enableVideo:(BOOL)enableVideo enableAudio:(BOOL)enableAudio;
- (void)answerChatCall:(uint64_t)chatId enableVideo:(BOOL)enableVideo enableAudio:(BOOL)enableAudio delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)answerChatCall:(uint64_t)chatId enableVideo:(BOOL)enableVideo;
- (void)hangChatCall:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)hangChatCall:(uint64_t)callId;
- (void)endChatCall:(uint64_t)callId;
- (void)enableAudioForChat:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)enableAudioForChat:(uint64_t)chatId;
- (void)disableAudioForChat:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)disableAudioForChat:(uint64_t)chatId;
- (void)enableVideoForChat:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)enableVideoForChat:(uint64_t)chatId;
- (void)disableVideoForChat:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)disableVideoForChat:(uint64_t)chatId;
- (void)setCallOnHoldForChat:(uint64_t)chatId onHold:(BOOL)onHold delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)setCallOnHoldForChat:(uint64_t)chatId onHold:(BOOL)onHold;
- (void)openVideoDevice;
- (void)openVideoDeviceWithDelegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)releaseVideoDevice;
- (void)releaseVideoDeviceWithDelegate:(id<MEGAChatRequestDelegate>)delegate;
- (nullable MEGAChatCall *)chatCallForCallId:(uint64_t)callId;
- (nullable MEGAChatCall *)chatCallForChatId:(uint64_t)chatId;
- (nullable MEGAHandleList *)chatCallsWithState:(MEGAChatCallStatus)callState;
- (nullable MEGAHandleList *)chatCallsIds;
- (BOOL)hasCallInChatRoom:(uint64_t)chatId;
- (NSInteger)getMaxVideoCallParticipants;
- (NSInteger)getMaxCallParticipants;
- (uint64_t)myClientIdHandleForChatId:(uint64_t)chatId;
- (BOOL)isAudioLevelMonitorEnabledForChatId:(uint64_t)chatId;
- (void)enableAudioMonitor:(BOOL)enable chatId:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)enableAudioMonitor:(BOOL)enable chatId:(uint64_t)chatId;
- (void)requestHiResVideoForChatId:(uint64_t)chatId clientId:(uint64_t)clientId delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)stopHiResVideoForChatId:(uint64_t)chatId clientIds:(NSArray<NSNumber *> *)clientIds delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)requestLowResVideoForChatId:(uint64_t)chatId clientIds:(NSArray<NSNumber *> *)clientIds delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)stopLowResVideoForChatId:(uint64_t)chatId clientIds:(NSArray<NSNumber *> *)clientIds delegate:(id<MEGAChatRequestDelegate>)delegate;
#endif

#pragma mark - Debug log messages

+ (void)setLogLevel:(MEGAChatLogLevel)level;
+ (void)setLogToConsole:(BOOL)enable;
+ (void)setLogObject:(nullable id<MEGAChatLoggerDelegate>)delegate;
+ (void)setLogWithColors:(BOOL)userColors;

#pragma mark - Exceptions

+ (void)setCatchException:(BOOL)enable;

#pragma mark - Rich links

+ (BOOL)hasUrl:(NSString *)text;

#pragma mark - Node history

- (BOOL)openNodeHistoryForChat:(uint64_t)chatId delegate:(id<MEGAChatNodeHistoryDelegate>)delegate;
- (BOOL)closeNodeHistoryForChat:(uint64_t)chatId delegate:(id<MEGAChatNodeHistoryDelegate>)delegate;
- (void)addNodeHistoryDelegate:(uint64_t)chatId delegate:(id<MEGAChatNodeHistoryDelegate>)delegate;
- (void)removeNodeHistoryDelegate:(uint64_t)chatId delegate:(id<MEGAChatNodeHistoryDelegate>)delegate;
- (MEGAChatSource)loadAttachmentsForChat:(uint64_t)chatId count:(NSInteger)count;

#pragma mark - Enumeration to NSString

+ (nullable NSString *)stringForMEGAChatInitState:(MEGAChatInit)initState;

@end

NS_ASSUME_NONNULL_END
