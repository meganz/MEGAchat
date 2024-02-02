
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM (NSInteger, MEGAChatScheduledFlagsType) {
    MEGAChatScheduledFlagsTypeDontSendEmails = 0,
};

@interface MEGAChatScheduledFlags : NSObject

- (instancetype)clone;

@property (readonly, nonatomic) BOOL emailsEnabled;
@property (readonly, nonatomic) BOOL isEmpty;

- (void)setEmailsEnabled:(BOOL)sendEmails;
- (void)reset;

- (instancetype)initWithSendEmails:(BOOL)sendEmails;

@end

NS_ASSUME_NONNULL_END
