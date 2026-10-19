#version 450

#extension GL_EXT_samplerless_texture_functions : require
layout(set = 1, binding = 3) uniform texture2D curve_texture;
layout(set = 1, binding = 4) uniform sampler curve_texture_separated;
layout(set = 1, binding = 7) uniform utexture2D band_texture;
layout(set = 1, binding = 9) uniform sampler2D combined_texture;
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
void main()
{
    color = texture(sampler2D(curve_texture, curve_texture_separated), uv)
          + vec4(texelFetch(band_texture, ivec2(0), 0))
          + texture(combined_texture, uv);
}
