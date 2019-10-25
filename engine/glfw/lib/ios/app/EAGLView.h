#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>

/*
This class wraps the CAEAGLLayer from CoreAnimation into a convenient UIView subclass.
The view content is basically an EAGL surface you render your OpenGL scene into.
Note that setting the view non-opaque will only work if the EAGL surface has an alpha channel.
*/
@interface EAGLView : UIView<UIKeyInput, UITextInput> {
@public
    CADisplayLink* displayLink;
@private
    GLuint backingWidth;
    GLuint backingHeight;
    EAGLContext *context;
    EAGLContext *auxContext;
    GLuint viewRenderbuffer, viewFramebuffer;
    GLuint depthStencilRenderbuffer;
    int countDown;
    int swapInterval;
    UIKeyboardType keyboardType;
    BOOL secureTextEntry;
    id <UITextInputDelegate> inputDelegate;
    NSMutableString *markedText;
}

- (void)swapBuffers;
- (void)newFrame;
- (void)setupView;

@end

@interface EAGLView ()

@property (nonatomic, retain) EAGLContext *context;
@property (nonatomic, retain) EAGLContext *auxContext;
@property (nonatomic) BOOL keyboardActive;
// TODO: Cooldown "timer" *hack* for backspace and enter release
#define TEXT_KEY_COOLDOWN (10)
@property (nonatomic) int textkeyActive;
@property (nonatomic) int autoCloseKeyboard;

- (BOOL) createFramebuffer;
- (void) destroyFramebuffer;
- (void) clearMarkedText;

- (int) getWindowWidth;
- (int) getWindowHeight;
- (GLuint) getViewFrameBuffer;

@end
