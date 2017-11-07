//========================================================================
// GLFW - An OpenGL framework
// Platform:    Cocoa/NSOpenGL
// API Version: 2.7
// WWW:         http://www.glfw.org/
//------------------------------------------------------------------------
// Copyright (c) 2009-2010 Camilla Berglund <elmindreda@elmindreda.org>
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would
//    be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.
//
//========================================================================

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <OpenGLES/EAGL.h>
#import <OpenGLES/ES2/gl.h>
#import <OpenGLES/ES2/glext.h>
#import <OpenGLES/EAGLDrawable.h>
#import <QuartzCore/QuartzCore.h>

#include "internal.h"
#include "platform.h"

enum StartupPhase
{
    INITIAL,
    INIT1,
    INIT2,
    COMPLETE,
};

enum StartupPhase g_StartupPhase = INITIAL;
void* g_ReservedStack = 0;
int g_SwapCount = 0;

static int g_AccelerometerEnabled = 0;
static double g_AccelerometerFrequency = 1.0 / 60.0;

static int g_IsReboot = 0;
/*
Notes about the crazy startup
In order to have a classic event-loop we must "bail" the built-in event dispatch loop
using setjmp/longjmp. Moreover, data must be loaded before applicationDidFinishLaunching
is completed as the launch image is removed moments after applicationDidFinishLaunching finish.
It's also imperative that applicationDidFinishLaunching completes entirely. If not, the launch image
isn't removed as the startup sequence isn't completed properly. Something that complicates this matter
is that it isn't allowed to longjmp in a setjmp as the stack is squashed. By allocating a lot
of stack *before* UIApplicationMain is invoked we can be quite confident that stack used by UIApplicationMain
and descendants is kept intact. This is a crazy hack but we don't have much of a choice. Only
alternative is to modify glfw to have a structure similar to Cocoa Touch.

Additionally we postpone startup sequence until we have swapped gl-buffers twice in
order to avoid black screen between launch image and game content.
*/


//************************************************************************
//****               Platform implementation functions                ****
//************************************************************************

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


#define MAX_APP_DELEGATES (32)
id<UIApplicationDelegate> g_AppDelegates[MAX_APP_DELEGATES];
int g_AppDelegatesCount = 0;
id<UIApplicationDelegate> g_ApplicationDelegate = 0;

@interface AppDelegateProxy : NSObject <UIApplicationDelegate>

@end

@implementation AppDelegateProxy

// NOTE: Don't understand why this special case is required. "forwardInvocation" et al
// should be able to intercept all invocations but for some unknown reason not handleOpenURL
-(BOOL) application:(UIApplication *)application handleOpenURL:(NSURL *)url {
    SEL sel = @selector(application:handleOpenURL:);
    BOOL handled = NO;

    if ([g_ApplicationDelegate respondsToSelector:sel]) {
        if ([g_ApplicationDelegate application: application handleOpenURL: url])
            handled = YES;
    }

    for (int i = 0; i < g_AppDelegatesCount; ++i) {
        if ([g_AppDelegates[i] respondsToSelector: sel]) {
            if ([g_AppDelegates[i] application: application handleOpenURL: url])
                handled = YES;
        }
    }
    return handled;
}

-(BOOL) application:(UIApplication *)application openURL:(NSURL *)url sourceApplication:(NSString *)sourceApplication annotation:(id)annotation{
    SEL sel = @selector(application:openURL:sourceApplication:annotation:);
    BOOL handled = NO;

   for (int i = 0; i < g_AppDelegatesCount; ++i) {
        if ([g_AppDelegates[i] respondsToSelector: sel])  {
            if ([g_AppDelegates[i] application: application openURL: url sourceApplication:sourceApplication annotation:(id)annotation])
                handled = YES;
        }
    }

    // handleOpenURL is deprecated. We call it from here as if openURL is implemented, handleOpenURL won't be called.
    if ([self application: application handleOpenURL:url])
        handled = YES;

    return handled;
}

