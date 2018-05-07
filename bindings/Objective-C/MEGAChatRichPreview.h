
#import <Foundation/Foundation.h>

@interface MEGAChatRichPreview : NSObject

@property (readonly, nonatomic) NSString *text;
@property (readonly, nonatomic) NSString *title;
@property (readonly, nonatomic) NSString *previewDescription;
@property (readonly, nonatomic) NSString *image;
@property (readonly, nonatomic) NSString *imageFormat;
@property (readonly, nonatomic) NSString *icon;
@property (readonly, nonatomic) NSString *iconFormat;
@property (readonly, nonatomic) NSString *url;

@end
