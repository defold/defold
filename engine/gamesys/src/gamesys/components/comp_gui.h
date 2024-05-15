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

#ifndef DM_GAMESYS_COMP_GUI_H
#define DM_GAMESYS_COMP_GUI_H

#include <stdint.h>
#include <gui/gui.h>
#include <dmsdk/gamesys/gui.h>

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

    // Used only in engine_service.cpp for resource profiling
    typedef bool (*FDynamicTextturesIterator)(dmhash_t gui_res_id, dmhash_t name_hash, uint32_t size, void* user_ctx);
    void IterateDynamicTextures(dmhash_t gui_res_id, dmGameObject::SceneNode* node, FDynamicTextturesIterator callback, void* user_ctx);
}

#endif // DM_GAMESYS_COMP_GUI_H
