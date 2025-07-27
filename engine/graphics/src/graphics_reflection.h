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

#ifndef DM_GRAPHICS_REFLECTION_H
#define DM_GRAPHICS_REFLECTION_H

#include <dlib/hash.h>
#include <graphics/graphics_ddf.h>

namespace dmGraphics
{
    struct ShaderResourceType
    {
        union
        {
            dmGraphics::ShaderDesc::ShaderDataType m_ShaderType;
            uint32_t                               m_TypeIndex;
        };
        uint8_t m_UseTypeIndex : 1;
    };

    struct ShaderResourceMember
    {
        char*                       m_Name;
        dmhash_t                    m_NameHash;
        ShaderResourceType          m_Type;
        uint32_t                    m_ElementCount;
        uint16_t                    m_Offset;
    };

    struct ShaderResourceTypeInfo
    {
        char*                         m_Name;
        dmhash_t                      m_NameHash;
        dmArray<ShaderResourceMember> m_Members;
    };

    void GetShaderResourceTypes(HProgram prog, const ShaderResourceTypeInfo** types, uint32_t* count);
}

#endif // DM_GRAPHICS_REFLECTION_H
