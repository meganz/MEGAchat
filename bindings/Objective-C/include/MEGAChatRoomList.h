#import <Foundation/Foundation.h>
#import "MEGAChatRoom.h"

NS_ASSUME_NONNULL_BEGIN

@interface MEGAChatRoomList : NSObject

@property (readonly, nonatomic) NSInteger size;

- (nullable MEGAChatRoom *)chatRoomAtIndex:(NSUInteger)index;

@end

NS_ASSUME_NONNULL_END
