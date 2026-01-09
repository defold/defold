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

#ifndef DM_GAMESYS_FONTGEN_H
#define DM_GAMESYS_FONTGEN_H

#include <dmsdk/dlib/hash.h>
#include <dmsdk/font/font.h>
#include <dmsdk/extension/extension.h>

#include <dlib/job_thread.h>
#include <dmsdk/gamesys/resources/res_font.h>

struct TextGlyph; // font/text_layout.h

namespace dmGameSystem
{
    struct FontResource;

    dmExtension::Result FontGenInitialize(dmExtension::Params* params);
    dmExtension::Result FontGenFinalize(dmExtension::Params* params);

    float FontGenGetBasePadding(); // E.g. 3
    float FontGenGetEdgeValue(); // [0 .. 255]

    // Resource api
    struct FontGenJobData;
    FontGenJobData* FontGenCreateJobData(FontResource* resource, uint32_t num_glyphs);
    void            FontGenDestroyJobData(FontGenJobData* jobdata);

    dmJobThread::HJob FontGenAddGlyphByIndex(FontGenJobData* jobdata, HFont font, uint32_t glyph_index, dmGameSystem::FPrewarmTextCallback cbk, void* cbk_ctx);
    dmJobThread::HJob FontGenAddGlyphs(FontGenJobData* jobdata, TextGlyph* glyphs, uint32_t num_glyphs, dmGameSystem::FPrewarmTextCallback cbk, void* cbk_ctx);

    // If we're busy waiting for created glyphs
    void FontGenFlushFinishedJobs(uint64_t timeout);
}

#endif // DM_GAMESYS_FONTGEN_H
