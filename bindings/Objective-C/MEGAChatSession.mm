
#import "MEGAChatSession.h"
#import "megachatapi.h"
#import "MEGAChatCall.h"
#import "MEGASdk.h"

using namespace megachat;

@interface MEGAChatSession ()

@property MegaChatSession *megaChatSession;
@property BOOL cMemoryOwn;

@end

@implementation MEGAChatSession

- (instancetype)initWithMegaChatSession:(MegaChatSession *)megaChatSession cMemoryOwn:(BOOL)cMemoryOwn {
    NSParameterAssert(megaChatSession);
    
    if (self = [super init]) {
        _megaChatSession = megaChatSession;
        _cMemoryOwn = cMemoryOwn;
    }
    
    return self;
}

- (void)dealloc {
    if (self.cMemoryOwn){
        delete _megaChatSession;
    }
}

- (instancetype)clone {
    return self.megaChatSession ? [[MEGAChatSession alloc] initWithMegaChatSession:self.megaChatSession cMemoryOwn:YES] : nil;
}

- (MegaChatSession *)getCPtr {
    return self.megaChatSession;
}

- (MEGAChatSessionStatus)status {
    return (MEGAChatSessionStatus) (self.megaChatSession ? self.megaChatSession->getStatus() : 0);
}

- (MEGAChatSessionTermCode)termCode {
    return (MEGAChatSessionTermCode) (self.megaChatSession ? self.megaChatSession->getTermCode(): -1);
}

- (BOOL)hasAudio {
    return self.megaChatSession ? self.megaChatSession->hasAudio() : NO;
}

- (BOOL)hasVideo {
    return self.megaChatSession ? self.megaChatSession->hasVideo() : NO;
}

- (uint64_t)peerId {
    return self.megaChatSession ? self.megaChatSession->getPeerid() : 0;
}

- (uint64_t)clientId {
    return self.megaChatSession ? self.megaChatSession->getClientid() : 0;
}

- (BOOL)audioDetected {
    return self.megaChatSession ? self.megaChatSession->isAudioDetected() : NO;
}

- (BOOL)isOnHold {
    return self.megaChatSession ? self.megaChatSession->isOnHold() : NO;
}

- (NSInteger)changes {
    return self.megaChatSession ? self.megaChatSession->getChanges() : 0;
}

- (BOOL)isHighResVideo {
    return self.megaChatSession ? self.megaChatSession->isHiResVideo() : NO;
}

- (BOOL)isLowResVideo {
    return self.megaChatSession ? self.megaChatSession->isLowResVideo() : NO;
}

- (BOOL)canReceiveVideoHiRes {
    return self.megaChatSession ? self.megaChatSession->canRecvVideoHiRes() : NO;
}

- (BOOL)canReceiveVideoLowRes {
    return self.megaChatSession ? self.megaChatSession->canRecvVideoLowRes() : NO;
}

- (BOOL)hasChanged:(MEGAChatSessionChange)change {
    return self.megaChatSession ? self.megaChatSession->hasChanged((int)change) : NO;
}

- (NSString *)description {
    NSString *peerId = [MEGASdk base64HandleForUserHandle:self.peerId];
    NSString *clientId = [MEGASdk base64HandleForUserHandle:self.clientId];
    NSString *hasAudio = self.hasAudio ? @"ON" : @"OFF";
    NSString *hasVideo = self.hasVideo ? @"ON" : @"OFF";
    NSString *audioDetected = self.audioDetected ? @"YES" : @"NO";
    NSString *changes = [self stringForChanges];
    NSString *isHighResVideo = self.isHighResVideo ? @"YES" : @"NO";
    NSString *isLowResVideo = self.isLowResVideo ? @"YES" : @"NO";
    NSString *canReceiveVideoHiRes = self.canReceiveVideoHiRes ? @"YES" : @"NO";
    NSString *canReceiveVideoLowRes = self.canReceiveVideoLowRes ? @"YES" : @"NO";
    return [NSString stringWithFormat:@"<%@: peerId=%@; clientId=%@; hasAudio=%@; hasVideo=%@; changes=%@; audioDetected=%@, isHighResVideo: %@, isLowResVideo: %@, canReceiveVideoHiRes: %@, canReceiveVideoLowRes: %@>", self.class, peerId, clientId, hasAudio, hasVideo, changes, audioDetected, isHighResVideo, isLowResVideo, canReceiveVideoHiRes, canReceiveVideoLowRes];
}

- (NSString *)stringForChanges {
    NSString *changes = @"";
    if ([self hasChanged:MEGAChatSessionChangeNoChanges]) {
        changes = [changes stringByAppendingString:@" | NO CHANGES"];
    }
    if ([self hasChanged:MEGAChatSessionChangeStatus]) {
        switch (self.status) {
            case MEGAChatSessionStatusDestroyed:
                changes = [changes stringByAppendingString:@" | STATUS DESTROYED"];
                break;
                
            case MEGAChatSessionStatusInvalid:
                changes = [changes stringByAppendingString:@" | STATUS INVALID"];
                break;
                
            case MEGAChatSessionStatusInProgress:
                changes = [changes stringByAppendingString:@" | STATUS IN PROGRESS"];
                break;
        }
    }
    if ([self hasChanged:MEGAChatSessionChangeRemoteAvFlags]) {
        changes = [changes stringByAppendingString:@" | AV FLAGS"];
    }
    if ([self hasChanged:MEGAChatSessionChangeSpeakRequested]) {
        changes = [changes stringByAppendingString:@" | SPEAK REQUESTED"];
    }
    if ([self hasChanged:MEGAChatSessionChangeAudioLevel]) {
        changes = [changes stringByAppendingString:@" | AUDIO LEVEL"];
    }
    if ([self hasChanged:MEGAChatSessionChangeOnLowRes]) {
        changes = [changes stringByAppendingString:@" | ON LOW RES"];
    }
    if ([self hasChanged:MEGAChatSessionChangeOnHiRes]) {
        changes = [changes stringByAppendingString:@" | ON HI RES"];
    }
    if ([self hasChanged:MEGAChatSessionChangeOnHold]) {
        changes = [changes stringByAppendingString:@" | ON HOLD"];
    }
    
    if (changes.length < 4) {
        return changes;
    } else {
        return [changes substringFromIndex:3];
    }
}

@end
