
#import <Foundation/Foundation.h>

typedef NS_ENUM (NSInteger, MEGAChatSessionStatus) {
    MEGAChatSessionStatusInvalid = 0xFF,
    MEGAChatSessionStatusInitial = 0,
    MEGAChatSessionStatusInProgress,
    MEGAChatSessionStatusDestroyed
};

@interface MEGAChatSession : NSObject

@property (nonatomic, readonly) MEGAChatSessionStatus status;

@property (nonatomic, readonly, getter=hasAudio) BOOL audio;
@property (nonatomic, readonly, getter=hasVideo) BOOL video;

@property (nonatomic, readonly) uint64_t peerId;
@property (nonatomic, readonly) BOOL audioDetected;
@property (nonatomic, readonly) NSInteger networkQuality;

@end
