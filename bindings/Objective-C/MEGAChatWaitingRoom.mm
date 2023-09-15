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

-(MEGAHandleList *)peers {
    return self.megaChatWaitingRoom->getPeers() ? [[MEGAHandleList alloc] initWithMegaHandleList:self.megaChatWaitingRoom->getPeers() cMemoryOwn: YES] : nil;
}

- (MEGAChatWaitingRoomStatus)peerStatus:(uint64_t)peerId {
    return self.megaChatWaitingRoom ? MEGAChatWaitingRoomStatus(self.megaChatWaitingRoom->getPeerStatus(peerId)) : MEGAChatWaitingRoomStatusUnknown;
}

- (NSString *)stringForStatus:(MEGAChatWaitingRoomStatus)status {
    if (!self.megaChatWaitingRoom) return @"";

    const char *val = self.megaChatWaitingRoom->peerStatusToString(int(status));
    if (!val) return @"";
    return @(val);
}

@end