- (void)forwardInvocation:(NSInvocation *)anInvocation {
    BOOL invoked = NO;
    if ([g_ApplicationDelegate respondsToSelector: [anInvocation selector]]) {
        [anInvocation invokeWithTarget: g_ApplicationDelegate];
        invoked = YES;
    }

    for (int i = 0; i < g_AppDelegatesCount; ++i) {
        if ([g_AppDelegates[i] respondsToSelector: [anInvocation selector]]) {
            [anInvocation invokeWithTarget: g_AppDelegates[i]];
            invoked = YES;
        }
    }

    if (!invoked) {
        [super forwardInvocation:anInvocation];
    }
}

- (BOOL)respondsToSelector:(SEL)aSelector {
    if ([g_ApplicationDelegate respondsToSelector: aSelector]) {
        return YES;
    }

    for (int i = 0; i < g_AppDelegatesCount; ++i) {
        if ([g_AppDelegates[i] respondsToSelector: aSelector]) {
            return YES;
        }
    }

    return [super respondsToSelector: aSelector];
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
    NSMethodSignature* signature = [super methodSignatureForSelector:aSelector];

    if (!signature)
    {
        for (int i = 0; i < g_AppDelegatesCount; ++i) {
            if ([g_AppDelegates[i] respondsToSelector: aSelector]) {
                return [g_AppDelegates[i] methodSignatureForSelector:aSelector];
            }
        }
    }
    return signature;
}

@end

GLFWAPI void glfwRegisterUIApplicationDelegate(void* delegate)
{
    if (g_AppDelegatesCount >= MAX_APP_DELEGATES) {
        printf("Max UIApplicationDelegates reached (%d)", MAX_APP_DELEGATES);
    } else {
        g_AppDelegates[g_AppDelegatesCount++] = (id<UIApplicationDelegate>) delegate;
    }
}

GLFWAPI void glfwUnregisterUIApplicationDelegate(void* delegate)
{
    assert(g_AppDelegatesCount > 0);
    for (int i = 0; i < g_AppDelegatesCount; ++i)
    {
        if (g_AppDelegates[i] == delegate)
        {
            g_AppDelegates[i] = g_AppDelegates[g_AppDelegatesCount - 1];
            g_AppDelegatesCount--;
            return;
        }
    }
    assert(false && "app delegate not found");
}

@interface IndexedPosition : UITextPosition {
    NSUInteger _index;
}
@property (nonatomic) NSUInteger index;
+ (IndexedPosition *)positionWithIndex:(NSUInteger)index;
@end

@interface IndexedRange : UITextRange {
    NSRange _range;
}
@property (nonatomic) NSRange range;
+ (IndexedRange *)rangeWithNSRange:(NSRange)range;

@end

@implementation IndexedPosition
@synthesize index = _index;

+ (IndexedPosition *)positionWithIndex:(NSUInteger)index {
    IndexedPosition *pos = [[IndexedPosition alloc] init];
    pos.index = index;
    return [pos autorelease];
}

@end

@implementation IndexedRange
@synthesize range = _range;

+ (IndexedRange *)rangeWithNSRange:(NSRange)nsrange {
    if (nsrange.location == NSNotFound)
        return nil;
    IndexedRange *range = [[IndexedRange alloc] init];
    range.range = nsrange;
    return [range autorelease];
}

- (UITextPosition *)start {
    return [IndexedPosition positionWithIndex:self.range.location];
}

- (UITextPosition *)end {
        return [IndexedPosition positionWithIndex:(self.range.location + self.range.length)];
}

-(BOOL)isEmpty {
    return (self.range.length == 0);
}
@end


/*
This class wraps the CAEAGLLayer from CoreAnimation into a convenient UIView subclass.
The view content is basically an EAGL surface you render your OpenGL scene into.
Note that setting the view non-opaque will only work if the EAGL surface has an alpha channel.
*/
@interface EAGLView : UIView<UIKeyInput, UITextInput> {

@private
    GLint backingWidth;
    GLint backingHeight;
    EAGLContext *context;
    EAGLContext *auxContext;
    GLuint viewRenderbuffer, viewFramebuffer;
    GLuint depthStencilRenderbuffer;
    CADisplayLink* displayLink;
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

@end

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
    _glfwWin.view = self;
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
    }

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
                           toPosition:(UITextPosition *)other { return nil; }

