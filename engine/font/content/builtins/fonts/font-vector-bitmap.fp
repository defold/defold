#version 140

#extension GL_EXT_samplerless_texture_functions : require

precision highp float;
precision highp int;

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
    // Match the combined shader's masked samples before taking derivatives.
    vec3 effects = vec3(0.0);
    if (var_mode > 0.5 && var_mode < 2.5)
    {
        effects = textureLod(effect_bitmap, var_texcoord, 0.0).rgb;
    }
    float smoothing = max(0.5 * fwidth(effects.g), 0.0001);
    float coverage = var_mode < 1.5
        ? smoothstep(var_banding.x - smoothing, var_banding.x + smoothing, effects.g)
        : effects.b;
    vec4 face_color = font_effect_face_color(var_effect, var_color, false);
    out_fragColor = coverage * vec4(face_color.rgb * face_color.a, face_color.a);
    if (out_fragColor.a <= 0.0) discard;

#ifdef FONT_VECTOR_PICKING
    out_fragColor = id;
#endif
}
