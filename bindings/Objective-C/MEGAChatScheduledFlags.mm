
#import "MEGAChatScheduledFlags.h"
#import "megachatapi.h"

using namespace megachat;

@interface MEGAChatScheduledFlags()

@property MegaChatScheduledFlags *megaChatScheduledFlags;
@property BOOL cMemoryOwn;

@end

@implementation MEGAChatScheduledFlags

- (instancetype)initWithSendEmails:(BOOL)sendEmails {
    self = [super init];
    
    MegaChatScheduledFlags *megaChatScheduledFlags = MegaChatScheduledFlags::createInstance();
    megaChatScheduledFlags->setSendEmails(sendEmails);
    
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

- (BOOL)emailsEnabled {
    if (!self.megaChatScheduledFlags) { return NO; };
    return self.megaChatScheduledFlags->sendEmails();
}

- (void)setEmailsEnabled:(BOOL)sendEmails {
    if (self.megaChatScheduledFlags) {
        self.megaChatScheduledFlags->setSendEmails(sendEmails);
    };
}

- (void)reset {
    if (self.megaChatScheduledFlags) {
        self.megaChatScheduledFlags->reset();
    };
}

@end
