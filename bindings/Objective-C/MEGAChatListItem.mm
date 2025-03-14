#import "MEGAChatListItem.h"
#import "megachatapi.h"
#import "MEGAChatMessage.h"
#import "MEGAChatRoom.h"
#import "MEGASdk.h"

using namespace megachat;

@interface MEGAChatListItem ()

@property MegaChatListItem *megaChatListItem;
@property BOOL cMemoryOwn;

@end

@implementation MEGAChatListItem

- (instancetype)initWithMegaChatListItem:(megachat::MegaChatListItem *)megaChatListItem cMemoryOwn:(BOOL)cMemoryOwn {
    self = [super init];
    
    if (self != nil) {
        _megaChatListItem = megaChatListItem;
        _cMemoryOwn = cMemoryOwn;
    }
    
    return self;
}

- (void)dealloc {
    if (self.cMemoryOwn){
        delete _megaChatListItem;
    }
}

- (MegaChatListItem *)getCPtr {
    return self.megaChatListItem;
}

- (NSString *)description {
    NSString *changes      = [MEGAChatListItem stringForChangeType:self.changes];
    NSString *active       = self.isActive ? @"YES" : @"NO";
    NSString *group        = self.isGroup ? @"YES" : @"NO";
    NSString *publicChat   = self.isPublicChat ? @"YES" : @"NO";
    NSString *preview      = self.preview ? @"YES" : @"NO";
    NSString *ownPrivilege = [MEGAChatListItem stringForOwnPrivilege:self.ownPrivilege];
    NSString *type         = [MEGAChatListItem stringForMessageType:self.lastMessageType];
    NSString *base64ChatId = [MEGASdk base64HandleForUserHandle:self.chatId];
    
#ifdef DEBUG
    return [NSString stringWithFormat:@"<%@: chatId=%@, title=%@, changes=%@, last message=%@, last date=%@, last type=%@, own privilege=%@, unread=%ld, group=%@, active=%@, public chat=%@, preview=%@>",
            [self class], base64ChatId, self.title, changes, self.lastMessage, self.lastMessageDate, type, ownPrivilege, (long)self.unreadCount, group, active, publicChat, preview];
#else
    return [NSString stringWithFormat:@"<%@: chatId=%@, changes=%@, last date=%@, last type=%@, own privilege=%@, unread=%ld, group=%@, active=%@, public chat=%@, preview=%@>",
            [self class], base64ChatId, changes, self.lastMessageDate, type, ownPrivilege, (long)self.unreadCount, group, active, publicChat, preview];
#endif

    
}

- (uint64_t)chatId {
    return self.megaChatListItem ? self.megaChatListItem->getChatId() : MEGACHAT_INVALID_HANDLE;
}

- (NSString *)title {
    if (!self.megaChatListItem) return nil;
    const char *ret = self.megaChatListItem->getTitle();
    return ret ? [[NSString alloc] initWithUTF8String:ret] : nil;
}

- (MEGAChatListItemChangeType)changes {
    return (MEGAChatListItemChangeType) (self.megaChatListItem ? self.megaChatListItem->getChanges() : 0x00);
}

- (MEGAChatRoomPrivilege)ownPrivilege {
    return self.megaChatListItem ? (MEGAChatRoomPrivilege)self.megaChatListItem->getOwnPrivilege() : MEGAChatRoomPrivilegeUnknown;
}

- (NSInteger)unreadCount {
    return self.megaChatListItem ? self.megaChatListItem->getUnreadCount() : 0;
}

- (BOOL)isGroup {
    return self.megaChatListItem ? self.megaChatListItem->isGroup() : NO;
}

- (BOOL)isPublicChat {
    return self.megaChatListItem ? self.megaChatListItem->isPublic() : NO;
}

- (BOOL)isPreview {
    return self.megaChatListItem ? self.megaChatListItem->isPreview() : NO;
}

- (uint64_t)peerHandle {
    return self.megaChatListItem ? self.megaChatListItem->getPeerHandle() : MEGACHAT_INVALID_HANDLE;
}

- (BOOL)isActive {
    return self.megaChatListItem ? self.megaChatListItem->isActive() : NO;
}

- (BOOL)isDeleted {
    return self.megaChatListItem ? self.megaChatListItem->isDeleted() : NO;
}

- (BOOL)isMeeting {
    return self.megaChatListItem ? self.megaChatListItem->isMeeting() : NO;
}

