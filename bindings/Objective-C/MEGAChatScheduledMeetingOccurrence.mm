
#import "MEGAChatScheduledMeetingOccurrence.h"
#import "megachatapi.h"

using namespace megachat;

@interface MEGAChatScheduledMeetingOccurrence()

@property MegaChatScheduledMeetingOccurr *megaChatScheduledMeetingOccurr;
@property BOOL cMemoryOwn;

@end

@implementation MEGAChatScheduledMeetingOccurrence

- (instancetype)initWithMegaChatScheduledMeetingOccurrence:(megachat::MegaChatScheduledMeetingOccurr *)megaChatScheduledMeetingOccurr cMemoryOwn:(BOOL)cMemoryOwn {
    self = [super init];
    
    if (self != nil) {
        _megaChatScheduledMeetingOccurr = megaChatScheduledMeetingOccurr;
        _cMemoryOwn = cMemoryOwn;
    }
    
    return self;
}

- (void)dealloc {
    if (self.cMemoryOwn){
        delete _megaChatScheduledMeetingOccurr;
    }
}

- (MegaChatScheduledMeetingOccurr *)getCPtr {
    return self.megaChatScheduledMeetingOccurr;
}

- (instancetype)clone {
    return self.megaChatScheduledMeetingOccurr ? [[MEGAChatScheduledMeetingOccurrence alloc] initWithMegaChatScheduledMeetingOccurrence:self.megaChatScheduledMeetingOccurr cMemoryOwn:YES] : nil;
}

- (BOOL)isCancelled {
    return self.megaChatScheduledMeetingOccurr ? self.megaChatScheduledMeetingOccurr->cancelled() : NO;
}

- (uint64_t)scheduledId {
    return self.megaChatScheduledMeetingOccurr ? self.megaChatScheduledMeetingOccurr->schedId() : MEGACHAT_INVALID_HANDLE;
}

- (uint64_t)parentScheduledId {
    return self.megaChatScheduledMeetingOccurr ? self.megaChatScheduledMeetingOccurr->parentSchedId() : MEGACHAT_INVALID_HANDLE;
}

- (uint64_t)overrides {
    if (!self.megaChatScheduledMeetingOccurr) return MEGACHAT_INVALID_HANDLE;

    return self.megaChatScheduledMeetingOccurr->overrides();
}

- (NSString *)timezone {
    if (!self.megaChatScheduledMeetingOccurr) return nil;
    
    const char *val = self.megaChatScheduledMeetingOccurr->timezone();
    if (!val) return nil;
    return @(val);
}

- (uint64_t)startDateTime {
    if (!self.megaChatScheduledMeetingOccurr) return MEGACHAT_INVALID_HANDLE;
    return self.megaChatScheduledMeetingOccurr->startDateTime();
}

- (uint64_t)endDateTime {
    if (!self.megaChatScheduledMeetingOccurr) return MEGACHAT_INVALID_HANDLE;
    return self.megaChatScheduledMeetingOccurr->endDateTime();
}

@end
