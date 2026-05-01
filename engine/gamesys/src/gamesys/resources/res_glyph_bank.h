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

#ifndef DM_GAMESYS_RES_GLYPH_BANK_H
#define DM_GAMESYS_RES_GLYPH_BANK_H

#include <dmsdk/font/font.h>
#include <dmsdk/resource/resource.h>
#include <render/font_ddf.h>

namespace dmGameSystem
{
    struct GlyphBankResource;

    HFont GetFont(GlyphBankResource* resource);

    dmRenderDDF::GlyphBank* GetGlyphBank(GlyphBankResource* resource);

    dmResource::Result ResGlyphBankPreload(const dmResource::ResourcePreloadParams* params);

    dmResource::Result ResGlyphBankCreate(const dmResource::ResourceCreateParams* params);

    dmResource::Result ResGlyphBankDestroy(const dmResource::ResourceDestroyParams* params);

    dmResource::Result ResGlyphBankRecreate(const dmResource::ResourceRecreateParams* params);
}

#endif // DM_GAMESYS_RES_GLYPH_BANK_H
