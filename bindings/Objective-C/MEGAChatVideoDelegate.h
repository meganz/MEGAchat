
#import <Foundation/Foundation.h>
#import "MEGAChatCall.h"

@class MEGAChatSdk;

@protocol MEGAChatVideoDelegate <NSObject>

@optional

- (void)onChatVideoData:(MEGAChatSdk *)api chatId:(uint64_t)chatId width:(NSInteger)width height:(NSInteger)height buffer:(NSData *)buffer;

@end
