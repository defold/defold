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

#ifdef FONT_VECTOR_PICKING
uniform picking_uniforms
{
    vec4 id;
};
#endif

out vec4 out_fragColor;

void main()
{
    float coverage = var_texcoord.y > 0.0
        ? 1.0 - step(var_texcoord.y, fract(var_texcoord.x)) : 1.0;
    vec4 face_color = font_effect_face_color(var_effect, var_color, false);
    out_fragColor = coverage * vec4(face_color.rgb * face_color.a, face_color.a);
    if (out_fragColor.a <= 0.0) discard;

#ifdef FONT_VECTOR_PICKING
    out_fragColor = id;
#endif
}
