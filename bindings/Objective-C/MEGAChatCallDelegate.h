
#import <Foundation/Foundation.h>
#import "MEGAChatCall.h"
#import "MEGAChatError.h"

@class MEGAChatSdk;

@protocol MEGAChatCallDelegate <NSObject>

@optional

- (void)onChatCallUpdate:(MEGAChatSdk *)api call:(MEGAChatCall *)call;

@end
