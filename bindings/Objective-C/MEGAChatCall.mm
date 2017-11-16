
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
    NSString *localAudio = [self hasLocalAudio] ? @"ON" : @"OFF";
    NSString *localVideo = [self hasLocalVideo] ? @"ON" : @"OFF";
    NSString *remoteAudio = [self hasRemoteAudio] ? @"ON" : @"OFF";
    NSString *remoteVideo = [self hasRemoteVideo] ? @"ON" : @"OFF";
    return [NSString stringWithFormat:@"<%@: status=%@, chatId=%@, callId=%@, changes=%ld, duration=%lld, initial ts=%lld, final ts=%lld, local: audio %@ video %@, remote: audio %@ video %@>", [self class], status, base64ChatId, base64CallId, self.changes, self.duration, self.initialTimeStamp, self.finalTimeStamp, localAudio, localVideo, remoteAudio, remoteVideo];
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

- (MEGAChatCallChangeType)changes {
    return (MEGAChatCallChangeType) (self.megaChatCall ? self.megaChatCall->getChanges() : 0);
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
    const char *val = self.megaChatCall->getTemporaryError();
    if (!val) return nil;
    
    NSString *ret = [[NSString alloc] initWithUTF8String:val];
    
    delete [] val;
    return ret;
}

- (BOOL)hasLocalAudio {
    return self.megaChatCall ? self.megaChatCall->hasAudio(true) : NO;
}

- (BOOL)hasLocalVideo {
    return self.megaChatCall ? self.megaChatCall->hasVideo(true) : NO;
}

- (BOOL)hasRemoteAudio {
    return self.megaChatCall ? self.megaChatCall->hasAudio(false) : NO;
}

- (BOOL)hasRemoteVideo {
    return self.megaChatCall ? self.megaChatCall->hasVideo(false) : NO;
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
            
        default:
            result = @"Default";
            break;
    }
    return result;
}

@end
