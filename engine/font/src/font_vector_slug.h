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

#ifndef DM_FONT_VECTOR_SLUG_H
#define DM_FONT_VECTOR_SLUG_H
#include <slug/slug.hpp>
#include <dmsdk/font/font.h>
static const uint32_t FONT_VECTOR_SLUG_WIDTH = dmSlug::TEXTURE_WIDTH;
static const uint32_t FONT_VECTOR_SLUG_MAX_HEIGHT = dmSlug::MAX_TEXTURE_HEIGHT;
typedef dmSlug::Path  FontVectorSlugGlyph;
typedef dmSlug::Data  FontVectorSlugData;

void FontVectorSlugBegin(FontVectorSlugData* data);
// Appends one glyph using unhinted, unsplit quadratic outlines in em-space.
// Each band header/list and each curve's two texels stay within a texture row.
// On failure, existing records and counters are preserved.
bool FontVectorSlugAddGlyph(FontVectorSlugData* data, const FontGlyphOutline& outline, float font_size, uint32_t bands, FontVectorSlugGlyph* glyph);

// Normalizes runtime or prebaked glyph geometry to its original glyph bounds.
// Inputs are borrowed. On failure, existing records and counters are preserved.
bool FontVectorSlugAddFontGlyph(FontVectorSlugData* data, const FontGlyph& source, uint32_t bands, FontVectorSlugGlyph* glyph);

#endif
