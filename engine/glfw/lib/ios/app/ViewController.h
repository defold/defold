#include "EAGLView.h"
#include <setjmp.h>

@interface ViewController : UIViewController<UIContentContainer, UIAccelerometerDelegate>
{
    EAGLView *glView;
    CGSize cachedViewSize;
}

- (EAGLContext *)initialiseGlContext;
- (EAGLContext *)initialiseGlAuxContext:(EAGLContext *)parentContext;
- (void)createGlView;

// iOS 8.0.0 - 8.0.2
- (CGSize)getIntendedViewSize;
- (CGPoint)getIntendedFrameOrigin:(CGSize)size;
- (BOOL)shouldUpdateViewFrame;
- (void)updateViewFramesWorkaround;

@property (nonatomic, retain) IBOutlet EAGLView *glView;

@end
