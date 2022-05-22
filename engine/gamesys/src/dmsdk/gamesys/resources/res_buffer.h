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

#ifndef DMSDK_GAMESYS_RES_BUFFER_H
#define DMSDK_GAMESYS_RES_BUFFER_H

#include <stdint.h>

#include <dmsdk/dlib/buffer.h>
#include <dmsdk/dlib/hash.h>
#include <gamesys/buffer_ddf.h>

namespace dmGameSystem
{
    struct BufferResource
    {
        dmBufferDDF::BufferDesc* m_BufferDDF;
        dmBuffer::HBuffer        m_Buffer;
        dmhash_t                 m_NameHash;
        uint32_t                 m_ElementCount;    // The number of vertices
        uint32_t                 m_Stride;          // The vertex size (bytes)
        uint32_t                 m_Version;
    };
}

#endif // DMSDK_GAMESYS_RES_BUFFER_H
