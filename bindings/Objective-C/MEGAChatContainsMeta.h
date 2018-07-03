
#import <Foundation/Foundation.h>

#import "MEGAChatRichPreview.h"

typedef NS_ENUM(NSInteger, MEGAChatContainsMetaType) {
    MEGAChatContainsMetaTypeInvalid     = -1,
    MEGAChatContainsMetaTypeRichPreview = 0
};

@interface MEGAChatContainsMeta : NSObject

@property (readonly, nonatomic) MEGAChatContainsMetaType type;
@property (readonly, nonatomic) MEGAChatRichPreview *richPreview;

@end
