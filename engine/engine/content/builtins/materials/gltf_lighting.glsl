#ifndef DEFOLD_PBR_LIGHTING
#define DEFOLD_PBR_LIGHTING

#include "/builtins/materials/gltf_brdf.glsl"
#include "/builtins/materials/lighting.glsl"

PBRLightContribution empty_pbr_contribution()
{
    PBRLightContribution result;
    result.diffuse = vec3(0.0);
    result.specular = vec3(0.0);
    return result;
}

PBRLighting empty_pbr_lighting()
{
    PBRLighting result;
    result.direct = empty_pbr_contribution();
    result.indirect = empty_pbr_contribution();
    return result;
}

void add_pbr_contribution(inout PBRLightContribution total, PBRLightContribution value)
{
    total.diffuse += value.diffuse;
    total.specular += value.specular;
}

PBRLightContribution evaluate_pbr_light(MaterialInfo material, PBRSurface surface, PBRLightSample light)
{
    PBRLightContribution result;
    evaluate_brdf(material, surface.normal, surface.view, light.direction, light.radiance,
                  result.diffuse, result.specular);
    return result;
}

// Convenience path; extensions can write the same loop to inject per-light visibility.
PBRLightContribution evaluate_pbr_direct(MaterialInfo material, PBRSurface surface, mat4 view_matrix)
{
    PBRLightContribution total = empty_pbr_contribution();
    int count = light_count();

    for (int i = 0; i < MAX_LIGHT_COUNT; ++i)
    {
        if (i >= count)
        {
            break;
        }

        PBRLightSample light;
        sample_light(i, surface.position, view_matrix, light.direction, light.radiance);
        add_pbr_contribution(total, evaluate_pbr_light(material, surface, light));
    }

    return total;
}

// Simple ambient fallback rather than a sampled environment. Roughness reduces
// its approximate specular contribution; IBL/GI can replace this function's result.
PBRLightContribution evaluate_pbr_ambient(MaterialInfo material, vec3 ambient)
{
    PBRLightContribution result;
    result.diffuse = material.diffuseColor * ambient;
    result.specular = material.f0 * ambient * (1.0 - 0.5 * material.perceptualRoughness);
    return result;
}

// Material AO, SSAO and GI visibility attenuate indirect lighting only.
// Any per-light shadow visibility must already be applied to direct lighting.
vec3 composite_pbr_lighting(PBRMaterial material, PBRLighting lighting,
                           float diffuse_visibility, float specular_visibility)
{
    if (material.unlit)
    {
        return material.baseColor.rgb;
    }

    vec3 color = lighting.direct.diffuse + lighting.direct.specular;
    color += lighting.indirect.diffuse * diffuse_visibility;
    color += lighting.indirect.specular * specular_visibility;
    color += material.emissive;
    return color;
}

#endif
