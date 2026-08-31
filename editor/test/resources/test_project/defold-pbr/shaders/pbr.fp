#version 140

in vec2 var_texcoord0;

uniform sampler2D PbrMetallicRoughness_baseColorTexture;

out vec4 color;

void main()
{
    color = texture(PbrMetallicRoughness_baseColorTexture, var_texcoord0);
}
