#import "BaseView.h"
#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>

/*
This class wraps the CAEAGLLayer from CoreAnimation into a convenient UIView subclass.
The view content is basically an EAGL surface you render your OpenGL scene into.
Note that setting the view non-opaque will only work if the EAGL surface has an alpha channel.
*/
@interface EAGLView : BaseView {
@public
    EAGLContext *context;
    EAGLContext *auxContext;
@private
    GLuint viewRenderbuffer, viewFramebuffer;
    GLuint depthStencilRenderbuffer;
}

+ (BaseView*)createView:(CGRect)bounds recreate:(BOOL)recreate;
- (void)swapBuffers;
- (void)setupView;
- (void)teardownView;
- (void)setCurrentContext;

@end

@interface EAGLView ()

@property (nonatomic, retain) EAGLContext *context;
@property (nonatomic, retain) EAGLContext *auxContext;

@end
