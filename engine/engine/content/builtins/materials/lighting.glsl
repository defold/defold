#ifndef DEFOLD_LIGHTING_GLSL
#define DEFOLD_LIGHTING_GLSL

#define LIGHT_DIRECTIONAL 0
#define LIGHT_POINT       1
#define LIGHT_SPOT        2

struct Light
{
    vec4 position;
    vec4 color;
    vec4 direction_range;
    vec4 params;
};

#ifdef EDITOR
uniform Light lights[MAX_LIGHTS];
uniform vec4 light_info;
#else
uniform LightBuffer
{
    vec4  light_info;     // xyz: ambient light, w: number of active lights
    Light lights[MAX_LIGHTS];
};
#endif

vec3 world_to_view_point(vec3 p)
{
    return (var_view * vec4(p, 1.0)).xyz;
}

vec3 world_to_view_dir(vec3 d)
{
    return normalize((var_view * vec4(d, 0.0)).xyz);
}

vec3 calculate_light(int index, vec3 normal, vec3 view_position)
{
    Light light = lights[index];
    int   type  = int(light.params.x);

    if (type == LIGHT_DIRECTIONAL)
    {
        vec3 L = -world_to_view_dir(light.direction_range.xyz);
        return light.color.rgb * light.params.y * max(dot(normal, L), 0.0);
    }
    else if (type == LIGHT_POINT)
    {
        vec3  to_light = world_to_view_point(light.position.xyz) - view_position;
        float dist     = length(to_light);
        float atten    = clamp(1.0 - (dist / light.direction_range.w), 0.0, 1.0);
        atten *= atten;
        vec3  L        = normalize(to_light);
        return light.color.rgb * light.params.y * max(dot(normal, L), 0.0) * atten;
    }
    else if (type == LIGHT_SPOT)
    {
        vec3  to_light = world_to_view_point(light.position.xyz) - view_position;
        float dist     = length(to_light);
        float atten    = clamp(1.0 - (dist / light.direction_range.w), 0.0, 1.0);
        atten *= atten;
        vec3  L         = normalize(to_light);
        vec3  spot_dir  = world_to_view_dir(light.direction_range.xyz);
        float inner_cos = cos(0.5 * light.params.z - 0.00001);
        float outer_cos = cos(0.5 * light.params.w);
        float spot_i    = smoothstep(outer_cos, inner_cos, dot(-L, spot_dir));
        return light.color.rgb * light.params.y * max(dot(normal, L), 0.0) * atten * spot_i;
    }

    return vec3(0.0);
}

#endif
