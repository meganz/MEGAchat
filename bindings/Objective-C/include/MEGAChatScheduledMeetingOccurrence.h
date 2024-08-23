
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface MEGAChatScheduledMeetingOccurrence : NSObject

@property (readonly, nonatomic) BOOL isCancelled;
@property (readonly, nonatomic) uint64_t scheduledId;
@property (readonly, nonatomic) uint64_t parentScheduledId;
@property (readonly, nonatomic) uint64_t overrides;
@property (readonly, nonatomic, nullable) NSString *timezone;
@property (readonly, nonatomic) uint64_t startDateTime;
@property (readonly, nonatomic) uint64_t endDateTime;

@end

NS_ASSUME_NONNULL_END
