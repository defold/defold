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

#ifndef _android_window_backend_h_
#define _android_window_backend_h_

#include "mobile_shared/window_mobile_private.h"

int   dmNativeAndroidPlatformGetWindowRefreshRate(void);
int   dmNativeAndroidPlatformOpenWindow(int client_api);
void  dmNativeAndroidPlatformCloseWindow(void);
void  dmNativeAndroidPlatformSwapBuffers(void);
void  dmNativeAndroidPlatformSwapInterval(int interval);
int32_t dmNativeAndroidPlatformVerifySurface(void);
void  dmNativeAndroidPlatformSetPendingResizeBecauseOfInsets(void);
void  dmNativeAndroidPlatformOnTermWindow(void);
void  dmNativeAndroidPlatformOnInitWindow(void);
void  dmNativeAndroidPlatformOnGainedFocus(void);
void  dmNativeAndroidPlatformOnResize(void);
void  dmNativeAndroidPlatformAfterFlushEvents(void);
void  dmNativeAndroidPlatformDestroyWindow(void);
int   dmNativeAndroidPlatformQueryAuxContext(void);
void* dmNativeAndroidPlatformAcquireAuxContext(void);
void  dmNativeAndroidPlatformUnacquireAuxContext(void* context);

#endif
