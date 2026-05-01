#version 430

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(rgba32f) uniform image2D texture_out;

uniform uniforms
{
    vec4 color;
};

void main()
{
    ivec2 tex_coord = ivec2(gl_GlobalInvocationID.xy);
    imageStore(texture_out, tex_coord, color);
}
