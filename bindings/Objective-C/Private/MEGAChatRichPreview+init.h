
#import "MEGAChatRichPreview.h"
#import "megachatapi.h"

@interface MEGAChatRichPreview (init)

- (instancetype)initWithMegaChatRichPreview:(const megachat::MegaChatRichPreview *)megaChatRichPreview cMemoryOwn:(BOOL)cMemoryOwn;

@end
