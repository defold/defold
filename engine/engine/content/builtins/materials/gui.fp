#version 140

in mediump vec2 var_texcoord0;
in mediump vec4 var_color;

out vec4 out_fragColor;

uniform mediump sampler2D texture_sampler;

void main()
{
    out_fragColor = texture(texture_sampler, var_texcoord0.xy) * var_color;
}
