#version 140

#define MAX_LIGHTS 32

struct LightData
{
    vec4 position;        // xyz: position, w:unused
    vec4 color;           // RGBA (matches LightParams order)
    vec4 direction_range; // xyz: normalized direction; w: range
    vec4 params;          // x: type (0 dir, 1 point, 2 spot; matches dmRender::LightType)
                          // y: intensity
                          // z: innerConeAngle (radians, spot only)
                          // w: outerConeAngle (radians, spot only)
};

uniform Light
{
    LightData lights[MAX_LIGHTS];
};

uniform fs_uniforms
{
    vec4 lights_count;
};

out vec4 out_fragColor;

void main()
{
    vec4 light_accum = vec4(0.0);
    for (int i=0; i < int(lights_count.x); i++)
    {
        light_accum += lights[i].color;
    }
    out_fragColor = light_accum;
}
