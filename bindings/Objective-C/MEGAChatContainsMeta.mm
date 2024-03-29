
#import "MEGAChatContainsMeta.h"

#import "megachatapi.h"

#import "MEGAChatRichPreview+init.h"
#import "MEGAChatGeolocation+init.h"
#import "MEGAChatGiphy+init.h"

using namespace megachat;

@interface MEGAChatContainsMeta ()

@property MegaChatContainsMeta *megaChatContainsMeta;
@property BOOL cMemoryOwn;

@end

@implementation MEGAChatContainsMeta

- (instancetype)initWithMegaChatContainsMeta:(MegaChatContainsMeta *)megaChatContainsMeta cMemoryOwn:(BOOL)cMemoryOwn {
    NSParameterAssert(megaChatContainsMeta);
    
    if (self = [super init]) {
        _megaChatContainsMeta = megaChatContainsMeta;
        _cMemoryOwn = cMemoryOwn;
    }
    
    return self;
}

- (void)dealloc {
    if (self.cMemoryOwn){
        delete _megaChatContainsMeta;
    }
}

- (MegaChatContainsMeta *)getCPtr {
    return self.megaChatContainsMeta;
}

- (MEGAChatContainsMetaType)type {
    return self.megaChatContainsMeta ? (MEGAChatContainsMetaType)self.megaChatContainsMeta->getType() : MEGAChatContainsMetaTypeInvalid;
}

- (nullable NSString *)textMessage {
    if (!self.megaChatContainsMeta) return nil;
    const char *ret = self.megaChatContainsMeta->getTextMessage();
    return ret ? [[NSString alloc] initWithUTF8String:ret] : nil;
}

- (nullable MEGAChatRichPreview *)richPreview {
    return self.megaChatContainsMeta->getRichPreview() ? [[MEGAChatRichPreview alloc] initWithMegaChatRichPreview:self.megaChatContainsMeta->getRichPreview()->copy() cMemoryOwn:YES] : nil;
}

- (nullable MEGAChatGeolocation *)geolocation {
    return self.megaChatContainsMeta->getGeolocation() ? [[MEGAChatGeolocation alloc] initWithMegaChatGeolocation:self.megaChatContainsMeta->getGeolocation()->copy() cMemoryOwn:YES] : nil;
}

- (nullable MEGAChatGiphy *)giphy {
    return self.megaChatContainsMeta->getGiphy() ? [[MEGAChatGiphy alloc] initWithMegaChatGiphy:self.megaChatContainsMeta->getGiphy()->copy() cMemoryOwn:YES] : nil;
}

@end
