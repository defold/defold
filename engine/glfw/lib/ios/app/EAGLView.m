// OpenGL view

/*
 * Launching your Application in Landscape
 * https://developer.apple.com/library/ios/#technotes/tn2244/_index.html
 *
 * This is something we might want to add support for soon. The following is required:
 *   - Flip width/height when creating the window (in applicationDidFinishLaunching)
 *   - return (interfaceOrientation == UIInterfaceOrientationLandscapeRight);
 *     from shouldAutorotateToInterfaceOrientation
 *
 *   See TN2244 for more information
 */



#include "EAGLView.h"
#import "TextUtil.h"
#import "AppDelegate.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <OpenGLES/EAGL.h>
#import <OpenGLES/ES2/gl.h>
#import <OpenGLES/ES2/glext.h>
#import <OpenGLES/EAGLDrawable.h>
#import <QuartzCore/QuartzCore.h>

#include "internal.h"


_GLFWwin                    g_Savewin;
static int                  g_AccelerometerEnabled = 0;
static double               g_AccelerometerFrequency = 1.0 / 60.0;

// AppDelegate.m
extern UIWindow*            g_ApplicationWindow;
extern AppDelegate*         g_AppDelegate;

// ViewController.m
extern EAGLContext*         g_glContext;
extern EAGLContext*         g_glAuxContext;

static EAGLView*            g_EAGLView = 0;

//************************************************************************
//****               Platform implementation functions                ****
//************************************************************************

static void LogGLError(GLint err)
{
#ifdef GL_ES_VERSION_2_0
    printf("gl error %d\n", err);
#else
    printf("gl error %d: %s\n", err, gluErrorString(err));
#endif
}

#define CHECK_GL_ERROR \
    { \
        GLint err = glGetError(); \
        if (err != 0) \
        { \
            LogGLError(err); \
            assert(0); \
        } \
    }\


/*
This class wraps the CAEAGLLayer from CoreAnimation into a convenient UIView subclass.
The view content is basically an EAGL surface you render your OpenGL scene into.
Note that setting the view non-opaque will only work if the EAGL surface has an alpha channel.
*/


@implementation EAGLView

@synthesize context;

+ (Class)layerClass
{
    return [CAEAGLLayer class];
}

- (id) init {
  self = [super init];
  if (self != nil) {
      [self setSwapInterval: 1];
      markedText = [[NSMutableString alloc] initWithCapacity:128];
  }
    viewRenderbuffer = 0;
    viewFramebuffer = 0;
    depthStencilRenderbuffer = 0;

  _glfwInput.MouseEmulationTouch = 0;
  return self;
}

