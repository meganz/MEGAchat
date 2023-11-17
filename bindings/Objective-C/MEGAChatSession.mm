
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

- (BOOL)isOnHold {
    return self.megaChatSession ? self.megaChatSession->isOnHold() : NO;
}

- (MEGAChatSessionChange)changes {
    return (MEGAChatSessionChange) (self.megaChatSession ? self.megaChatSession->getChanges() : MEGAChatSessionChangeNoChanges);
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

- (BOOL)hasCamera {
    return self.megaChatSession ? self.megaChatSession->hasCamera() : NO;
}

- (BOOL)isLowResCamera {
    return self.megaChatSession ? self.megaChatSession->isLowResCamera() : NO;
}

- (BOOL)isHiResCamera {
    return self.megaChatSession ? self.megaChatSession->isHiResCamera() : NO;
}

- (BOOL)hasScreenShare {
    return self.megaChatSession ? self.megaChatSession->hasScreenShare() : NO;
}

- (BOOL)isLowResScreenShare {
    return self.megaChatSession ? self.megaChatSession->isLowResScreenShare() : NO;
}

- (BOOL)isHiResScreenShare {
    return self.megaChatSession ? self.megaChatSession->isHiResScreenShare() : NO;
}

- (BOOL)hasChanged:(MEGAChatSessionChange)change {
    return self.megaChatSession ? self.megaChatSession->hasChanged((int)change) : NO;
}

- (BOOL)isModerator {
    return self.megaChatSession ? self.megaChatSession->isModerator() : NO;
}

- (BOOL)isAudioDetected {
    return self.megaChatSession ? self.megaChatSession->isAudioDetected() : NO;
}

- (BOOL)isRecording {
    return self.megaChatSession ? self.megaChatSession->isRecording() : NO;
}

- (BOOL)isSpeakAllowed {
    return self.megaChatSession ? self.megaChatSession->isSpeakAllowed() : NO;
}

- (BOOL)hasSpeakPermission {
    return self.megaChatSession ? self.megaChatSession->hasSpeakPermission() : NO;
}

- (BOOL)hasPendingSpeakRequest {
    return self.megaChatSession ? self.megaChatSession->hasPendingSpeakRequest() : NO;
}

- (NSString *)description {
    NSString *peerId = [MEGASdk base64HandleForUserHandle:self.peerId];
    NSString *clientId = [MEGASdk base64HandleForUserHandle:self.clientId];
    NSString *isModerator = self.isModerator ? @"YES" : @"NO";
    NSString *hasAudio = self.hasAudio ? @"ON" : @"OFF";
    NSString *hasVideo = self.hasVideo ? @"ON" : @"OFF";
    NSString *changes = [self stringForChanges];
    NSString *isHighResVideo = self.isHighResVideo ? @"YES" : @"NO";
    NSString *isLowResVideo = self.isLowResVideo ? @"YES" : @"NO";
    NSString *canReceiveVideoHiRes = self.canReceiveVideoHiRes ? @"YES" : @"NO";
    NSString *canReceiveVideoLowRes = self.canReceiveVideoLowRes ? @"YES" : @"NO";
    NSString *audioDetected = self.isAudioDetected ? @"YES" : @"NO";
    NSString *isRecording = self.isRecording ? @"YES" : @"NO";
    NSString *isSpeakAllowed = self.isSpeakAllowed ? @"YES" : @"NO";
    NSString *hasSpeakPermission = self.hasSpeakPermission ? @"YES" : @"NO";
    NSString *hasPendingSpeakRequest = self.hasPendingSpeakRequest ? @"YES" : @"NO";

    return [NSString stringWithFormat:@"<%@: peerId=%@; clientId=%@; isModerator=%@; hasAudio=%@; hasVideo=%@; changes=%@; isHighResVideo: %@, isLowResVideo: %@, canReceiveVideoHiRes: %@, canReceiveVideoLowRes: %@, audioDetected=%@, isRecording=%@, isSpeakAllowed=%@, hasSpeakPermission=%@, hasPendingSpeakRequest=%@>", self.class, peerId, clientId, isModerator, hasAudio, hasVideo, changes, isHighResVideo, isLowResVideo, canReceiveVideoHiRes, canReceiveVideoLowRes, audioDetected, isRecording, isSpeakAllowed, hasSpeakPermission, hasPendingSpeakRequest];
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
