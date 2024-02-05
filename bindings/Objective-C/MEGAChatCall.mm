
#import "MEGAChatCall.h"
#import "megachatapi.h"
#import "MEGASdk.h"
#import "MEGAHandleList+init.h"
#import "MEGAChatSession+init.h"
#import "MEGAChatWaitingRoom+init.h"

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
    NSString *ringing = [self isRinging] ? @"YES" : @"NO";
    return [NSString stringWithFormat:@"<%@: status=%@, chatId=%@, callId=%@, uuid=%@, changes=%ld, notificationType=%ld, duration=%lld, initial ts=%lld, final ts=%lld, local: audio %@ video %@, term code=%@, ringing %@, participants: %@, numParticipants: %ld>", [self class], status, base64ChatId, base64CallId, self.uuid.UUIDString, self.changes, self.notificationType, self.duration, self.initialTimeStamp, self.finalTimeStamp, localAudio, localVideo, termCode, ringing, self.participants, (long)self.numParticipants];
}

- (uint64_t)chatId {
    return self.megaChatCall ? self.megaChatCall->getChatid() : MEGACHAT_INVALID_HANDLE;
}

- (uint64_t)callId {
    return self.megaChatCall ? self.megaChatCall->getCallId() : MEGACHAT_INVALID_HANDLE;
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

- (MEGAChatCallStatus)status {
    return (MEGAChatCallStatus) (self.megaChatCall ? self.megaChatCall->getStatus() : 0);
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

- (MEGAChatCallTermCode)termCode {
    return (MEGAChatCallTermCode) (self.megaChatCall ? self.megaChatCall->getTermCode() : 0);
}

- (MEGAChatCallNotificationType)notificationType {
    return self.megaChatCall ? MEGAChatCallNotificationType(self.megaChatCall->getNotificationType()) : MEGAChatCallNotificationTypeInvalid;
}

- (MEGAChatCallNetworkQuality)networkQuality {
    return (MEGAChatCallNetworkQuality) (self.megaChatCall ? self.megaChatCall->getNetworkQuality() : 0);
}

- (uint64_t)caller {
    return self.megaChatCall ? self.megaChatCall->getCaller() : MEGACHAT_INVALID_HANDLE;
}

- (uint64_t)auxHandle {
    return self.megaChatCall ? self.megaChatCall->getAuxHandle() : MEGACHAT_INVALID_HANDLE;
}

- (uint64_t)handleWithChange {
    return self.megaChatCall ? self.megaChatCall->getHandle() : MEGACHAT_INVALID_HANDLE;
}

- (uint64_t)peeridCallCompositionChange {
    return self.megaChatCall ? self.megaChatCall->getPeeridCallCompositionChange() : MEGACHAT_INVALID_HANDLE;
}

- (MEGAChatCallCompositionChange)callCompositionChange {
    return (MEGAChatCallCompositionChange) (self.megaChatCall ? self.megaChatCall->getCallCompositionChange() : MEGAChatCallCompositionChangeNoChange);
}

- (NSInteger)numParticipants {
    return self.megaChatCall ? self.megaChatCall->getNumParticipants() : 0;
}

- (MEGAHandleList *)participants {
    return self.megaChatCall ? [[MEGAHandleList alloc] initWithMegaHandleList:self.megaChatCall->getPeeridParticipants() cMemoryOwn:YES] : nil;
}

- (MEGAHandleList *)sessionsClientId {
    return self.megaChatCall ? [[MEGAHandleList alloc] initWithMegaHandleList:self.megaChatCall->getSessionsClientid() cMemoryOwn:YES] : nil;
}

- (BOOL)hasLocalAudio {
    return self.megaChatCall ? self.megaChatCall->hasLocalAudio() : NO;
}

- (BOOL)hasLocalVideo {
    return self.megaChatCall ? self.megaChatCall->hasLocalVideo() : NO;
}

- (BOOL)isOwnModerator {
    return self.megaChatCall ? self.megaChatCall->isOwnModerator() : NO;
}

- (BOOL)isAudioDetected {
    return self.megaChatCall ? self.megaChatCall->isAudioDetected() : NO;
}

- (BOOL)isRinging {
    return self.megaChatCall ? self.megaChatCall->isRinging() : NO;
}

- (BOOL)isOnHold {
    return self.megaChatCall ? self.megaChatCall->isOnHold() : NO;
}

- (BOOL)isIgnored {
    return self.megaChatCall ? self.megaChatCall->isIgnored() : NO;
}

- (BOOL)isIncoming {
    return self.megaChatCall ? self.megaChatCall->isIncoming() : NO;
}

- (BOOL)isOutgoing {
    return self.megaChatCall ? self.megaChatCall->isOutgoing() : NO;
}

- (BOOL)isSpeakRequestEnabled {
    return self.megaChatCall ? self.megaChatCall->isSpeakRequestEnabled() : NO;
}

- (BOOL)isOwnClientCaller {
    return self.megaChatCall ? self.megaChatCall->isOwnClientCaller() : NO;
}

- (BOOL)isSpeakPermissionFlagEnabled {
    return self.megaChatCall ? self.megaChatCall->getFlag() : NO;
}

- (MEGAHandleList *)moderators {
    return self.megaChatCall->getModerators() ? [[MEGAHandleList alloc] initWithMegaHandleList:self.megaChatCall->getModerators()->copy() cMemoryOwn: YES] : nil;
}

- (MEGAChatWaitingRoomStatus)waitingRoomJoiningStatus {
    return self.megaChatCall ? MEGAChatWaitingRoomStatus(self.megaChatCall->getWrJoiningState()) : MEGAChatWaitingRoomStatusUnknown;
}

- (MEGAChatWaitingRoom *)waitingRoom {
    return self.megaChatCall->getWaitingRoom() ? [[MEGAChatWaitingRoom alloc] initWithMegaChatWaitingRoom:self.megaChatCall->getWaitingRoom()->copy() cMemoryOwn:YES] : nil;
}

- (MEGAHandleList *)waitingRoomHandleList {
    return self.megaChatCall->getHandleList() ? [[MEGAHandleList alloc] initWithMegaHandleList:self.megaChatCall->getHandleList()->copy() cMemoryOwn: YES] : nil;
}

- (MEGAHandleList *)speakersList {
    return self.megaChatCall->getSpeakersList() ? [[MEGAHandleList alloc] initWithMegaHandleList:self.megaChatCall->getSpeakersList()->copy() cMemoryOwn: YES] : nil;
}

- (MEGAHandleList *)speakRequestsList {
    return self.megaChatCall->getSpeakRequestsList() ? [[MEGAHandleList alloc] initWithMegaHandleList:self.megaChatCall->getSpeakRequestsList()->copy() cMemoryOwn: YES] : nil;
}

- (BOOL)hasChangedForType:(MEGAChatCallChangeType)changeType {
    return self.megaChatCall ? self.megaChatCall->hasChanged((int)changeType) : NO;
}

- (MEGAHandleList *)sessionsClientIdByUserHandle:(uint64_t)userHandle {
    return self.megaChatCall ? [[MEGAHandleList alloc] initWithMegaHandleList:self.megaChatCall->getSessionsClientidByUserHandle(userHandle) cMemoryOwn:YES] : nil;
}

- (BOOL)hasUserSpeakPermission:(uint64_t)userHandle {
    return self.megaChatCall ? self.megaChatCall->hasUserSpeakPermission(userHandle) : NO;
}

- (BOOL)hasUserPendingSpeakRequest:(uint64_t)userHandle {
    return self.megaChatCall ? self.megaChatCall->hasUserPendingSpeakRequest(userHandle) : NO;
}

- (nullable MEGAChatSession *)sessionForClientId:(uint64_t)clientId {
    return self.megaChatCall ? [[MEGAChatSession alloc] initWithMegaChatSession:self.megaChatCall->getMegaChatSession(clientId) cMemoryOwn:NO] : nil;
}

- (NSString *)termcodeString:(MEGAChatCallTermCode)termcode {
    if (!self.megaChatCall) return @"";

    const char *val = self.megaChatCall->termcodeToString((int)termcode);
    if (!val) return @"";
    return @(val);
}

- (NSString *)genericMessage {
    if (!self.megaChatCall) return @"";

    const char *val = self.megaChatCall->getGenericMessage();
    if (!val) return @"";
    return @(val);
}

+ (NSString *)stringForStatus:(MEGAChatCallStatus)status {
    NSString *result;
    switch (status) {
        case MEGAChatCallStatusInitial:
            result = @"Initial";
            break;
        case MEGAChatCallStatusUserNoPresent:
            result = @"User no present";
            break;
        case MEGAChatCallStatusConnecting:
            result = @"Connecting";
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
        default:
            result = @"Undefined";
            break;
    }
    return result;
}

+ (NSString *)stringForTermCode:(MEGAChatCallTermCode)termCode {
    NSString *result;
    switch (termCode) {
        case MEGAChatCallTermCodeInvalid:
            result = @"Invalid";
            break;
        case MEGAChatCallTermCodeUserHangup:
            result = @"User hangup";
            break;
        case MEGAChatCallTermCodeTooManyParticipants:
            result = @"Too many participants";
            break;
        case MEGAChatCallTermCodeCallReject:
            result = @"Call reject";
            break;
        case MEGAChatCallTermCodeError:
            result = @"Error";
            break;
        case MEGAChatCallTermCodeNoParticipate:
            result = @"Removed from chatroom";
            break;
        case MEGAChatCallTermCodeTooManyClients:
            result = @"Too many clients";
            break;
        case MEGAChatCallTermCodeProtocolVersion:
            result = @"Protocol version";
            break;
        case MEGAChatCallTermCodeKicked:
            result = @"Kicked";
            break;
        case MEGAChatCallTermCodeWaitingRoomTimeout:
            result = @"Waiting room timeout";
            break;
    }
    return result;
}

@end
