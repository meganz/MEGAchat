
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
@property (readonly, nonatomic) NSString *timezone;
@property (readonly, nonatomic) uint64_t startDateTime;
@property (readonly, nonatomic) uint64_t endDateTime;
@property (readonly, nonatomic) NSString *title;
@property (readonly, nonatomic) NSString *description;
@property (readonly, nonatomic) NSString *attributes;
@property (readonly, nonatomic) uint64_t overrides;
@property (readonly, nonatomic) MEGAChatScheduledFlags *flags;
@property (readonly, nonatomic) MEGAChatScheduledRules *rules;

- (instancetype)clone;

- (BOOL)hasChangedForType:(MEGAChatScheduledMeetingChangeType)changeType;

@end

NS_ASSUME_NONNULL_END
