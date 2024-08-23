
#import "MEGAChatMessage.h"

#import "megachatapi.h"

#import "MEGAChatContainsMeta+init.h"
#import "MEGANodeList+init.h"
#import "MEGAHandleList+init.h"
#import "MEGASdk.h"

using namespace megachat;

@interface MEGAChatMessage ()

@property MegaChatMessage *megaChatMessage;
@property BOOL cMemoryOwn;

@end

@implementation MEGAChatMessage

- (instancetype)initWithMegaChatMessage:(megachat::MegaChatMessage *)megaChatMessage cMemoryOwn:(BOOL)cMemoryOwn {
    NSParameterAssert(megaChatMessage);
    self = [super init];
    
    if (self != nil) {
        _megaChatMessage = megaChatMessage;
        _cMemoryOwn = cMemoryOwn;
    }
    
    return self;
}

- (void)dealloc {
    if (self.cMemoryOwn){
        delete _megaChatMessage;
    }
}

- (MegaChatMessage *)getCPtr {
    return self.megaChatMessage;
}

- (NSString *)description {
    NSString *status = [MEGAChatMessage stringForStatus:self.status];
    NSString *type = [MEGAChatMessage stringForType:self.type];
    NSString *changes = [MEGAChatMessage stringForChangeType:self.changes];
    NSString *code = [MEGAChatMessage stringForCode:self.code];
    NSString *base64MessageId = [MEGASdk base64HandleForUserHandle:self.messageId];
    NSString *base64TemporalId = [MEGASdk base64HandleForUserHandle:self.temporalId];
    NSString *base64userHandle = [MEGASdk base64HandleForUserHandle:self.userHandle];
    NSString *endCallReason = [MEGAChatMessage stringForEndCallReason:self.termCode];

#ifdef DEBUG
    return [NSString stringWithFormat:@"<%@: messageId=%@, temporalId=%@, status=%@, index=%ld, user handle=%@, type=%@, timestamp=%@, content=%@, edited=%@, deleted=%@, editable=%@, management message=%@, userHandleOfAction=%lld, privilege=%ld, changes=%@, code=%@, end call reason=%@>",
            [self class],base64MessageId, base64TemporalId, status,  self.messageIndex, base64userHandle, type, self.timestamp, self.content, @(self.edited), @(self.deleted), @(self.editable), @(self.managementMessage), self.userHandleOfAction, (long)self.privilege, changes, code, endCallReason];
#else
    return [NSString stringWithFormat:@"<%@: messageId=%@, temporalId=%@, status=%@, index=%ld, user handle=%@, type=%@, timestamp=%@, edited=%@, deleted=%@, editable=%@, management message=%@, userHandleOfAction=%lld, privilege=%ld, changes=%@, code=%@, end call reason=%@>",
            [self class],base64MessageId, base64TemporalId, status,  self.messageIndex, base64userHandle, type, self.timestamp, @(self.edited), @(self.deleted), @(self.editable), @(self.managementMessage), self.userHandleOfAction, (long)self.privilege, changes, code, endCallReason];
#endif
}

- (MEGAChatMessageStatus)status {
    return (MEGAChatMessageStatus) (self.megaChatMessage ? self.megaChatMessage->getStatus() : 0);
}

- (uint64_t)messageId {
    return self.megaChatMessage ? self.megaChatMessage->getMsgId() : MEGACHAT_INVALID_HANDLE;
}

- (uint64_t)temporalId {
    return self.megaChatMessage ? self.megaChatMessage->getTempId() : MEGACHAT_INVALID_HANDLE;
}

- (NSInteger)messageIndex {
    return self.megaChatMessage ? self.megaChatMessage->getMsgIndex() : 0;
}

- (uint64_t)userHandle {
    return self.megaChatMessage ? self.megaChatMessage->getUserHandle() : MEGACHAT_INVALID_HANDLE;
}

- (MEGAChatMessageType)type {
    return (MEGAChatMessageType) (self.megaChatMessage ? self.megaChatMessage->getType() : 0);
}

- (BOOL)hasConfirmedReactions {
    return self.megaChatMessage ? self.megaChatMessage->hasConfirmedReactions() : NO;
}

- (NSDate *)timestamp {
    return self.megaChatMessage ? [[NSDate alloc] initWithTimeIntervalSince1970:self.megaChatMessage->getTimestamp()] : nil;
}

