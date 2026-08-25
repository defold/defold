#version 140

#define MAX_LIGHTS 4

struct Light
{
    vec4 position;
    vec4 color;
    vec4 direction_range;
    vec4 params;
};
uniform LightBuffer
{
    vec4  light_info;
    Light lights[MAX_LIGHTS];
};

out vec4 out_fragColor;

void main()
{
    int light_count = int(light_info.w);
    vec4 light_accum = vec4(light_info.xyz, 0.0);
    if (0 < light_count)
        light_accum += lights[0].color;
    if (1 < light_count)
        light_accum += lights[1].color;
    if (2 < light_count)
        light_accum += lights[2].color;
    if (3 < light_count)
        light_accum += lights[3].color;
    out_fragColor = light_accum;
}
