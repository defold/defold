// Copyright 2020-2026 The Defold Foundation
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

#include "window_macos.h"
#include "../platform_window_opengl.h"
#include <dlib/log.h>
#include <math.h>

namespace dmPlatform
{
    bool OpenGLCreateContexts(HWindow window)
    {
        uint32_t major = 3, minor = 3;
        const WindowCreateParams& params = window->m_Params;
        if (!OpenGLGetVersion(params.m_GraphicsApiVersionHint, &major, &minor) && params.m_GraphicsApiVersionHint)
        {
            dmLogWarning("Unsupported OpenGL version hint %d. Using OpenGL 3.3.", params.m_GraphicsApiVersionHint);
        }

        if (major > 4 || (major == 4 && minor > 1))
        {
            return false;
        }

        NSOpenGLPixelFormatAttribute attributes[] = {
            NSOpenGLPFAOpenGLProfile, (NSOpenGLPixelFormatAttribute)(major == 4 ? NSOpenGLProfileVersion4_1Core : NSOpenGLProfileVersion3_2Core),
            NSOpenGLPFAAccelerated, NSOpenGLPFAClosestPolicy, NSOpenGLPFADoubleBuffer,
            NSOpenGLPFAColorSize, 24, NSOpenGLPFAAlphaSize, 8,
            NSOpenGLPFADepthSize, 24, NSOpenGLPFAStencilSize, 8,
            NSOpenGLPFASampleBuffers, (NSOpenGLPixelFormatAttribute)(params.m_Samples > 0),
            NSOpenGLPFASamples, (NSOpenGLPixelFormatAttribute)params.m_Samples, 0
        };
        NSOpenGLPixelFormat* format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
        if (!format)
        {
            return false;
        }

        window->m_Context = [[NSOpenGLContext alloc] initWithFormat:format shareContext:nil];
        if (window->m_Context)
        {
            window->m_AuxContext = [[NSOpenGLContext alloc] initWithFormat:format shareContext:window->m_Context];
        }

        [format release];
        if (!window->m_Context)
        {
            return false;
        }

        [window->m_View setWantsBestResolutionOpenGLSurface:YES];
        window->m_Window.colorSpace = [NSColorSpace sRGBColorSpace];
        [window->m_Context setView:window->m_View];
        [window->m_Context makeCurrentContext];
        return true;
    }

    void OpenGLDestroyContexts(HWindow window)
    {
        NSOpenGLContext* current = [NSOpenGLContext currentContext];
        if (current && (current == window->m_Context || current == window->m_AuxContext))
        {
            [NSOpenGLContext clearCurrentContext];
        }

        [window->m_Context clearDrawable];
        [window->m_AuxContext release];
        [window->m_Context release];
    }

    int32_t OpenGLGetDefaultFramebufferId()
    {
        return 0;
    }

    uintptr_t GetProcAddress(HWindow window, const char* name)
    {
        CFBundleRef bundle = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.opengl"));
        if (!bundle)
        {
            return 0;
        }

        CFStringRef symbol = CFStringCreateWithCString(NULL, name, kCFStringEncodingASCII);
        uintptr_t address = (uintptr_t)CFBundleGetFunctionPointerForName(bundle, symbol);
        CFRelease(symbol);
        return address;
    }

    void SetSwapInterval(HWindow window, uint32_t interval)
    {
        GLint value = (GLint)interval;
        [window->m_Context setValues:&value forParameter:NSOpenGLContextParameterSwapInterval];
    }

    void SwapBuffers(HWindow window)
    {
        @autoreleasepool
        {
            // NSOpenGL ignores the swap interval when the window is fully occluded.
            if (!(window->m_Window.occlusionState & NSWindowOcclusionStateVisible))
            {
                GLint interval = 0;
                [window->m_Context getValues:&interval forParameter:NSOpenGLContextParameterSwapInterval];
                if (interval > 0)
                {
                    NSInteger refresh_rate = window->m_Window.screen.maximumFramesPerSecond;
                    NSTimeInterval period = (double)interval / (refresh_rate > 0 ? refresh_rate : 60);
                    NSTimeInterval now = NSProcessInfo.processInfo.systemUptime;
                    [NSThread sleepForTimeInterval:period - fmod(now, period)];
                }
            }

            [window->m_Context flushBuffer];
        }
    }

    void* AcquireAuxContext(HWindow window)
    {
        [window->m_AuxContext makeCurrentContext];
        return window->m_AuxContext;
    }

    void UnacquireAuxContext(HWindow, void*)
    {
        [NSOpenGLContext clearCurrentContext];
    }

}
