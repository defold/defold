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

#ifndef DM_FONT_TTF_H
#define DM_FONT_TTF_H

#include <stdint.h>

struct Font;
typedef Font* HFont;

HFont FontLoadFromMemoryTTF(const char* name, const void* data, uint32_t data_size, bool allocate);

bool FontGetGlyphBoxTTF(HFont font, uint32_t glyph_index, int32_t* x0, int32_t* y0, int32_t* x1, int32_t* y1);

#endif // DM_FONT_TTF_H
