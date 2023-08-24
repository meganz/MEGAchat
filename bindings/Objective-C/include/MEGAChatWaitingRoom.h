#import <Foundation/Foundation.h>
#import "MEGAHandleList.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM (NSInteger, MEGAChatWaitingRoomStatus) {
    MEGAChatWaitingRoomStatusUnknown        = -1,
    MEGAChatWaitingRoomStatusNotAllowed     = 0,
    MEGAChatWaitingRoomStatusAllowed        = 1,
};

@interface MEGAChatWaitingRoom : NSObject

@property (nonatomic, readonly) MEGAHandleList *peers;
@property (nonatomic, readonly) int64_t size;

- (MEGAChatWaitingRoomStatus)peerStatus:(uint64_t)peerId;

- (NSString *)stringForStatus:(MEGAChatWaitingRoomStatus)status;

@end

NS_ASSUME_NONNULL_END
