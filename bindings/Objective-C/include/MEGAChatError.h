#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, MEGAChatErrorType) {
    MEGAChatErrorTypeOk      = 0,
    MEGAChatErrorTypeUnknown = -1,
    MEGAChatErrorTypeArgs    = -2,
    MEGAChatErrorTooMany     = -6,
    MEGAChatErrorTypeNoEnt   = -9,
    MEGAChatErrorTypeAccess  = -11,
    MegaChatErrorTypeExist   = -12
};

@interface MEGAChatError : NSObject

@property (readonly, nonatomic) MEGAChatErrorType type;

@property (readonly, nonatomic, nullable) NSString *name;

@end

NS_ASSUME_NONNULL_END
