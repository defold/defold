layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(rgba32f) uniform image2D texture_out;

struct Light
{
    vec4 position;
    vec4 color;
    vec4 direction_range;
    vec4 params;
};

layout(std430) readonly buffer LightBuffer
{
    vec4  light_info;
    Light lights[];
};

void main()
{
    vec4 light_accum = vec4(light_info.xyz, 0.0);
    for (int i = 0; i < int(light_info.w); i++)
    {
        light_accum += lights[i].color;
    }
    imageStore(texture_out, ivec2(gl_GlobalInvocationID.xy), light_accum);
}
