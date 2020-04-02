
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

- (uint64_t)clientId {
    return self.megaChatSession ? self.megaChatSession->getClientid() : 0;
}

- (NSInteger)networkQuality {
    return self.megaChatSession ? self.megaChatSession->getNetworkQuality() : 0;
}

- (BOOL)audioDetected {
    return self.megaChatSession ? self.megaChatSession->getAudioDetected() : NO;
}

- (NSInteger)termCode {
    return self.megaChatSession ? self.megaChatSession->getTermCode() : 0;
}

- (BOOL)isLocalTermCode {
    return self.megaChatSession ? self.megaChatSession->isLocalTermCode() : NO;
}

- (NSInteger)changes {
    return self.megaChatSession ? self.megaChatSession->getChanges() : 0;
}

- (BOOL)hasChanged:(MEGAChatSessionChange)change {
    return self.megaChatSession ? self.megaChatSession->hasChanged((int)change) : NO;
}

- (NSString *)description {
    NSString *peerId = [MEGASdk base64HandleForUserHandle:self.peerId];
    NSString *clientId = [MEGASdk base64HandleForUserHandle:self.clientId];
    NSString *termCode = [MEGAChatCall stringForTermCode:(MEGAChatCallTermCode)self.termCode];
    NSString *hasAudio = self.hasAudio ? @"ON" : @"OFF";
    NSString *hasVideo = self.hasVideo ? @"ON" : @"OFF";
    NSString *localTermCode = self.isLocalTermCode ? @"YES" : @"NO";
    NSString *audioDetected = self.audioDetected ? @"YES" : @"NO";
    NSString *changes = [self stringForChanges];
    return [NSString stringWithFormat:@"<%@: peerId=%@; clientId=%@; hasAudio=%@; hasVideo=%@; changes=%@; audioDetected=%@; networkQuality=%ld; termCode=%@; is local term code %@>", self.class, peerId, clientId, hasAudio, hasVideo, changes, audioDetected, self.networkQuality, termCode, localTermCode];
}

- (NSString *)stringForChanges {
    NSString *changes = @"";
    if ([self hasChanged:MEGAChatSessionChangeNoChanges]) {
        changes = [changes stringByAppendingString:@" | NO CHANGES"];
    }
    if ([self hasChanged:MEGAChatSessionChangeStatus]) {
        switch (self.status) {
            case MEGAChatSessionStatusInitial:
                changes = [changes stringByAppendingString:@" | STATUS INITIAL"];
                break;
                
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
    if ([self hasChanged:MEGAChatSessionChangeNetworkQuality]) {
        changes = [changes stringByAppendingString:@" | NETWORK QUALITY"];
    }
    if ([self hasChanged:MEGAChatSessionChangeAudioLevel]) {
        changes = [changes stringByAppendingString:@" | AUDIO LEVEL"];
    }
    if ([self hasChanged:MEGAChatSessionChangeOperative]) {
        changes = [changes stringByAppendingString:@" | OPERATIVE"];
    }
    
    if ([changes isEqualToString:@""]) {
        return @"CHANGES NOT MANAGED";
    } else {
        return [changes substringFromIndex:3];
    }
}

@end
