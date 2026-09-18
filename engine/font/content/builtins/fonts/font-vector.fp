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

#ifndef SLUG_LEGACY_GL
#extension GL_EXT_samplerless_texture_functions : require
precision highp float;
precision highp int;
#endif
#include "/builtins/fonts/font-vector-slug.glsl"
uniform highp sampler2D effect_bitmap;
#ifdef SLUG_LEGACY_GL
varying vec2 var_texcoord;
varying float var_mode;
varying vec4 var_color;
varying vec4 var_banding;
varying vec4 var_glyph;
varying float var_curve_count;
#define out_fragColor gl_FragColor
#else
in highp vec2 var_texcoord;
flat in highp float var_mode;
in mediump vec4 var_color;
flat in highp vec4 var_banding;
flat in highp ivec4 var_glyph;
flat in highp float var_curve_count;
out vec4 out_fragColor;
#endif
#ifdef FONT_VECTOR_PICKING
uniform vec4 id;
#endif
void main()
{
    vec2 emsPerPixel = fwidth(var_texcoord);
    vec3 effects = vec3(0.0);
    if (var_mode > 0.5)
    {
#ifdef SLUG_LEGACY_GL
        effects = texture2D(effect_bitmap, var_texcoord).rgb;
#else
        // The effect atlas has no mipmaps. Explicit LOD avoids implicit
        // derivatives inside this non-uniform branch on WebGPU.
        effects = textureLod(effect_bitmap, var_texcoord, 0.0).rgb;
#endif
    }
    // WGSL requires derivative operations outside non-uniform control flow.
    float smoothing = max(0.5 * fwidth(effects.g), 0.0001);
    float coverage;
    if (var_mode > 0.5)
    {
        if (var_mode < 1.5)
        {
            coverage = smoothstep(var_banding.x - smoothing, var_banding.x + smoothing, effects.g);
        }
        else
            coverage = effects.b;
    }
    else
#ifdef SLUG_LEGACY_GL
        coverage = SlugRender(var_texcoord, emsPerPixel, var_banding, ivec4(floor(var_glyph + 0.5)));
#else
        coverage = SlugRender(var_texcoord, emsPerPixel, var_banding, var_glyph);
#endif
    out_fragColor = coverage * vec4(var_color.rgb * var_color.a, var_color.a);
    if ((var_mode < 0.5 && var_curve_count <= 0.0) || out_fragColor.a <= 0.0) discard;
#ifdef FONT_VECTOR_PICKING
    out_fragColor = id;
#endif
}
