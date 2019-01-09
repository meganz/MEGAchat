
#import "MEGAChatGeolocation.h"
#import "megachatapi.h"

NS_ASSUME_NONNULL_BEGIN

@interface MEGAChatGeolocation (init)

- (instancetype)initWithMegaChatGeolocation:(const megachat::MegaChatGeolocation *)megaChatGeolocation cMemoryOwn:(BOOL)cMemoryOwn;

@end

NS_ASSUME_NONNULL_END
