#version 140

in mediump vec2 var_texcoord0;

out vec4 out_fragColor;

uniform fs_uniforms
{
    mediump vec4 tint;
};

void main()
{
    out_fragColor = vec4(var_texcoord0.xy, tint.xy);
}
