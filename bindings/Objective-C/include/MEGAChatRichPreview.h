
#import <Foundation/Foundation.h>

@interface MEGAChatRichPreview : NSObject

@property (nullable, readonly, nonatomic) NSString *text;
@property (nullable, readonly, nonatomic) NSString *title;
@property (nullable, readonly, nonatomic) NSString *previewDescription;
@property (nullable, readonly, nonatomic) NSString *image;
@property (nullable, readonly, nonatomic) NSString *imageFormat;
@property (nullable, readonly, nonatomic) NSString *icon;
@property (nullable, readonly, nonatomic) NSString *iconFormat;
@property (nullable, readonly, nonatomic) NSString *url;

@end
