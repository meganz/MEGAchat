
#import <Foundation/Foundation.h>
#import "MEGAChatCall.h"
#import "MEGAChatError.h"

NS_ASSUME_NONNULL_BEGIN

@class MEGAChatSdk;

@protocol MEGAChatCallDelegate <NSObject>

@optional

- (void)onChatCallUpdate:(MEGAChatSdk *)api call:(MEGAChatCall *)call;

- (void)onChatSessionUpdate:(MEGAChatSdk *)api chatId:(uint64_t)chatId callId:(uint64_t)callId session:(MEGAChatSession *)session;

@end

NS_ASSUME_NONNULL_END
