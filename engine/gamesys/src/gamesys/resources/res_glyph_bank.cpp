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

#include "res_glyph_bank.h"

#include <string.h>

#include <font/font_glyphbank.h>

namespace dmGameSystem
{
    struct GlyphBankResource
    {
        dmFontDDF::GlyphBank* m_DDF;
        HFont                 m_Font;
        FontGlyphBankProvider m_Provider;
    };

    static uint32_t GlyphBankGetCodepoint(void* context, uint32_t glyph_index)
    {
        dmFontDDF::GlyphBank* glyph_bank = (dmFontDDF::GlyphBank*)context;
        return glyph_bank->m_Glyphs[glyph_index].m_Character;
    }

    static bool GlyphBankGetGlyph(void* context, uint32_t glyph_index, FontGlyphBankGlyph* output)
    {
        dmFontDDF::GlyphBank*              glyph_bank = (dmFontDDF::GlyphBank*)context;
        const dmFontDDF::GlyphBank::Glyph& glyph = glyph_bank->m_Glyphs[glyph_index];
        if (glyph.m_GlyphDataSize > 0 &&
            (glyph.m_GlyphDataOffset > glyph_bank->m_GlyphData.m_Count ||
             glyph.m_GlyphDataSize > glyph_bank->m_GlyphData.m_Count - glyph.m_GlyphDataOffset))
        {
            return false;
        }

        memset(output, 0, sizeof(*output));
        output->m_Codepoint = glyph.m_Character;
        output->m_Width = glyph.m_Width;
        output->m_Advance = glyph.m_Advance;
        output->m_LeftBearing = glyph.m_LeftBearing;
        output->m_Ascent = glyph.m_Ascent;
        output->m_Descent = glyph.m_Descent;
        if (glyph.m_GlyphDataSize != 0)
        {
            const uint8_t* glyph_data = glyph_bank->m_GlyphData.m_Data + glyph.m_GlyphDataOffset;
            output->m_Data = glyph_data + 1;
            output->m_DataSize = (uint32_t)glyph.m_GlyphDataSize - 1;
            output->m_BitmapFlags = glyph_data[0];
        }
        return true;
    }

    static void SetupGlyphBankProvider(GlyphBankResource* resource, dmFontDDF::GlyphBank* glyph_bank)
    {
        resource->m_Provider.m_Context = glyph_bank;
        resource->m_Provider.m_GetCodepoint = GlyphBankGetCodepoint;
        resource->m_Provider.m_GetGlyph = GlyphBankGetGlyph;
        resource->m_Provider.m_Destroy = 0;
        resource->m_Provider.m_ResourceSize = sizeof(*glyph_bank) +
                                              (uint64_t)glyph_bank->m_Glyphs.m_Count * sizeof(dmFontDDF::GlyphBank::Glyph) +
                                              glyph_bank->m_GlyphData.m_Count;
        resource->m_Provider.m_GlyphCount = glyph_bank->m_Glyphs.m_Count;
        resource->m_Provider.m_GlyphPadding = (uint32_t)glyph_bank->m_GlyphPadding;
        resource->m_Provider.m_GlyphChannels = glyph_bank->m_GlyphChannels;
        resource->m_Provider.m_MaxAscent = glyph_bank->m_MaxAscent;
        resource->m_Provider.m_MaxDescent = glyph_bank->m_MaxDescent;
    }

    HFont GetFont(GlyphBankResource* resource)
    {
        return resource->m_Font;
    }

    dmFontDDF::GlyphBank* GetGlyphBank(GlyphBankResource* resource)
    {
        return resource->m_DDF;
    }

    dmResource::Result ResGlyphBankPreload(const dmResource::ResourcePreloadParams* params)
    {
        dmFontDDF::GlyphBank* ddf;
        dmDDF::Result e = dmDDF::LoadMessage<dmFontDDF::GlyphBank>(params->m_Buffer, params->m_BufferSize, &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_PROTOBUF_ERROR;
        }

        *params->m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResGlyphBankCreate(const dmResource::ResourceCreateParams* params)
    {
        GlyphBankResource* resource = new GlyphBankResource();
        resource->m_DDF = (dmFontDDF::GlyphBank*)params->m_PreloadData;
        SetupGlyphBankProvider(resource, resource->m_DDF);
        resource->m_Font = FontCreateGlyphBank(params->m_Filename, &resource->m_Provider);
        if (!resource->m_Font)
        {
            dmDDF::FreeMessage(resource->m_DDF);
            delete resource;
            return dmResource::RESULT_OUT_OF_MEMORY;
        }
        dmResource::SetResource(params->m_Resource, resource);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResGlyphBankDestroy(const dmResource::ResourceDestroyParams* params)
    {
        GlyphBankResource* resource = (GlyphBankResource*)dmResource::GetResource(params->m_Resource);
        if (resource->m_Font)
        {
            FontDestroy(resource->m_Font);
        }

        if (resource->m_DDF != 0x0)
        {
            dmDDF::FreeMessage(resource->m_DDF);
        }
        delete resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResGlyphBankRecreate(const dmResource::ResourceRecreateParams* params)
    {
        GlyphBankResource* glyph_bank = (GlyphBankResource*)dmResource::GetResource(params->m_Resource);

        dmFontDDF::GlyphBank* ddf;
        dmDDF::Result e = dmDDF::LoadMessage<dmFontDDF::GlyphBank>(params->m_Buffer, params->m_BufferSize, &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_PROTOBUF_ERROR;
        }

        dmFontDDF::GlyphBank* old_ddf = glyph_bank->m_DDF;
        glyph_bank->m_DDF = ddf;
        SetupGlyphBankProvider(glyph_bank, ddf);

        if (old_ddf)
        {
            dmDDF::FreeMessage(old_ddf);
        }
        return dmResource::RESULT_OK;
    }
}
