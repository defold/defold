#ifndef DEFOLD_LIGHTING_GLSL
#define DEFOLD_LIGHTING_GLSL

#define LIGHT_DIRECTIONAL 0
#define LIGHT_POINT       1
#define LIGHT_SPOT        2

#ifdef EDITOR
uniform Light
{
    vec4 position;
    vec4 color;
    vec4 direction_range;
    vec4 params;
} lights[MAX_LIGHT_COUNT];

uniform LightBuffer
{
    vec4 light_info;
};
#else
struct Light
{
    vec4 position;
    vec4 color;
    vec4 direction_range;
    vec4 params;
};

uniform LightBuffer
{
    // xyz: ambient light, w: number of active lights
    vec4  light_info;
    Light lights[MAX_LIGHT_COUNT];
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

vec3 ambient_light()
{
     return light_info.xyz;
}

vec3 diffuse_lambert(int index, vec3 normal, vec3 view_position)
{
    int type = int(lights[index].params.x);
    if (type == LIGHT_DIRECTIONAL)
    {
        vec3 L = -world_to_view_dir(lights[index].direction_range.xyz);
        return lights[index].color.rgb * lights[index].params.y * max(dot(normal, L), 0.0);
    }
    else if (type == LIGHT_POINT)
    {
        vec3  to_light = world_to_view_point(lights[index].position.xyz) - view_position;
        float dist     = length(to_light);
        float atten    = clamp(1.0 - (dist / lights[index].direction_range.w), 0.0, 1.0);
        atten *= atten;
        vec3  L        = normalize(to_light);
        return lights[index].color.rgb * lights[index].params.y * max(dot(normal, L), 0.0) * atten;
    }
    else if (type == LIGHT_SPOT)
    {
        vec3  to_light = world_to_view_point(lights[index].position.xyz) - view_position;
        float dist     = length(to_light);
        float atten    = clamp(1.0 - (dist / lights[index].direction_range.w), 0.0, 1.0);
        atten *= atten;
        vec3  L         = normalize(to_light);
        vec3  spot_dir  = world_to_view_dir(lights[index].direction_range.xyz);
        float inner_cos = cos(0.5 * lights[index].params.z - 0.00001);
        float outer_cos = cos(0.5 * lights[index].params.w);
        float spot_i    = smoothstep(outer_cos, inner_cos, dot(-L, spot_dir));
        return lights[index].color.rgb * lights[index].params.y * max(dot(normal, L), 0.0) * atten * spot_i;
    }

    return vec3(0.0);
}

vec3 diffuse_lambert(vec3 view_normal, vec3 view_position)
{
    vec3 total_light = vec3(0.0);
    int light_count = int(light_info.w);

    for (int i = 0; i < MAX_LIGHT_COUNT; ++i)
    {
        if (i >= light_count)
        {
            break;
        }
        total_light += diffuse_lambert(i, view_normal, view_position);
    }

    return total_light;
}

#endif
