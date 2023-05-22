
#import "MEGAChatScheduledRules.h"
#import "megachatapi.h"
#import "megaapi.h"

using namespace megachat;
using namespace mega;

@interface MEGAChatScheduledRules()

@property MegaChatScheduledRules *megaChatScheduledRules;
@property BOOL cMemoryOwn;

@property (nonatomic) MEGAChatScheduledRulesFrequency frequency;
@property (nonatomic) NSInteger interval;
@property (nonatomic) uint64_t until;
@property (nonatomic, nullable) NSArray <NSNumber *> *byWeekDay;
@property (nonatomic, nullable) NSArray <NSNumber *> *byMonthDay;
@property (nonatomic, nullable) NSArray<NSArray<NSNumber *> *> *byMonthWeekDay;

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

- (instancetype)initWithFrequency:(MEGAChatScheduledRulesFrequency)frequency
                         interval:(NSInteger)interval
                            until:(uint64_t)until
                        byWeekDay:(NSArray <NSNumber *> *)byWeekDay
                       byMonthDay:(NSArray <NSNumber *> *)byMonthDay
                   byMonthWeekDay:(NSArray<NSArray<NSNumber *> *> *)byMonthWeekDay {
    self = [self initWithMegaChatScheduledRules:MegaChatScheduledRules::createInstance(frequency) cMemoryOwn:YES];
    
    if (self != nil) {
        self.interval = interval;
        self.until = until;
        self.byWeekDay = byWeekDay;
        self.byMonthDay = byMonthDay;
        self.byMonthWeekDay = byMonthWeekDay;
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

- (void)setFrequency:(MEGAChatScheduledRulesFrequency)frequency {
    if (self.megaChatScheduledRules) {
        self.megaChatScheduledRules->setFreq(frequency);
    }
}

- (NSInteger)interval {
    if (!self.megaChatScheduledRules) { return 0; }
    return self.megaChatScheduledRules->interval();
}

- (void)setInterval:(NSInteger)interval {
    if (self.megaChatScheduledRules) {
        self.megaChatScheduledRules->setInterval(interval);
    }
}

- (uint64_t)until {
    if (!self.megaChatScheduledRules) { return 0; }
    return self.megaChatScheduledRules->until();
}

- (void)setUntil:(uint64_t)until {
    if (self.megaChatScheduledRules) {
        self.megaChatScheduledRules->setUntil(until);
    }
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

- (void)setByWeekDay:(NSArray<NSNumber *> *)byWeekDay {
    if (self.megaChatScheduledRules) {
        if (byWeekDay) {
            MegaIntegerList *integerList = MegaIntegerList::createInstance();
            
            for (int i = 0; i < byWeekDay.count; i++) {
                integerList->add(byWeekDay[i].longLongValue);
            }

            self.megaChatScheduledRules->setByWeekDay(integerList);
            delete integerList;
        } else {
            self.megaChatScheduledRules->setByWeekDay(nil);
        }
    }
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

- (void)setByMonthDay:(NSArray<NSNumber *> *)byMonthDay {
    if (self.megaChatScheduledRules) {
        if (byMonthDay) {
            MegaIntegerList *integerList = MegaIntegerList::createInstance();
            
            for (int i = 0; i < byMonthDay.count; i++) {
                integerList->add(byMonthDay[i].longLongValue);
            }

            self.megaChatScheduledRules->setByMonthDay(integerList);
            delete integerList;
        } else {
            self.megaChatScheduledRules->setByMonthDay(nil);
        }
    }
}

- (NSMutableArray< NSMutableArray<NSNumber *> *> *)byMonthWeekDay {
    if (!self.megaChatScheduledRules || !self.megaChatScheduledRules->byMonthWeekDay()) { return nil; }

    MegaIntegerMap *integerMap = self.megaChatScheduledRules->byMonthWeekDay()->copy();
    NSMutableArray< NSMutableArray<NSNumber *> *> *integerArray = [NSMutableArray arrayWithCapacity:integerMap->size()];
    
    for (int i = 0; i < integerMap->size(); i++)
    {
        MegaIntegerList *keyList = integerMap->getKeys();
        for (int i = 0; i < keyList->size(); i++)
        {
            uint64_t key = keyList->get(i);
            MegaIntegerList *valueList = integerMap->get(key);
            NSMutableArray<NSNumber *> *keyValueArray = @[[NSNumber.alloc initWithInt:key], [NSNumber.alloc initWithInt:valueList->get(0)]];
            [integerArray addObject:keyValueArray];
        }
    }
    
    delete integerMap;
    return integerArray;
}

- (void)setByMonthWeekDay:(NSArray<NSArray<NSNumber *> *> *)byMonthWeekDay {
    if (self.megaChatScheduledRules) {
        if (byMonthWeekDay) {
            MegaIntegerMap *integerMap = MegaIntegerMap::createInstance();
            
            for (int i = 0; i < byMonthWeekDay.count; i++) {
                NSArray<NSNumber *> *keyValue = byMonthWeekDay[i];
                if (keyValue.count == 2) {
                    integerMap->set(keyValue[0].longLongValue, keyValue[1].longLongValue);
                }
            }

            self.megaChatScheduledRules->setByMonthWeekDay(integerMap);
            delete integerMap;
        } else {
            self.megaChatScheduledRules->setByMonthWeekDay(nil);
        }
    }
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
