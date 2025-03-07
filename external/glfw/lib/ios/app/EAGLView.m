// Copyright 2020-2023 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

// OpenGL view

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

EAGLContext*                g_glContext = 0;
EAGLContext*                g_glAuxContext = 0;

// AppDelegate.m
extern _GLFWwin             g_Savewin;
extern UIWindow*            g_ApplicationWindow;
extern AppDelegate*         g_ApplicationDelegate;

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


@implementation EAGLView

@synthesize context;
@synthesize auxContext;

+ (Class)layerClass
{
    return [CAEAGLLayer class];
}

+ (EAGLContext*)initialiseGlContext
{
    EAGLContext* ctx = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];

    if (!ctx || ![EAGLContext setCurrentContext:ctx])
    {
        return nil;
    }
    return ctx;
}

+ (EAGLContext *)initialiseGlAuxContext:(EAGLContext *)parentContext
{
    EAGLContext* ctx = [[EAGLContext alloc] initWithAPI:[parentContext API] sharegroup: [parentContext sharegroup]];
    if (!ctx)
    {
        return nil;
    }
    return ctx;
}

+ (BaseView*)createView:(CGRect)bounds recreate:(BOOL)recreate
{
    EAGLContext* glContext = nil;
    EAGLContext* glAuxContext = nil;
    if (recreate) {
        // We must recycle the GL context, since the engine will be performing operations
        // (e.g. creating shaders and textures) that depend upon it.
        glContext = g_glContext;
        glAuxContext = g_glAuxContext;
    }

    if (!glContext) {
        glContext = [EAGLView initialiseGlContext];
        glAuxContext = [EAGLView initialiseGlAuxContext: glContext];
    }
    g_glContext = glContext;
    g_glAuxContext = glAuxContext;

    CGFloat scaleFactor = [[UIScreen mainScreen] scale];
    g_EAGLView = [[[EAGLView alloc] initWithFrame: bounds] autorelease];
    g_EAGLView.context = glContext;
    g_EAGLView.auxContext = glAuxContext;
    g_EAGLView.contentScaleFactor = scaleFactor;
    g_EAGLView.layer.contentsScale = scaleFactor;

    [g_EAGLView createFramebuffer];
    return g_EAGLView;
}

// called from initWithFrame
- (void)setupView
{
    viewRenderbuffer = 0;
    viewFramebuffer = 0;
    depthStencilRenderbuffer = 0;

    // Get the layer
    CAEAGLLayer *eaglLayer = (CAEAGLLayer *)self.layer;
    eaglLayer.opaque = YES;
    eaglLayer.drawableProperties = [NSDictionary dictionaryWithObjectsAndKeys:
                                    [NSNumber numberWithBool:NO], kEAGLDrawablePropertyRetainedBacking, kEAGLColorFormatRGBA8, kEAGLDrawablePropertyColorFormat, nil];
}

// called from dealloc
- (void)teardownView
{
    [EAGLContext setCurrentContext:context];
    [self destroyFramebuffer];
    [EAGLContext setCurrentContext:nil];
    if (auxContext != 0)
    {
        [auxContext release];
    }
    [context release];
}

- (void)swapBuffers
{
    if (_glfwWin.iconified)
        return;

    const GLenum discards[]  = {GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT};
    glBindFramebuffer(GL_FRAMEBUFFER, viewFramebuffer);
    glDiscardFramebufferEXT(GL_FRAMEBUFFER, 2, discards);

    glBindRenderbuffer(GL_RENDERBUFFER, viewRenderbuffer);
    [context presentRenderbuffer:GL_RENDERBUFFER];
}

- (void)setCurrentContext
{
    // NOTE: We rely on an active OpenGL-context as we have no concept of Begin/End rendering
    // As we replace view-controller and view when re-opening the "window" we must ensure that we always
    // have an active context (context is set to nil when view is deallocated)
    [EAGLContext setCurrentContext: context];
}

- (void)layoutSubviews
{
    [EAGLContext setCurrentContext:context];
    [self destroyFramebuffer];
    [self createFramebuffer];
    [self swapBuffers];
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

    GLint backingWidth;
    GLint backingHeight;
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &backingWidth);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &backingHeight);

    [self setWindowWidth:backingWidth];
    [self setWindowHeight:backingHeight];

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
}

@end


//========================================================================
// Query auxillary context
//========================================================================
int _glfwPlatformQueryAuxContextOpenGL()
{
    if(!_glfwWin.aux_context)
        return 0;
    return 1;
}

//========================================================================
// Acquire auxillary context for current thread
//========================================================================
void* _glfwPlatformAcquireAuxContextOpenGL()
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
void _glfwPlatformUnacquireAuxContextOpenGL(void* context)
{
    [EAGLContext setCurrentContext:nil];
}

int  _glfwPlatformOpenWindowOpenGL( int width, int height,
                              const _GLFWwndconfig *wndconfig,
                              const _GLFWfbconfig *fbconfig )
{
    // Width and height are set by the EAGLView
    _glfwWin.width = [g_EAGLView getWindowWidth];
    _glfwWin.height = [g_EAGLView getWindowHeight];

    _glfwWin.portrait = height > width ? GL_TRUE : GL_FALSE;

    // The desired orientation might have changed when rebooting to a new game
    g_Savewin.portrait = _glfwWin.portrait;

    _glfwWin.pixelFormat = nil;
    _glfwWin.delegate = g_ApplicationDelegate;

    _glfwWin.view = g_EAGLView;
    _glfwWin.window = g_ApplicationWindow;

    _glfwWin.context = g_glContext;
    _glfwWin.aux_context = g_glAuxContext;
    _glfwWin.clientAPI = GLFW_OPENGL_API;

    _glfwWin.frameBuffer = [g_EAGLView getViewFramebuffer];
    return GL_TRUE;
}
