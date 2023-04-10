
#import "MEGAChatScheduledFlags.h"
#import "megachatapi.h"

using namespace megachat;

@interface MEGAChatScheduledFlags()

@property MegaChatScheduledFlags *megaChatScheduledFlags;
@property BOOL cMemoryOwn;

@end

@implementation MEGAChatScheduledFlags

- (instancetype)initWith:(BOOL)emailsDisabled {
    self = [super init];
    
    MegaChatScheduledFlags *megaChatScheduledFlags = MegaChatScheduledFlags::createInstance();
    megaChatScheduledFlags->setEmailsDisabled(emailsDisabled);
    
    return [self initWithMegaChatScheduledFlags:megaChatScheduledFlags cMemoryOwn:YES];
}

- (instancetype)initWithMegaChatScheduledFlags:(MegaChatScheduledFlags *)megaChatScheduledFlags cMemoryOwn:(BOOL)cMemoryOwn {
    self = [super init];
    
    if (self != nil) {
        _megaChatScheduledFlags = megaChatScheduledFlags;
        _cMemoryOwn = cMemoryOwn;
    }
    
    return self;
}

- (void)dealloc {
    if (self.cMemoryOwn){
        delete _megaChatScheduledFlags;
    }
}

- (MegaChatScheduledFlags *)getCPtr {
    return self.megaChatScheduledFlags;
}

- (instancetype)clone {
    return self.megaChatScheduledFlags ? [[MEGAChatScheduledFlags alloc] initWithMegaChatScheduledFlags:self.megaChatScheduledFlags cMemoryOwn:YES] : nil;
}

- (BOOL)isEmpty {
    if (!self.megaChatScheduledFlags) { return NO; };
    return self.megaChatScheduledFlags->isEmpty();
}

- (BOOL)emailsDisabled {
    if (!self.megaChatScheduledFlags) { return NO; };
    return self.megaChatScheduledFlags->emailsDisabled();
}

- (void)setEmailsDisabled:(BOOL)disable {
    if (self.megaChatScheduledFlags) {
        self.megaChatScheduledFlags->setEmailsDisabled(disable);
    };
}

- (void)reset {
    if (self.megaChatScheduledFlags) {
        self.megaChatScheduledFlags->reset();
    };
}

@end
