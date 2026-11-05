#version 140

#include "/builtins/fonts/font-effect.glsl"

in mediump vec2 var_texcoord0;
in highp vec4 var_effect;
in mediump vec4 var_face_color;
in mediump vec4 var_outline_color;
in mediump vec4 var_sdf_params;
in highp vec2 var_decoration;

out vec4 out_fragColor;

uniform mediump sampler2D texture_sampler;

float decoration_mask()
{
    return var_decoration.y > 0.0 ? 1.0 - step(var_decoration.y, fract(var_decoration.x)) : 1.0;
}

void main()
{
    vec4 effect_face_color = font_effect_face_color(var_effect, var_face_color, true);
    mediump float distance = texture(texture_sampler, var_texcoord0).x;

    mediump float sdf_edge = var_sdf_params.x;
    mediump float sdf_outline = var_sdf_params.y;
    mediump float sdf_smoothing = var_sdf_params.z;

    mediump float alpha = smoothstep(sdf_edge - sdf_smoothing, sdf_edge + sdf_smoothing, distance);
    mediump float outline_alpha = smoothstep(sdf_outline - sdf_smoothing, sdf_outline + sdf_smoothing, distance);
    mediump vec4 color = mix(var_outline_color, effect_face_color, alpha);

    out_fragColor = color * outline_alpha * decoration_mask();
}
