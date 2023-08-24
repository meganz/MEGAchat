#import "MEGAChatWaitingRoom.h"
#import "megachatapi.h"

@interface MEGAChatWaitingRoom (init)

- (instancetype)initWithMegaChatWaitingRoom:(megachat::MegaChatWaitingRoom *)megaChatWaitingRoom cMemoryOwn:(BOOL)cMemoryOwn;
- (megachat::MegaChatWaitingRoom *)getCPtr;

@end
