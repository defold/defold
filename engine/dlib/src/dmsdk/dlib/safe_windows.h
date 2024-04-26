// Copyright 2020-2024 The Defold Foundation
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

#if defined(_MSC_VER)

#ifndef DMSDK_SAFE_WINDOWS_H
#define DMSDK_SAFE_WINDOWS_H

#define WIN32_LEAN_AND_MEAN 1
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#ifdef PlaySound
#undef PlaySound
#endif

#ifdef DrawText
#undef DrawText
#endif

#ifdef DispatchMessage
#undef DispatchMessage
#endif

#ifdef FreeModule
#undef FreeModule
#endif

#ifdef GetTextMetrics
#undef GetTextMetrics
#endif

#undef MAX_TOUCH_COUNT

#endif // DMSDK_SAFE_WINDOWS_H

#endif // defined(_MSC_VER)
