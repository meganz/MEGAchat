
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

- (instancetype)clone;
- (NSArray <NSNumber *>*)byWeekDay;
- (NSArray <NSNumber *>*)byMonthDay;
- (NSMutableArray< NSMutableArray<NSNumber *> *> *)byMonthWeekDay;
- (BOOL)isValidFrequency:(MEGAChatScheduledRulesFrequency)frequency;
- (BOOL)isValidInterval:(NSInteger)interval;

@end

NS_ASSUME_NONNULL_END
