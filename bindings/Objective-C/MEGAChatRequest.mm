#import "MEGAChatRequest.h"

#import "megachatapi.h"

#import "MEGAChatMessage+init.h"
#import "MEGAChatPeerList+init.h"
#import "MEGANodeList+init.h"
#import "MEGAHandleList+init.h"
#import "MEGAChatScheduledMeetingOccurrence+init.h"
#import "MEGAChatScheduledMeeting+init.h"

using namespace megachat;

@interface MEGAChatRequest()

@property MegaChatRequest *megaChatRequest;
@property BOOL cMemoryOwn;

@end

@implementation MEGAChatRequest

- (instancetype)initWithMegaChatRequest:(MegaChatRequest *)megaChatRequest cMemoryOwn:(BOOL)cMemoryOwn {
    self = [super init];
    
    if (self != nil) {
        _megaChatRequest = megaChatRequest;
        _cMemoryOwn = cMemoryOwn;
    }
    
    return self;
}

- (void)dealloc {
    if (self.cMemoryOwn){
        delete _megaChatRequest;
    }
}

- (instancetype)clone {
    return  self.megaChatRequest ? [[MEGAChatRequest alloc] initWithMegaChatRequest:self.megaChatRequest->copy() cMemoryOwn:YES] : nil;
}

- (MegaChatRequest *)getCPtr {
    return self.megaChatRequest;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@: requestString=%@, type=%@>",
            [self class], self.requestString, @(self.type)];
}

- (MEGAChatRequestType)type {
    return (MEGAChatRequestType) (self.megaChatRequest ? self.megaChatRequest->getType() : -1);
}

- (NSString *)requestString {
    if (!self.megaChatRequest) return nil;
    const char *ret = self.megaChatRequest->getRequestString();
    return ret ? [[NSString alloc] initWithUTF8String:ret] : nil;
}

- (NSInteger)tag {
    return self.megaChatRequest ? self.megaChatRequest->getTag() : 0;
}

- (long long)number {
    return self.megaChatRequest ? self.megaChatRequest->getNumber() : 0;
}

- (BOOL)isFlag {
    return self.megaChatRequest ? self.megaChatRequest->getFlag() : NO;
}

- (MEGAChatPeerList *)megaChatPeerList {
    return self.megaChatRequest->getMegaChatPeerList() ? [[MEGAChatPeerList alloc] initWithMegaChatPeerList:self.megaChatRequest->getMegaChatPeerList() cMemoryOwn:YES] : nil;
}

- (uint64_t)chatHandle {
    return self.megaChatRequest ? self.megaChatRequest->getChatHandle() : MEGACHAT_INVALID_HANDLE;
}

- (uint64_t)userHandle {
    return self.megaChatRequest ? self.megaChatRequest->getUserHandle() : MEGACHAT_INVALID_HANDLE;
}

- (NSInteger)privilege {
    return self.megaChatRequest ? self.megaChatRequest->getPrivilege() : 0;
}

- (NSString *)text {
    if (!self.megaChatRequest) return nil;
    const char *ret = self.megaChatRequest->getText();
    return ret ? [[NSString alloc] initWithUTF8String:ret] : nil;
}

- (NSURL *)link {
    return self.megaChatRequest ? [NSURL URLWithString:[NSString stringWithUTF8String:self.megaChatRequest->getLink()]] : nil;
}

- (MEGAChatMessage *)chatMessage {
    return self.megaChatRequest->getMegaChatMessage() ? [[MEGAChatMessage alloc] initWithMegaChatMessage:self.megaChatRequest->getMegaChatMessage()->copy() cMemoryOwn:YES] : nil;
}

- (MEGANodeList *)nodeList {
    return self.megaChatRequest->getMegaNodeList() ? [[MEGANodeList alloc] initWithNodeList:self.megaChatRequest->getMegaNodeList()->copy() cMemoryOwn:YES] : nil;
}

- (NSInteger)paramType {
    return self.megaChatRequest ? self.megaChatRequest->getParamType() : 0;
}

- (MEGAHandleList *)megaHandleList {
    return self.megaChatRequest->getMegaHandleList() ? [[MEGAHandleList alloc] initWithMegaHandleList:self.megaChatRequest->getMegaHandleList()->copy() cMemoryOwn:YES] : nil;
}

- (NSArray<MEGAChatScheduledMeeting *> *)scheduledMeetingList {
    if (!self.megaChatRequest) return nil;
    MegaChatScheduledMeetingList *chatScheduledMeetingList = self.megaChatRequest->getMegaChatScheduledMeetingList();
    if (!chatScheduledMeetingList) return nil;
    NSMutableArray<MEGAChatScheduledMeeting *> *scheduledMeetings = [NSMutableArray arrayWithCapacity:chatScheduledMeetingList->size()];

    for (int i = 0; i < chatScheduledMeetingList->size(); i++)
    {
        MegaChatScheduledMeeting *megaChatScheduledMeeting = chatScheduledMeetingList->at(i)->copy();

        MEGAChatScheduledMeeting *scheduledMeeting = [[MEGAChatScheduledMeeting alloc] initWithMegaChatScheduledMeeting:megaChatScheduledMeeting cMemoryOwn:YES];
        [scheduledMeetings addObject:scheduledMeeting];
    }
    
    return scheduledMeetings;
}

- (NSArray<MEGAChatScheduledMeetingOccurrence *> *)chatScheduledMeetingOccurrences {
    if (!self.megaChatRequest) return [NSArray new];
    MegaChatScheduledMeetingOccurrList *scheduledMeetingOccurrList = self.megaChatRequest->getMegaChatScheduledMeetingOccurrList();
    if(!scheduledMeetingOccurrList) return [NSArray new];
    NSMutableArray<MEGAChatScheduledMeetingOccurrence *> *scheduledMeetingsOccurrences = [NSMutableArray arrayWithCapacity:scheduledMeetingOccurrList->size()];

    for (int i = 0; i < scheduledMeetingOccurrList->size(); i++)
    {
        MegaChatScheduledMeetingOccurr *megaChatScheduledMeetingOccurr = scheduledMeetingOccurrList->at(i)->copy();

        MEGAChatScheduledMeetingOccurrence *scheduledMeetingOccurrence = [[MEGAChatScheduledMeetingOccurrence alloc] initWithMegaChatScheduledMeetingOccurrence: megaChatScheduledMeetingOccurr cMemoryOwn:YES];
        [scheduledMeetingsOccurrences addObject:scheduledMeetingOccurrence];
    }
    
    return scheduledMeetingsOccurrences;
}

@end
