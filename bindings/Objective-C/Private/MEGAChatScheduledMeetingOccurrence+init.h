
#import "MEGAChatScheduledMeetingOccurrence.h"
#import "megachatapi.h"

@interface MEGAChatScheduledMeetingOccurrence (init)

- (instancetype)initWithMegaChatScheduledMeetingOccurrence:(megachat::MegaChatScheduledMeetingOccurr *)megaChatScheduledMeetingOccurr cMemoryOwn:(BOOL)cMemoryOwn;
- (megachat::MegaChatScheduledMeeting *)getCPtr;

@end
