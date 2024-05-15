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

#include "res_glyph_bank.h"

namespace dmGameSystem
{
    dmResource::Result ResGlyphBankPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmRenderDDF::GlyphBank* ddf;
        dmDDF::Result e = dmDDF::LoadMessage<dmRenderDDF::GlyphBank>(params.m_Buffer, params.m_BufferSize, &ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        *params.m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResGlyphBankCreate(const dmResource::ResourceCreateParams& params)
    {
        GlyphBankResource* resource   = new GlyphBankResource();
        resource->m_DDF               = (dmRenderDDF::GlyphBank*) params.m_PreloadData;
        params.m_Resource->m_Resource = (void*) resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResGlyphBankDestroy(const dmResource::ResourceDestroyParams& params)
    {
        GlyphBankResource* resource = (GlyphBankResource*) params.m_Resource->m_Resource;
        if (resource->m_DDF != 0x0)
        {
            dmDDF::FreeMessage(resource->m_DDF);
        }
        delete resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResGlyphBankRecreate(const dmResource::ResourceRecreateParams& params)
    {
        GlyphBankResource* glyph_bank = (GlyphBankResource*)params.m_Resource->m_Resource;

        dmRenderDDF::GlyphBank* ddf;
        dmDDF::Result e = dmDDF::LoadMessage<dmRenderDDF::GlyphBank>(params.m_Buffer, params.m_BufferSize, &ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        if (glyph_bank->m_DDF)
        {
            dmDDF::FreeMessage(glyph_bank->m_DDF);
        }

        glyph_bank->m_DDF = ddf;
        return dmResource::RESULT_OK;
    }
}
