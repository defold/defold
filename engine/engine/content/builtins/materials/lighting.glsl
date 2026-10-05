#ifndef DEFOLD_LIGHTING_GLSL
#define DEFOLD_LIGHTING_GLSL

#ifndef MAX_LIGHT_COUNT
#define MAX_LIGHT_COUNT 8
#endif

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

// Shared bindings and light sampling; no material model or fragment inputs.
const float LIGHT_EPSILON = 0.00001;

vec3 light_normalize(vec3 direction)
{
    float length_squared = dot(direction, direction);
    return length_squared > LIGHT_EPSILON * LIGHT_EPSILON ? direction * inversesqrt(length_squared) : vec3(0.0);
}

vec3 world_to_view_point(vec3 p, mat4 view_matrix)
{
    return (view_matrix * vec4(p, 1.0)).xyz;
}

vec3 world_to_view_dir(vec3 d, mat4 view_matrix)
{
    return light_normalize(mat3(view_matrix) * d);
}

int light_count()
{
    return clamp(int(light_info.w), 0, MAX_LIGHT_COUNT);
}

vec3 ambient_light()
{
    return light_info.xyz;
}

// Produces a view-space surface-to-light direction and attenuated radiance.
// Keep the index available to callers for per-light shadow/probe metadata.
void sample_light(int index, vec3 view_position, mat4 view_matrix,
                  out vec3 direction, out vec3 radiance)
{
    direction = vec3(0.0);
    radiance = vec3(0.0);
    int type = int(lights[index].params.x);
    float attenuation = 1.0;
    if (type == LIGHT_DIRECTIONAL)
    {
        direction = -world_to_view_dir(lights[index].direction_range.xyz, view_matrix);
    }
    else if (type == LIGHT_POINT || type == LIGHT_SPOT)
    {
        vec3 to_light = world_to_view_point(lights[index].position.xyz, view_matrix) - view_position;
        float distance_to_light = length(to_light);
        direction = light_normalize(to_light);
        float range = max(lights[index].direction_range.w, LIGHT_EPSILON);
        attenuation = clamp(1.0 - distance_to_light / range, 0.0, 1.0);
        attenuation *= attenuation;
        if (type == LIGHT_SPOT)
        {
            vec3 spot_direction = world_to_view_dir(lights[index].direction_range.xyz, view_matrix);
            float outer_cos = cos(0.5 * lights[index].params.w);
            float inner_cos = max(cos(0.5 * lights[index].params.z), outer_cos + LIGHT_EPSILON);
            attenuation *= smoothstep(outer_cos, inner_cos, dot(-direction, spot_direction));
        }
    }
    else
    {
        return;
    }
    radiance = lights[index].color.rgb * lights[index].params.y * attenuation;
}

vec3 diffuse_lambert(int index, vec3 normal, vec3 view_position, mat4 view_matrix)
{
    vec3 direction;
    vec3 radiance;
    sample_light(index, view_position, view_matrix, direction, radiance);
    return radiance * max(dot(normal, direction), 0.0);
}

vec3 diffuse_lambert(vec3 view_normal, vec3 view_position, mat4 view_matrix)
{
    vec3 total_light = vec3(0.0);
    int count = light_count();

    for (int i = 0; i < MAX_LIGHT_COUNT; ++i)
    {
        if (i >= count)
        {
            break;
        }
        total_light += diffuse_lambert(i, view_normal, view_position, view_matrix);
    }

    return total_light;
}

#endif
