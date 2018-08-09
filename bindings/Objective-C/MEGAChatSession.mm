
#import "MEGAChatSession.h"
#import "megachatapi.h"

using namespace megachat;

@interface MEGAChatSession ()

@property MegaChatSession *megaChatSession;
@property BOOL cMemoryOwn;

@end

@implementation MEGAChatSession

- (instancetype)initWithMegaChatSession:(MegaChatSession *)megaChatSession cMemoryOwn:(BOOL)cMemoryOwn {
    NSParameterAssert(MEGAChatSession);
    
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

- (BOOL)hasAudio {
    return self.megaChatSession ? self.megaChatSession->hasAudio() : NO;
}

- (BOOL)hasVideo {
    return self.megaChatSession ? self.megaChatSession->hasVideo() : NO;
}

- (uint64_t)peerId {
    return self.megaChatSession ? self.megaChatSession->getPeerid() : 0;
}

- (NSInteger)networkQuality {
    return self.megaChatSession ? self.megaChatSession->getNetworkQuality() : 0;
}

- (BOOL)audioDetected {
    return self.megaChatSession ? self.megaChatSession->getAudioDetected() : NO;
}

@end
