
#import "MEGAChatGeolocation.h"

#import "megachatapi.h"

using namespace megachat;

@interface MEGAChatGeolocation ()

@property MegaChatGeolocation *megaChatGeolocation;
@property BOOL cMemoryOwn;

@end

@implementation MEGAChatGeolocation

- (instancetype)initWithMegaChatGeolocation:(MegaChatGeolocation *)megaChatGeolocation cMemoryOwn:(BOOL)cMemoryOwn {
    NSParameterAssert(megaChatGeolocation);
    
    if (self = [super init]) {
        _megaChatGeolocation = megaChatGeolocation;
        _cMemoryOwn = cMemoryOwn;
    }
    
    return self;
}

- (void)dealloc {
    if (self.cMemoryOwn) {
        delete _megaChatGeolocation;
    }
}

- (float)longitude {
    return self.megaChatGeolocation->getLongitude();
}

- (float)latitude {
    return self.megaChatGeolocation->getLatitude();
}

- (NSString *)image {
    if (!self.megaChatGeolocation) return nil;    
    return self.megaChatGeolocation->getImage() ? [[NSString alloc] initWithUTF8String:self.megaChatGeolocation->getImage()] : nil;
}

@end
