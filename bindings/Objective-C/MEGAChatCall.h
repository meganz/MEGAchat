
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
    MEGAChatCallStatusUserNoPresent
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
    MEGAChatCallChangeTypeRemoteAVFlags = 0x04,
    MEGAChatCallChangeTypeTemporaryError = 0x08,
    MEGAChatCallChangeTypeRingingStatus = 0x10,
    MEGAChatCallChangeTypeSessionStatus = 0x20,
    MEGAChatCallChangeTypeCallComposition = 0x40,
    MEGAChatCallChangeTypeNetworkQuality = 0x80,
    MEGAChatCallChangeTypeAudioLevel = 0x100
};

@interface MEGAChatCall : NSObject

@property (nonatomic, readonly) MEGAChatCallStatus status;
@property (nonatomic, readonly) uint64_t chatId;
@property (nonatomic, readonly) uint64_t callId;
@property (nonatomic, readonly) MEGAChatCallChangeType changes;
@property (nonatomic, readonly) int64_t duration;
@property (nonatomic, readonly) int64_t initialTimeStamp;
@property (nonatomic, readonly) int64_t finalTimeStamp;
@property (nonatomic, readonly) NSString *temporaryError;
@property (nonatomic, readonly, getter=hasLocalAudio) BOOL localAudio;
@property (nonatomic, readonly, getter=hasLocalVideo) BOOL localVideo;
@property (nonatomic, readonly, getter=hasAudioInitialCall) BOOL audioInitialCall;
@property (nonatomic, readonly, getter=hasVideoInitialCall) BOOL videoInitialCall;
@property (nonatomic, readonly) MEGAChatCallTermCode termCode;
@property (nonatomic, readonly, getter=isLocalTermCode) BOOL localTermCode;
@property (nonatomic, readonly, getter=isRinging) BOOL ringing;
@property (nonatomic, readonly) uint64_t peerSessionStatusChange;
@property (nonatomic, readonly) NSInteger numParticipants;
@property (nonatomic, readonly) MEGAHandleList *sessions;
@property (nonatomic, readonly) MEGAHandleList *participants;

- (BOOL)hasChangedForType:(MEGAChatCallChangeType)changeType;

- (MEGAChatSession *)sessionForPeer:(uint64_t)peerId;
- (instancetype)clone;

@end
