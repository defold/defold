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

#ifndef DM_GAMESYS_RES_FONT_PRIVATE_H
#define DM_GAMESYS_RES_FONT_PRIVATE_H

#include <stdint.h>

#include <render/font_ddf.h>
#include <render/font/fontmap.h>

#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/hashtable.h>
#include <dmsdk/resource/resource.h>
#include <dmsdk/render/render.h>
#include <dmsdk/gamesys/resources/res_ttf.h>

#include <gamesys/fontgen/fontgen.h>

namespace dmGameSystem
{
    struct MaterialResource;
    struct GlyphBankResource;
    struct FontResource;

    struct FontJobResourceInfo
    {
        dmArray<void*>          m_Resources; // the resources that are incref'ed for this job
        FontGenJobData*         m_FontGenJobData; // the job scratch data, owned by the sentinel job
        FontResource*           m_Resource;
        dmJobThread::HJob       m_Job;

        FPrewarmTextCallback    m_Callback;
        void*                   m_CallbackContext;
    };

    struct FontResource
    {
        dmRenderDDF::FontMap*   m_DDF;
        dmRender::HFontMap      m_FontMap;
        MaterialResource*       m_MaterialResource;
        GlyphBankResource*      m_GlyphBankResource;
        TTFResource*            m_TTFResource;          // the default ttf resource (if it's a dynamic font)
        dmJobThread::HContext   m_Jobs;
        dmResource::HFactory    m_Factory;
        uint32_t                m_CacheCellPadding;
        uint32_t                m_ResourceSize;         // For correct resource usage reporting
        bool                    m_IsDynamic;            // Are the glyphs populated at runtime?
        uint8_t                 m_Padding;              // Extra space for outline + shadow
        uint8_t                 m_Prewarming:1;         // If true, it is currently waiting for glyphs to be prewamed (dynamic fonts only)
        uint8_t                 m_PrewarmDone:1;

        dmHashTable64<TTFResource*> m_TTFResources;  // Maps path hash to a resource
        dmHashTable32<uint64_t>     m_FontHashes;    // Maps HFont path hash to a resource
        dmhash_t                    m_PathHash;

        dmArray<FontJobResourceInfo*>  m_PendingJobs;

        FontResource();

        // Used when recreating a font
        void Swap(FontResource* src);
    };

    void   ResFontDebugPrint(FontResource* font);
}

#endif // DM_GAMESYS_RES_FONT_PRIVATE_H