- (void) setBaseWritingDirection: (UITextWritingDirection)writingDirection forRange:(UITextRange *)range { }

- (void)swapBuffers
{
    if (g_StartupPhase == COMPLETE) {
        // Do not poll event before startup sequence is completed
        NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
        while (countDown > 0)
        {
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1, TRUE);
        }
        [pool release];

        countDown = swapInterval;
    }

    g_SwapCount++;

    // NOTE: We poll events above and the application might be iconfied
    // At least when running in frame-rates < 60
    if (!_glfwWin.iconified && g_StartupPhase == COMPLETE)
    {
        const GLenum discards[]  = {GL_DEPTH_ATTACHMENT};
        glBindFramebuffer(GL_FRAMEBUFFER, viewFramebuffer);
        glDiscardFramebufferEXT(GL_FRAMEBUFFER, 1, discards);

        glBindRenderbuffer(GL_RENDERBUFFER, viewRenderbuffer);
        [context presentRenderbuffer:GL_RENDERBUFFER];
    }
}

- (void)newFrame
{
    countDown--;
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
    } else if (prevPhase == GLFW_PHASE_BEGAN && newPhase == GLFW_PHASE_ENDED) {
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
            [self touchStart: glfwt withTouch: t];

            if (glfwt == _glfwInput.MouseEmulationTouch) {
                [self updateGlfwMousePos: glfwt->X y: glfwt->Y];
                _glfwInputMouseClick( GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS );
            }
        }
    }
}

