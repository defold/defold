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

#ifndef DM_WINDOW_IOS_H
#define DM_WINDOW_IOS_H

#include "native_handles.h"
#include <OpenGLES/ES3/gl.h>
#ifdef __OBJC__
#import <UIKit/UIKit.h>
#endif

int   dmNativeOSIsSceneActive(void);
int   dmNativeOSOpenWindowOpenGL(int width, int height);
int   dmNativeOSOpenWindowVulkan(int width, int height);
int   dmNativeOSQueryAuxContextOpenGL(void);
int   dmNativeOSQueryAuxContextVulkan(void);
void* dmNativeOSAcquireAuxContextOpenGL(void);
void* dmNativeOSAcquireAuxContextVulkan(void);
void  dmNativeOSUnacquireAuxContextOpenGL(void* context);
void  dmNativeOSUnacquireAuxContextVulkan(void* context);

#endif