- (id)initWithFrame:(CGRect)frame
{
    self.multipleTouchEnabled = YES;
    self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    //_glfwWin.view = self;
    g_EAGLView = self;
    if ((self = [super initWithFrame:frame]))
    {
        // Get the layer
        CAEAGLLayer *eaglLayer = (CAEAGLLayer *)self.layer;

        eaglLayer.opaque = YES;
        eaglLayer.drawableProperties = [NSDictionary dictionaryWithObjectsAndKeys:
                                        [NSNumber numberWithBool:NO], kEAGLDrawablePropertyRetainedBacking, kEAGLColorFormatRGBA8, kEAGLDrawablePropertyColorFormat, nil];

        displayLink = [[UIScreen mainScreen] displayLinkWithTarget:self selector:@selector(newFrame)];
        [displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
        displayLink.frameInterval = 1;

        [self setupView];
    }

    markedText = [[NSMutableString alloc] initWithCapacity:128];

    _glfwInput.MouseEmulationTouch = 0;
    for (int i = 0; i < GLFW_MAX_TOUCH; ++i)
    {
        _glfwInput.Touch[i].Id = i;
        _glfwInput.Touch[i].Reference = 0x0;
        _glfwInput.Touch[i].Phase = GLFW_PHASE_IDLE;
    }

    // // Let the engine know it's started
    // printf("MAWE: g_EngineStartedFn\n");
    // g_EngineStartedFn(g_EngineStartedContext);

    return self;
}

- (void)setupView
{
}

//========================================================================
// UITextInput protocol methods
//
// These are used for;
//   * Keyboard input (insertText:)
//   * Keeping track of "marked text" (unfinished keyboard input)
//========================================================================

- (UITextRange *)selectedTextRange {
    // We always return the "caret" position as the end of the marked text
    return [IndexedRange rangeWithNSRange:NSMakeRange(markedText.length, 0)];
}

- (UITextRange *)markedTextRange {
    return [IndexedRange rangeWithNSRange:NSMakeRange(0, markedText.length)];
}

- (void)unmarkText {
    if ([markedText length] > 0) {
        [self insertText: markedText];

        // Clear marked text
        [inputDelegate textWillChange: self];
        [markedText setString: @""];
        [inputDelegate textDidChange: self];
        _glfwSetMarkedText("");
    }
}

- (NSString *)textInRange:(UITextRange *)range
{
    IndexedRange* _range = (IndexedRange*)range;
    NSString* sub_string = [markedText substringWithRange:_range.range];
    return sub_string;
}

- (void)replaceRange:(UITextRange *)range
            withText:(NSString *)text
{
    IndexedRange* _range = (IndexedRange*)range;
    [markedText replaceCharactersInRange:_range.range withString:text];
}

- (UITextRange *)textRangeFromPosition:(UITextPosition *)fromPosition
                            toPosition:(UITextPosition *)toPosition
{
    IndexedPosition *from = (IndexedPosition *)fromPosition;
    IndexedPosition *to = (IndexedPosition *)toPosition;
    NSRange range = NSMakeRange(MIN(from.index, to.index), ABS(to.index - from.index));
    return [IndexedRange rangeWithNSRange:range];
}

- (UITextPosition *)positionFromPosition:(UITextPosition *)position
                                  offset:(NSInteger)offset
{
    IndexedPosition *pos = (IndexedPosition *)position;
    NSInteger end = pos.index + offset;
    if (end > markedText.length || end < 0)
        return nil;
    return [IndexedPosition positionWithIndex:end];
}

- (NSInteger)offsetFromPosition:(UITextPosition *)fromPosition
                     toPosition:(UITextPosition *)toPosition
{
    IndexedPosition *f = (IndexedPosition *)fromPosition;
    IndexedPosition *t = (IndexedPosition *)toPosition;
    return (t.index - f.index);
}

- (id< UITextInputDelegate >) inputDelegate {
    return inputDelegate;
}

- (void) setInputDelegate: (id <UITextInputDelegate>) delegate {
    inputDelegate = delegate;
}

- (id <UITextInputTokenizer>) tokenizer {
    return [[UITextInputStringTokenizer alloc] initWithTextInput:self];
}

- (UITextWritingDirection) baseWritingDirectionForPosition: (UITextPosition *)position inDirection: (UITextStorageDirection)direction {
    return UITextWritingDirectionRightToLeft;
}

- (UITextAutocorrectionType) autocorrectionType {
    return UITextAutocorrectionTypeNo;
}

- (UITextSpellCheckingType) spellCheckingType {
    return UITextSpellCheckingTypeNo;
}


//========================================================================
// UITextInput protocol methods stubs
//
// We use only a subset of the methods in the UITextInput protocol
// to get "marked text" functionality. The methods below need to be
// implemented just to satisfy the protocol but are not used.
//========================================================================

- (void)setSelectedTextRange:(UITextRange *)range {}
- (UITextPosition *) endOfDocument { return nil; }
- (UITextPosition *) beginningOfDocument { return nil; }

- (UITextPosition *)closestPositionToPoint:(CGPoint)point { return nil; }
- (NSArray *)selectionRectsForRange:(UITextRange *)range { return nil; }
- (UITextPosition *)closestPositionToPoint:(CGPoint)point
                               withinRange:(UITextRange *)range { return nil; }
- (UITextRange *)characterRangeAtPoint:(CGPoint)point { return nil; }

- (UITextPosition *)positionWithinRange:(UITextRange *)range
                    farthestInDirection:(UITextLayoutDirection)direction { return nil; }
- (UITextRange *)characterRangeByExtendingPosition:(UITextPosition *)position
                                       inDirection:(UITextLayoutDirection)direction { return nil; }
- (CGRect)firstRectForRange:(UITextRange *)range { return CGRectMake(0, 0, 0, 0); }
- (CGRect)caretRectForPosition:(UITextPosition *)position { return CGRectMake(0, 0, 0, 0); }

- (UITextPosition *)positionFromPosition:(UITextPosition *)position
                             inDirection:(UITextLayoutDirection)direction
                                  offset:(NSInteger)offset { return nil; }

- (NSComparisonResult)comparePosition:(UITextPosition *)position
                           toPosition:(UITextPosition *)other { return NSOrderedSame; }

- (void) setBaseWritingDirection: (UITextWritingDirection)writingDirection forRange:(UITextRange *)range { }

- (void)swapBuffers
{
    //NSLog(@"swapBuffers");
    //if (g_StartupPhase == COMPLETE) {
    //     // Do not poll event before startup sequence is completed
    //     NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    //     while (countDown > 0)
    //     {
    //         CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1, TRUE);
    //     }
    //     [pool release];

    //     countDown = swapInterval;
    // //}

    //g_SwapCount++;

    // NOTE: We poll events above and the application might be iconfied
    // At least when running in frame-rates < 60
    //if (!_glfwWin.iconified && g_StartupPhase == COMPLETE)
    if (_glfwWin.iconified)
        return;

    {
        // if (!g_EngineIsRunningFn(g_EngineRunContext)) {
        //     NSLog(@"Exiting app!");
        //     [[NSThread mainThread] exit];
        // }
        //g_EngineStepFn(g_EngineRunContext);

        const GLenum discards[]  = {GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT};
        glBindFramebuffer(GL_FRAMEBUFFER, viewFramebuffer);
        glDiscardFramebufferEXT(GL_FRAMEBUFFER, 2, discards);

        glBindRenderbuffer(GL_RENDERBUFFER, viewRenderbuffer);
        [context presentRenderbuffer:GL_RENDERBUFFER];
    }
}

- (void)newFrame
{
    if (_glfwWin.iconified)
        return;

    countDown--;

    [g_AppDelegate appUpdate]; // will eventually call glfwSwapBuffers -> swapBuffers
}

- (void) setSwapInterval: (int) interval
{
    if (interval < 1)
    {
        interval = 1;
    }
    swapInterval = interval;
    countDown = swapInterval;
}

- (GLFWTouch*) touchById: (UITouch*) ref
{
    int32_t i;

    GLFWTouch* freeTouch = 0x0;
    for (i=0;i!=GLFW_MAX_TOUCH;i++)
    {
        _glfwInput.Touch[i].Id = i;
        if (_glfwInput.Touch[i].Reference == ref) {
            return &_glfwInput.Touch[i];
        }

        // Save touch entry for later if we need to "alloc" one in case we don't find the current reference.
        if (freeTouch == 0x0 && _glfwInput.Touch[i].Reference == 0x0) {
            freeTouch = &_glfwInput.Touch[i];
        }
    }

    if (freeTouch != 0x0) {
        freeTouch->Reference = ref;
    }

    return freeTouch;
}

- (void) updateGlfwMousePos: (int32_t) x y: (int32_t) y
{
    _glfwInput.MousePosX = x;
    _glfwInput.MousePosY = y;
}

- (void) touchStart: (GLFWTouch*) glfwt withTouch: (UITouch*) t
{
    // When a new touch starts, and there was no previous one, this will be our mouse emulation touch.
    if (_glfwInput.MouseEmulationTouch == 0x0) {
        _glfwInput.MouseEmulationTouch = glfwt;
    }

    CGPoint touchLocation = [t locationInView:self];
    CGPoint prevTouchLocation = [t previousLocationInView:self];
    CGFloat scaleFactor = self.contentScaleFactor;

    int x = touchLocation.x * scaleFactor;
    int y = touchLocation.y * scaleFactor;

    glfwt->Phase = GLFW_PHASE_BEGAN;
    glfwt->X = x;
    glfwt->Y = y;
    glfwt->DX = 0;
    glfwt->DY = 0;
}

- (void) touchUpdate: (GLFWTouch*) glfwt withTouch: (UITouch*) t
{
    CGPoint touchLocation = [t locationInView:self];
    CGPoint prevTouchLocation = [t previousLocationInView:self];
    CGFloat scaleFactor = self.contentScaleFactor;

    int x = touchLocation.x * scaleFactor;
    int y = touchLocation.y * scaleFactor;
    int px = prevTouchLocation.x * scaleFactor;
    int py = prevTouchLocation.y * scaleFactor;

    int prevPhase = glfwt->Phase;
    int newPhase = t.phase;

    // If previous phase was TAPPED, we need to return early since we currently cannot buffer actions/phases.
    if (prevPhase == GLFW_PHASE_TAPPED) {
        return;
    }

    // If this touch is currently used for mouse emulation, and it ended, unset the mouse emulation pointer.
    if (newPhase == GLFW_PHASE_ENDED && _glfwInput.MouseEmulationTouch == glfwt) {
        _glfwInput.MouseEmulationTouch = 0x0;
    }

    // This is an invalid touch order, we need to recieve a began or moved
    // phase before moving pushing any more move inputs.
    if (prevPhase == GLFW_PHASE_ENDED && newPhase == GLFW_PHASE_MOVED) {
        return;
    }

    glfwt->TapCount = t.tapCount;
    glfwt->X = x;
    glfwt->Y = y;
    glfwt->DX = x - px;
    glfwt->DY = y - py;

    // If we recieved both a began and moved for the same touch during one frame/update,
    // just update the coordinates but leave the phase as began.
    if (prevPhase == GLFW_PHASE_BEGAN && newPhase == GLFW_PHASE_MOVED) {
        return;

    // If a touch both began and ended during one frame/update, set the phase as
    // tapped and we will send the released event during next update (see input.c).
    } else if (prevPhase == GLFW_PHASE_BEGAN && newPhase == GLFW_PHASE_ENDED) {
        glfwt->Phase = GLFW_PHASE_TAPPED;
        return;
    }

    glfwt->Phase = t.phase;

}

- (int) fillTouchStart: (UIEvent*) event
{
    NSSet *touches = [event allTouches];

    for (UITouch *t in touches)
    {
        if (GLFW_PHASE_BEGAN == t.phase) {
            GLFWTouch* glfwt = [self touchById: t];
            if (glfwt == 0x0) {
                // Could not find corresponding GLFWTouch.
                // Possibly due to too many touches at once,
                // we only support GLFW_MAX_TOUCH.
                continue;
            }

            // We can't start/begin a new touch if it already has an ongoing phase (ie not idle).
            if (glfwt->Phase != GLFW_PHASE_IDLE) {
                continue;
            }

            [self touchStart: glfwt withTouch: t];

            if (glfwt == _glfwInput.MouseEmulationTouch) {
                [self updateGlfwMousePos: glfwt->X y: glfwt->Y];
                _glfwInputMouseClick( GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS );
            }
        }
    }
}

- (void) fillTouch: (UIEvent*) event forPhase:(UITouchPhase) phase
{
    NSSet *touches = [event allTouches];

    for (UITouch *t in touches)
    {
        if (phase == t.phase) {
            GLFWTouch* glfwt = [self touchById: t];
            if (glfwt == 0x0) {
                // Could not find corresponding GLFWTouch.
                // Possibly due to too many touches at once,
                // we only support GLFW_MAX_TOUCH.
                continue;
            }

            // We can only update previous touches that has been initialized (began, moved etc).
            if (glfwt->Phase == GLFW_PHASE_IDLE) {
                glfwt->Reference = 0x0;
                continue;
            }

            [self touchUpdate: glfwt withTouch: t];

            if (glfwt == _glfwInput.MouseEmulationTouch || !_glfwInput.MouseEmulationTouch) {
                [self updateGlfwMousePos: glfwt->X y: glfwt->Y];
                if ((phase == GLFW_PHASE_ENDED || phase == GLFW_PHASE_CANCELLED)) {
                    _glfwInputMouseClick( GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE );
                } else {
                    if (_glfwWin.mousePosCallback) {
                        _glfwWin.mousePosCallback(glfwt->X, glfwt->Y);
                    }
                }
            }
        }
    }
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
    [self fillTouch: event forPhase: UITouchPhaseMoved];
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
    if (self.keyboardActive && self.autoCloseKeyboard) {
        // Implicitly hide keyboard
        _glfwShowKeyboard(0, 0, 0);
    }

    [self fillTouchStart: event];
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
    [self fillTouch: event forPhase: UITouchPhaseEnded];
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event
{
    [self fillTouch: event forPhase: UITouchPhaseCancelled];
}

- (BOOL)canBecomeFirstResponder
{
    return YES;
}

- (BOOL)hasText
{
    return YES;
}

- (void)setMarkedText:(NSString *)newMarkedText selectedRange:(NSRange)selectedRange {
    [markedText setString:newMarkedText];
    _glfwSetMarkedText([markedText UTF8String]);
}

- (void)insertText:(NSString *)theText
{
    int length = [theText length];

    if (length == 1 && [theText characterAtIndex: 0] == 10) {
        _glfwInputKey( GLFW_KEY_ENTER, GLFW_PRESS );
        self.textkeyActive = TEXT_KEY_COOLDOWN;
        return;
    }

    for(int i = 0;  i < length;  i++) {
        // Trick to "fool" glfw. Otherwise repeated characters will be filtered due to repeat
        _glfwInputChar( [theText characterAtIndex:i], GLFW_RELEASE );
        _glfwInputChar( [theText characterAtIndex:i], GLFW_PRESS );
    }
}

- (void)deleteBackward
{
    if (markedText.length > 0)
    {
        [markedText setString:@""];
        _glfwSetMarkedText("");
    } else {
        _glfwInputKey( GLFW_KEY_BACKSPACE, GLFW_PRESS );
        self.textkeyActive = TEXT_KEY_COOLDOWN;
    }
}

- (void)clearMarkedText
{
    [inputDelegate textWillChange: self];
    [markedText setString: @""];
    [inputDelegate textDidChange: self];
    _glfwSetMarkedText("");
}

- (UIKeyboardType) keyboardType
{
    return keyboardType;
}

- (void) setKeyboardType: (UIKeyboardType) type
{
    keyboardType = type;
}

- (BOOL) isSecureTextEntry
{
    return secureTextEntry;
}

- (void) setSecureTextEntry: (BOOL) secureEntry
{
    secureTextEntry = secureEntry;
}

- (UIReturnKeyType) returnKeyType
{
    return UIReturnKeyDefault;
}

- (void)layoutSubviews
{
    [EAGLContext setCurrentContext:context];
    [self destroyFramebuffer];
    [self createFramebuffer];
    [self swapBuffers];
}

- (int) getWindowWidth
{
    return backingWidth;
}

- (int) getWindowHeight
{
    return backingHeight;
}

- (GLuint) getViewFramebuffer
{
    return viewFramebuffer;
}

- (BOOL)createFramebuffer
{
    glGenFramebuffers(1, &viewFramebuffer);
    glGenRenderbuffers(1, &viewRenderbuffer);

    glBindFramebuffer(GL_FRAMEBUFFER, viewFramebuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, viewRenderbuffer);
    [context renderbufferStorage:GL_RENDERBUFFER fromDrawable:(CAEAGLLayer*)self.layer];
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, viewRenderbuffer);

    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &backingWidth);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &backingHeight);

    // _glfwWin.width = backingWidth;
    // _glfwWin.height = backingHeight;
    // _glfwWin.frameBuffer = viewFramebuffer;

    if (_glfwWin.windowSizeCallback)
    {
        _glfwWin.windowSizeCallback( backingWidth, backingHeight );
    }

    // Setup packed depth and stencil buffers
    glGenRenderbuffers(1, &depthStencilRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depthStencilRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, backingWidth, backingHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencilRenderbuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthStencilRenderbuffer);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        NSLog(@"failed to make complete framebuffer object %x", glCheckFramebufferStatus(GL_FRAMEBUFFER));
        return NO;
    }
    CHECK_GL_ERROR
    return YES;
}

