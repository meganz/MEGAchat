
#import <Foundation/Foundation.h>

#import "MEGAChatContainsMeta.h"
#import "MEGANodeList.h"
#import "MEGAHandleList.h"

typedef NS_ENUM(NSInteger, MEGAChatMessageStatus) {
    MEGAChatMessageStatusUnknown        = -1,
    MEGAChatMessageStatusSending        = 0,
    MEGAChatMessageStatusSendingManual  = 1,
    MEGAChatMessageStatusServerReceived = 2,
    MEGAChatMessageStatusServerRejected = 3,
    MEGAChatMessageStatusDelivered      = 4,
    MEGAChatMessageStatusNotSeen        = 5,
    MEGAChatMessageStatusSeen           = 6
};

typedef NS_ENUM(NSInteger, MEGAChatMessageType) {
    MEGAChatMessageTypeUnknown           = -1,
    MEGAChatMessageTypeInvalid           = 0,
    MEGAChatMessageTypeNormal            = 1,
    MEGAChatMessageTypeAlterParticipants = 2,
    MEGAChatMessageTypeTruncate          = 3,
    MEGAChatMessageTypePrivilegeChange   = 4,
    MEGAChatMessageTypeChatTitle         = 5,
    MEGAChatMessageTypeCallEnded         = 6,
    MEGAChatMessageTypeCallStarted       = 7,
    MEGAChatMessageTypeHighestManagement = 7,
    MEGAChatMessageTypeAttachment        = 101,
    MEGAChatMessageTypeRevokeAttachment  = 102, /// Obsolete
    MEGAChatMessageTypeContact           = 103,
    MEGAChatMessageTypeContainsMeta      = 104,
    MEGAChatMessageTypeVoiceClip         = 105
};

typedef NS_ENUM(NSInteger, MEGAChatMessageChangeType) {
    MEGAChatMessageChangeTypeStatus  = 0x01,
    MEGAChatMessageChangeTypeContent = 0x02,
    MEGAChatMessageChangeTypeAccess  = 0x04  /// When the access to attached nodes has changed (obsolete)
};

typedef NS_ENUM(NSInteger, MEGAChatMessageReason) {
    MEGAChatMessageReasonPeersChanged  = 1,
    MEGAChatMessageReasonTooOld        = 2,
    MEGAChatMessageReasonGeneralReject = 3,
    MEGAChatMessageReasonNoWriteAccess = 4,
    MEGAChatMessageReasonNoChanges     = 6
};

typedef NS_ENUM(NSInteger, MEGAChatMessageEndCallReason) {
    MEGAChatMessageEndCallReasonEnded = 1,
    MEGAChatMessageEndCallReasonRejected = 2,
    MEGAChatMessageEndCallReasonNoAnswer = 3,
    MEGAChatMessageEndCallReasonFailed = 4,
    MEGAChatMessageEndCallReasonCancelled = 5
};

@interface MEGAChatMessage : NSObject

@property (readonly, nonatomic) MEGAChatMessageStatus status;
@property (readonly, nonatomic) uint64_t messageId;
@property (readonly, nonatomic) uint64_t temporalId;
@property (readonly, nonatomic) NSInteger messageIndex;
@property (readonly, nonatomic) uint64_t userHandle;
@property (readonly, nonatomic) MEGAChatMessageType type;
@property (readonly, nonatomic) NSDate *timestamp;
@property (readonly, nonatomic) NSString *content;
@property (readonly, nonatomic, getter=isEdited) BOOL edited;
@property (readonly, nonatomic, getter=isDeleted) BOOL deleted;
@property (readonly, nonatomic, getter=isEditable) BOOL editable;
@property (readonly, nonatomic, getter=isDeletable) BOOL deletable;
@property (readonly, nonatomic, getter=isManagementMessage) BOOL managementMessage;
@property (readonly, nonatomic) uint64_t userHandleOfAction;
@property (readonly, nonatomic) NSInteger privilege;
@property (readonly, nonatomic) MEGAChatMessageChangeType changes;
@property (readonly, nonatomic) MEGAChatMessageReason code;
@property (readonly, nonatomic) NSUInteger usersCount;
@property (readonly, nonatomic) MEGANodeList *nodeList;
@property (readonly, nonatomic) MEGAHandleList *handleList;
@property (readonly, nonatomic) NSInteger duration;
@property (readonly, nonatomic) MEGAChatMessageEndCallReason termCode;
@property (readonly, nonatomic) uint64_t rowId;
@property (readonly, nonatomic) MEGAChatContainsMeta *containsMeta;

- (instancetype)clone;

- (BOOL)hasChangedForType:(MEGAChatMessageChangeType)changeType;
- (uint64_t)userHandleAtIndex:(NSUInteger)index;
- (NSString *)userNameAtIndex:(NSUInteger)index;
- (NSString *)userEmailAtIndex:(NSUInteger)index;

+ (NSString *)stringForChangeType:(MEGAChatMessageChangeType)changeType;
+ (NSString *)stringForStatus:(MEGAChatMessageStatus)status;
+ (NSString *)stringForType:(MEGAChatMessageType)type;
+ (NSString *)stringForCode:(MEGAChatMessageReason)code;
+ (NSString *)stringForEndCallReason:(MEGAChatMessageEndCallReason)reason;

@end