- (NSString *)content {
    if (!self.megaChatMessage) return nil;
    const char *ret = self.megaChatMessage->getContent();
    return ret ? [[NSString alloc] initWithUTF8String:ret] : nil;
}

- (BOOL)isEdited {
    return self.megaChatMessage ? self.megaChatMessage->isEdited() : NO;
}

- (BOOL)isDeleted {
    return self.megaChatMessage ? self.megaChatMessage->isDeleted() : NO;
}

- (BOOL)isEditable {
    return self.megaChatMessage ? self.megaChatMessage->isEditable() : NO;
}

- (BOOL)isDeletable {
    return self.megaChatMessage ? self.megaChatMessage->isDeletable() : NO;
}

- (BOOL)isManagementMessage {
    return self.megaChatMessage ? self.megaChatMessage->isManagementMessage() : NO;
}

- (uint64_t)userHandleOfAction {
    return self.megaChatMessage ? self.megaChatMessage->getHandleOfAction() : MEGACHAT_INVALID_HANDLE;
}

- (NSInteger)privilege {
    return self.megaChatMessage ? self.megaChatMessage->getPrivilege() : -2;
}

- (MEGAChatMessageChangeType)changes {
    return (MEGAChatMessageChangeType) (self.megaChatMessage ? self.megaChatMessage->getChanges() : 0x00);
}

- (MEGAChatMessageReason)code {
    return (MEGAChatMessageReason) (self.megaChatMessage ? self.megaChatMessage->getCode() : 0);
}

- (NSUInteger)usersCount {
    return self.megaChatMessage ? self.megaChatMessage->getUsersCount() : 0;
}

- (MEGANodeList *)nodeList {
    return self.megaChatMessage->getMegaNodeList() ? [[MEGANodeList alloc] initWithNodeList:self.megaChatMessage->getMegaNodeList()->copy() cMemoryOwn:YES] : nil;
}

- (MEGAHandleList *)handleList {
    if (!self.megaChatMessage) return nil;
    return self.megaChatMessage->getMegaHandleList() ? [[MEGAHandleList alloc] initWithMegaHandleList:self.megaChatMessage->getMegaHandleList()->copy() cMemoryOwn:YES] : nil;
}

- (NSInteger)duration {
    return self.megaChatMessage ? self.megaChatMessage->getDuration() : 0;
}

- (NSUInteger)retentionTime {
    return self.megaChatMessage ? self.megaChatMessage->getRetentionTime() : 0;
}

- (MEGAChatMessageEndCallReason)termCode {
    return (MEGAChatMessageEndCallReason) (self.megaChatMessage ? self.megaChatMessage->getTermCode() : 0);
}

- (uint64_t)rowId {
    return self.megaChatMessage ? self.megaChatMessage->getRowId() : MEGACHAT_INVALID_HANDLE;
}

- (MEGAChatContainsMeta *)containsMeta {
    return self.megaChatMessage->getContainsMeta() ? [[MEGAChatContainsMeta alloc] initWithMegaChatContainsMeta:self.megaChatMessage->getContainsMeta()->copy() cMemoryOwn:YES] : nil;
}

- (BOOL)hasChangedForType:(MEGAChatMessageChangeType)changeType {
    return self.megaChatMessage ? self.megaChatMessage->hasChanged((int)changeType) : NO;
}

- (uint64_t)userHandleAtIndex:(NSUInteger)index {
    return self.megaChatMessage ? self.megaChatMessage->getUserHandle((unsigned int)index) : MEGACHAT_INVALID_HANDLE;
}

- (NSString *)userNameAtIndex:(NSUInteger)index {
    return self.megaChatMessage ? [[NSString alloc] initWithUTF8String:self.megaChatMessage->getUserName((unsigned int)index)] : nil;
}

- (NSString *)userEmailAtIndex:(NSUInteger)index {
    return self.megaChatMessage ? [[NSString alloc] initWithUTF8String:self.megaChatMessage->getUserEmail((unsigned int)index)] : nil;
}

- (BOOL)hasScheduledMeetingChangeForType:(MEGAChatMessageScheduledMeetingChangeType)changeType {
    return self.megaChatMessage ? self.megaChatMessage->hasSchedMeetingChanged((int)changeType) : NO;
}

