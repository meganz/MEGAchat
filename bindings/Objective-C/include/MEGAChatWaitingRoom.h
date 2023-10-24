#import <Foundation/Foundation.h>
#import "MEGAHandleList.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM (NSInteger, MEGAChatWaitingRoomStatus) {
    MEGAChatWaitingRoomStatusUnknown        = -1,
    MEGAChatWaitingRoomStatusNotAllowed     = 0,
    MEGAChatWaitingRoomStatusAllowed        = 1,
};

@interface MEGAChatWaitingRoom : NSObject

@property (nullable, nonatomic, readonly) MEGAHandleList *users;
@property (nonatomic, readonly) int64_t size;

- (MEGAChatWaitingRoomStatus)userStatus:(uint64_t)peerId;

- (NSString *)stringForStatus:(MEGAChatWaitingRoomStatus)status;

@end

NS_ASSUME_NONNULL_END
