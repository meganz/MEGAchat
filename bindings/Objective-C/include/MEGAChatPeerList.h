#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface MEGAChatPeerList : NSObject

@property (readonly, nonatomic) NSInteger size;

- (void)addPeerWithHandle:(uint64_t)hande privilege:(NSInteger)privilege;
- (uint64_t)peerHandleAtIndex:(NSInteger)index;
- (NSInteger)peerPrivilegeAtIndex:(NSInteger)index;

@end

NS_ASSUME_NONNULL_END