- (void)destroyFramebuffer
{
    if (viewFramebuffer)
    {
        glBindRenderbuffer(GL_RENDERBUFFER, viewRenderbuffer);
        CHECK_GL_ERROR
        [context renderbufferStorage:GL_RENDERBUFFER fromDrawable: nil];
        CHECK_GL_ERROR

        glDeleteFramebuffers(1, &viewFramebuffer);
        CHECK_GL_ERROR
        viewFramebuffer = 0;
        glDeleteRenderbuffers(1, &viewRenderbuffer);
        CHECK_GL_ERROR
        viewRenderbuffer = 0;

        if(depthStencilRenderbuffer)
        {
            glDeleteRenderbuffers(1, &depthStencilRenderbuffer);
            CHECK_GL_ERROR
            depthStencilRenderbuffer = 0;
        }
    }
}

- (void)dealloc
{
    if (displayLink != 0)
    {
        [displayLink release];
    }
    [EAGLContext setCurrentContext:context];
    [self destroyFramebuffer];
    [EAGLContext setCurrentContext:nil];
    if (auxContext != 0)
    {
        [auxContext release];
    }
    [context release];
    [super dealloc];
}

@end


//========================================================================
// Query auxillary context
//========================================================================
int _glfwPlatformQueryAuxContext()
{
    if(!_glfwWin.aux_context)
        return 0;
    return 1;
}

