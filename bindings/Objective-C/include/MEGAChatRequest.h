#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM (NSInteger, MEGAChatRequestType) {
    MEGAChatRequestTypeInitialize, // (Obsolete)
    MEGAChatRequestTypeConnect, // (obsolete)
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
    MEGAChatRequestTypeLoadAudioVideoDevices, // Deprecated
    MEGAChatRequestTypeArchiveChatRoom,
    MEGAChatRequestTypePushReceived,
    MEGAChatRequestTypeSetLastGreenVisible,
    MEGAChatRequestTypeLastGreen,
    MEGAChatRequestTypeLoadPreview,
    MEGAChatRequestTypeChatLinkHandle,
    MEGAChatRequestTypeSetPrivateMode,
    MEGAChatRequestTypeAutojoinPublicChat,
    MEGAChatRequestTypeChangeVideoStream,
    MEGAChatRequestTypeImportMessages,
    MEGAChatRequestTypeSetRetentionTime,
    MEGAChatRequestTypeSetCallOnHold,
    MEGAChatRequestTypeEnableAudioLevelMonitor,
    MEGAChatRequestTypeManageReaction,
    MEGAChatRequestTypeGetPeerAttributes,
    MEGAChatRequestTypeRequestSpeak, // Deprecated
    MEGAChatRequestTypeApproveSpeak, // Deprecated
    MEGAChatRequestTypeRequestHighResVideo,
    MEGAChatRequestTypeRequestLowResVideo,
    MEGAChatRequestTypeOpenVideoDevice,
    MEGAChatRequestTypeRequestHiResQuality,
    MEGAChatRequestTypeDeleteSpeaker, // Deprecated
    MEGAChatRequestTypeRequestSvcLayers,
    MEGAChatRequestTypeSetChatRoomOptions,
    MEGAChatRequestTypeCreateScheduledMeeting, // Deprecated
    MEGAChatRequestTypeDeleteScheduledMeeting,
    MEGAChatRequestTypeFetchScheduledMeetingOccurrences,
    MEGAChatRequestTypeUpdateScheduledMeetingOcurrence,
    MEGAChatRequestTypeUpdateScheduledMeeting,
    MEGAChatRequestTypeWaitingRoomPush,
    MEGAChatRequestTypeWaitingRoomAllow,
    MEGAChatRequestTypeWaitingRoomKick,
    MEGAChatRequestTypeRingIndividualInCall,
    MEGAChatRequestTypeMute,
    MEGAChatRequestTypeSpeakerAddOrDelete,
    MEGAChatRequestTypeSpeakRequestAddOrDelete,
    MEGAChatRequestTypeRejectCall,
};

enum {
    audio = 0,
    video = 1
};

@class MEGAChatMessage;
@class MEGAChatPeerList;
@class MEGANodeList;
@class MEGAHandleList;
@class MEGAChatScheduledMeetingOccurrence;
@class MEGAChatScheduledMeeting;

@interface MEGAChatRequest : NSObject

@property (readonly, nonatomic, nullable) NSArray<MEGAChatScheduledMeeting *> *scheduledMeetingList;
@property (readonly, nonatomic, nullable) MEGAChatPeerList *megaChatPeerList;
@property (readonly, nonatomic, nullable) MEGAHandleList *megaHandleList;
@property (readonly, nonatomic, nullable) MEGAChatMessage *chatMessage;
@property (readonly, nonatomic, nullable) NSString *requestString;
@property (readonly, nonatomic, nullable) MEGANodeList *nodeList;
@property (readonly, nonatomic, nullable) NSString *text;
@property (readonly, nonatomic, nullable) NSURL *link;
@property (readonly, nonatomic, getter=isFlag) BOOL flag;
@property (readonly, nonatomic) long long number;
@property (readonly, nonatomic) MEGAChatRequestType type;
@property (readonly, nonatomic) NSInteger tag;
@property (readonly, nonatomic) uint64_t chatHandle;
@property (readonly, nonatomic) uint64_t userHandle;
@property (readonly, nonatomic) NSInteger privilege;
@property (readonly, nonatomic) NSInteger paramType;
@property (readonly, nonatomic) NSArray<MEGAChatScheduledMeetingOccurrence *> *chatScheduledMeetingOccurrences;

- (instancetype)clone;

@end

NS_ASSUME_NONNULL_END
