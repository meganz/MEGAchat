
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM (NSInteger, MEGAChatScheduledRulesFrequency) {
    MEGAChatScheduledRulesFrequencyInvalid = -1,
    MEGAChatScheduledRulesFrequencyDaily = 0,
    MEGAChatScheduledRulesFrequencyWeekly,
    MEGAChatScheduledRulesFrequencyMonthly,
};

@interface MEGAChatScheduledRules : NSObject

@property (readonly, nonatomic) MEGAChatScheduledRulesFrequency frequency;
@property (readonly, nonatomic) NSInteger interval;
@property (readonly, nonatomic) uint64_t until;
@property (readonly, nonatomic, nullable) NSArray <NSNumber *> *byWeekDay;
@property (readonly, nonatomic, nullable) NSArray <NSNumber *> *byMonthDay;
@property (readonly, nonatomic, nullable) NSArray<NSArray<NSNumber *> *> *byMonthWeekDay;

- (instancetype)initWithFrequency:(MEGAChatScheduledRulesFrequency)frequency
                         interval:(NSInteger)interval
                            until:(uint64_t)until
                        byWeekDay:(nullable NSArray <NSNumber *> *)byWeekDay
                       byMonthDay:(nullable NSArray <NSNumber *> *)byMonthDay
                   byMonthWeekDay:(nullable NSArray<NSArray<NSNumber *> *> *)byMonthWeekDay;
- (BOOL)isValidFrequency:(MEGAChatScheduledRulesFrequency)frequency;
- (BOOL)isValidInterval:(NSInteger)interval;

@end

NS_ASSUME_NONNULL_END
