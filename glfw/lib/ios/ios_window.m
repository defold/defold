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
#import <OpenGLES/ES1/gl.h>
#import <OpenGLES/ES1/glext.h>
#import <OpenGLES/EAGLDrawable.h>
#import <QuartzCore/QuartzCore.h>

#include "internal.h"
#include "platform.h"

//************************************************************************
//****               Platform implementation functions                ****
//************************************************************************

// OpenGL view

/*
This class wraps the CAEAGLLayer from CoreAnimation into a convenient UIView subclass.
The view content is basically an EAGL surface you render your OpenGL scene into.
Note that setting the view non-opaque will only work if the EAGL surface has an alpha channel.
*/
@interface EAGLView : UIView {

@private
    GLint backingWidth;
    GLint backingHeight;
    EAGLContext *context;
    GLuint viewRenderbuffer, viewFramebuffer;
    GLuint depthRenderbuffer;
}

- (void)swapBuffers;
- (void)setupView;

@end

@interface EAGLView ()

@property (nonatomic, retain) EAGLContext *context;

- (BOOL) createFramebuffer;
- (void) destroyFramebuffer;

@end

@implementation EAGLView

@synthesize context;

+ (Class)layerClass
{
    return [CAEAGLLayer class];
}

- (id)initWithFrame:(CGRect)frame
{
    _glfwWin.view = self;
    if ((self = [super initWithFrame:frame]))
    {
        // Get the layer
        CAEAGLLayer *eaglLayer = (CAEAGLLayer *)self.layer;

        eaglLayer.opaque = YES;
        eaglLayer.drawableProperties = [NSDictionary dictionaryWithObjectsAndKeys:
                                        [NSNumber numberWithBool:NO], kEAGLDrawablePropertyRetainedBacking, kEAGLColorFormatRGBA8, kEAGLDrawablePropertyColorFormat, nil];

        context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES1];

        if (!context || ![EAGLContext setCurrentContext:context])
        {
            [self release];
            return nil;
        }

        [self setupView];
    }
    return self;
}

- (void)setupView
{
}

- (void)swapBuffers
{
    glBindRenderbufferOES(GL_RENDERBUFFER_OES, viewRenderbuffer);
    [context presentRenderbuffer:GL_RENDERBUFFER_OES];
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
    UITouch *touch = [touches anyObject];
    CGPoint touchLocation = [touch locationInView:self];

    _glfwInput.MousePosX = touchLocation.x;
    _glfwInput.MousePosY = touchLocation.y;

    if( _glfwWin.mousePosCallback )
    {
        _glfwWin.mousePosCallback( _glfwInput.MousePosX, _glfwInput.MousePosY );
    }
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
    UITouch *touch = [touches anyObject];
    CGPoint touchLocation = [touch locationInView:self];

    _glfwInput.MousePosX = touchLocation.x;
    _glfwInput.MousePosY = touchLocation.y;

    _glfwInputMouseClick( GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS );
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
    UITouch *touch = [touches anyObject];
    CGPoint touchLocation = [touch locationInView:self];

    _glfwInput.MousePosX = touchLocation.x;
    _glfwInput.MousePosY = touchLocation.y;

    _glfwInputMouseClick( GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE );
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
    glGenFramebuffersOES(1, &viewFramebuffer);
    glGenRenderbuffersOES(1, &viewRenderbuffer);

    glBindFramebufferOES(GL_FRAMEBUFFER_OES, viewFramebuffer);
    glBindRenderbufferOES(GL_RENDERBUFFER_OES, viewRenderbuffer);
    [context renderbufferStorage:GL_RENDERBUFFER_OES fromDrawable:(CAEAGLLayer*)self.layer];
    glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_RENDERBUFFER_OES, viewRenderbuffer);

    glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_WIDTH_OES, &backingWidth);
    glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_HEIGHT_OES, &backingHeight);

    // Setup depth buffers
    glGenRenderbuffersOES(1, &depthRenderbuffer);
    glBindRenderbufferOES(GL_RENDERBUFFER_OES, depthRenderbuffer);
    glRenderbufferStorageOES(GL_RENDERBUFFER_OES, GL_DEPTH_COMPONENT16_OES, backingWidth, backingHeight);
    glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_DEPTH_ATTACHMENT_OES, GL_RENDERBUFFER_OES, depthRenderbuffer);

    if(glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES) != GL_FRAMEBUFFER_COMPLETE_OES)
    {
        NSLog(@"failed to make complete framebuffer object %x", glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES));
        return NO;
    }

    return YES;
}

- (void)destroyFramebuffer
{
    glDeleteFramebuffersOES(1, &viewFramebuffer);
    viewFramebuffer = 0;
    glDeleteRenderbuffersOES(1, &viewRenderbuffer);
    viewRenderbuffer = 0;

    if(depthRenderbuffer)
    {
        glDeleteRenderbuffersOES(1, &depthRenderbuffer);
        depthRenderbuffer = 0;
    }
}

- (void)dealloc
{
    if ([EAGLContext currentContext] == context)
    {
        [EAGLContext setCurrentContext:nil];
    }

    [context release];
    [super dealloc];
}

@end


//========================================================================
// Here is where the window is created, and the OpenGL rendering context is
// created
//========================================================================


@interface AppDelegate : NSObject <UIApplicationDelegate>
{
    UIWindow *window;
    EAGLView *glView;
}

@property (nonatomic, retain) IBOutlet UIWindow *window;
@property (nonatomic, retain) IBOutlet EAGLView *glView;

@end


@implementation AppDelegate

@synthesize window;
@synthesize glView;

- (void)applicationDidFinishLaunching:(UIApplication *)application
{
    // NOTE: On iPhone4 the "resolution" is 480x320 and not 960x640
    // Points vs pixels (and scale factors). I'm not sure that this correct though
    // and that we really get the correct and highest physical resolution in pixels.
    CGRect bounds = [UIScreen mainScreen].bounds;

    window = [[UIWindow alloc] initWithFrame:bounds];
    glView = [[EAGLView alloc] initWithFrame:window.bounds];
    [window addSubview:glView];
    [window makeKeyAndVisible];

    _glfwWin.width = bounds.size.width;
    _glfwWin.height = bounds.size.height;

    // We can't hijack the event loop here. We post-pone it to ensure that the application is
    // completely initialized
    [NSTimer scheduledTimerWithTimeInterval:0.001 target:self selector:@selector(hijackEventLoop) userInfo:nil repeats:NO];
}

- (void)hijackEventLoop
{
    // Long jump back to start and run our own event-loop/game-loop
    longjmp(_glfwWin.bailEventLoopBuf,1);
}

- (void)applicationWillResignActive:(UIApplication *)application
{
    // We should pause the update loop when this message is sent
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
}

- (void)applicationWillTerminate:(UIApplication *)application
{
}

- (void)dealloc
{
    [window release];
    [glView release];
    [super dealloc];
}

@end


int  _glfwPlatformOpenWindow( int width, int height,
                              const _GLFWwndconfig *wndconfig,
                              const _GLFWfbconfig *fbconfig )
{
    _glfwWin.pixelFormat = nil;
    _glfwWin.window = nil;
    _glfwWin.context = nil;
    _glfwWin.delegate = nil;
    _glfwWin.view = nil;

    /*
     * NOTE:
     * We ignore the following
     * wndconfig->*
     * fbconfig->*
     */

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
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, TRUE);
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