//========================================================================
// Acquire auxillary context for current thread
//========================================================================
void* _glfwPlatformAcquireAuxContext()
{
    if(!_glfwWin.aux_context)
    {
        fprintf( stderr, "Unable to make OpenGL aux context current, is NULL\n" );
        return 0;
    }
    if(![EAGLContext setCurrentContext:_glfwWin.aux_context])
    {
        fprintf( stderr, "Unable to make OpenGL aux context current, setCurrentContext failed\n" );
        return 0;
    }
    return _glfwWin.aux_context;
}

//========================================================================
// Unacquire auxillary context for current thread
//========================================================================
void _glfwPlatformUnacquireAuxContext(void* context)
{
    [EAGLContext setCurrentContext:nil];
}


//========================================================================
// Reset keyboard input state (clears marked text)
//========================================================================

void _glfwResetKeyboard( void )
{
    EAGLView* view = (EAGLView*) _glfwWin.view;
    [view clearMarkedText];
}

//========================================================================
// Get physical accelerometer
//========================================================================

int _glfwPlatformGetAcceleration(float* x, float* y, float* z)
{
    if (g_AccelerometerEnabled) {
        *x = _glfwInput.AccX;
        *y = _glfwInput.AccY;
        *z = _glfwInput.AccZ;
    }
    return g_AccelerometerEnabled;
}

