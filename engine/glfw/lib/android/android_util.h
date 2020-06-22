// Copyright 2020 The Defold Foundation
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

#define CMD_INPUT_CHAR (0)
#define CMD_INPUT_MARKED_TEXT (1)

struct Command
{
    int m_Command;
    void* m_Data;
};

void SaveWin(_GLFWwin* win);
void RestoreWin(_GLFWwin* win);

int init_gl(_GLFWwin* win);

void final_gl(_GLFWwin* win);

void create_gl_surface(_GLFWwin* win);

void destroy_gl_surface(_GLFWwin* win);

int query_gl_aux_context(_GLFWwin* win);

void* acquire_gl_aux_context(_GLFWwin* win);

void unacquire_gl_aux_context(_GLFWwin* win);

#endif // _ANDROID_UTIL_H_