- (void) fillTouch: (UIEvent*) event forPhase: phase
{
    NSSet *touches = [event allTouches];

    for (UITouch *t in touches)
    {
        if (phase == t.phase) {
            GLFWTouch* glfwt = [self touchById: t];
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

    _glfwWin.width = backingWidth;
    _glfwWin.height = backingHeight;
    _glfwWin.frameBuffer = viewFramebuffer;

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
// Here is where the window is created, and the OpenGL rendering context is
// created
//========================================================================


// View controller

@interface ViewController : UIViewController<UIContentContainer, UIAccelerometerDelegate>
{
    EAGLView *glView;
    CGSize cachedViewSize;
}

- (EAGLContext *)initialiseGlContext;
- (EAGLContext *)initialiseGlAuxContext;
- (void)createGlView;

// iOS 8.0.0 - 8.0.2
- (CGSize)getIntendedViewSize;
- (CGPoint)getIntendedFrameOrigin:(CGSize)size;
- (BOOL)shouldUpdateViewFrame;
- (void)updateViewFramesWorkaround;

@property (nonatomic, retain) IBOutlet EAGLView *glView;

@end

@implementation ViewController

@synthesize glView;

- (void)dealloc
{
    [glView release];
    [super dealloc];
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    self.view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.view.autoresizesSubviews = YES;

    [self createGlView];

    _glfwWin.viewController = self;

    float version = [[UIDevice currentDevice].systemVersion floatValue];

    if (version < 6)
    {
        // NOTE: This is only required for older versions of iOS
        // In iOS 6 new method was introduced for rotation logc, see AppDelegate
        UIInterfaceOrientation orientation = _glfwWin.portrait ? UIInterfaceOrientationPortrait : UIInterfaceOrientationLandscapeRight;
        [[UIApplication sharedApplication] setStatusBarOrientation: orientation animated: NO];
    }
}

- (void)createGlView
{
    EAGLContext* glContext = nil;
    EAGLContext* glAuxContext = nil;
    if (glView) {
        // We must recycle the GL context, since the engine will be performing operations
        // (e.g. creating shaders and textures) that depend upon it.
        glContext = glView.context;
        glAuxContext = glView.auxContext;
        [glView removeFromSuperview];
    }

    if (!glContext) {
        glContext = [self initialiseGlContext];
        glAuxContext = [self initialiseGlAuxContext: glContext];
    }
    _glfwWin.context = glContext;
    _glfwWin.aux_context = glAuxContext;

    CGRect bounds = self.view.bounds;
    float version = [[UIDevice currentDevice].systemVersion floatValue];
    if (8.0 <= version && 8.1 > version) {
        CGSize size = [self getIntendedViewSize];
        CGRect parent_bounds = self.view.bounds;
        parent_bounds.size = size;

        if ([self shouldUpdateViewFrame]) {
            CGPoint origin = [self getIntendedFrameOrigin: size];

            CGRect parent_frame = self.view.frame;
            parent_frame.origin = origin;
            parent_frame.size = size;

            self.view.frame = parent_frame;
        }
        bounds = parent_bounds;
    }
    cachedViewSize = bounds.size;

    CGFloat scaleFactor = [[UIScreen mainScreen] scale];
    glView = [[[EAGLView alloc] initWithFrame: bounds] autorelease];
    glView.context = glContext;
    glView.auxContext = glAuxContext;
    glView.contentScaleFactor = scaleFactor;
    glView.layer.contentsScale = scaleFactor;
    [[self view] addSubview:glView];

    [glView createFramebuffer];
}

- (void)updateViewFramesWorkaround
{
    float version = [[UIDevice currentDevice].systemVersion floatValue];
    if (8.0 <= version && 8.1 > version) {
        CGRect parent_frame = self.view.frame;
        CGRect parent_bounds = self.view.bounds;

        CGSize size = [self getIntendedViewSize];

        parent_bounds.size = size;

        if ([self shouldUpdateViewFrame]) {
            CGPoint origin = [self getIntendedFrameOrigin: size];
            parent_frame.origin = origin;
            parent_frame.size = size;

            self.view.frame = parent_frame;
        }
        glView.frame = parent_bounds;
    }
}

- (BOOL)shouldUpdateViewFrame
{
    UIDevice *device = [UIDevice currentDevice];
    UIDeviceOrientation orientation = device.orientation;
    bool update_parent_frame = false;
    switch (orientation) {
        case UIDeviceOrientationLandscapeLeft:
            update_parent_frame = true;
            break;
        case UIDeviceOrientationLandscapeRight:
            update_parent_frame = true;
            break;
        case UIDeviceOrientationPortrait:
            update_parent_frame = true;
            break;
        case UIDeviceOrientationPortraitUpsideDown:
            update_parent_frame = true;
            break;
        default:
            break;
    }
    return update_parent_frame;
}

- (CGSize)getIntendedViewSize
{
    CGSize result;
    CGRect parent_bounds = self.view.bounds;

    if (0 != g_IsReboot) {
        parent_bounds.size = cachedViewSize;
    }
    bool flipBounds = false;
    if (_glfwWin.portrait) {
        flipBounds = parent_bounds.size.width > parent_bounds.size.height;
    } else {
        flipBounds = parent_bounds.size.width < parent_bounds.size.height;
    }
    if (flipBounds) {
        result.width = parent_bounds.size.height;
        result.height = parent_bounds.size.width;
    } else {
        result = parent_bounds.size;
    }
    return result;
}

- (CGPoint)getIntendedFrameOrigin:(CGSize)size
{
    UIDevice *device = [UIDevice currentDevice];
    UIDeviceOrientation orientation = device.orientation;
    CGPoint origin;
    origin.x = 0.0f;
    origin.y = 0.0f;
    switch (orientation) {
        case UIDeviceOrientationLandscapeLeft:
            break;
        case UIDeviceOrientationLandscapeRight:
            origin.x = -(size.width - size.height);
            origin.y = size.width - size.height;

            if (g_IsReboot && cachedViewSize.width != size.width) {
                origin.x = 0.0f;
                origin.y = 0.0f;
            }
            break;
        case UIDeviceOrientationPortrait:
            if (g_IsReboot && cachedViewSize.width != size.width) {
                origin.x = -(size.width - size.height);
            }
            break;
        case UIDeviceOrientationPortraitUpsideDown:
            if (g_IsReboot && cachedViewSize.width != size.width) {
                origin.y = (size.width - size.height);
            }
            break;
        default:
            break;
    }
    return origin;
}

- (EAGLContext *)initialiseGlContext
{
    EAGLContext *context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];

    if (!context || ![EAGLContext setCurrentContext:context])
    {
        return nil;
    }

    return context;
}

- (EAGLContext *)initialiseGlAuxContext:(EAGLContext *)parentContext
{
    EAGLContext *context = [[EAGLContext alloc] initWithAPI:[parentContext API] sharegroup: [parentContext sharegroup]];

    if (!context)
    {
        return nil;
    }

    return context;
}

- (void)viewDidAppear:(BOOL)animated
{
    // NOTE: We rely on an active OpenGL-context as we have no concept of Begin/End rendering
    // As we replace view-controller and view when re-opening the "window" we must ensure that we always
    // have an active context (context is set to nil when view is deallocated)
    [EAGLContext setCurrentContext: glView.context];

    [super viewDidAppear: animated];

    if (g_StartupPhase == INIT2) {
        longjmp(_glfwWin.bailEventLoopBuf, 1);
    }
}

- (void)viewDidUnload
{
    [super viewDidUnload];
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
    // NOTE: For iOS < 6
    if (_glfwWin.portrait)
    {
        return   interfaceOrientation == UIInterfaceOrientationPortrait
              || interfaceOrientation == UIInterfaceOrientationPortraitUpsideDown;
    }
    else
    {
        return   interfaceOrientation == UIInterfaceOrientationLandscapeRight
              || interfaceOrientation == UIInterfaceOrientationLandscapeLeft;
    }
}

- (void)didRotateFromInterfaceOrientation:(UIInterfaceOrientation)fromInterfaceOrientation
{

}

-(BOOL)shouldAutorotate{
    // NOTE: Only for iOS6
    return YES;
}

-(NSUInteger)supportedInterfaceOrientations {
    // NOTE: Only for iOS6
    return UIInterfaceOrientationMaskLandscape | UIInterfaceOrientationMaskPortrait | UIInterfaceOrientationMaskPortraitUpsideDown;
}

#pragma mark UIContentContainer

// Introduced in iOS8.0
- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator
{
    [self updateViewFramesWorkaround];
}

- (void)accelerometer:(UIAccelerometer *)accelerometer didAccelerate:(UIAcceleration *)acceleration
{
    _glfwInput.AccX = acceleration.x;
    _glfwInput.AccY = acceleration.y;
    _glfwInput.AccZ = acceleration.z;
}
@end

// Application delegate

@interface AppDelegate : NSObject <UIApplicationDelegate>
{
    UIWindow *window;
}

- (void)forceDeviceOrientation;
- (void)reinit:(UIApplication *)application;

@property (nonatomic, retain) IBOutlet UIWindow *window;

@end

_GLFWwin g_Savewin;

@implementation AppDelegate

@synthesize window;

- (void)forceDeviceOrientation;
{
    // Running iOS8, if we don't force a change in the device orientation then
    // the framebuffer will not be created with the correct orientation.
    UIDevice *device = [UIDevice currentDevice];
    UIDeviceOrientation desired = UIDeviceOrientationLandscapeRight;
    if (_glfwWin.portrait) {
        desired = UIDeviceOrientationPortrait;
    } else if (UIDeviceOrientationLandscapeLeft == device.orientation) {
        desired = UIDeviceOrientationLandscapeLeft;
    }
    [device setValue: [NSNumber numberWithInteger: desired] forKey:@"orientation"];
}

- (void)reinit:(UIApplication *)application
{
    g_IsReboot = 1;

    // Restore window data
    _glfwWin = g_Savewin;

    // To avoid a race, since _glfwPlatformOpenWindow does not block,
    // update the glfw's cached screen dimensions ahead of time.
    BOOL flipScreen = NO;
    if (_glfwWin.portrait) {
        flipScreen = _glfwWin.width > _glfwWin.height;
    } else {
        flipScreen = _glfwWin.width < _glfwWin.height;
    }
    if (flipScreen) {
        float tmp = _glfwWin.width;
        _glfwWin.width = _glfwWin.height;
        _glfwWin.height = tmp;
    }

    [self forceDeviceOrientation];

    float version = [[UIDevice currentDevice].systemVersion floatValue];
    if (8.0 <= version && 8.1 > version) {
        // These suspect versions of iOS will crash if we proceed to recreate the GL view.
        return;
    }

    // We then rebuild the GL view back within the application's event loop.
    dispatch_async(dispatch_get_main_queue(), ^{
        ViewController *controller = (ViewController *)window.rootViewController;
        [controller createGlView];
    });
}


- (BOOL)application:(UIApplication *)application willFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    BOOL handled = NO;
    for (int i = 0; i < g_AppDelegatesCount; ++i) {
        if ([g_AppDelegates[i] respondsToSelector: @selector(application:willFinishLaunchingWithOptions:)]) {
            if ([g_AppDelegates[i] application:application willFinishLaunchingWithOptions:launchOptions])
                handled = YES;
        }
    }
    return handled;
}


- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    [self forceDeviceOrientation];

    // NOTE: On iPhone4 the "resolution" is 480x320 and not 960x640
    // Points vs pixels (and scale factors). I'm not sure that this correct though
    // and that we really get the correct and highest physical resolution in pixels.
    CGRect bounds = [UIScreen mainScreen].bounds;

    window = [[UIWindow alloc] initWithFrame:bounds];
    window.rootViewController = [[[ViewController alloc] init] autorelease];
    [window makeKeyAndVisible];
    _glfwWin.window = window;

    UIApplication* app = [UIApplication sharedApplication];
    AppDelegateProxy* proxy = [[AppDelegateProxy alloc] init];
    g_ApplicationDelegate = [app.delegate retain];
    app.delegate = proxy;

    for (int i = 0; i < g_AppDelegatesCount; ++i) {
        if ([g_AppDelegates[i] respondsToSelector: @selector(applicationDidFinishLaunching:)]) {
            [g_AppDelegates[i] applicationDidFinishLaunching: application];
        }
    }

    if (!setjmp(_glfwWin.finishInitBuf))
    {
        g_StartupPhase = INIT1;
        longjmp(_glfwWin.bailEventLoopBuf, 1);
    }
    else
    {
        g_StartupPhase = INIT2;
    }

    BOOL handled = NO;

    for (int i = 0; i < g_AppDelegatesCount; ++i) {
        if ([g_AppDelegates[i] respondsToSelector: @selector(application:didFinishLaunchingWithOptions:)]) {
            if ([g_AppDelegates[i] application:application didFinishLaunchingWithOptions:launchOptions])
                handled = YES;
        }
    }

    return handled;
}

