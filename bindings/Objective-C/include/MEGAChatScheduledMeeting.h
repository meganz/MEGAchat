
#import <Foundation/Foundation.h>

#import "MEGAChatScheduledFlags.h"
#import "MEGAChatScheduledRules.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM (NSInteger, MEGAChatScheduledMeetingChangeType) {
    MEGAScheduledMeetingChangeTypeNew = 0,
    MEGAScheduledMeetingChangeTypeParent,
    MEGAScheduledMeetingChangeTypeTimezone,
    MEGAScheduledMeetingChangeTypeStart,
    MEGAScheduledMeetingChangeTypeEnd,
    MEGAScheduledMeetingChangeTypeTitle,
    MEGAScheduledMeetingChangeTypeDescription,
    MEGAScheduledMeetingChangeTypeAttributes,
    MEGAScheduledMeetingChangeTypeOverrideDate,
    MEGAScheduledMeetingChangeTypeCancelled,
    MEGAScheduledMeetingChangeTypeFlags,
    MEGAScheduledMeetingChangeTypeRules,
    MEGAScheduledMeetingChangeTypeFlagsSize,
};

@interface MEGAChatScheduledMeeting : NSObject

@property (readonly, nonatomic) BOOL isCancelled;
@property (readonly, nonatomic) BOOL isNew;
@property (readonly, nonatomic) BOOL isDeleted;
@property (readonly, nonatomic) uint64_t chatId;
@property (readonly, nonatomic) uint64_t scheduledId;
@property (readonly, nonatomic) uint64_t parentScheduledId;
@property (readonly, nonatomic) uint64_t organizerUserId;
@property (readonly, nonatomic, nullable) NSString *timezone;
@property (readonly, nonatomic) uint64_t startDateTime;
@property (readonly, nonatomic) uint64_t endDateTime;
@property (readonly, nonatomic, nullable) NSString *title;
@property (readonly, nonatomic, nullable) NSString *description;
@property (readonly, nonatomic, nullable) NSString *attributes;
@property (readonly, nonatomic) uint64_t overrides;
@property (readonly, nonatomic, nullable) MEGAChatScheduledFlags *flags;
@property (readonly, nonatomic, nullable) MEGAChatScheduledRules *rules;

- (BOOL)hasChangedForType:(MEGAChatScheduledMeetingChangeType)changeType;

@end

NS_ASSUME_NONNULL_END
