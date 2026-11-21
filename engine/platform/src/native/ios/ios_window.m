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

#include "internal.h"
#include "ios/app/BaseView.h"

extern Nativewin g_Savewin;

void dmNativeOSCloseWindow( void )
{
    // Preserve UIKit and graphics objects across reboot. Callbacks belong to the closing engine.
    g_Savewin = (Nativewin){
        .window = dmNativeWin.window, .view = dmNativeWin.view, .viewController = dmNativeWin.viewController,
        .context = dmNativeWin.context, .aux_context = dmNativeWin.aux_context, .frameBuffer = dmNativeWin.frameBuffer,
        .width = dmNativeWin.width, .height = dmNativeWin.height, .portrait = dmNativeWin.portrait,
        .clientAPI = dmNativeWin.clientAPI
    };
}

int dmNativeOSGetDefaultFramebuffer( )
{
    return dmNativeWin.frameBuffer; // non zero only if OpenGLES
}

void dmNativeOSSetWindowSize( int width, int height )
{
}

void dmNativeOSIconifyWindow( void )
{
}

void dmNativeOSSwapBuffers( void )
{
    BaseView* view = (BaseView*)dmNativeWin.view;
    [view swapBuffers];
}

void dmNativeOSSwapInterval( int interval )
{
    BaseView* view = (BaseView*)dmNativeWin.view;
    [view setSwapInterval: interval];
}

id dmNativeGetiOSUIWindow(void)
{
    return dmNativeWin.window;
}

id dmNativeGetiOSUIView(void)
{
    return dmNativeWin.view;
}

id dmNativeGetiOSEAGLContext(void)
{
    return dmNativeWin.context;
}
