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

#ifndef DM_GAMESYS_COMP_GUI_H
#define DM_GAMESYS_COMP_GUI_H

#include <stdint.h>
#include <gui/gui.h>

namespace dmMessage
{
    struct URL;
}

namespace dmGameSystem
{
    // Used by the engine to setup the dmGui::HContext
    void GuiGetURLCallback(dmGui::HScene scene, dmMessage::URL* url);
    uintptr_t GuiGetUserDataCallback(dmGui::HScene scene);
    dmhash_t GuiResolvePathCallback(dmGui::HScene scene, const char* path, uint32_t path_size);
    void GuiGetTextMetricsCallback(const void* font, const char* text, float width, bool line_break, float leading, float tracking, dmGui::TextMetrics* out_metrics);
}

#endif // DM_GAMESYS_COMP_GUI_H
