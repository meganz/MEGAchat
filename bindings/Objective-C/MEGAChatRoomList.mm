#import "MEGAChatRoomList.h"
#import "megachatapi.h"
#import "MEGAChatRoom+init.h"

using namespace megachat;

@interface MEGAChatRoomList ()

@property MegaChatRoomList *megaChatRoomList;
@property BOOL cMemoryOwn;

@end

@implementation MEGAChatRoomList

- (instancetype)initWithMegaChatRoomList:(MegaChatRoomList *)megaChatRoomList cMemoryOwn:(BOOL)cMemoryOwn {
    self = [super init];
    
    if (self != nil) {
        _megaChatRoomList = megaChatRoomList;
        _cMemoryOwn = cMemoryOwn;
    }
    
    return self;
}

- (void)dealloc {
    if (self.cMemoryOwn){
        delete _megaChatRoomList;
    }
}

- (MegaChatRoomList *)getCPtr {
    return self.megaChatRoomList;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@: size=%ld>",
            [self class], (long)self.size];
}

- (NSInteger)size {
    return self.megaChatRoomList ? self.megaChatRoomList->size() : -1;
}

- (MEGAChatRoom *)chatRoomAtIndex:(NSUInteger)index {
    return self.megaChatRoomList->get((unsigned int)index) ? [[MEGAChatRoom alloc] initWithMegaChatRoom:self.megaChatRoomList->get((unsigned int)index)->copy() cMemoryOwn:YES] : nil;
}

@end
