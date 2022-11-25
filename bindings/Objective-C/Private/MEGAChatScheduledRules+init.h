
#import "MEGAChatScheduledRules.h"
#import "megachatapi.h"

@interface MEGAChatScheduledRules (init)

- (instancetype)initWithMegaChatScheduledRules:(megachat::MegaChatScheduledRules *)MegaChatScheduledRules cMemoryOwn:(BOOL)cMemoryOwn;
- (megachat::MegaChatScheduledRules *)getCPtr;

@end


