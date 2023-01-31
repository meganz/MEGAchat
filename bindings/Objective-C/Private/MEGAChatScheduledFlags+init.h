
#import "MEGAChatScheduledFlags.h"
#import "megachatapi.h"

@interface MEGAChatScheduledFlags (init)

- (instancetype)initWithMegaChatScheduledFlags:(megachat::MegaChatScheduledFlags *)megaChatScheduledFlags cMemoryOwn:(BOOL)cMemoryOwn;
- (megachat::MegaChatScheduledFlags *)getCPtr;

@end


