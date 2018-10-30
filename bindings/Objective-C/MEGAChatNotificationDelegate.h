
#import <Foundation/Foundation.h>

@class MEGAChatSdk, MEGAChatMessage;

NS_ASSUME_NONNULL_BEGIN

@protocol MEGAChatNotificationDelegate <NSObject>

@optional

- (void)onChatNotification:(MEGAChatSdk *)api chatId:(uint64_t)chatId message:(MEGAChatMessage *)message;

@end

NS_ASSUME_NONNULL_END
