
#import <Foundation/Foundation.h>
#import "MEGAHandleList.h"
#import "MEGAChatSession.h"
#import "MEGAChatWaitingRoom.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM (NSInteger, MEGAChatCallStatus) {
    MEGAChatCallStatusUndefined = -1,
    MEGAChatCallStatusInitial = 0,
    MEGAChatCallStatusUserNoPresent,
    MEGAChatCallStatusConnecting,
    MEGAChatCallStatusWaitingRoom,
    MEGAChatCallStatusJoining,
    MEGAChatCallStatusInProgress,
    MEGAChatCallStatusTerminatingUserParticipation,
    MEGAChatCallStatusDestroyed
};

typedef NS_ENUM (NSInteger, MEGAChatCallSessionStatus) {
    MEGAChatCallSessionStatusInitial = 0,
    MEGAChatCallSessionStatusInProgress,
    MEGAChatCallSessionStatusDestroyed,
    MEGAChatCallSessionStatusNoSession
};

typedef NS_ENUM (NSInteger, MEGAChatCallTermCode) {
    MEGAChatCallTermCodeInvalid = -1,
    MEGAChatCallTermCodeUserHangup = 0,
    MEGAChatCallTermCodeTooManyParticipants = 1,
    MEGAChatCallTermCodeCallReject = 2,
    MEGAChatCallTermCodeError = 3,
    MEGAChatCallTermCodeNoParticipate = 4,
    MEGAChatCallTermCodeTooManyClients = 5,
    MEGAChatCallTermCodeProtocolVersion = 6,
    MEGAChatCallTermCodeKicked = 7,
    MEGAChatCallTermCodeWaitingRoomTimeout = 8,
    MEGAChatCallTermCodeCallDurationLimit = 9,
    MEGAChatCallTermCodeCallUsersLimit = 10
};

typedef NS_ENUM (NSInteger, MEGAChatCallChangeType) {
    MEGAChatCallChangeTypeNoChanges = 0x00,
    MEGAChatCallChangeTypeStatus = 0x01,
    MEGAChatCallChangeTypeLocalAVFlags = 0x02,
    MEGAChatCallChangeTypeRingingStatus = 0x04,
    MEGAChatCallChangeTypeCallComposition = 0x08,
    MEGAChatCallChangeTypeCallOnHold = 0x10,
    MEGAChatCallChangeTypeCallSpeak = 0x20,
    MEGAChatCallChangeTypeAudioLevel = 0x40,
    MEGAChatCallChangeTypeNetworkQuality = 0x80,
    MEGAChatCallChangeTypeOutgoingRingingStop = 0x100,
    MEGAChatCallChangeTypeOwnPermissions = 0x200,
    MEGAChatCallChangeTypeGenericNotification = 0x400,
    MEGAChatCallChangeTypeWaitingRoomAllow = 0x800,
    MEGAChatCallChangeTypeWaitingRoomDeny = 0x1000,
    MEGAChatCallChangeTypeWaitingRoomComposition = 0x2000,
    MEGAChatCallChangeTypeWaitingRoomUsersEntered = 0x4000,
    MEGAChatCallChangeTypeWaitingRoomUsersLeave = 0x8000,
    MEGAChatCallChangeTypeWaitingRoomUsersAllow = 0x10000,
    MEGAChatCallChangeTypeWaitingRoomUsersDeny = 0x20000,
    MEGAChatCallChangeTypeWaitingRoomPushedFromCall = 0x40000,
    MEGAChatCallChangeTypeSpeakRequested = 0x80000,
    MEGAChatCallChangeTypeCallWillEnd = 0x100000,
    MEGAChatCallChangeTypeCallLimitsUpdated = 0x200000,
};

typedef NS_ENUM (NSInteger, MEGAChatCallConfiguration) {
    MEGAChatCallConfigurationWithAudio = 0,
    MEGAChatCallConfigurationWithVideo = 1,
    MEGAChatCallConfigurationAnyFlag = 2,
};

typedef NS_ENUM (NSInteger, MEGAChatCallCompositionChange) {
    MEGAChatCallCompositionChangePeerRemoved = -1,
    MEGAChatCallCompositionChangeNoChange = 0,
    MEGAChatCallCompositionChangePeerAdded = 1,
};

typedef NS_ENUM (NSInteger, MEGAChatCallNetworkQuality) {
    MEGAChatCallNetworkQualityBad = 0,
    MEGAChatCallNetworkQualityGood = 1,
};

