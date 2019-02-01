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


#import "MEGASdk.h"

typedef NS_ENUM (NSInteger, MEGAChatLogLevel) {
    MEGAChatLogLevelFatal = 0,
    MEGAChatLogLevelError,
    MEGAChatLogLevelWarning,
    MEGAChatLogLevelInfo,
    MEGAChatLogLevelVerbose,
    MEGAChatLogLevelDebug,
    MEGAChatLogLevelMax
};

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
    MEGAChatInitNoCache           = 7
};

typedef NS_ENUM (NSInteger, MEGAChatConnection) {
    MEGAChatConnectionOffline    = 0,
    MEGAChatConnectionInProgress = 1,
    MEGAChatConnectionLogging    = 2,
    MEGAChatConnectionOnline     = 3
};

@interface MEGAChatSdk : NSObject

@property (nonatomic, assign) uint64_t myUserHandle;
@property (nonatomic, readonly) NSString *myFirstname;
@property (nonatomic, readonly) NSString *myLastname;
@property (nonatomic, readonly) NSString *myFullname;
@property (nonatomic, readonly) NSString *myEmail;
@property (nonatomic, readonly) MEGAChatRoomList *chatRooms;
@property (nonatomic, readonly) MEGAChatListItemList *chatListItems;
@property (nonatomic, readonly) NSInteger unreadChats;
@property (nonatomic, readonly) MEGAChatListItemList *activeChatListItems;
@property (nonatomic, readonly) MEGAChatListItemList *inactiveChatListItems;
@property (nonatomic, readonly) MEGAChatListItemList *archivedChatListItems;
@property (nonatomic, readonly, getter=areAllChatsLoggedIn) BOOL allChatsLoggedIn;
@property (nonatomic, readonly, getter=isOnlineStatusPending) BOOL onlineStatusPending;

#pragma mark - Init

- (instancetype)init:(MEGASdk *)megaSDK;

- (MEGAChatInit)initKarereWithSid:(NSString *)sid;

- (MEGAChatInit)initState;

- (void)connectWithDelegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)connect;

- (void)connectInBackgroundWithDelegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)connectInBackground;

- (void)disconnectWithDelegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)disconnect;
- (MEGAChatConnection)chatConnectionState:(uint64_t)chatId;
- (void)retryPendingConnections;
- (void)reconnect;

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
- (void)removeChatDelegate:(id<MEGAChatDelegate>)delegate;

- (void)addChatNotificationDelegate:(id<MEGAChatNotificationDelegate>)delegate;
- (void)removeChatNotificationDelegate:(id<MEGAChatNotificationDelegate>)delegate;

#ifndef KARERE_DISABLE_WEBRTC

- (void)addChatCallDelegate:(id<MEGAChatCallDelegate>)delegate;
- (void)removeChatCallDelegate:(id<MEGAChatCallDelegate>)delegate;

- (void)addChatLocalVideo:(uint64_t)chatId delegate:(id<MEGAChatVideoDelegate>)delegate;
- (void)removeChatLocalVideo:(uint64_t)chatId delegate:(id<MEGAChatVideoDelegate>)delegate;

- (void)addChatRemoteVideo:(uint64_t)chatId peerId:(uint64_t)peerId delegate:(id<MEGAChatVideoDelegate>)delegate;
- (void)removeChatRemoteVideo:(uint64_t)chatId peerId:(uint64_t)peerId delegate:(id<MEGAChatVideoDelegate>)delegate;

#endif

#pragma mark - Chat rooms and chat list items

- (MEGAChatRoom *)chatRoomForChatId:(uint64_t)chatId;
- (MEGAChatRoom *)chatRoomByUser:(uint64_t)userHandle;

- (MEGAChatListItem *)chatListItemForChatId:(uint64_t)chatId;

- (uint64_t)chatIdByUserHandle:(uint64_t)userHandle;

#pragma mark - Users attributes

- (void)userEmailByUserHandle:(uint64_t)userHandle delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)userEmailByUserHandle:(uint64_t)userHandle;
- (void)userFirstnameByUserHandle:(uint64_t)userHandle delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)userFirstnameByUserHandle:(uint64_t)userHandle;
- (void)userLastnameByUserHandle:(uint64_t)userHandle delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)userLastnameByUserHandle:(uint64_t)userHandle;

- (NSString *)contacEmailByHandle:(uint64_t)userHandle;
- (uint64_t)userHandleByEmail:(NSString *)email;

#pragma mark - Chat management

- (void)createChatGroup:(BOOL)group peers:(MEGAChatPeerList *)peers delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)createChatGroup:(BOOL)group peers:(MEGAChatPeerList *)peers;

- (void)inviteToChat:(uint64_t)chatId user:(uint64_t)userHandle privilege:(NSInteger)privilege
            delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)inviteToChat:(uint64_t)chatId user:(uint64_t)userHandle privilege:(NSInteger)privilege;

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

- (void)archiveChat:(uint64_t)chatId archive:(BOOL)archive delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)archiveChat:(uint64_t)chatId archive:(BOOL)archive;

- (BOOL)openChatRoom:(uint64_t)chatId delegate:(id<MEGAChatRoomDelegate>)delegate;

- (void)closeChatRoom:(uint64_t)chatId delegate:(id<MEGAChatRoomDelegate>)delegate;

- (MEGAChatSource)loadMessagesForChat:(uint64_t)chatId count:(NSInteger)count;
- (BOOL)isFullHistoryLoadedForChat:(uint64_t)chatId;

