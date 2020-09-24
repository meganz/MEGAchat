
//#import "MEGAChatGiphy.h"
//
//@implementation MEGAChatGiphy
//
//@end



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

- (void)dealloc {
    if (self.cMemoryOwn) {
        delete _megaChatGiphy;
    }
}

//- (float)longitude {
//    return self.megaChatGiphy->getLongitude();
//}
//
//- (float)latitude {
//    return self.megaChatGeolocation->getLatitude();
//}
//
//- (NSString *)image {
//    if (!self.megaChatGeolocation) return nil;
//    return self.megaChatGeolocation->getImage() ? [[NSString alloc] initWithUTF8String:self.megaChatGeolocation->getImage()] : nil;
//}

@end
