
#import <Foundation/Foundation.h>

#import "MEGAChatRichPreview.h"
#import "MEGAChatGeolocation.h"
#import "MEGAChatGiphy.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, MEGAChatContainsMetaType) {
    MEGAChatContainsMetaTypeInvalid     = -1,
    MEGAChatContainsMetaTypeRichPreview = 0,
    MEGAChatContainsMetaTypeGeolocation = 1,
    MEGAChatContainsMetaTypeGiphy = 3
};

@interface MEGAChatContainsMeta : NSObject

@property (readonly, nonatomic) MEGAChatContainsMetaType type;
@property (nullable, readonly, nonatomic) NSString *textMessage;
@property (nullable, readonly, nonatomic) MEGAChatRichPreview *richPreview;
@property (nullable, readonly, nonatomic) MEGAChatGeolocation *geolocation;
@property (nullable, readonly, nonatomic) MEGAChatGiphy *giphy;

@end

NS_ASSUME_NONNULL_END