- (MEGAChatMessage *)messageForChat:(uint64_t)chatId messageId:(uint64_t)messageId;
- (MEGAChatMessage *)messageFromNodeHistoryForChat:(uint64_t)chatId messageId:(uint64_t)messageId;
- (MEGAChatMessage *)sendMessageToChat:(uint64_t)chatId message:(NSString *)message;
- (MEGAChatMessage *)attachContactsToChat:(uint64_t)chatId contacts:(NSArray *)contacts;
- (MEGAChatMessage *)forwardContactFromChat:(uint64_t)sourceChatId messageId:(uint64_t)messageId targetChatId:(uint64_t)targetChatId;
- (void)attachNodesToChat:(uint64_t)chatId nodes:(NSArray *)nodesArray delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)attachNodesToChat:(uint64_t)chatId nodes:(NSArray *)nodesArray;
- (MEGAChatMessage *)sendGeolocationToChat:(uint64_t)chatId longitude:(float)longitude latitude:(float)latitude image:(NSString *)image;
- (MEGAChatMessage *)editGeolocationForChat:(uint64_t)chatId messageId:(uint64_t)messageId longitude:(float)longitude latitude:(float)latitude image:(NSString *)image;
- (void)revokeAttachmentToChat:(uint64_t)chatId node:(uint64_t)nodeHandle delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)revokeAttachmentToChat:(uint64_t)chatId node:(uint64_t)nodeHandle;
- (void)attachNodeToChat:(uint64_t)chatId node:(uint64_t)nodeHandle delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)attachNodeToChat:(uint64_t)chatId node:(uint64_t)nodeHandle;
- (MEGAChatMessage *)revokeAttachmentMessageForChat:(uint64_t)chatId messageId:(uint64_t)messageId;
- (BOOL)isRevokedNode:(uint64_t)nodeHandle inChat:(uint64_t)chatId;
- (void)attachVoiceMessageToChat:(uint64_t)chatId node:(uint64_t)nodeHandle delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)attachVoiceMessageToChat:(uint64_t)chatId node:(uint64_t)nodeHandle;
- (MEGAChatMessage *)editMessageForChat:(uint64_t)chatId messageId:(uint64_t)messageId message:(NSString *)message;
- (MEGAChatMessage *)deleteMessageForChat:(uint64_t)chatId messageId:(uint64_t)messageId;
- (MEGAChatMessage *)removeRichLinkForChat:(uint64_t)chatId messageId:(uint64_t)messageId;
- (BOOL)setMessageSeenForChat:(uint64_t)chatId messageId:(uint64_t)messageId;
- (MEGAChatMessage *)lastChatMessageSeenForChat:(uint64_t)chatId;
- (void)removeUnsentMessageForChat:(uint64_t)chatId rowId:(uint64_t)rowId;

- (void)sendTypingNotificationForChat:(uint64_t)chatId;
- (void)sendStopTypingNotificationForChat:(uint64_t)chatId;
- (void)saveCurrentState;
- (void)pushReceivedWithBeep:(BOOL)beep delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)pushReceivedWithBeep:(BOOL)beep;
- (void)pushReceivedWithBeep:(BOOL)beep chatId:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)pushReceivedWithBeep:(BOOL)beep chatId:(uint64_t)chatId;

#pragma mark - Audio and video calls

#ifndef KARERE_DISABLE_WEBRTC

- (MEGAStringList *)chatAudioInDevices;
- (MEGAStringList *)chatVideoInDevices;
- (BOOL)setChatAudioInDevices:(NSString *)devices;
- (BOOL)setChatVideoInDevices:(NSString *)devices;
- (void)startChatCall:(uint64_t)chatId enableVideo:(BOOL)enableVideo delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)startChatCall:(uint64_t)chatId enableVideo:(BOOL)enableVideo;
- (void)answerChatCall:(uint64_t)chatId enableVideo:(BOOL)enableVideo delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)answerChatCall:(uint64_t)chatId enableVideo:(BOOL)enableVideo;
- (void)hangChatCall:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)hangChatCall:(uint64_t)chatId;
- (void)hangAllChatCallsWithDelegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)hangAllChatCalls;
- (void)enableAudioForChat:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)enableAudioForChat:(uint64_t)chatId;
- (void)disableAudioForChat:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)disableAudioForChat:(uint64_t)chatId;
- (void)enableVideoForChat:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)enableVideoForChat:(uint64_t)chatId;
- (void)disableVideoForChat:(uint64_t)chatId delegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)disableVideoForChat:(uint64_t)chatId;
- (void)loadAudioVideoDeviceListWithDelegate:(id<MEGAChatRequestDelegate>)delegate;
- (void)loadAudioVideoDeviceList;
- (MEGAChatCall *)chatCallForCallId:(uint64_t)callId;
- (MEGAChatCall *)chatCallForChatId:(uint64_t)chatId;
@property (nonatomic, readonly) NSInteger numCalls;
- (MEGAHandleList *)chatCalls;
- (MEGAHandleList *)chatCallsIds;
- (BOOL)hasCallInChatRoom:(uint64_t)chatId;
- (void)enableGroupChatCalls:(BOOL)enable;
- (BOOL)areGroupChatCallEnabled;
- (NSInteger)getMaxVideoCallParticipants;
- (NSInteger)getMaxCallParticipants;

#endif

#pragma mark - Debug log messages

+ (void)setLogLevel:(MEGAChatLogLevel)level;
+ (void)setLogToConsole:(BOOL)enable;
+ (void)setLogObject:(id<MEGAChatLoggerDelegate>)delegate;
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
- (NSInteger)loadAttachmentsForChat:(uint64_t)chatId count:(NSInteger)count;


@end
