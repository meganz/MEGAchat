
#import "MEGAChatCall.h"
#import "megachatapi.h"
#import "MEGASdk.h"
#import "MEGAHandleList+init.h"
#import "MEGAChatSession+init.h"

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
    NSString *termCode = [MEGAChatCall stringForTermCode:self.termCode];
    NSString *base64ChatId = [MEGASdk base64HandleForUserHandle:self.chatId];
    NSString *base64CallId = [MEGASdk base64HandleForUserHandle:self.callId];
    NSString *localAudio = [self hasLocalAudio] ? @"ON" : @"OFF";
    NSString *localVideo = [self hasLocalVideo] ? @"ON" : @"OFF";
    NSString *remoteAudio = [self hasAudioInitialCall] ? @"ON" : @"OFF";
    NSString *remoteVideo = [self hasVideoInitialCall] ? @"ON" : @"OFF";
    NSString *localTermCode = [self isLocalTermCode] ? @"YES" : @"NO";
    NSString *ringing = [self isRinging] ? @"YES" : @"NO";
    return [NSString stringWithFormat:@"<%@: status=%@, chatId=%@, callId=%@, uuid=%@ changes=%ld, duration=%lld, initial ts=%lld, final ts=%lld, local: audio %@ video %@, remote: audio %@ video %@, term code=%@, local term code %@, ringing %@, sessions: %@, participants: %@, numParticipants: %ld>", [self class], status, base64ChatId, base64CallId, self.uuid.UUIDString, self.changes, self.duration, self.initialTimeStamp, self.finalTimeStamp, localAudio, localVideo, remoteAudio, remoteVideo, termCode, localTermCode, ringing, [self sessionsPeerId], self.participants, (long)self.numParticipants];
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

- (BOOL)hasLocalAudio {
    return self.megaChatCall ? self.megaChatCall->hasLocalAudio() : NO;
}

- (BOOL)hasLocalVideo {
    return self.megaChatCall ? self.megaChatCall->hasLocalVideo() : NO;
}

- (BOOL)hasAudioInitialCall {
    return self.megaChatCall ? self.megaChatCall->hasAudioInitialCall() : NO;
}

- (BOOL)hasVideoInitialCall {
    return self.megaChatCall ? self.megaChatCall->hasVideoInitialCall() : NO;
}

- (BOOL)hasChangedForType:(MEGAChatCallChangeType)changeType {
    return self.megaChatCall ? self.megaChatCall->hasChanged((int)changeType) : NO;
}

- (MEGAChatCallTermCode)termCode {
    return (MEGAChatCallTermCode) (self.megaChatCall ? self.megaChatCall->getTermCode() : 0);
}

- (BOOL)isLocalTermCode {
    return self.megaChatCall ? self.megaChatCall->isLocalTermCode() : NO;
}

- (BOOL)isRinging {
    return self.megaChatCall ? self.megaChatCall->isRinging() : NO;
}

- (uint64_t)peeridCallCompositionChange {
    return self.megaChatCall ? self.megaChatCall->getPeeridCallCompositionChange() : MEGACHAT_INVALID_HANDLE;
}

- (uint64_t)clientidCallCompositionChange {
    return self.megaChatCall ? self.megaChatCall->getClientidCallCompositionChange() : MEGACHAT_INVALID_HANDLE;
}

- (uint64_t)callCompositionChange {
    return self.megaChatCall ? self.megaChatCall->getCallCompositionChange() : MEGACHAT_INVALID_HANDLE;
}

- (MEGAHandleList *)sessionsPeerId {
    return self.megaChatCall ? [[MEGAHandleList alloc] initWithMegaHandleList:self.megaChatCall->getSessionsPeerid() cMemoryOwn:YES] : nil;
}

- (MEGAHandleList *)sessionsClientId {
    return self.megaChatCall ? [[MEGAHandleList alloc] initWithMegaHandleList:self.megaChatCall->getSessionsClientid() cMemoryOwn:YES] : nil;
}

- (MEGAHandleList *)participants {
    return self.megaChatCall ? [[MEGAHandleList alloc] initWithMegaHandleList:self.megaChatCall->getPeeridParticipants() cMemoryOwn:YES] : nil;
}

- (NSInteger)numParticipants {
    return self.megaChatCall ? self.megaChatCall->getNumParticipants(MEGAChatCallConfigurationAnyFlag) : 0;
}

- (BOOL)isOnHold {
    return self.megaChatCall ? self.megaChatCall->isOnHold() : NO;
}

- (NSUUID *)uuid {
    unsigned char tempUuid[128];
    uint64_t tempChatId = self.chatId;
    uint64_t tempCallId = self.callId;
    memcpy(tempUuid, &tempChatId, sizeof(tempChatId));
    memcpy(tempUuid + sizeof(tempChatId), &tempCallId, sizeof(tempCallId));
    
    NSUUID *uuid = [NSUUID.alloc initWithUUIDBytes:tempUuid];
    return uuid;
}

- (NSInteger)numParticipantsWithCallConfiguration:(MEGAChatCallConfiguration)callConfiguration {
    return self.megaChatCall ? self.megaChatCall->getNumParticipants(callConfiguration) : 0;
}

- (MEGAChatSession *)sessionForPeer:(uint64_t)peerId clientId:(uint64_t)clientId {
    return self.megaChatCall ? [[MEGAChatSession alloc] initWithMegaChatSession:self.megaChatCall->getMegaChatSession(peerId, clientId) cMemoryOwn:NO] : nil;
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
        case MEGAChatCallStatusTerminatingUserParticipation:
            result = @"Terminating user participation";
            break;
        case MEGAChatCallStatusDestroyed:
            result = @"Destroyed";
            break;
        case MEGAChatCallStatusUserNoPresent:
            result = @"User no present";
            break;
        case MEGAChatCallStatusReconnecting:
            result = @"Reconnecting";
            break;
            
        default:
            result = @"Default";
            break;
    }
    return result;
}

+ (NSString *)stringForTermCode:(MEGAChatCallTermCode)termCode {
    NSString *result;
    switch (termCode) {
        case MEGAChatCallTermCodeUserHangup:
            result = @"User hangup";
            break;
        case MEGAChatCallTermCodeCallReqCancel:
            result = @"Call req cancel";
            break;
        case MEGAChatCallTermCodeCallReject:
            result = @"Call reject";
            break;
        case MEGAChatCallTermCodeAnswerElseWhere:
            result = @"Answer else where";
            break;
        case MEGAChatCallTermCodeRejectElseWhere:
            result = @"Reject else where";
            break;
        case MEGAChatCallTermCodeAnswerTimeout:
            result = @"Answer timeout";
            break;
        case MEGAChatCallTermCodeRingOutTimeout:
            result = @"Ring out timeout";
            break;
        case MEGAChatCallTermCodeAppTerminating:
            result = @"App terminating";
            break;
        case MEGAChatCallTermCodeBusy:
            result = @"Busy";
            break;
        case MEGAChatCallTermCodeNotFinished:
            result = @"Not finished";
            break;
        case MEGAChatCallTermCodeError:
            result = @"Error";
            break;
            
        default:
            result = @"Default";
            break;
    }
    return result;
}

@end