- (void)applicationWillResignActive:(UIApplication *)application
{
    // We should pause the update loop when this message is sent
    _glfwWin.iconified = GL_TRUE;

    // According to Apple glFinish() should be called here
    glFinish();
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
    if(_glfwWin.windowFocusCallback)
        _glfwWin.windowFocusCallback(0);
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
    _glfwWin.iconified = GL_FALSE;
    if(_glfwWin.windowFocusCallback)
        _glfwWin.windowFocusCallback(1);
}

- (void)applicationWillTerminate:(UIApplication *)application
{
}

- (void)dealloc
{
    [window release];
    [super dealloc];
}

@end


int  _glfwPlatformOpenWindow( int width, int height,
                              const _GLFWwndconfig *wndconfig,
                              const _GLFWfbconfig *fbconfig )
{

    _glfwWin.portrait = height > width ? GL_TRUE : GL_FALSE;

    // The desired orientation might have changed when rebooting to a new game
    g_Savewin.portrait = _glfwWin.portrait;
    /*
     * This is somewhat of a hack. We can't recreate the application here.
     * Instead we reinit the app and return and keep application and windows as is
     * We should either move application creation to glfwInit or skip glfw altogether.
     */
    UIApplication* app = [UIApplication sharedApplication];
    if (app)
    {
        [g_ApplicationDelegate reinit: app];
        return GL_TRUE;
    }

    _glfwWin.pixelFormat = nil;
    _glfwWin.window = nil;
    _glfwWin.context = nil;
    _glfwWin.aux_context = nil;
    _glfwWin.delegate = nil;
    _glfwWin.view = nil;

    /*
     * NOTE:
     * We ignore the following
     * wndconfig->*
     * fbconfig->*
     */

    const int stack_size = 1 << 18;
    // Store stack pointer in a global variable.
    // Otherwise the allocated stack might be removed by the optimizer
    g_ReservedStack = alloca(stack_size);
    if (!setjmp(_glfwWin.bailEventLoopBuf) )
    {
        char* argv[] = { "dummy" };
        int retVal = UIApplicationMain(1, argv, nil, @"AppDelegate");
        (void) retVal;
    }
    else
    {
    }

    return GL_TRUE;
}