GLFWAPI void glfwAccelerometerEnable()
{
    [[UIAccelerometer sharedAccelerometer] setUpdateInterval:g_AccelerometerFrequency];
    [[UIAccelerometer sharedAccelerometer] setDelegate:_glfwWin.viewController];
    g_AccelerometerEnabled = 1;
}

//========================================================================
// Keyboard
//========================================================================

void _glfwShowKeyboard( int show, int type, int auto_close )
{
    EAGLView* view = (EAGLView*) _glfwWin.view;
    view.secureTextEntry = NO;
    switch (type) {
        case GLFW_KEYBOARD_DEFAULT:
            view.keyboardType = UIKeyboardTypeDefault;
            break;
        case GLFW_KEYBOARD_NUMBER_PAD:
            view.keyboardType = UIKeyboardTypeNumberPad;
            break;
        case GLFW_KEYBOARD_EMAIL:
            view.keyboardType = UIKeyboardTypeEmailAddress;
            break;
        case GLFW_KEYBOARD_PASSWORD:
            view.secureTextEntry = YES;
            view.keyboardType = UIKeyboardTypeDefault;
            break;
        default:
            view.keyboardType = UIKeyboardTypeDefault;
    }
    view.textkeyActive = -1;
    view.autoCloseKeyboard = auto_close;
    if (show) {
        view.keyboardActive = YES;
        [_glfwWin.view becomeFirstResponder];
    } else {
        view.keyboardActive = NO;
        [_glfwWin.view resignFirstResponder];
    }
}


