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

#pragma once

#include "automation_private.h"

namespace dmAutomation
{
    // Defold private API adapter.
    //
    // This file intentionally isolates calls into Defold APIs that are not part
    // of the stable public extension SDK. Keep all private render/gui access
    // behind this small wrapper so the rest of Automation Bridge only depends
    // on high-level helpers and can fall back when internals change.
    void EngineAccessInitialize(dmExtension::Params* params);
    void EngineAccessFinalize();
    uint32_t EngineAccessGetResourceVersion(void* resource);
    uint64_t EngineAccessGetComponentVersion(const dmGameObject::SceneNode* node);
    bool EngineAccessCanSetWindowSize();
    bool EngineAccessSetWindowSize(uint32_t width, uint32_t height);
    bool EngineAccessComputeBounds(const dmGameObject::SceneNode* scene_node, const Node* node, const Snapshot* snapshot, Bounds* out_bounds);
    void EngineAccessDrawInputVisualization(InputVisualization* visualization);
}