//========================================================================
// Properly kill the window / video display
//========================================================================

void _glfwPlatformCloseWindow( void )
{
    // Save window as glfw clears the memory on close
    g_Savewin = _glfwWin;
}

int _glfwPlatformGetDefaultFramebuffer( )
{
    return _glfwWin.frameBuffer;
}

//========================================================================
// Set the window title
//========================================================================

void _glfwPlatformSetWindowTitle( const char *title )
{
}

//========================================================================
// Set the window size
//========================================================================

void _glfwPlatformSetWindowSize( int width, int height )
{
}

//========================================================================
// Set the window position
//========================================================================

void _glfwPlatformSetWindowPos( int x, int y )
{
}

//========================================================================
// Iconify the window
//========================================================================

void _glfwPlatformIconifyWindow( void )
{
}

//========================================================================
// Restore (un-iconify) the window
//========================================================================

void _glfwPlatformRestoreWindow( void )
{
}

//========================================================================
// Swap buffers
//========================================================================

void _glfwPlatformSwapBuffers( void )
{
    [ _glfwWin.view swapBuffers ];
}

//========================================================================
// Set double buffering swap interval
//========================================================================

void _glfwPlatformSwapInterval( int interval )
{
    [ _glfwWin.view setSwapInterval: interval ];
}

