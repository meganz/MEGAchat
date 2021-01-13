
#import <Foundation/Foundation.h>

#import "MEGAChatRichPreview.h"
#import "MEGAChatGeolocation.h"
#import "MEGAChatGiphy.h"

typedef NS_ENUM(NSInteger, MEGAChatContainsMetaType) {
    MEGAChatContainsMetaTypeInvalid     = -1,
    MEGAChatContainsMetaTypeRichPreview = 0,
    MEGAChatContainsMetaTypeGeolocation = 1,
    MEGAChatContainsMetaTypeGiphy = 3
};

@interface MEGAChatContainsMeta : NSObject

@property (readonly, nonatomic) MEGAChatContainsMetaType type;
@property (readonly, nonatomic) NSString *textMessage;
@property (readonly, nonatomic) MEGAChatRichPreview *richPreview;
@property (readonly, nonatomic) MEGAChatGeolocation *geolocation;
@property (readonly, nonatomic) MEGAChatGiphy *giphy;

@end
