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

#ifndef DM_GAMESYS_RES_BUFFER_H
#define DM_GAMESYS_RES_BUFFER_H

#include <stdint.h>

#include <resource/resource.h>
#include <dlib/buffer.h>
#include "buffer_ddf.h"

namespace dmGameSystem
{
    struct BufferResource
    {
        dmBufferDDF::BufferDesc* m_BufferDDF;
        dmBuffer::HBuffer        m_Buffer;
        uint64_t                 m_ElementCount;

        /// Needed to keep track of data change on resource reload.
        uint32_t                 m_Version;
    };

    dmResource::Result ResBufferPreload(const dmResource::ResourcePreloadParams& params);

    dmResource::Result ResBufferCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResBufferDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResBufferRecreate(const dmResource::ResourceRecreateParams& params);
}

#endif // DM_GAMESYS_RES_BUFFER_H
