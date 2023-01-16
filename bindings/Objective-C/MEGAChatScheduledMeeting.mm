
#import "MEGAChatScheduledMeeting.h"
#import "megachatapi.h"

#import "MEGAChatScheduledFlags+init.h"
#import "MEGAChatScheduledRules+init.h"

using namespace megachat;

@interface MEGAChatScheduledMeeting()

@property MegaChatScheduledMeeting *megaChatScheduledMeeting;
@property BOOL cMemoryOwn;

@end

@implementation MEGAChatScheduledMeeting

- (instancetype)initWithMegaChatScheduledMeeting:(megachat::MegaChatScheduledMeeting *)megaChatScheduledMeeting cMemoryOwn:(BOOL)cMemoryOwn {
    self = [super init];
    
    if (self != nil) {
        _megaChatScheduledMeeting = megaChatScheduledMeeting;
        _cMemoryOwn = cMemoryOwn;
    }
    
    return self;
}

- (void)dealloc {
    if (self.cMemoryOwn){
        delete _megaChatScheduledMeeting;
    }
}

- (MegaChatScheduledMeeting *)getCPtr {
    return self.megaChatScheduledMeeting;
}

- (instancetype)clone {
    return self.megaChatScheduledMeeting ? [[MEGAChatScheduledMeeting alloc] initWithMegaChatScheduledMeeting:self.megaChatScheduledMeeting cMemoryOwn:YES] : nil;
}

- (BOOL)isCancelled {
    return self.megaChatScheduledMeeting ? self.megaChatScheduledMeeting->cancelled() : NO;
}

- (BOOL)isNew {
    return self.megaChatScheduledMeeting ? self.megaChatScheduledMeeting->isNew() : NO;
}

- (BOOL)isDeleted {
    return self.megaChatScheduledMeeting ? self.megaChatScheduledMeeting->isDeleted() : NO;
}

- (uint64_t)chatId {
    return self.megaChatScheduledMeeting ? self.megaChatScheduledMeeting->chatId() : MEGACHAT_INVALID_HANDLE;
}

- (uint64_t)scheduledId {
    return self.megaChatScheduledMeeting ? self.megaChatScheduledMeeting->schedId() : MEGACHAT_INVALID_HANDLE;
}

- (uint64_t)parentScheduledId {
    return self.megaChatScheduledMeeting ? self.megaChatScheduledMeeting->parentSchedId() : MEGACHAT_INVALID_HANDLE;
}

- (uint64_t)organizerUserId {
    return self.megaChatScheduledMeeting ? self.megaChatScheduledMeeting->organizerUserId() : MEGACHAT_INVALID_HANDLE;
}

- (NSString *)timezone {
    const char *val = self.megaChatScheduledMeeting->timezone();
    if (!val) return nil;
    return @(val);
}

- (uint64_t)startDateTime {
    return self.megaChatScheduledMeeting->startDateTime();
}

- (uint64_t)endDateTime {
    return self.megaChatScheduledMeeting->endDateTime();
}

- (NSString *)title {
    const char *val = self.megaChatScheduledMeeting->title();
    if (!val) return nil;
    return @(val);
}

- (NSString *)description {
    const char *val = self.megaChatScheduledMeeting->description();
    if (!val) return nil;
    return @(val);
}

- (NSString *)attributes {
    const char *val = self.megaChatScheduledMeeting->attributes();
    if (!val) return nil;
    return @(val);
}

- (uint64_t)overrides {
    return self.megaChatScheduledMeeting->overrides();
}

- (MEGAChatScheduledFlags *)flags {
    if (!self.megaChatScheduledMeeting) return nil;
    return [MEGAChatScheduledFlags.alloc initWithMegaChatScheduledFlags:self.megaChatScheduledMeeting->flags() cMemoryOwn:YES];
}

- (MEGAChatScheduledRules *)rules {
    if (!self.megaChatScheduledMeeting) return nil;
    return [MEGAChatScheduledRules.alloc initWithMegaChatScheduledRules:self.megaChatScheduledMeeting->rules() cMemoryOwn:YES];
}

- (BOOL)hasChangedForType:(MEGAChatScheduledMeetingChangeType)changeType {
    if (!self.megaChatScheduledMeeting) return NO;
    return self.megaChatScheduledMeeting->hasChanged(changeType);
}

@end
