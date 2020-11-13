
#import "MEGAChatGiphy.h"
#import "megachatapi.h"

NS_ASSUME_NONNULL_BEGIN

@interface MEGAChatGiphy (init)

- (instancetype)initWithMegaChatGiphy:(const megachat::MegaChatGiphy *)megaChatGiphy cMemoryOwn:(BOOL)cMemoryOwn;

@end

NS_ASSUME_NONNULL_END
