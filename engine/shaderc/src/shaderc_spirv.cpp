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

#include <stdlib.h>
#include <string.h>

#include <dmsdk/dlib/array.h>

#include <spirv/spirv.h>

#include "shaderc_private.h"

namespace dmShaderc
{
    ShaderCompilerSPIRV* NewShaderCompilerSPIRV(HShaderContext context)
    {
        ShaderCompilerSPIRV* compiler = (ShaderCompilerSPIRV*) malloc(sizeof(ShaderCompilerSPIRV));
        memset(compiler, 0, sizeof(ShaderCompilerSPIRV));

        compiler->m_BaseCompiler.m_Language = SHADER_LANGUAGE_SPIRV;
        compiler->m_SPIRVCode     = (uint8_t*) malloc(context->m_ShaderCodeSize);
        compiler->m_SPIRVCodeSize = context->m_ShaderCodeSize;
        memcpy(compiler->m_SPIRVCode, context->m_ShaderCode, context->m_ShaderCodeSize);

        return compiler;
    }

    void DeleteShaderCompilerSPIRV(ShaderCompilerSPIRV* compiler)
    {
        compiler->m_RemapResources.SetCapacity(0);
        free(compiler->m_SPIRVCode);
        free(compiler);
    }

    static void AddRemapEntry(dmArray<ShaderCompilerSPIRV::RemapResourceEntry>& entries, uint64_t name_hash, uint32_t id, uint8_t old_value, uint8_t new_value, ShaderCompilerSPIRV::RemapResourceEntry::Type type)
    {
        ShaderCompilerSPIRV::RemapResourceEntry entry;
        entry.m_NameHash         = name_hash;
        entry.m_Id               = id;
        entry.m_Type             = type;
        entry.m_NewValue.m_Value = new_value;
        entry.m_OldValue.m_Value = old_value;

        entries.OffsetCapacity(1);
        entries.Push(entry);
    }

    void SetResourceLocationSPIRV(HShaderContext context, ShaderCompilerSPIRV* compiler, uint64_t name_hash, uint8_t location)
    {
        const ShaderResource* res = FindShaderResourceInputOutput(context, name_hash);
        if (res)
        {
            AddRemapEntry(compiler->m_RemapResources, name_hash, res->m_Id, res->m_Location, location, ShaderCompilerSPIRV::RemapResourceEntry::TYPE_LOCATION);
        }
    }

    void SetResourceBindingSPIRV(HShaderContext context, ShaderCompilerSPIRV* compiler, uint64_t name_hash, uint8_t binding)
    {
        const ShaderResource* res = FindShaderResourceUniform(context, name_hash);
        if (res)
        {
            AddRemapEntry(compiler->m_RemapResources, name_hash, res->m_Id, res->m_Binding, binding, ShaderCompilerSPIRV::RemapResourceEntry::TYPE_BINDING);
        }
    }

    void SetResourceSetSPIRV(HShaderContext context, ShaderCompilerSPIRV* compiler, uint64_t name_hash, uint8_t set)
    {
        const ShaderResource* res = FindShaderResourceUniform(context, name_hash);
        if (res)
        {
            AddRemapEntry(compiler->m_RemapResources, name_hash, res->m_Id, res->m_Set, set, ShaderCompilerSPIRV::RemapResourceEntry::TYPE_SET);
        }
    }

    static ShaderCompilerSPIRV::RemapResourceEntry* FindEntryById(dmArray<ShaderCompilerSPIRV::RemapResourceEntry>& entries, uint32_t id, ShaderCompilerSPIRV::RemapResourceEntry::Type type)
    {
        for (int i = 0; i < entries.Size(); ++i)
        {
            if (entries[i].m_Type == type && entries[i].m_Id == id)
            {
                return &entries[i];
            }
        }
        return 0;
    }

    ShaderCompileResult* CompileSPIRV(HShaderContext context, ShaderCompilerSPIRV* compiler, const ShaderCompilerOptions& options)
    {
        uint32_t* source = (uint32_t*) compiler->m_SPIRVCode;
        uint32_t num_words_total = compiler->m_SPIRVCodeSize / sizeof(uint32_t);
        uint32_t spirv_header_size_in_words = 5; // 20 bytes

        for (uint32_t i = spirv_header_size_in_words; i < num_words_total;)
        {
            uint32_t word_count = source[i] >> 16;
            uint32_t opcode     = source[i] & 0xFFFF;

            if (opcode == SpvOpDecorate)
            {
                uint32_t id = source[i + 1];
                uint32_t type = source[i + 2];
                ShaderCompilerSPIRV::RemapResourceEntry* entry = 0;

                if (type == SpvDecorationLocation)
                {
                    entry = FindEntryById(compiler->m_RemapResources, id, ShaderCompilerSPIRV::RemapResourceEntry::TYPE_LOCATION);
                }
                else if (type == SpvDecorationBinding)
                {
                    entry = FindEntryById(compiler->m_RemapResources, id, ShaderCompilerSPIRV::RemapResourceEntry::TYPE_BINDING);
                }
                else if (type == SpvDecorationDescriptorSet)
                {
                    entry = FindEntryById(compiler->m_RemapResources, id, ShaderCompilerSPIRV::RemapResourceEntry::TYPE_SET);
                }

                if (entry)
                {
                    source[i + 3] = entry->m_NewValue.m_Value;
                }
            }

            i += word_count;
        }

        ShaderCompileResult* result = (ShaderCompileResult*) malloc(sizeof(ShaderCompileResult));
        memset(result, 0, sizeof(ShaderCompileResult));
        result->m_Data.SetCapacity(compiler->m_SPIRVCodeSize);
        result->m_Data.SetSize(compiler->m_SPIRVCodeSize);
        memcpy(result->m_Data.Begin(), compiler->m_SPIRVCode, compiler->m_SPIRVCodeSize);
        result->m_LastError = "";
        result->m_HLSLNumWorkGroupsId = 0xff;
        return result;
    }
}
