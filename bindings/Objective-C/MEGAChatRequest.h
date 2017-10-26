#import <Foundation/Foundation.h>

typedef NS_ENUM (NSInteger, MEGAChatRequestType) {
    MEGAChatRequestTypeInitialize, // (Obsolete)
    MEGAChatRequestTypeConnect,
    MEGAChatRequestTypeDelete,
    MEGAChatRequestTypeLogout,
    MEGAChatRequestTypeSetOnlineStatus,
    MEGAChatRequestTypeStartChatCall,
    MEGAChatRequestTypeAnswerChatCall,
    MEGAChatRequestTypeDisableAudioVideoCall,
    MEGAChatRequestTypeHangChatCall,
    MEGAChatRequestTypeCreateChatRoom,
    MEGAChatRequestTypeRemoveFromChatRoom,
    MEGAChatRequestTypeInviteToChatRoom,
    MEGAChatRequestTypeUpdatePeerPermissions,
    MEGAChatRequestTypeEditChatRoomName,
    MEGAChatRequestTypeEditChatRoomPic,
    MEGAChatRequestTypeTruncateHistory,
    MEGAChatRequestTypeShareContact,
    MEGAChatRequestTypeGetFirstname,
    MEGAChatRequestTypeGetLastname,
    MEGAChatRequestTypeDisconnect,
    MEGAChatRequestTypeGetEmail,
    MEGAChatRequestTypeNodeMessage,
    MEGAChatRequestTypeRevokeNodeMessage,
    MEGAChatRequestTypeSetBackgroundStatus,
    MEGAChatRequestTypeRetryPendingConnections,
    MEGAChatRequestTypeSendTypingNotification,
    MEGAChatRequestTypeSignalActivity,
    MEGAChatRequestTypeSetPresencePersist,
    MEGAChatRequestTypeSetPresenceAutoaway,
    MEGAChatRequestTypeLoadAudioVideoDevices
};

typedef NS_ENUM (NSInteger, MEGAChatRequest) {
    MEGAChatRequestAudio = 0,
    MEGAChatRequestVideo = 1
};

@class MEGAChatMessage;
@class MEGAChatPeerList;
@class MEGANodeList;

@interface MEGAChatRequest : NSObject

@property (readonly, nonatomic) MEGAChatRequestType type;
@property (readonly, nonatomic) NSString *requestString;
@property (readonly, nonatomic) NSInteger tag;
@property (readonly, nonatomic) long long number;
@property (readonly, nonatomic, getter=isFlag) BOOL flag;
@property (readonly, nonatomic) MEGAChatPeerList *megaChatPeerList;
@property (readonly, nonatomic) uint64_t chatHandle;
@property (readonly, nonatomic) uint64_t userHandle;
@property (readonly, nonatomic) NSInteger privilege;
@property (readonly, nonatomic) NSString *text;
@property (readonly, nonatomic) MEGAChatMessage *chatMessage;
@property (readonly, nonatomic) MEGANodeList *nodeList;
@property (readonly, nonatomic) NSInteger paramType;

- (instancetype)clone;

@end
