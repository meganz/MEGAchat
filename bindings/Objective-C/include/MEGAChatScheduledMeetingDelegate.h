
#import <Foundation/Foundation.h>
#import "MEGAChatScheduledMeeting.h"

NS_ASSUME_NONNULL_BEGIN

@protocol MEGAChatScheduledMeetingDelegate <NSObject>

@optional

- (void)onChatSchedMeetingUpdate:(MEGAChatSdk *)api scheduledMeeting:(MEGAChatScheduledMeeting *)scheduledMeeting;

- (void)onSchedMeetingOccurrencesUpdate:(MEGAChatSdk *)api chatId:(uint64_t)chatId append:(BOOL)append;

@end

NS_ASSUME_NONNULL_END
