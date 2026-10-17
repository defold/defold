#version 330

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

// The production Slug coverage shader is inserted here by fontviewer.
#include "font-vector-slug.glsl"
#include "font-vector-paint.glsl"
flat in int var_paint;
in vec4 var_color;
flat in vec4 var_clip;
in vec2 var_font_position;
in vec2 var_position;
out vec4 out_fragColor;

void main()
{
    vec2 dx = dFdx(var_font_position);
    vec2 dy = dFdy(var_font_position);
    if (any(lessThan(var_position, var_clip.xy)) || any(greaterThan(var_position, var_clip.zw)))
        discard;
    out_fragColor = FontVectorPaintGlyphColor(var_paint, var_font_position, dx, dy, var_color);
}