typedef NS_ENUM (NSInteger, MEGAChatCallNotificationType) {
    MEGAChatCallNotificationTypeInvalid = 0,
    MEGAChatCallNotificationTypeSfuError = 1,
    MEGAChatCallNotificationTypeSfuDeny = 2,
};

@interface MEGAChatCall : NSObject

@property (nonatomic, readonly) uint64_t chatId;
@property (nonatomic, readonly) uint64_t callId;
@property (nonatomic, readonly) NSUUID *uuid;

@property (nonatomic, readonly) MEGAChatCallStatus status;
@property (nonatomic, readonly) MEGAChatCallChangeType changes;
@property (nonatomic, readonly) int64_t duration;
@property (nonatomic, readonly) int64_t initialTimeStamp;
@property (nonatomic, readonly) int64_t finalTimeStamp;
@property (nonatomic, readonly) int64_t callWillEndTimeStamp;
@property (nonatomic, readonly) MEGAChatCallTermCode termCode;
@property (nonatomic, readonly) NSInteger durationLimit;
@property (nonatomic, readonly) NSInteger usersLimit;
@property (nonatomic, readonly) NSInteger clientsLimit;
@property (nonatomic, readonly) NSInteger clientsPerUserLimit;
@property (nonatomic, readonly) MEGAChatCallNotificationType notificationType;
@property (nonatomic, readonly) MEGAChatCallNetworkQuality networkQuality;
@property (nonatomic, readonly) uint64_t caller;
@property (nonatomic, readonly) uint64_t auxHandle;
@property (nonatomic, readonly) uint64_t handleWithChange;

@property (nonatomic, readonly) uint64_t peeridCallCompositionChange;
@property (nonatomic, readonly) MEGAChatCallCompositionChange callCompositionChange;
@property (nonatomic, readonly) NSInteger numParticipants;
@property (nonatomic, readonly) MEGAHandleList *participants;
@property (nonatomic, readonly) MEGAHandleList *sessionsClientId;

@property (nonatomic, readonly, getter=hasLocalAudio) BOOL localAudio;
@property (nonatomic, readonly, getter=hasLocalVideo) BOOL localVideo;
@property (nonatomic, readonly, getter=isOwnModerator) BOOL ownModerator;
@property (nonatomic, readonly, getter=isAudioDetected) BOOL audioDetected;
@property (nonatomic, readonly, getter=isRinging) BOOL ringing;
@property (nonatomic, readonly, getter=isOnHold) BOOL onHold;
@property (nonatomic, readonly, getter=isIgnored) BOOL ignored;
@property (nonatomic, readonly, getter=isIncoming) BOOL incoming;
@property (nonatomic, readonly, getter=isOutgoing) BOOL outgoing;
@property (nonatomic, readonly, getter=isSpeakRequestEnabled) BOOL speakRequestEnabled;
@property (nonatomic, readonly, getter=isOwnClientCaller) BOOL ownClientCaller;
@property (nonatomic, readonly, getter=isSpeakPermissionFlagEnabled) BOOL speakPermissionFlagEnabled;

@property (nonatomic, readonly) MEGAHandleList *moderators;

@property (nonatomic, readonly) MEGAChatWaitingRoomStatus waitingRoomJoiningStatus;
@property (nonatomic, readonly) MEGAChatWaitingRoom *waitingRoom;
@property (nonatomic, readonly) MEGAHandleList *waitingRoomHandleList;

@property (nonatomic, readonly) MEGAHandleList *speakersList;
@property (nonatomic, readonly) MEGAHandleList *speakRequestsList;

- (BOOL)hasChangedForType:(MEGAChatCallChangeType)changeType;

- (MEGAHandleList *)sessionsClientIdByUserHandle:(uint64_t)userHandle;

- (BOOL)hasUserSpeakPermission:(uint64_t)userHandle;
- (BOOL)hasUserPendingSpeakRequest:(uint64_t)userHandle;

- (nullable MEGAChatSession *)sessionForClientId:(uint64_t)clientId;

- (instancetype)clone;

- (NSString *)termcodeString:(MEGAChatCallTermCode)termcode;

- (NSString *)genericMessage;

+ (NSString *)stringForTermCode:(MEGAChatCallTermCode)termCode;

@end

NS_ASSUME_NONNULL_END
