#import <Foundation/Foundation.h>
#import "MEGAChatLogLevel.h"

NS_ASSUME_NONNULL_BEGIN

@protocol MEGAChatLoggerDelegate <NSObject>

@optional

- (void)logWithLevel:(MEGAChatLogLevel)logLevel message:(NSString *)message;

@end

NS_ASSUME_NONNULL_END
