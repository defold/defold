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

#ifndef DM_GAMESYS_RES_BUFFER_H
#define DM_GAMESYS_RES_BUFFER_H

#include <dmsdk/resource/resource.hpp>
#include <dmsdk/gamesys/resources/res_buffer.h>

namespace dmGameSystem
{
    // BUFC files are little-endian: magic[4], version:u16, streams:u16,
    // elements:u32, stride:u32; then streams of hash:u64, offset:u32,
    // type:u8, components:u8, padding:u16; then the interleaved payload.
    enum
    {
        BUFFER_FORMAT_VERSION = 1,
        BUFFER_FILE_HEADER_SIZE = 16,
        BUFFER_FILE_STREAM_SIZE = 16,
    };

    dmResource::Result ResBufferPreload(const dmResource::ResourcePreloadParams* params);

    dmResource::Result ResBufferCreate(const dmResource::ResourceCreateParams* params);

    dmResource::Result ResBufferDestroy(const dmResource::ResourceDestroyParams* params);

    dmResource::Result ResBufferRecreate(const dmResource::ResourceRecreateParams* params);
}

#endif // DM_GAMESYS_RES_BUFFER_H
