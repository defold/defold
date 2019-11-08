#import "BaseView.h"
#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>

@interface VulkanView : BaseView {
@private
}

+ (BaseView*)createView:(CGRect)bounds recreate:(BOOL)recreate;
- (void)swapBuffers;
- (void)newFrame;
- (void)setupView;

@end

@interface VulkanView ()

@end
