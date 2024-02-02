#import <Foundation/Foundation.h>
#import "MEGAChatListItem.h"

NS_ASSUME_NONNULL_BEGIN

@interface MEGAChatListItemList : NSObject

@property (readonly, nonatomic) NSUInteger size;

- (nullable MEGAChatListItem *)chatListItemAtIndex:(NSUInteger)index;

@end

NS_ASSUME_NONNULL_END
