#import "MEGAChatRoom.h"
#import "megachatapi.h"
#import "MEGASdk.h"

using namespace megachat;

@interface MEGAChatRoom ()

@property MegaChatRoom *megaChatRoom;
@property BOOL cMemoryOwn;

@end

@implementation MEGAChatRoom

- (instancetype)initWithMegaChatRoom:(MegaChatRoom *)megaChatRoom cMemoryOwn:(BOOL)cMemoryOwn {
    NSParameterAssert(megaChatRoom);
    self = [super init];
    
    if (self != nil) {
        _megaChatRoom = megaChatRoom;
        _cMemoryOwn = cMemoryOwn;
    }
    
    return self;
}

- (void)dealloc {
    if (self.cMemoryOwn){
        delete _megaChatRoom;
    }
}

- (MegaChatRoom *)getCPtr {
    return self.megaChatRoom;
}

- (NSString *)description {
    NSString *ownPrivilege = [MEGAChatRoom stringForPrivilege:self.ownPrivilege];
    NSString *changes      = [MEGAChatRoom stringForChangeType:self.changes];
    NSString *active       = self.isActive ? @"YES" : @"NO";
    NSString *group        = self.isGroup ? @"YES" : @"NO";
    NSString *publicChat   = self.isPublicChat ? @"YES" : @"NO";
    NSString *preview      = self.preview ? @"YES" : @"NO";
    NSString *base64ChatId = [MEGASdk base64HandleForUserHandle:self.chatId];
#ifdef DEBUG
    return [NSString stringWithFormat:@"<%@: chatId=%@, title=%@, own privilege=%@, peer count=%lu, group=%@, changes=%@, unread=%ld, user typing=%llu, active=%@ public chat=%@, preview=%@>",
            [self class], base64ChatId, self.title, ownPrivilege, (unsigned long)self.peerCount, group, changes, (long)self.unreadCount, self.userTypingHandle, active, publicChat, preview];
#else
    return [NSString stringWithFormat:@"<%@: chatId=%@, own privilege=%@, peer count=%lu, group=%@, changes=%@, unread=%ld, user typing=%llu, active=%@, public chat=%@, preview=%@>",
            [self class], base64ChatId, ownPrivilege, (unsigned long)self.peerCount, group, changes, (long)self.unreadCount, self.userTypingHandle, active, publicChat, preview];
#endif

}

- (uint64_t)chatId {
    return self.megaChatRoom ? self.megaChatRoom->getChatId() : MEGACHAT_INVALID_HANDLE;
}

- (MEGAChatRoomPrivilege)ownPrivilege {
    return (MEGAChatRoomPrivilege) (self.megaChatRoom ?  self.megaChatRoom->getOwnPrivilege() : -2);
}

- (NSUInteger)peerCount {
    return self.megaChatRoom ? self.megaChatRoom->getPeerCount() : 0;
}

- (BOOL)isGroup {
    return self.megaChatRoom ? self.megaChatRoom->isGroup() : NO;
}

- (BOOL)isPublicChat {
    return self.megaChatRoom ? self.megaChatRoom->isPublic() : NO;
}

- (BOOL)isPreview {
    return self.megaChatRoom ? self.megaChatRoom->isPreview() : NO;
}

- (BOOL)isNoteToSelf {
    return self.megaChatRoom ? self.megaChatRoom->isNoteToSelf() : NO;
}

- (NSString *)authorizationToken {
    const char *val = self.megaChatRoom->getAuthorizationToken();
    if (!val) return nil;
    
    NSString *ret = [[NSString alloc] initWithUTF8String:val];
    
    delete [] val;
    return ret;
}

- (NSString *)title {
    if (!self.megaChatRoom) return nil;
    const char *ret = self.megaChatRoom->getTitle();
    return ret ? [[NSString alloc] initWithUTF8String:ret] : nil;
}

- (BOOL)hasCustomTitle {
    return self.megaChatRoom ? self.megaChatRoom->hasCustomTitle() : NO;
}

- (MEGAChatRoomChangeType)changes {
    return (MEGAChatRoomChangeType) ( self.megaChatRoom ? self.megaChatRoom->getChanges() : 0x00);
}

