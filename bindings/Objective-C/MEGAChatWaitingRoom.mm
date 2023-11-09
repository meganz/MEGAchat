#import "MEGAChatWaitingRoom.h"
#import "megachatapi.h"
#import "MEGAHandleList+init.h"

using namespace megachat;

@interface MEGAChatWaitingRoom ()

@property MegaChatWaitingRoom *megaChatWaitingRoom;
@property BOOL cMemoryOwn;

@end

@implementation MEGAChatWaitingRoom

- (instancetype)initWithMegaChatWaitingRoom:(MegaChatWaitingRoom *)megaChatWaitingRoom cMemoryOwn:(BOOL)cMemoryOwn {
    self = [super init];
    
    if (self != nil) {
        _megaChatWaitingRoom = megaChatWaitingRoom;
        _cMemoryOwn = cMemoryOwn;
    }
    
    return self;
}

- (void)dealloc {
    if (self.cMemoryOwn) {
        delete _megaChatWaitingRoom;
    }
}

- (instancetype)clone {
    return self.megaChatWaitingRoom ? [[MEGAChatWaitingRoom alloc] initWithMegaChatWaitingRoom:self.megaChatWaitingRoom cMemoryOwn:YES] : nil;
}

- (MegaChatWaitingRoom *)getCPtr {
    return self.megaChatWaitingRoom;
}

- (int64_t)size {
    return self.megaChatWaitingRoom ? self.megaChatWaitingRoom->size() : 0;
}

-(MEGAHandleList *)users {
    return self.megaChatWaitingRoom->getUsers() ? [[MEGAHandleList alloc] initWithMegaHandleList:self.megaChatWaitingRoom->getUsers() cMemoryOwn: YES] : nil;
}

- (MEGAChatWaitingRoomStatus)userStatus:(uint64_t)peerId {
    return self.megaChatWaitingRoom ? MEGAChatWaitingRoomStatus(self.megaChatWaitingRoom->getUserStatus(peerId)) : MEGAChatWaitingRoomStatusUnknown;
}

- (NSString *)stringForStatus:(MEGAChatWaitingRoomStatus)status {
    if (!self.megaChatWaitingRoom) return @"";

    const char *val = self.megaChatWaitingRoom->userStatusToString(int(status));
    if (!val) return @"";
    return @(val);
}

@end
