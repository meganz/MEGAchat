
#import <Foundation/Foundation.h>
#import "MEGAChatScheduledMeeting.h"

NS_ASSUME_NONNULL_BEGIN

@interface MEGAChatScheduledMeetingList : NSObject

@property (readonly, nonatomic) NSUInteger size;

- (instancetype)clone;
- (nullable MEGAChatScheduledMeeting *)scheduledMeetingAtIndex:(NSUInteger)index;
- (void)insertScheduledMeeting:(MEGAChatScheduledMeeting *)scheduledMeeting;

@end

NS_ASSUME_NONNULL_END
