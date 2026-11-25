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

#ifndef DM_WINDOW_PLACEMENT_H
#define DM_WINDOW_PLACEMENT_H

#include "window.hpp"

struct WindowPlacement;
namespace dmWindowState
{
    struct State;
}

namespace dmPlatform
{
    void        InitWindowPlacement(HWindow window, const WindowCreateParams& params);
    bool        RestoreWindowPlacement(HWindow window);
    const char* GetWindowPlacementPath(HWindow window);
    void        WindowPlacementChanged(HWindow window);
    void        WindowPlacementMonitorsChanged();
    void        WindowPlacementMonitorsChanged(HWindow window);
    void        WindowPlacementSizeRequested(HWindow window);
    void        WindowPlacementShown(HWindow window);
    void        UpdateWindowPlacement(HWindow window, bool flush);
    void        ClampWindowPlacement(HWindow window);
    void        DeleteWindowPlacement(HWindow window);
    // Each desktop backend supplies native geometry in the units stored by window_state.
    WindowPlacement*& PlacementStorage(HWindow window);
    const char*       PlacementBackend(HWindow);
    bool              NormalPlacementWindow(HWindow window);
    bool              ReadPlacementGeometry(HWindow window, dmWindowState::State* state, bool flush = false);
    bool              RestorePlacementGeometry(HWindow window, const dmWindowState::State& saved, bool restore_size);
    void              ApplyPlacementSize(HWindow window, int width, int height);

}

#endif