- (NSInteger)unreadCount {
    return self.megaChatRoom ? self.megaChatRoom->getUnreadCount() : 0;
}

- (uint64_t)userTypingHandle {
    return self.megaChatRoom ? self.megaChatRoom->getUserTyping() : MEGACHAT_INVALID_HANDLE;
}

- (uint64_t)userHandle {
    return self.megaChatRoom ? self.megaChatRoom->getUserHandle() : MEGACHAT_INVALID_HANDLE;
}

- (BOOL)isActive {
    return self.megaChatRoom ? self.megaChatRoom->isActive() : NO;
}

- (BOOL)isArchived {
    return self.megaChatRoom ? self.megaChatRoom->isArchived() : NO;
}

- (BOOL)isMeeting {
    return self.megaChatRoom ? self.megaChatRoom->isMeeting() : NO;
}

- (NSUInteger)retentionTime {
    return self.megaChatRoom ? self.megaChatRoom->getRetentionTime() : 0;
}

- (uint64_t)creationTimeStamp {
    return self.megaChatRoom ? self.megaChatRoom->getCreationTs() : MEGACHAT_INVALID_HANDLE;
}

- (NSUInteger)previewersCount {
    return self.megaChatRoom ? self.megaChatRoom->getNumPreviewers() : 0;
}

- (BOOL)isOpenInviteEnabled {
    return self.megaChatRoom ? self.megaChatRoom->isOpenInvite() : NO;
}

- (BOOL)isWaitingRoomEnabled {
    return self.megaChatRoom ? self.megaChatRoom->isWaitingRoom() : NO;
}

- (MEGAChatRoomPrivilege)peerPrivilegeByHandle:(uint64_t)userHande {
    return self.megaChatRoom ? MEGAChatRoomPrivilege(self.megaChatRoom->getPeerPrivilegeByHandle(userHande)) : MEGAChatRoomPrivilegeUnknown;
}

- (uint64_t)peerHandleAtIndex:(NSUInteger)index {
    return self.megaChatRoom ? self.megaChatRoom->getPeerHandle((int)index) : MEGACHAT_INVALID_HANDLE;
}

- (MEGAChatRoomPrivilege)peerPrivilegeAtIndex:(NSUInteger)index {
    return (MEGAChatRoomPrivilege) (self.megaChatRoom ? self.megaChatRoom->getPeerPrivilege((int)index) : -2);
}

- (BOOL)hasChangedForType:(MEGAChatRoomChangeType)changeType {
    return self.megaChatRoom ? self.megaChatRoom->hasChanged((int)changeType) : NO;
}

+ (NSString *)stringForPrivilege:(MEGAChatRoomPrivilege)privilege {
    return [[NSString alloc] initWithUTF8String:MegaChatRoom::privToString((int)privilege)];
}

+ (NSString *)stringForChangeType:(MEGAChatRoomChangeType)changeType {
    NSString *result;
    switch (changeType) {
        case MEGAChatRoomChangeTypeStatus:
            result = @"Status";
            break;
        case MEGAChatRoomChangeTypeUnreadCount:
            result = @"Unread count";
            break;
        case MEGAChatRoomChangeTypeParticipants:
            result = @"Participants";
            break;
        case MEGAChatRoomChangeTypeTitle:
            result = @"Title";
            break;
        case MEGAChatRoomChangeTypeUserTyping:
            result = @"User typing";
            break;
        case MEGAChatRoomChangeTypeClosed:
            result = @"Closed";
            break;
        case MEGAChatRoomChangeTypeOwnPriv:
            result = @"Privilege change";
            break;
        case MEGAChatRoomChangeTypeUserStopTyping:
            result = @"User stops typing";
            break;
        case MEGAChatRoomChangeTypeArchive:
            result = @"Archived";
            break;
        case MEGAChatRoomChangeTypeCall:
            result = @"Call";
            break;
        case MEGAChatRoomChangeTypeChatMode:
            result = @"Chat mode";
            break;
        case MEGAChatRoomChangeTypeUpdatePreviewers:
            result = @"Update previewers";
            break;
            
        default:
            result = @"Default";
            break;
    }
    return result;
}

@end
