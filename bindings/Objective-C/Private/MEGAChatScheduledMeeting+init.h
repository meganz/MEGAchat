
#import "MEGAChatScheduledMeeting.h"
#import "megachatapi.h"

@interface MEGAChatScheduledMeeting (init)

- (instancetype)initWithMegaChatScheduledMeeting:(megachat::MegaChatScheduledMeeting *)megaChatScheduledMeeting cMemoryOwn:(BOOL)cMemoryOwn;
- (megachat::MegaChatScheduledMeeting *)getCPtr;

@end