//========================================================================
// Write back window parameters into GLFW window structure
//========================================================================

void _glfwPlatformRefreshWindowParams( void )
{
}

//========================================================================
// Poll for new window and input events
//========================================================================

void _glfwPlatformPollEvents( void )
{
    if (g_StartupPhase == INIT1 && g_SwapCount > 1) {
        if (!setjmp(_glfwWin.bailEventLoopBuf))
        {
            longjmp(_glfwWin.finishInitBuf, 1);
        }
        else
        {
            g_StartupPhase = COMPLETE;
        }
        return;
    }

    if (g_StartupPhase != COMPLETE) {
        return;
    }

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    SInt32 result;
    do {
        result = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, TRUE);
    } while (result == kCFRunLoopRunHandledSource);
    [pool release];

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

//========================================================================
// Wait for new window and input events
//========================================================================

void _glfwPlatformWaitEvents( void )
{
}

//========================================================================
// Hide mouse cursor (lock it)
//========================================================================

void _glfwPlatformHideMouseCursor( void )
{
}

//========================================================================
// Show mouse cursor (unlock it)
//========================================================================

void _glfwPlatformShowMouseCursor( void )
{
}

//========================================================================
// Set physical mouse cursor position
//========================================================================

void _glfwPlatformSetMouseCursorPos( int x, int y )
{
}

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

//========================================================================
// Defold extension: Get native references (window, view and context)
//========================================================================
GLFWAPI id glfwGetiOSUIWindow(void)
{
    return _glfwWin.window;
};
GLFWAPI id glfwGetiOSUIView(void)
{
    return _glfwWin.view;
};
GLFWAPI id glfwGetiOSEAGLContext(void)
{
    return _glfwWin.context;
};

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


GLFWAPI void glfwAccelerometerEnable()
{
    [[UIAccelerometer sharedAccelerometer] setUpdateInterval:g_AccelerometerFrequency];
    [[UIAccelerometer sharedAccelerometer] setDelegate:_glfwWin.viewController];
    g_AccelerometerEnabled = 1;
}


