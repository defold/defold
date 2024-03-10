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

#ifndef DMGRAPHICS_GLSL_UNIFORM_PARSER_H
#define DMGRAPHICS_GLSL_UNIFORM_PARSER_H

#include <stdint.h>
#include "../graphics.h"

namespace dmGraphics
{
    enum GLSLUniformParserBindingType
    {
        UNIFORM   = 0,
        ATTRIBUTE = 1,
    };

    typedef void (*ShaderBindingCallback)(GLSLUniformParserBindingType binding_type, const char* name, uint32_t name_length, Type type, uint32_t size, uintptr_t userdata);

    bool GLSLUniformParse(ShaderDesc::Language language, const char* buffer, ShaderBindingCallback cb, uintptr_t userdata);
    bool GLSLAttributeParse(ShaderDesc::Language language, const char* buffer, ShaderBindingCallback cb, uintptr_t userdata);
}

#endif // DMGRAPHICS_GLSL_UNIFORM_PARSER_H
