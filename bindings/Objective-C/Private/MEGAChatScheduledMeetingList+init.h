

#import "MEGAChatScheduledMeetingList.h"
#import "megachatapi.h"

@interface MEGAChatScheduledMeetingList (init)

- (instancetype)initWithMegaChatScheduledMeetingList:(megachat::MegaChatScheduledMeetingList *)megaChatScheduledMeetingList cMemoryOwn:(BOOL)cMemoryOwn;
- (megachat::MegaChatScheduledMeetingList *)getCPtr;

@end