//========================================================================
// Poll for new window and input events
//========================================================================

void _glfwPlatformPollEvents( void )
{
    // if (g_StartupPhase == INIT1 && g_SwapCount > 1) {
    //     printf("_glfwPlatformPollEvents setting bailEventLoopBuf");
    //     if (!setjmp(_glfwWin.bailEventLoopBuf))
    //     {
    //     printf("_glfwPlatformPollEvents calling finishInitBuf");
    //         longjmp(_glfwWin.finishInitBuf, 1);
    //     }
    //     else
    //     {
    //         g_StartupPhase = COMPLETE;
    //     }
    //     return;
    // }

    // if (g_StartupPhase != COMPLETE) {
    //     return;
    // }

    // NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    // SInt32 result;
    // do {
    //     result = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, TRUE);
    // } while (result == kCFRunLoopRunHandledSource);
    // [pool release];

    EAGLView* view = (EAGLView*) _glfwWin.view;
    if (view.keyboardActive) {
        if (view.textkeyActive == 0) {
            _glfwInputKey( GLFW_KEY_BACKSPACE, GLFW_RELEASE );
            _glfwInputKey( GLFW_KEY_ENTER, GLFW_RELEASE );
        } else {
            view.textkeyActive = view.textkeyActive - 1;
        }
    }
}


