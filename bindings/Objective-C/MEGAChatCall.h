
#import <Foundation/Foundation.h>

typedef NS_ENUM (NSInteger, MEGAChatCallStatus) {
    MEGAChatCallStatusInitial = 0,
    MEGAChatCallStatusHasLocalStream,
    MEGAChatCallStatusRequestSent,
    MEGAChatCallStatusRingIn,
    MEGAChatCallStatusJoining,
    MEGAChatCallStatusInProgress,
    MEGAChatCallStatusTerminating,
    MEGAChatCallStatusDestroyed,
    MEGAChatCallStatusDisconnected
};

typedef NS_ENUM (NSInteger, MEGAChatCallChangeType) {
    MEGAChatCallChangeTypeStatus = 0x01,
    MEGAChatCallChangeTypeLocalAVFlags = 0x02,
    MEGAChatCallChangeTypeRemoteAVFlags = 0x04,
    MEGAChatCallChangeTypeTemporaryError = 0x08
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
@property (nonatomic, readonly, getter=hasRemoteAudio) BOOL remoteAudio;
@property (nonatomic, readonly, getter=hasRemoteVideo) BOOL remoteVideo;

- (BOOL)hasChangedForType:(MEGAChatCallChangeType)changeType;


- (instancetype)clone;

@end
