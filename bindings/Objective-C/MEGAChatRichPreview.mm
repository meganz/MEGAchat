
#import "MEGAChatRichPreview.h"

#import "megachatapi.h"

using namespace megachat;

@interface MEGAChatRichPreview ()

@property MegaChatRichPreview *megaChatRichPreview;
@property BOOL cMemoryOwn;

@end

@implementation MEGAChatRichPreview

- (instancetype)initWithMegaChatRichPreview:(MegaChatRichPreview *)megaChatRichPreview cMemoryOwn:(BOOL)cMemoryOwn {
    NSParameterAssert(megaChatRichPreview);
    
    if (self = [super init]) {
        _megaChatRichPreview = megaChatRichPreview;
        _cMemoryOwn = cMemoryOwn;
    }
    
    return self;
}

- (void)dealloc {
    if (self.cMemoryOwn){
        delete _megaChatRichPreview;
    }
}

- (MegaChatRichPreview *)getCPtr {
    return self.megaChatRichPreview;
}

- (NSString *)text {
    if (!self.megaChatRichPreview) return nil;
    const char *ret = self.megaChatRichPreview->getText();
    return ret ? [[NSString alloc] initWithUTF8String:ret] : nil;
}

- (NSString *)title {
    if (!self.megaChatRichPreview) return nil;
    const char *ret = self.megaChatRichPreview->getTitle();
    return ret ? [[NSString alloc] initWithUTF8String:ret] : nil;
}

- (NSString *)previewDescription {
    if (!self.megaChatRichPreview) return nil;
    const char *ret = self.megaChatRichPreview->getDescription();
    return ret ? [[NSString alloc] initWithUTF8String:ret] : nil;
}

- (NSString *)image {
    if (!self.megaChatRichPreview) return nil;
    const char *ret = self.megaChatRichPreview->getImage();
    return ret ? [[NSString alloc] initWithUTF8String:ret] : nil;
}

- (NSString *)imageFormat {
    if (!self.megaChatRichPreview) return nil;
    const char *ret = self.megaChatRichPreview->getImageFormat();
    return ret ? [[NSString alloc] initWithUTF8String:ret] : nil;
}

- (NSString *)icon {
    if (!self.megaChatRichPreview) return nil;
    const char *ret = self.megaChatRichPreview->getIcon();
    return ret ? [[NSString alloc] initWithUTF8String:ret] : nil;
}

- (NSString *)iconFormat {
    if (!self.megaChatRichPreview) return nil;
    const char *ret = self.megaChatRichPreview->getIconFormat();
    return ret ? [[NSString alloc] initWithUTF8String:ret] : nil;
}

- (NSString *)url {
    if (!self.megaChatRichPreview) return nil;
    const char *ret = self.megaChatRichPreview->getUrl();
    return ret ? [[NSString alloc] initWithUTF8String:ret] : nil;
}

@end
