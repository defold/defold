#version 330
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
    float coverage;
    if (var_mode > 0.5)
    {
#ifdef SLUG_LEGACY_GL
        vec3 effects = texture2D(effect_bitmap, var_texcoord).rgb;
#else
        vec3 effects = texture(effect_bitmap, var_texcoord).rgb;
#endif
        if (var_mode < 1.5)
        {
            float smoothing = max(0.5 * fwidth(effects.g), 0.0001);
            coverage = smoothstep(var_banding.x - smoothing, var_banding.x + smoothing, effects.g);
        }
        else
            coverage = effects.b;
    }
    else
#ifdef SLUG_LEGACY_GL
        coverage = SlugRender(var_texcoord, var_banding, ivec4(floor(var_glyph + 0.5)));
#else
        coverage = SlugRender(var_texcoord, var_banding, var_glyph);
#endif
    out_fragColor = coverage * vec4(var_color.rgb * var_color.a, var_color.a);
    if ((var_mode < 0.5 && var_curve_count <= 0.0) || out_fragColor.a <= 0.0) discard;
#ifdef FONT_VECTOR_PICKING
    out_fragColor = id;
#endif
}
