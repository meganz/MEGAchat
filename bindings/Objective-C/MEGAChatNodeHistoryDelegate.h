
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@protocol MEGAChatNodeHistoryDelegate <NSObject>

@optional

- (void)onAttachmentLoaded:(MEGAChatSdk *)api message:(MEGAChatMessage * _Nullable)message;
- (void)onAttachmentReceived:(MEGAChatSdk *)api message:(MEGAChatMessage *)message;
- (void)onAttachmentDeleted:(MEGAChatSdk *)api messageId:(uint64_t)messageId;
- (void)onTruncate:(MEGAChatSdk *)api messageId:(uint64_t)messageId;

@end

NS_ASSUME_NONNULL_END
