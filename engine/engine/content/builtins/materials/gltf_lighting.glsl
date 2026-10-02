#ifndef DEFOLD_PBR_LIGHTING
#define DEFOLD_PBR_LIGHTING

#include "/builtins/materials/gltf_brdf.glsl"

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

// Optional fallback. IBL/GI can replace this contribution entirely.
PBRLightContribution evaluate_pbr_ambient(MaterialInfo material, vec3 ambient)
{
    PBRLightContribution result;
    result.diffuse = material.diffuseColor * ambient;
    result.specular = material.f0 * ambient * (1.0 - 0.5 * material.perceptualRoughness);
    return result;
}

// Visibility is explicit: callers combine material AO, SSAO and GI visibility.
// Direct lighting has already received per-light shadow visibility.
vec3 composite_pbr_lighting(PBRMaterial material, PBRLighting lighting,
                           float diffuse_visibility, float specular_visibility)
{
    if (material.unlit)
        return material.baseColor.rgb;
    return lighting.direct.diffuse + lighting.direct.specular
         + lighting.indirect.diffuse * diffuse_visibility
         + lighting.indirect.specular * specular_visibility + material.emissive;
}

#endif
