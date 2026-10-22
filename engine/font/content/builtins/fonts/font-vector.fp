#version 140

#extension GL_EXT_samplerless_texture_functions : require

precision highp float;
precision highp int;

#include "/builtins/fonts/font-vector-slug.glsl"
#include "/builtins/fonts/font-vector-paint.glsl"
#include "/builtins/fonts/font-effect.glsl"

in highp vec2 var_font_position;
in highp vec2 var_texcoord;
in mediump vec4 var_color;
in highp vec4 var_effect;
flat in highp float var_paint;
flat in highp float var_mode;
flat in highp vec4 var_banding;
flat in highp ivec4 var_glyph;
flat in highp float var_curve_count;

uniform highp sampler2D effect_bitmap;

#ifdef FONT_VECTOR_PICKING
uniform picking_uniforms
{
    vec4 id;
};
#endif

out vec4 out_fragColor;

void main()
{
    vec2 emsPerPixel = fwidth(var_texcoord);
    vec2 font_dx = dFdx(var_font_position);
    vec2 font_dy = dFdy(var_font_position);
    vec3 effects = vec3(0.0);
    if (var_mode > 0.5 && var_mode < 2.5)
    {
        // The effect atlas has no mipmaps. Explicit LOD avoids implicit
        // derivatives inside this non-uniform branch on WebGPU.
        effects = textureLod(effect_bitmap, var_texcoord, 0.0).rgb;
    }
    // WGSL requires derivative operations outside non-uniform control flow.
    float smoothing = max(0.5 * fwidth(effects.g), 0.0001);
    float coverage;
    if (var_mode > 2.5 && var_mode < 3.5)
    {
        // Decorations use the shared layout's solid/dashed quad geometry.
        coverage = var_texcoord.y > 0.0 ? 1.0 - step(var_texcoord.y, fract(var_texcoord.x)) : 1.0;
    }
    else if (var_mode > 0.5 && var_mode < 2.5)
    {
        if (var_mode < 1.5)
        {
            coverage = smoothstep(var_banding.x - smoothing, var_banding.x + smoothing, effects.g);
        }
        else
        {
            coverage = effects.b;
        }
    }
    else if (var_mode < 0.5)
    {
        coverage = SlugRender(var_texcoord, emsPerPixel, var_banding, var_glyph);
    }
    else
        coverage = 0.0;

    vec4 face_color = font_effect_face_color(var_effect, var_color, false);
    out_fragColor = coverage * vec4(face_color.rgb * face_color.a, face_color.a);
    if (var_mode > 3.5)
    {
        out_fragColor = FontVectorPaintGlyphColor(int(var_paint + 0.5), var_font_position, font_dx, font_dy, face_color);
    }
    if ((var_mode < 0.5 && var_curve_count <= 0.0) || out_fragColor.a <= 0.0)
    {
        discard;
    }

#ifdef FONT_VECTOR_PICKING
    out_fragColor = id;
#endif
}
