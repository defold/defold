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

#ifndef DM_GRAPHICS_PRIVATE_H
#define DM_GRAPHICS_PRIVATE_H

#include <stdint.h>
#include "graphics.h"

namespace dmGraphics
{
    PipelineState GetDefaultPipelineState();
    void SetPipelineStateValue(PipelineState& pipeline_state, State state, uint8_t value);
    uint64_t GetDrawCount();
    void SetForceFragmentReloadFail(bool should_fail);
    void SetForceVertexReloadFail(bool should_fail);

    // Gets the bits per pixel from uncompressed formats
    uint32_t GetTextureFormatBitsPerPixel(TextureFormat format);

    bool IsTextureFormatCompressed(TextureFormat format);
}

#endif // #ifndef DM_GRAPHICS_PRIVATE_H
