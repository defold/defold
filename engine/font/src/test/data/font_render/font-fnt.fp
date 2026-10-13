#version 140

in mediump vec2 var_texcoord0;
in mediump vec4 var_face_color;
in highp vec2 var_decoration;

out vec4 out_fragColor;

uniform mediump sampler2D texture_sampler;

float decoration_mask()
{
    return var_decoration.y > 0.0 ? 1.0 - step(var_decoration.y, fract(var_decoration.x)) : 1.0;
}

void main()
{
    out_fragColor = texture(texture_sampler, var_texcoord0.xy) * var_face_color * var_face_color.a * decoration_mask();
}
