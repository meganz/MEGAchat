#import <Foundation/Foundation.h>
#import "MEGAChatLogLevel.h"

@protocol MEGAChatLoggerDelegate <NSObject>

@optional

- (void)logWithLevel:(MEGAChatLogLevel)logLevel message:(NSString *)message;

@end
