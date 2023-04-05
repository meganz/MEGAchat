

#import "MEGAChatScheduledMeetingList.h"
#import "MEGAChatScheduledMeeting+init.h"
#import "megachatapi.h"

using namespace megachat;

@interface MEGAChatScheduledMeetingList()

@property MegaChatScheduledMeetingList *megaChatScheduledMeetingList;
@property BOOL cMemoryOwn;

@end

@implementation MEGAChatScheduledMeetingList

- (instancetype)initWithMegaChatScheduledMeetingList:(megachat::MegaChatScheduledMeetingList *)megaChatScheduledMeetingList cMemoryOwn:(BOOL)cMemoryOwn {
    self = [super init];
    
    if (self != nil) {
        _megaChatScheduledMeetingList = megaChatScheduledMeetingList;
        _cMemoryOwn = cMemoryOwn;
    }
    
    return self;
}

- (void)dealloc {
    if (self.cMemoryOwn){
        delete _megaChatScheduledMeetingList;
    }
}

- (instancetype)clone {
    return self.megaChatScheduledMeetingList ? [[MEGAChatScheduledMeetingList alloc] initWithMegaChatScheduledMeetingList:self.megaChatScheduledMeetingList->copy() cMemoryOwn:YES] : nil;
}

- (MegaChatScheduledMeetingList *)getCPtr {
    return self.megaChatScheduledMeetingList;
}

- (NSUInteger)size {
    return self.megaChatScheduledMeetingList ? self.megaChatScheduledMeetingList->size() : -1;
}

- (MEGAChatScheduledMeeting *)scheduledMeetingAtIndex:(NSUInteger)index {
    if (self.megaChatScheduledMeetingList) {
        return [[MEGAChatScheduledMeeting alloc] initWithMegaChatScheduledMeeting:self.megaChatScheduledMeetingList->at((unsigned int)index)->copy() cMemoryOwn:YES];
    }
    
    return nil;
}

- (void)insertScheduledMeeting:(MEGAChatScheduledMeeting *)scheduledMeeting {
    if (self.megaChatScheduledMeetingList) {
        self.megaChatScheduledMeetingList->insert(scheduledMeeting.getCPtr);
    }
}

@end
