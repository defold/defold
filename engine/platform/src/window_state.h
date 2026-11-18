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

#ifndef DM_WINDOW_STATE_H
#define DM_WINDOW_STATE_H

#include <stdint.h>

namespace dmWindowState
{
    struct Rect
    {
        int32_t m_X, m_Y, m_Width, m_Height;
    };

    struct Monitor
    {
        Rect     m_WorkArea;
        uint64_t m_NameHash;
        float    m_ScaleX, m_ScaleY;
    };

    struct State
    {
        Rect    m_Rect; // Absolute content rectangle in GLFW screen coordinates.
        Monitor m_Monitor;
        int32_t m_ConfigWidth, m_ConfigHeight;
        char    m_ResetToken[40];
        bool    m_HasPosition;
    };

    enum LoadResult
    {
        LOAD_OK,
        LOAD_MISSING,
        LOAD_INVALID,
        LOAD_UNSUPPORTED_VERSION
    };

    LoadResult Load(const char* path, State* state);
    bool Save(const char* path, const State& state);
    void ReadResetToken(const char* path, char* token, uint32_t capacity);
    bool RestoreSize(const State& state, int32_t width, int32_t height, const char* reset_token);
    // Monitors are in GLFW order, with the primary monitor first.
    uint32_t SelectMonitor(const State& state, const Monitor* monitors, uint32_t count);
    Rect RestoreRect(const State& state, const Monitor& monitor, bool scale_screen_coordinates);
    void ClampPosition(Rect* rect, const Rect& work, int left, int top, int right, int bottom);
}

#endif
