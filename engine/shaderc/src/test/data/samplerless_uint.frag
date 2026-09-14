#version 450
#extension GL_EXT_samplerless_texture_functions : require
uniform utexture2D band_texture;
out vec4 color;
void main()
{
    color = vec4(texelFetch(band_texture, ivec2(0), 0));
}
