
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface MEGAChatGeolocation : NSObject

@property (readonly, nonatomic) float longitude;
@property (readonly, nonatomic) float latitude;
@property (nullable, readonly, nonatomic) NSString *image;

@end

NS_ASSUME_NONNULL_END
