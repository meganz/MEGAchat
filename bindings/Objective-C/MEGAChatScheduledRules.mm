
#import "MEGAChatScheduledRules.h"
#import "megachatapi.h"
#import "megaapi.h"

using namespace megachat;
using namespace mega;

@interface MEGAChatScheduledRules()

@property MegaChatScheduledRules *megaChatScheduledRules;
@property BOOL cMemoryOwn;

@end

@implementation MEGAChatScheduledRules

- (instancetype)initWithMegaChatScheduledRules:(megachat::MegaChatScheduledRules *)MegaChatScheduledRules cMemoryOwn:(BOOL)cMemoryOwn {
    
}

- (MegaChatScheduledRules *)getCPtr {
    return self.megaChatScheduledRules;
}

- (instancetype)clone {
    return self.megaChatScheduledRules ? [[MEGAChatScheduledRules alloc] initWithMegaChatScheduledRules:self.megaChatScheduledRules cMemoryOwn:YES] : nil;
}

- (MEGAChatScheduledRulesFrequency)frequency {
    if (!self.megaChatScheduledRules) { return MEGAChatScheduledRulesFrequencyInvalid; }
    return MEGAChatScheduledRulesFrequency(self.megaChatScheduledRules->freq());
}

- (NSInteger)interval {
    if (!self.megaChatScheduledRules) { return 0; }
    return self.megaChatScheduledRules->interval();
}

- (NSString *)until {
    const char *val = self.megaChatScheduledRules->until();
    if (!val) return nil;
    NSString *ret = [[NSString alloc] initWithUTF8String:val];
    delete [] val;
    return ret;
}

- (NSArray <NSNumber *>*)byWeekDay {
    if (!self.megaChatScheduledRules) { return nil; }
    MegaIntegerList *integerList = self.megaChatScheduledRules->byWeekDay()->copy();
    NSMutableArray<NSNumber *> *integerArray = [NSMutableArray arrayWithCapacity:integerList->size()];

    for (int i = 0; i < integerList->size(); i++)
    {
        [integerArray addObject:[NSNumber.alloc initWithInt:integerList->get(i)]];
    }

    delete integerList;
    return integerArray;
}

- (NSArray <NSNumber *>*)byMonthDay {
    if (!self.megaChatScheduledRules) { return nil; }
    MegaIntegerList *integerList = self.megaChatScheduledRules->byMonthDay()->copy();
    NSMutableArray<NSNumber *> *integerArray = [NSMutableArray arrayWithCapacity:integerList->size()];

    for (int i = 0; i < integerList->size(); i++)
    {
        [integerArray addObject:[NSNumber.alloc initWithInt:integerList->get(i)]];
    }

    delete integerList;
    return integerArray;
}

- (BOOL)isValidFrequency:(MEGAChatScheduledRulesFrequency)frequency {
    if (!self.megaChatScheduledRules) { return NO; }
    self.megaChatScheduledRules->isValidFreq(frequency);
}

- (BOOL)isValidInterval:(NSInteger)interval {
    if (!self.megaChatScheduledRules) { return NO; }
    self.megaChatScheduledRules->isValidInterval(interval);
}

@end