- (BOOL)isNoteToSelf {
    return self.megaChatListItem ? self.megaChatListItem->isNoteToSelf() : NO;
}

- (NSUInteger)previewersCount {
    return self.megaChatListItem ? self.megaChatListItem->getNumPreviewers() : 0;
}

- (NSString *)lastMessage {
    if (!self.megaChatListItem) return nil;
    const char *ret = self.megaChatListItem->getLastMessage();
    return ret ? [[NSString alloc] initWithUTF8String:ret] : nil;
}

- (uint64_t)lastMessageId {
    return self.megaChatListItem ? self.megaChatListItem->getLastMessageId() : MEGACHAT_INVALID_HANDLE;
}

- (MEGAChatMessageType)lastMessageType {
    return (MEGAChatMessageType) (self.megaChatListItem ? self.megaChatListItem->getLastMessageType() : 0);
}

- (uint64_t)lastMessageSender {
    return self.megaChatListItem ? self.megaChatListItem->getLastMessageSender() : MEGACHAT_INVALID_HANDLE;
}

- (NSDate *)lastMessageDate {
    return self.megaChatListItem ? [[NSDate alloc] initWithTimeIntervalSince1970:self.megaChatListItem->getLastTimestamp()] : nil;
}

- (MEGAChatMessageType)lastMessagePriv {
    return (MEGAChatMessageType) (self.megaChatListItem ? self.megaChatListItem->getLastMessagePriv() : 0);
}

- (uint64_t)lastMessageHandle {
    return self.megaChatListItem ? self.megaChatListItem->getLastMessageHandle() : MEGACHAT_INVALID_HANDLE;
}

- (BOOL)hasChangedForType:(MEGAChatListItemChangeType)changeType {
    return self.megaChatListItem ? self.megaChatListItem->hasChanged((int) changeType) : NO;
}

+ (NSString *)stringForChangeType:(MEGAChatListItemChangeType)changeType {
    NSString *result;
    switch (changeType) {
        case MEGAChatListItemChangeTypeStatus:
            result = @"Status";
            break;
        case MEGAChatListItemChangeTypeOwnPrivilege:
            result = @"Own Privilege";
            break;
        case MEGAChatListItemChangeTypeUnreadCount:
            result = @"Unread count";
            break;
        case MEGAChatListItemChangeTypeParticipants:
            result = @"Participants";
            break;
        case MEGAChatListItemChangeTypeTitle:
            result = @"Title";
            break;
        case MEGAChatListItemChangeTypeClosed:
            result = @"Closed";
            break;
        case MEGAChatListItemChangeTypeLastMsg:
            result = @"Last message";
            break;
        case MEGAChatListItemChangeTypeLastTs:
            result = @"Last timestamp";
            break;
        case MEGAChatListItemChangeTypeArchived:
            result = @"Archived";
            break;
        case MEGAChatListItemChangeTypeCall:
            result = @"Call";
            break;
        case MEGAChatListItemChangeTypeChatMode:
            result = @"Chat mode";
            break;
        case MEGAChatListItemChangeTypeUpdatePreviewers:
            result = @"Update previewers";
            break;
        case MEGAChatListItemChangeTypePreviewClosed:
            result = @"Preview closed";
            break;
        case MEGAChatListItemChangeTypeDelete:
            result = @"Delete";
            break;
            
        default:
            result = @"Default";
            break;
    }
    return result;
}

+ (NSString *)stringForOwnPrivilege:(MEGAChatRoomPrivilege)ownPrivilege {
    NSString *result;
    
    switch (ownPrivilege) {
        case MEGAChatRoomPrivilegeUnknown:
            result = @"Unknown";
            break;
        case MEGAChatRoomPrivilegeRm:
            result = @"Removed";
            break;
        case MEGAChatRoomPrivilegeRo:
            result = @"Read only";
            break;
        case MEGAChatRoomPrivilegeStandard:
            result = @"Standard";
            break;
        case MEGAChatRoomPrivilegeModerator:
            result = @"Moderator";
            break;
            
        default:
            result = @"Default";
            break;
    }
    return result;
}

+ (NSString *)stringForMessageType:(MEGAChatMessageType)type {
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
        case MEGAChatMessageTypeAttachment:
            result = @"Attachment";
            break;
        case MEGAChatMessageTypeSetRetentionTime:
            result = @"Set Retention Time";
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
            
        default:
            result = @"Default";
            break;
    }
    return result;
}

@end
