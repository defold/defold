#ifndef DEFOLD_PBR_LIGHTS
#define DEFOLD_PBR_LIGHTS

#include "/builtins/materials/gltf_lighting.glsl"

#ifndef MAX_LIGHT_COUNT
#define MAX_LIGHT_COUNT 8
#endif

// Matches Defold's LightBuffer ABI. Kept independent of the legacy lit shader.
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
    vec4 light_info;
    Light lights[MAX_LIGHT_COUNT];
};
#endif

int get_pbr_light_count()
{
    return clamp(int(light_info.w), 0, MAX_LIGHT_COUNT);
}

vec3 get_pbr_ambient()
{
    return light_info.xyz;
}

// index is retained by the caller for extension-owned shadow/probe metadata.
PBRLightSample sample_pbr_light(int index, vec3 view_position, mat4 view_matrix)
{
    PBRLightSample sample;
    sample.direction = vec3(0.0);
    sample.radiance = vec3(0.0);
    int type = int(lights[index].params.x);
    if (type < 0 || type > 2)
        return sample;
    float attenuation = 1.0;
    if (type == 0)
    {
        sample.direction = -pbr_normalize(mat3(view_matrix) * lights[index].direction_range.xyz, vec3(0.0));
    }
    else
    {
        vec3 to_light = (view_matrix * vec4(lights[index].position.xyz, 1.0)).xyz - view_position;
        float distance_to_light = length(to_light);
        sample.direction = pbr_normalize(to_light, vec3(0.0));
        float range = max(lights[index].direction_range.w, PBR_EPSILON);
        attenuation = saturate(1.0 - distance_to_light / range);
        attenuation *= attenuation;
        if (type == 2)
        {
            vec3 direction = pbr_normalize(mat3(view_matrix) * lights[index].direction_range.xyz, vec3(0.0));
            float outer_cos = cos(0.5 * lights[index].params.w);
            float inner_cos = max(cos(0.5 * lights[index].params.z), outer_cos + PBR_EPSILON);
            attenuation *= smoothstep(outer_cos, inner_cos, dot(-sample.direction, direction));
        }
    }
    sample.radiance = lights[index].color.rgb * lights[index].params.y * attenuation;
    return sample;
}

// Convenience path; extensions can write the same loop to inject per-light visibility.
PBRLightContribution evaluate_pbr_direct(MaterialInfo material, PBRSurface surface, mat4 view_matrix)
{
    PBRLightContribution total = empty_pbr_contribution();
    int count = get_pbr_light_count();
    for (int i = 0; i < MAX_LIGHT_COUNT; ++i)
    {
        if (i >= count)
            break;
        add_pbr_contribution(total, evaluate_pbr_light(material, surface,
                             sample_pbr_light(i, surface.position, view_matrix)));
    }
    return total;
}

#endif
