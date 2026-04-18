
layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(rgba32f) uniform image2D texture_out;

#define MAX_LIGHTS 32

struct Light
{
    vec4 position;        // xyz: position, w:unused
    vec4 color;           // RGBA (matches LightParams order)
    vec4 direction_range; // xyz: normalized direction; w: range
    vec4 params;          // x: type (0 dir, 1 point, 2 spot; matches dmRender::LightType)
};
uniform LightBuffer
{
    vec4  lights_count; // x: number of active lights
    Light lights[MAX_LIGHTS];
};

void main()
{
    vec4 light_accum = vec4(0.0);
    for (int i = 0; i < int(lights_count.x); i++)
    {
        light_accum += lights[i].color;
    }
    ivec2 tex_coord = ivec2(gl_GlobalInvocationID.xy);
    imageStore(texture_out, tex_coord, light_accum);
}
