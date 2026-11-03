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

#ifndef DM_FONTVIEWER_VECTOR_FONT_H
#define DM_FONTVIEWER_VECTOR_FONT_H

#include <dlib/vmath.h>
#include <text_layout.h>
#include <layout_vertex.h>
#include <dmsdk/graphics/graphics.h>

// Borrows fonts until Clear/Destroy. Owns Slug numeric textures and geometry.
struct FontViewerVectorFont;
FontViewerVectorFont* FontViewerVectorFontCreate();
void                  FontViewerVectorFontDestroy(FontViewerVectorFont* font);
void                  FontViewerVectorFontClear(FontViewerVectorFont* font);
// Returns ~0u when the source has no vector outlines. Unsupported color paints use an outline fallback.
bool FontViewerVectorFontAddGlyph(FontViewerVectorFont* font, HFont source, uint32_t glyph, uint32_t* index, bool* has_color);
void FontViewerVectorFontBegin(FontViewerVectorFont* font);
// Scale maps font units to pixels. Pixel scale also includes preview zoom for AA.
// Transform maps layout-local pixels to clip space.
// Clip is an axis-aligned rectangle in clip space (min x/y, max x/y).
void FontViewerVectorFontPack(FontViewerVectorFont* font, uint32_t index, float scale, float pixel_scale, float x, float y, const dmVMath::Matrix4& transform, const dmVMath::Vector4& clip, const FontLayoutVertexConfig& config, const TextLine& line, const TextGlyph& glyph, const TextGlyphRenderData& render_data, FontSpatialGradientCache* spatial_cache);
bool FontViewerVectorFontUpload(FontViewerVectorFont* font, dmGraphics::HContext context);
void FontViewerVectorFontDraw(FontViewerVectorFont* font, dmGraphics::HTexture effects);

#endif
