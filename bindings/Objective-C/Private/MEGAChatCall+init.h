
#import "MEGAChatCall.h"
#import "megachatapi.h"

@interface MEGAChatCall (init)

- (instancetype)initWithMegaChatCall:(megachat::MegaChatCall *)megaChatCall cMemoryOwn:(BOOL)cMemoryOwn;
- (megachat::MegaChatCall *)getCPtr;

@end