+ (NSString *)stringForChangeType:(MEGAChatMessageChangeType)changeType {
    NSString *result;
    switch (changeType) {
        case MEGAChatMessageChangeTypeStatus:
            result = @"Status";
            break;
        case MEGAChatMessageChangeTypeContent:
            result = @"Content";
            break;
        case MEGAChatMessageChangeTypeAccess:
            result = @"Access";
            break;
            
        default:
            result = @"Default";
            break;
    }
    return result;
}
+ (NSString *)stringForStatus:(MEGAChatMessageStatus)status {
    NSString *result;
    switch (status) {
        case MEGAChatMessageStatusUnknown:
            result = @"Unknown";
            break;
        case MEGAChatMessageStatusSending:
            result = @"Sending";
            break;
        case MEGAChatMessageStatusSendingManual:
            result = @"Sending manual";
            break;
        case MEGAChatMessageStatusServerReceived:
            result = @"Server received";
            break;
        case MEGAChatMessageStatusServerRejected:
            result = @"Server rejected";
            break;
        case MEGAChatMessageStatusDelivered:
            result = @"Delivered";
            break;
        case MEGAChatMessageStatusNotSeen:
            result = @"Not seen";
            break;
        case MEGAChatMessageStatusSeen:
            result = @"Seen";
            break;
            
        default:
            result = @"Default";
            break;
    }
    return result;
    
}

+ (NSString *)stringForType:(MEGAChatMessageType)type {
    NSString *result;
    switch (type) {
        case MEGAChatMessageTypeUnknown:
            result = @"Unknown";
            break;
        case MEGAChatMessageTypeInvalid:
            result = @"Invalid";
            break;
        case MEGAChatMessageTypeNormal:
            result = @"Normal";
            break;
        case MEGAChatMessageTypeAlterParticipants:
            result = @"Alter participants";
            break;
        case MEGAChatMessageTypeTruncate:
            result = @"Truncate";
            break;
        case MEGAChatMessageTypePrivilegeChange:
            result = @"Privilege change";
            break;
        case MEGAChatMessageTypeChatTitle:
            result = @"Chat title";
            break;
        case MEGAChatMessageTypeCallEnded:
            result = @"Call ended";
            break;
        case MEGAChatMessageTypeCallStarted:
            result = @"Call started";
            break;
        case MEGAChatMessageTypePublicHandleCreate:
            result = @"Public handle create";
            break;
        case MEGAChatMessageTypePublicHandleDelete:
            result = @"Public handle delete";
            break;
        case MEGAChatMessageTypeSetPrivateMode:
            result = @"Set private mode";
            break;
        case MEGAChatMessageTypeAttachment:
            result = @"Attachment";
            break;
        case MEGAChatMessageTypeRevokeAttachment:
            result = @"Revoke attachment";
            break;
        case MEGAChatMessageTypeContact:
            result = @"Contact";
            break;
        case MEGAChatMessageTypeContainsMeta:
            result = @"Contains meta";
            break;
        case MEGAChatMessageTypeVoiceClip:
            result = @"Voice clip";
            break;
        case MEGAChatMessageTypeScheduledMeeting:
            result = @"Scheduled Meeting";
            break;
        default:
            result = @"Default";
            break;
    }
    return result;
}

+ (NSString *)stringForCode:(MEGAChatMessageReason)code {
    NSString *result;
    switch (code) {
        case MEGAChatMessageReasonPeersChanged:
            result = @"Peers changed";
            break;
        case MEGAChatMessageReasonTooOld:
            result = @"Too old";
            break;
        case MEGAChatMessageReasonGeneralReject:
            result = @"General reject";
            break;
        case MEGAChatMessageReasonNoWriteAccess:
            result = @"No write access";
            break;
        default:
            result = @"Default";
            break;
    }
    return result;
}

+ (NSString *)stringForEndCallReason:(MEGAChatMessageEndCallReason)reason {
    NSString *result;
    switch (reason) {
        case MEGAChatMessageEndCallReasonEnded:
            result = @"Ended";
            break;
        case MEGAChatMessageEndCallReasonRejected:
            result = @"Rejected";
            break;
        case MEGAChatMessageEndCallReasonNoAnswer:
            result = @"No answer";
            break;
        case MEGAChatMessageEndCallReasonFailed:
            result = @"Failed";
            break;
        case MEGAChatMessageEndCallReasonCancelled:
            result = @"Cancelled";
            break;
        case MEGAChatMessageEndCallReasonByModerator:
            result = @"End by moderator";
            break;
        default:
            result = @"Default";
            break;
    }
    return result;
}

@end
