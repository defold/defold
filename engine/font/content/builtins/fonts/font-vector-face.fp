#version 140

#extension GL_EXT_samplerless_texture_functions : require

precision highp float;
precision highp int;

#include "/builtins/fonts/font-vector-slug.glsl"
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
    float coverage = SlugRenderWithBounds(var_texcoord, fwidth(var_texcoord), var_banding, var_glyph);
    if (var_curve_count <= 0.0) discard;
    vec4 face_color = font_effect_face_color(var_effect, var_color, false);
    out_fragColor = coverage * vec4(face_color.rgb * face_color.a, face_color.a);
    if (out_fragColor.a <= 0.0) discard;

#ifdef FONT_VECTOR_PICKING
    out_fragColor = id;
#endif
}
