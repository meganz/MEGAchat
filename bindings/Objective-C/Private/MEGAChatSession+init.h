
#import "MEGAChatSession.h"
#import "megachatapi.h"

@interface MEGAChatSession (init)

- (instancetype)initWithMegaChatSession:(megachat::MegaChatSession *)megaChatSession cMemoryOwn:(BOOL)cMemoryOwn;
- (megachat::MegaChatSession *)getCPtr;

@end
