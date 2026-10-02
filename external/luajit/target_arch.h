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

// Preprocess with the target compiler; the host CMake project reads these records.
#include "lj_arch.h"
#define LJ_EXPORT(name) LJ_CONFIG #name name

LJ_BITS LJ_ARCH_BITS
LJ_EXPORT(LUAJIT_TARGET)
LJ_EXPORT(LUAJIT_OS)
LJ_EXPORT(LJ_ARCH_HASFPU)
LJ_EXPORT(LJ_ABI_SOFTFP)
LJ_EXPORT(LJ_TARGET_UNALIGNED)
LJ_EXPORT(LUA_USERDATA_ALIGNMENT)
LJ_EXPORT(LUAJIT_SECURITY_PRNG)
LJ_EXPORT(LUAJIT_SECURITY_STRHASH)
LJ_EXPORT(LUAJIT_SECURITY_STRID)
LJ_EXPORT(LUAJIT_SECURITY_MCODE)
#if LJ_DUALNUM
LJ_CONFIG "LUAJIT_NUMMODE" 2
#else
LJ_CONFIG "LUAJIT_NUMMODE" 1
#endif
#if !LJ_HASJIT
LJ_CONFIG "LUAJIT_DISABLE_JIT" 1
#endif
#if !LJ_HASFFI
LJ_CONFIG "LUAJIT_DISABLE_FFI" 1
#endif
#if !LJ_HASBUFFER
LJ_CONFIG "LUAJIT_DISABLE_BUFFER" 1
#endif
#if !LJ_HASPROFILE
LJ_CONFIG "LUAJIT_DISABLE_PROFILE" 1
#endif
#ifdef LUAJIT_DISABLE_GC64
LJ_CONFIG "LUAJIT_DISABLE_GC64" 1
#endif
#ifdef LUAJIT_USE_SYSMALLOC
LJ_CONFIG "LUAJIT_USE_SYSMALLOC" 1
#endif
#if LJ_52
LJ_CONFIG "LUAJIT_ENABLE_LUA52COMPAT" 1
#endif
#ifdef LJ_ARCH_VERSION
LJ_VERSION LJ_ARCH_VERSION
#endif
#if LJ_TARGET_X64 && LJ_FR2
LJ_ARCH x64
#elif LJ_TARGET_X86ORX64
LJ_ARCH x86
#elif LJ_TARGET_ARM64
LJ_ARCH arm64
#elif LJ_TARGET_ARM
LJ_ARCH arm
#else
#error Unsupported LuaJIT target architecture
#endif

#if LJ_LE
LJ_DASM ENDIAN_LE
#endif

#if LJ_BE
LJ_DASM ENDIAN_BE
#endif

#if LJ_ARCH_BITS == 64
LJ_DASM P64
#endif

#if LJ_HASJIT
LJ_DASM JIT
#endif

#if LJ_HASFFI
LJ_DASM FFI
#endif

#if LJ_DUALNUM
LJ_DASM DUALNUM
#endif

#if LJ_ARCH_HASFPU
LJ_DASM FPU
#endif

#if !LJ_ABI_SOFTFP
LJ_DASM HFABI
#endif

#if LJ_NO_UNWIND
LJ_DASM NO_UNWIND
#endif

#if LJ_ABI_PAUTH
LJ_DASM PAUTH
#endif

#if LJ_ABI_BRANCH_TRACK
LJ_DASM BRANCH_TRACK
#endif

#if LJ_ABI_SHADOW_STACK
LJ_DASM SHADOW_STACK
#endif

#if LJ_TARGET_WINDOWS
LJ_DASM WIN
#endif

#if LJ_TARGET_ARM && LJ_TARGET_IOS
LJ_DASM IOS
#endif

#if LJ_ABI_PAUTH
LJ_EXPORT(LJ_ABI_PAUTH)
#endif

#if LJ_ABI_BRANCH_TRACK
LJ_EXPORT(LJ_ABI_BRANCH_TRACK)
#endif

#if LJ_ABI_SHADOW_STACK
LJ_EXPORT(LJ_ABI_SHADOW_STACK)
#endif

#if LJ_TARGET_CONSOLE
LJ_EXPORT(LJ_TARGET_CONSOLE)
#endif

#if LJ_TARGET_NX
LJ_EXPORT(LJ_TARGET_NX)
#endif

#if LJ_TARGET_XBOXONE
LJ_EXPORT(LJ_TARGET_XBOXONE)
#endif

#if LJ_NO_UNWIND
LJ_CONFIG "LUAJIT_NO_UNWIND" 1
#endif
