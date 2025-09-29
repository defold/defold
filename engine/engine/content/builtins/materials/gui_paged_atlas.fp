#version 140

in mediump vec2 var_texcoord0;
in mediump vec4 var_color;
in mediump float var_page_index;

out vec4 out_fragColor;

uniform mediump sampler2DArray texture_sampler;

void main()
{
    out_fragColor = texture(texture_sampler, vec3(var_texcoord0.xy, var_page_index)) * var_color;
}
