
#import <Foundation/Foundation.h>

typedef NS_ENUM (NSInteger, MEGAChatSessionStatus) {
    MEGAChatSessionStatusInvalid = 0xFF,
    MEGAChatSessionStatusInitial = 0,
    MEGAChatSessionStatusInProgress,
    MEGAChatSessionStatusDestroyed
};

typedef NS_ENUM (NSInteger, MEGAChatSessionChange) {
    MEGAChatSessionChangeNoChanges = 0x00,
    MEGAChatSessionChangeStatus = 0x01,
    MEGAChatSessionChangeRemoteAvFlags = 0x02,
    MEGAChatSessionChangeNetworkQuality = 0x04,
    MEGAChatSessionChangeAudioLevel = 0x08,
    MEGAChatSessionChangeOperative = 0x10,
    MEGAChatSessionChangeOnHold = 0x20,
};

@interface MEGAChatSession : NSObject

@property (nonatomic, readonly) MEGAChatSessionStatus status;

@property (nonatomic, readonly, getter=hasAudio) BOOL audio;
@property (nonatomic, readonly, getter=hasVideo) BOOL video;

@property (nonatomic, readonly) uint64_t peerId;
@property (nonatomic, readonly) uint64_t clientId;
@property (nonatomic, readonly) BOOL audioDetected;
@property (nonatomic, readonly, getter=isOnHold) BOOL onHold;
@property (nonatomic, readonly) NSInteger networkQuality;
@property (nonatomic, readonly) NSInteger termCode;
@property (nonatomic, readonly) BOOL isLocalTermCode;
@property (nonatomic, readonly) NSInteger changes;

- (BOOL)hasChanged:(MEGAChatSessionChange)change;

@end
