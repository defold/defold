#version 140

in mediump vec2 var_texcoord0;
in mediump vec4 var_face_color;

uniform mediump sampler2D texture_sampler;

out vec4 out_fragColor;

void main()
{
    out_fragColor = var_face_color * texture(texture_sampler, var_texcoord0).r;
}
