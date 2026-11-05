#version 140

#include "/builtins/fonts/font-effect.glsl"

in mediump vec2 var_texcoord0;
in highp vec4 var_effect;
in mediump vec4 var_face_color;
in mediump vec4 var_outline_color;
in highp vec2 var_decoration;

out vec4 out_fragColor;

uniform mediump sampler2D texture_sampler;

float decoration_mask()
{
    return var_decoration.y > 0.0 ? 1.0 - step(var_decoration.y, fract(var_decoration.x)) : 1.0;
}

void main()
{
    vec4 effect_face_color = font_effect_face_color(var_effect, var_face_color, false);
    mediump vec2 t = texture(texture_sampler, var_texcoord0.xy).xy;
    out_fragColor  = (vec4(effect_face_color.xyz, 1.0) * t.x * effect_face_color.w + vec4(var_outline_color.xyz * t.y * var_outline_color.w, t.y * var_outline_color.w) * (1.0 - t.x)) * decoration_mask();
}
