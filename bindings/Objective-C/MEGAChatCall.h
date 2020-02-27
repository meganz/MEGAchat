
#import <Foundation/Foundation.h>
#import "MEGAHandleList.h"
#import "MEGAChatSession.h"

typedef NS_ENUM (NSInteger, MEGAChatCallStatus) {
    MEGAChatCallStatusInitial = 0,
    MEGAChatCallStatusHasLocalStream,
    MEGAChatCallStatusRequestSent,
    MEGAChatCallStatusRingIn,
    MEGAChatCallStatusJoining,
    MEGAChatCallStatusInProgress,
    MEGAChatCallStatusTerminatingUserParticipation,
    MEGAChatCallStatusDestroyed,
    MEGAChatCallStatusUserNoPresent,
    MEGAChatCallStatusReconnecting
};

typedef NS_ENUM (NSInteger, MEGAChatCallSessionStatus) {
    MEGAChatCallSessionStatusInitial = 0,
    MEGAChatCallSessionStatusInProgress,
    MEGAChatCallSessionStatusDestroyed,
    MEGAChatCallSessionStatusNoSession
};

typedef NS_ENUM (NSInteger, MEGAChatCallTermCode) {
    MEGAChatCallTermCodeUserHangup = 0,
    MEGAChatCallTermCodeCallReqCancel = 1,
    MEGAChatCallTermCodeCallReject = 2,
    MEGAChatCallTermCodeAnswerElseWhere = 3,
    MEGAChatCallTermCodeRejectElseWhere = 4,
    MEGAChatCallTermCodeAnswerTimeout = 5,
    MEGAChatCallTermCodeRingOutTimeout = 6,
    MEGAChatCallTermCodeAppTerminating = 7,
    MEGAChatCallTermCodeBusy = 9,
    MEGAChatCallTermCodeNotFinished = 10,
    MEGAChatCallTermCodeDestroyByCallCollision = 19,
    MEGAChatCallTermCodeError = 21
};

typedef NS_ENUM (NSInteger, MEGAChatCallChangeType) {
    MEGAChatCallChangeTypeNoChages = 0x00,
    MEGAChatCallChangeTypeStatus = 0x01,
    MEGAChatCallChangeTypeLocalAVFlags = 0x02,
    MEGAChatCallChangeTypeRingingStatus = 0x04,
    MEGAChatCallChangeTypeCallComposition = 0x08,
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

@interface MEGAChatCall : NSObject

@property (nonatomic, readonly) MEGAChatCallStatus status;
@property (nonatomic, readonly) uint64_t chatId;
@property (nonatomic, readonly) uint64_t callId;
@property (nonatomic, readonly) MEGAChatCallChangeType changes;
@property (nonatomic, readonly) int64_t duration;
@property (nonatomic, readonly) int64_t initialTimeStamp;
@property (nonatomic, readonly) int64_t finalTimeStamp;
@property (nonatomic, readonly, getter=hasLocalAudio) BOOL localAudio;
@property (nonatomic, readonly, getter=hasLocalVideo) BOOL localVideo;
@property (nonatomic, readonly, getter=hasAudioInitialCall) BOOL audioInitialCall;
@property (nonatomic, readonly, getter=hasVideoInitialCall) BOOL videoInitialCall;
@property (nonatomic, readonly) MEGAChatCallTermCode termCode;
@property (nonatomic, readonly, getter=isLocalTermCode) BOOL localTermCode;
@property (nonatomic, readonly, getter=isRinging) BOOL ringing;
@property (nonatomic, readonly) uint64_t peeridCallCompositionChange;
@property (nonatomic, readonly) uint64_t clientidCallCompositionChange;
@property (nonatomic, readonly) uint64_t callCompositionChange;

@property (nonatomic, readonly) NSInteger numParticipants;
@property (nonatomic, readonly) MEGAHandleList *sessionsPeerId;
@property (nonatomic, readonly) MEGAHandleList *sessionsClientId;

@property (nonatomic, readonly) MEGAHandleList *participants;

@property (nonatomic, readonly) NSUUID *uuid;

- (BOOL)hasChangedForType:(MEGAChatCallChangeType)changeType;

- (NSInteger)numParticipantsWithCallConfiguration:(MEGAChatCallConfiguration)callConfiguration;

- (MEGAChatSession *)sessionForPeer:(uint64_t)peerId clientId:(uint64_t)clientId;

- (instancetype)clone;

+ (NSString *)stringForTermCode:(MEGAChatCallTermCode)termCode;

@end
