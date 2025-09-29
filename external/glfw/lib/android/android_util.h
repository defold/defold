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

#ifndef _ANDROID_UTIL_H_
#define _ANDROID_UTIL_H_

#include "internal.h"
#include <stdint.h>

#define CMD_INPUT_CHAR (0)
#define CMD_INPUT_MARKED_TEXT (1)

#define MAX_APP_COMMANDS (16)
#define APP_INPUT_EVENTS_SIZE_INCREASE_STEP (32)

struct Command
{
    int m_Command;
    void* m_Data;
};

struct InputEvent
{
    void*   m_Ref; // reference to a finger
    int32_t m_Type;
    int32_t m_Action;
    // touch
    int32_t m_X;
    int32_t m_Y;
    // keycode
    int32_t m_Code;
    int32_t m_Flags;
    int32_t m_Meta;
    int32_t m_ScanCode;
    int32_t m_Repeat;
    int32_t m_DeviceId;
    int32_t m_Source;
    int64_t m_DownTime;
    int64_t m_EventTime;
};

int init_gl(_GLFWwin_android* win);

void final_gl(_GLFWwin_android* win);

void create_gl_surface(_GLFWwin_android* win);

void destroy_gl_surface(_GLFWwin_android* win);

void make_current(_GLFWwin_android* win);

void update_width_height_info(_GLFWwin* win, _GLFWwin_android* win_android, int force);

int query_gl_aux_context(_GLFWwin_android* win);

void* acquire_gl_aux_context(_GLFWwin_android* win);

void unacquire_gl_aux_context(_GLFWwin_android* win);

void computeIconifiedState();
void androidDestroyWindow();

void    _glfwAndroidHandleCommand(struct android_app* app, int32_t cmd);
int32_t _glfwAndroidHandleInput(struct android_app* app, JNIEnv* env, struct InputEvent* event);


// Should only called after an error
// returns 1 if we the window/surface was ok
// returns 0 if we the window/surface was bad
int32_t _glfwAndroidVerifySurfaceError(EGLint error);

// From spinlock.h (we really should keep a C interface there as well!)

#if !defined(__aarch64__)

static inline void spinlock_lock(uint32_t* lock)
{
    uint32_t tmp;
     __asm__ __volatile__(
"1:     ldrex   %0, [%1]\n"
"       teq     %0, #0\n"
// "       wfene\n"  -- WFENE gives SIGILL for still unknown reasons on some 64-bit ARMs Aarch32 mode.
//                      Disassembly of libc.so and pthread_mutex_lock shows no use of the instruction.
"       strexeq %0, %2, [%1]\n"
"       teqeq   %0, #0\n"
"       bne     1b\n"
"        dsb\n"
     : "=&r" (tmp)
     : "r" (lock), "r" (1)
     : "cc");

}

static inline void spinlock_unlock(uint32_t* lock)
{
    __asm__ __volatile__(
"       dsb\n"
"       str     %1, [%0]\n"
    :
    : "r" (lock), "r" (0)
    : "cc");
}

#elif defined(__aarch64__)

// Asm implementation from https://gitlab-beta.engr.illinois.edu/ejclark2/linux/blob/f668cd1673aa2e645ae98b65c4ffb9dae2c9ac17/arch/arm64/include/asm/spinlock.h

static inline void spinlock_lock(uint32_t* lock)
{

    uint32_t tmp;
     __asm__ __volatile__(
"   sevl\n"
    "1: wfe\n"
    "2: ldaxr   %w0, [%1]\n"
    "   cbnz    %w0, 1b\n"
    "   stxr    %w0, %w2, [%1]\n"
    "   cbnz    %w0, 2b\n"
    : "=&r" (tmp)
    : "r" (lock), "r" (1)
    : "memory");

}

static inline void spinlock_unlock(uint32_t* lock)
{
    __asm__ __volatile__(
    "   stlr    %w1, [%0]\n"
    : : "r" (lock), "r" (0) : "memory");

}
#endif

#endif // _ANDROID_UTIL_H_
