#version 330

// Slug's root eligibility uses floatBitsToUint, a GLSL 330 builtin.
#extension GL_EXT_samplerless_texture_functions : require

precision highp float;
precision highp int;

#include "/builtins/fonts/font-vector-slug.glsl"

in highp vec2 var_texcoord;
in mediump vec4 var_color;
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
    if (var_mode > 2.5)
    {
        // Decorations use the shared layout's solid/dashed quad geometry.
        coverage = var_texcoord.y > 0.0 ? 1.0 - step(var_texcoord.y, fract(var_texcoord.x)) : 1.0;
    }
    else if (var_mode > 0.5)
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
    else
    {
        coverage = SlugRender(var_texcoord, emsPerPixel, var_banding, var_glyph);
    }

    out_fragColor = coverage * vec4(var_color.rgb * var_color.a, var_color.a);
    if ((var_mode < 0.5 && var_curve_count <= 0.0) || out_fragColor.a <= 0.0)
    {
        discard;
    }

#ifdef FONT_VECTOR_PICKING
    out_fragColor = id;
#endif
}
