
#import <Foundation/Foundation.h>
#import "MEGAChatListItem.h"
#import "MEGAChatPresenceConfig.h"
#import "MEGAChatDBError.h"

NS_ASSUME_NONNULL_BEGIN

@class MEGAChatSdk;

typedef NS_ENUM (NSInteger, MEGAChatInit);

@protocol MEGAChatDelegate <NSObject>

@optional

- (void)onChatListItemUpdate:(MEGAChatSdk *)api item:(MEGAChatListItem *)item;
- (void)onChatInitStateUpdate:(MEGAChatSdk *)api newState:(MEGAChatInit)newState;
- (void)onChatOnlineStatusUpdate:(MEGAChatSdk *)api userHandle:(uint64_t)userHandle status:(MEGAChatStatus)onlineStatus inProgress:(BOOL)inProgress;
- (void)onChatPresenceConfigUpdate:(MEGAChatSdk *)api presenceConfig:(MEGAChatPresenceConfig *)presenceConfig;
- (void)onChatConnectionStateUpdate:(MEGAChatSdk *)api chatId:(uint64_t)chatId newState:(int)newState;
- (void)onChatPresenceLastGreen:(MEGAChatSdk *)api userHandle:(uint64_t)userHandle lastGreen:(NSInteger)lastGreen;
- (void)onDbError:(MEGAChatSdk *)api error:(MEGAChatDBError)error message:(NSString *)message;

@end

NS_ASSUME_NONNULL_END
