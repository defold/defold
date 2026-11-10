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

#ifndef DM_MODELIMPORTER_BUFFERS_H
#define DM_MODELIMPORTER_BUFFERS_H

#include "modelimporter.h"

namespace dmModelImporter
{
    // Packs native geometry in canonical order and assigns each mesh's buffer index.
    // The scene owns the resulting DMMB bytes; node/material/morph data stays separate.
    bool CompileModelBuffers(Scene* scene);

    // Repack Java meshes after splitting, assigning the same canonical range indices.
    bool CompileModelBuffers(Mesh** meshes, uint32_t count, dmArray<uint8_t>& output);

    // Copies selected ranges from a compiled scene, preserving their canonical
    // order. The caller supplies sorted unique indices and owns the output bytes.
    bool SelectModelBuffers(const void* data, const uint32_t* indices, uint32_t count, dmArray<uint8_t>& output);
}

#endif // DM_MODELIMPORTER_BUFFERS_H
