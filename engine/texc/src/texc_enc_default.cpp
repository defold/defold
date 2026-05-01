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

#include "texc.h"
#include "texc_private.h"

namespace dmTexc
{
    bool DefaultEncode(DefaultEncodeSettings* settings, uint8_t** out, uint32_t* out_size)
    {
        uint32_t size = GetDataSize(settings->m_OutPixelFormat, settings->m_Width, settings->m_Height);
        if (!size)
            return false; // Unsupported format

        uint8_t* packed_data = (uint8_t*)malloc(size);
        ConvertRGBA8888ToPf(settings->m_Data, settings->m_Width, settings->m_Height, settings->m_OutPixelFormat, packed_data);

        *out = packed_data;
        *out_size = size;
        return true;
    }
}
