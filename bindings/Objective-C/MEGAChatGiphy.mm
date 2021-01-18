
#import "MEGAChatGiphy.h"

#import "megachatapi.h"

using namespace megachat;

@interface MEGAChatGiphy ()

@property MegaChatGiphy *megaChatGiphy;
@property BOOL cMemoryOwn;

@end

@implementation MEGAChatGiphy

- (instancetype)initWithMegaChatGiphy:(MegaChatGiphy *)megaChatGiphy cMemoryOwn:(BOOL)cMemoryOwn {
    NSParameterAssert(megaChatGiphy);
    
    if (self = [super init]) {
        _megaChatGiphy = megaChatGiphy;
        _cMemoryOwn = cMemoryOwn;
    }
    
    return self;
}

- (NSString *)mp4Src {
    if (!self.megaChatGiphy) return nil;
    return self.megaChatGiphy->getMp4Src() ? [[NSString alloc] initWithUTF8String:self.megaChatGiphy->getMp4Src()] : nil;
}

- (NSString *)webpSrc {
    if (!self.megaChatGiphy) return nil;
    return self.megaChatGiphy->getWebpSrc() ? [[NSString alloc] initWithUTF8String:self.megaChatGiphy->getWebpSrc()] : nil;
}

- (NSString *)title {
    if (!self.megaChatGiphy) return nil;
    return self.megaChatGiphy->getTitle() ? [[NSString alloc] initWithUTF8String:self.megaChatGiphy->getTitle()] : nil;
}

- (long)mp4Size {
    return self.megaChatGiphy->getMp4Size();
}

- (long)webpSize {
    return self.megaChatGiphy->getWebpSize();
}

- (int)width {
    return self.megaChatGiphy->getWidth();
}

- (int)height {
    return self.megaChatGiphy->getHeight();
}

- (void)dealloc {
    if (self.cMemoryOwn) {
        delete _megaChatGiphy;
    }
}

@end
