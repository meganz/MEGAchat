#import <Foundation/Foundation.h>
#import "MEGAChatRoom.h"

NS_ASSUME_NONNULL_BEGIN

@interface MEGAChatRoomList : NSObject

@property (readonly, nonatomic) NSInteger size;

- (instancetype)clone;

- (MEGAChatRoom *)chatRoomAtIndex:(NSUInteger)index;

@end

NS_ASSUME_NONNULL_END
