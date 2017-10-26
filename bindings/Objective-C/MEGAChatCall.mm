
#import "MEGAChatCall.h"
#import "megachatapi.h"
#import "MEGASdk.h"

using namespace megachat;

@interface MEGAChatCall ()

@property MegaChatCall *megaChatCall;
@property BOOL cMemoryOwn;

@end

@implementation MEGAChatCall

- (instancetype)initWithMegaChatCall:(MegaChatCall *)megaChatCall cMemoryOwn:(BOOL)cMemoryOwn {
    self = [super init];
    
    if (self != nil) {
        _megaChatCall = megaChatCall;
        _cMemoryOwn = cMemoryOwn;
    }
    
    return self;
}

- (void)dealloc {
    if (self.cMemoryOwn) {
        delete _megaChatCall;
    }
}

- (instancetype)clone {
    return self.megaChatCall ? [[MEGAChatCall alloc] initWithMegaChatCall:self.megaChatCall cMemoryOwn:YES] : nil;
}

- (MegaChatCall *)getCPtr {
    return self.megaChatCall;
}

- (NSString *)description {
    NSString *status = [MEGAChatCall stringForStatus:self.status];
    NSString *base64ChatId = [MEGASdk base64HandleForUserHandle:self.chatId];
    NSString *base64CallId = [MEGASdk base64HandleForUserHandle:self.callId];
    return [NSString stringWithFormat:@"<%@: status: %@, chatId: %@, callId: %@", [self class], status, base64ChatId, base64CallId];
}

- (MEGAChatCallStatus)status {
    return (MEGAChatCallStatus) (self.megaChatCall ? self.megaChatCall->getStatus() : 0);
}

- (uint64_t)chatId {
    return self.megaChatCall ? self.megaChatCall->getChatid() : MEGACHAT_INVALID_HANDLE;
}

- (uint64_t)callId {
    return self.megaChatCall ? self.megaChatCall->getId() : MEGACHAT_INVALID_HANDLE;
}

- (NSInteger)changes {
    return self.megaChatCall ? self.megaChatCall->getChanges() : 0;
}

- (int64_t)duration {
    return self.megaChatCall ? self.megaChatCall->getDuration() : 0;
}

- (int64_t)finalTimeStamp {
    return self.megaChatCall ? self.megaChatCall->getFinalTimeStamp() : 0;
}

- (int64_t)initialTimeStamp {
    return self.megaChatCall ? self.megaChatCall->getInitialTimeStamp() : 0;
}

- (NSString *)error {
    const char *val = self.megaChatCall->getError();
    if (!val) return nil;
    
    NSString *ret = [[NSString alloc] initWithUTF8String:val];
    
    delete [] val;
    return ret;
}

- (BOOL)hasAudio:(BOOL)local {
    return self.megaChatCall ? self.megaChatCall->hasAudio(local) : NO;
}

- (BOOL)hasVideo:(BOOL)local {
    return self.megaChatCall ? self.megaChatCall->hasVideo(local) : NO;
}

- (BOOL)hasChangedForType:(MEGAChatCallChangeType)changeType {
    return self.megaChatCall ? self.megaChatCall->hasChanged((int)changeType) : NO;
}

+ (NSString *)stringForStatus:(MEGAChatCallStatus)status {
    NSString *result;
    switch (status) {
        case MEGAChatCallStatusInitial:
            result = @"Initial";
            break;
        case MEGAChatCallStatusHasLocalStream:
            result = @"Has local stream";
            break;
        case MEGAChatCallStatusRequestSent:
            result = @"Request sent";
            break;
        case MEGAChatCallStatusRingIn:
            result = @"Ring in";
            break;
        case MEGAChatCallStatusJoining:
            result = @"Joining";
            break;
        case MEGAChatCallStatusInProgress:
            result = @"In progress";
            break;
        case MEGAChatCallStatusTerminating:
            result = @"Terminating";
            break;
        case MEGAChatCallStatusDestroyed:
            result = @"Destroyed";
            break;
        case MEGAChatCallStatusDisconnected:
            result = @"Disconnect";
            break;
            
        default:
            result = @"Default";
            break;
    }
    return result;
}

@end
