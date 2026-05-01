#version 430

out vec4 color;

uniform data_types
{
    int   type_int;
    uint  type_uint;
    float type_float;
    vec2  type_vec2;
    vec3  type_vec3;
    vec4  type_vec4;
    mat2  type_mat2;
    mat3  type_mat3;
    mat4  type_mat4;
    uvec2 type_uvec2;
    uvec3 type_uvec3;
    uvec4 type_uvec4;
};

uniform sampler2D                type_sampler2D;
uniform sampler3D                type_sampler3D;
uniform samplerCube              type_samplerCube;
uniform sampler2DArray           type_sampler2DArray;
uniform texture2D                type_texture2D;
uniform texture3D                type_texture3D;
uniform texture2DArray           type_texture2DArray;
uniform textureCube              type_textureCube;
uniform utexture2D               type_utexture2D;
uniform layout(rgba8ui) uimage2D type_uimage2D;
uniform layout(rgba32f) image2D  type_image2D;
uniform sampler                  type_sampler;

void main()
{
    color = type_vec4;
    color += texture(type_sampler2D, vec2(0.0, 0.0));
    color += texture(type_sampler3D, vec3(0.0, 0.0, 0.0));
    color += texture(type_samplerCube, vec3(0.0, 0.0, 0.0));
    color += texture(type_sampler2DArray, vec3(0.0, 0.0, 0.0));
    color += texture(sampler2D(type_texture2D, type_sampler), vec2(0.0, 0.0));
    color += texture(sampler3D(type_texture3D, type_sampler), vec3(0.0, 0.0, 0.0));
    color += texture(sampler2DArray(type_texture2DArray, type_sampler), vec3(0.0, 0.0, 0.0));
    color += texture(samplerCube(type_textureCube, type_sampler), vec3(0.0, 0.0, 0.0));
    color += texture(usampler2D(type_utexture2D, type_sampler), vec2(0.0, 0.0));
    color += imageLoad(type_uimage2D, ivec2(0.0, 0.0));
    color += imageLoad(type_image2D, ivec2(0.0, 0.0));
}
