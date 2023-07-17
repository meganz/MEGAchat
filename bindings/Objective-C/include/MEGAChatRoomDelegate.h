#import <Foundation/Foundation.h>
#import "MEGAChatRoom.h"
#import "MEGAChatMessage.h"

NS_ASSUME_NONNULL_BEGIN

@class MEGAChatSdk;

@protocol MEGAChatRoomDelegate <NSObject>

@optional

- (void)onChatRoomUpdate:(MEGAChatSdk *)api chat:(MEGAChatRoom *)chat;
- (void)onMessageLoaded:(MEGAChatSdk *)api message:(nullable MEGAChatMessage *)message;
- (void)onMessageReceived:(MEGAChatSdk *)api message:(MEGAChatMessage *)message;
- (void)onMessageUpdate:(MEGAChatSdk *)api message:(MEGAChatMessage *)message;
- (void)onHistoryReloaded:(MEGAChatSdk *)api chat:(MEGAChatRoom *)chat;
- (void)onReactionUpdate:(MEGAChatSdk *)api messageId:(uint64_t)messageId reaction:(NSString *)reaction count:(NSInteger)count;

@end

NS_ASSUME_NONNULL_END
