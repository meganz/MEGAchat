
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM (NSInteger, MEGAChatStatus);

@interface MEGAChatPresenceConfig : NSObject

@property (readonly, nonatomic) MEGAChatStatus onlineStatus;
@property (readonly, nonatomic, getter=isAutoAwayEnabled) BOOL autoAwayEnabled;
@property (readonly, nonatomic) int64_t autoAwayTimeout;
@property (readonly, nonatomic, getter=isPersist) BOOL persist;
@property (readonly, nonatomic, getter=isPending) BOOL pending;
@property (readonly, nonatomic, getter=isLastGreenVisible) BOOL lastGreenVisible;

@end

NS_ASSUME_NONNULL_END