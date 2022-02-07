// Copyright 2020-2022 The Defold Foundation
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

#ifndef DMSDK_SPINLOCKTYPES_H
#define DMSDK_SPINLOCKTYPES_H

#if defined(_MSC_VER) || defined(__EMSCRIPTEN__)
#include "spinlocktypes_atomic.h"
#elif defined(ANDROID)
#include "spinlocktypes_android.h"
#elif defined(__linux__)
#include "spinlocktypes_pthread.h"
#elif defined(__MACH__)
#include "spinlocktypes_mach.h"
#elif defined(__NX64__)
#include "spinlocktypes_nx64.h"
#elif defined(__SCE__)
#include "spinlocktypes_ps4.h"
#else
#error "Unsupported platform"
#endif

#endif // DMSDK_SPINLOCKTYPES_H