int _glfwPlatformGetWindowRefreshRate( void )
{
    EAGLView* view = (EAGLView*) _glfwWin.view;
    CADisplayLink* displayLink = view->displayLink;

    @try { // displayLink.preferredFramesPerSecond only supported on iOS 10.0 and higher, default to 0 for older versions.
        return displayLink.preferredFramesPerSecond;
    } @catch (NSException* exception) {
        return 0;
    }
}


int  _glfwPlatformOpenWindow( int width, int height,
                              const _GLFWwndconfig *wndconfig,
                              const _GLFWfbconfig *fbconfig )
{

    printf("_glfwPlatformOpenWindow\n");

    // Width and height are set by the EAGLView
    _glfwWin.width = [g_EAGLView getWindowWidth];
    _glfwWin.height = [g_EAGLView getWindowHeight];

    _glfwWin.portrait = height > width ? GL_TRUE : GL_FALSE;

    // The desired orientation might have changed when rebooting to a new game
    g_Savewin.portrait = _glfwWin.portrait;

    /*
     * This is somewhat of a hack. We can't recreate the application here.
     * Instead we reinit the app and return and keep application and windows as is
     * We should either move application creation to glfwInit or skip glfw altogether.
     */
    // UIApplication* app = [UIApplication sharedApplication];
    // if (app)
    // {
    //     printf("_glfwPlatformOpenWindow calling reinit");
    //     [g_ApplicationDelegate reinit: app];
    //     return GL_TRUE;
    // }

    _glfwWin.pixelFormat = nil;
    _glfwWin.delegate = g_AppDelegate;

    _glfwWin.view = g_EAGLView;
    _glfwWin.window = g_ApplicationWindow;

    _glfwWin.context = g_glContext;
    _glfwWin.aux_context = g_glAuxContext;

    _glfwWin.frameBuffer = [g_EAGLView getViewFramebuffer];

    /*
     * NOTE:
     * We ignore the following
     * wndconfig->*
     * fbconfig->*
     */

    // const int stack_size = 1 << 18;
    // // // Store stack pointer in a global variable.
    // // // Otherwise the allocated stack might be removed by the optimizer
    // g_ReservedStack = alloca(stack_size);

    //     printf("_glfwPlatformOpenWindow setting bailEventLoopBuf");
    // if (!setjmp(_glfwWin.bailEventLoopBuf) )
    // {
    //     char* argv[] = { "dummy" };
    //     int retVal = UIApplicationMain(1, argv, nil, @"AppDelegate");
    //     (void) retVal;
    // }
    // else
    // {
    // }

    printf("_glfwPlatformOpenWindow exiting\n");
    return GL_TRUE;
}
