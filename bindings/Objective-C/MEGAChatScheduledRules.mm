
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

- (instancetype)initWithMegaChatScheduledRules:(MegaChatScheduledRules *)megaChatScheduledRules cMemoryOwn:(BOOL)cMemoryOwn {
    self = [super init];
    
    if (self != nil) {
        _megaChatScheduledRules = megaChatScheduledRules;
        _cMemoryOwn = cMemoryOwn;
    }
    
    return self;
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

- (uint64_t)until {
    if (!self.megaChatScheduledRules) { return 0; }
    return self.megaChatScheduledRules->until();
}

- (NSArray <NSNumber *>*)byWeekDay {
    if (!self.megaChatScheduledRules || !self.megaChatScheduledRules->byWeekDay()) { return nil; }
    
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
    if (!self.megaChatScheduledRules || !self.megaChatScheduledRules->byMonthDay()) { return nil; }
        
    MegaIntegerList *integerList = self.megaChatScheduledRules->byMonthDay()->copy();
    NSMutableArray<NSNumber *> *integerArray = [NSMutableArray arrayWithCapacity:integerList->size()];

    for (int i = 0; i < integerList->size(); i++)
    {
        [integerArray addObject:[NSNumber.alloc initWithInt:integerList->get(i)]];
    }

    delete integerList;
    return integerArray;
}

- (NSMutableArray< NSMutableArray<NSNumber *> *> *)byMonthWeekDay {
    if (!self.megaChatScheduledRules || !self.megaChatScheduledRules->byMonthWeekDay()) { return nil; }

    MegaIntegerMap *integerMap = self.megaChatScheduledRules->byMonthWeekDay()->copy();
    NSMutableArray< NSMutableArray<NSNumber *> *> *integerArray = [NSMutableArray arrayWithCapacity:integerMap->size()];
    
    for (int i = 0; i < integerMap->size(); i++)
    {
        long long key;
        long long value;
        integerMap->at(i, key, value);
        
        NSMutableArray<NSNumber *> *keyValueArray = @[[NSNumber.alloc initWithInt:key], [NSNumber.alloc initWithInt:value]];
        [integerArray addObject:keyValueArray];
    }
    
    delete integerMap;
    return integerArray;
}

- (BOOL)isValidFrequency:(MEGAChatScheduledRulesFrequency)frequency {
    if (!self.megaChatScheduledRules) { return NO; }
    return self.megaChatScheduledRules->isValidFreq(frequency);
}

- (BOOL)isValidInterval:(NSInteger)interval {
    if (!self.megaChatScheduledRules) { return NO; }
    return self.megaChatScheduledRules->isValidInterval(interval);
}

@end
