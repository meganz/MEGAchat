
#import "MEGAChatContainsMeta.h"

#import "megachatapi.h"

#import "MEGAChatRichPreview+init.h"

using namespace megachat;

@interface MEGAChatContainsMeta ()

@property MegaChatContainsMeta *megaChatContainsMeta;
@property BOOL cMemoryOwn;

@end

@implementation MEGAChatContainsMeta

- (instancetype)initWithMegaChatContainsMeta:(MegaChatContainsMeta *)megaChatContainsMeta cMemoryOwn:(BOOL)cMemoryOwn {
    NSParameterAssert(megaChatMessage);
    
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

- (instancetype)clone {
    return self.megaChatContainsMeta ? [[MEGAChatContainsMeta alloc] initWithMegaChatContainsMeta:self.megaChatContainsMeta cMemoryOwn:YES] : nil;
}

- (MegaChatContainsMeta *)getCPtr {
    return self.megaChatContainsMeta;
}

- (MEGAChatContainsMetaType)type {
    return self.megaChatContainsMeta ? (MEGAChatContainsMetaType)self.megaChatContainsMeta->getType() : MEGAChatContainsMetaTypeInvalid;
}

- (MEGAChatRichPreview *)richPreview {
    return self.megaChatContainsMeta ? [[MEGAChatRichPreview alloc] initWithMegaChatRichPreview:self.megaChatContainsMeta->getRichPreview()->copy() cMemoryOwn:YES] : nil;
}

@end
