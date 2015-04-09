//
//  videoRenderer_objc.h
//  testapp
//
//  Created by Alex Vasilev on 4/7/15.
//  Copyright (c) 2015 Alex Vasilev. All rights reserved.
//

#ifndef testapp_videoRenderer_objc_h
#define testapp_videoRenderer_objc_h
#include "IVideoRenderer.h"
#include <UIKit/UIKit.h>

IB_DESIGNABLE
@interface VideoRendererObjc : UIImageView
{
    IVideoRenderer* mVideoRenderer;
}
@property IVideoRenderer* videoRenderer;
-(id)initRenderer;
-(void)setImageFunc:(UIImage*)image;
-(void)fillWithBlack;
@end

#endif
