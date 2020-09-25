
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface MEGAChatGiphy : NSObject

@property (nullable, readonly, nonatomic) NSString *mp4Src;
@property (nullable, readonly, nonatomic) NSString *webpSrc;

@property (readonly, nonatomic) long mp4Size;
@property (readonly, nonatomic) long webpSize;
@property (nullable, readonly, nonatomic) NSString *title;
@property (readonly, nonatomic) int width;
@property (readonly, nonatomic) int height;

@end

NS_ASSUME_NONNULL_END
