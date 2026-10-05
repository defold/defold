#ifndef DEFOLD_PBR_MATERIAL
#define DEFOLD_PBR_MATERIAL

#include "/builtins/materials/gltf_types.glsl"
#include "/builtins/materials/gltf_common.glsl"

// Pure derivation: callers may edit resolved inputs before this step.
MaterialInfo get_material_info(PBRMaterial input_material)
{
    MaterialInfo material;
    material.baseColor = input_material.baseColor;
    material.metallic = saturate(input_material.metallic);

    // Keep the specular lobe finite, then square the artist-facing roughness
    // to obtain the alpha parameter used by the GGX functions.
    material.perceptualRoughness = clamp(input_material.roughness, 0.04, 1.0);
    material.alphaRoughness = material.perceptualRoughness * material.perceptualRoughness;

    // Nonmetals reflect 4% at normal incidence. Metals use base color as their
    // specular reflectance and have no diffuse contribution.
    material.dielectricF0 = vec3(0.04);
    material.f0 = mix(material.dielectricF0, material.baseColor.rgb, material.metallic);
    material.f90 = vec3(1.0);
    material.diffuseColor = material.baseColor.rgb * (1.0 - material.metallic);
    material.specularWeight = 1.0;

    return material;
}

// BLEND outputs straight alpha; OPAQUE and surviving MASK fragments are opaque.
// The render pass owns blend factors and depth writes.
float get_pbr_alpha(PBRMaterial material)
{
    return material.alphaMode == PBR_ALPHA_BLEND ? saturate(material.baseColor.a) : 1.0;
}

bool pbr_alpha_discard(PBRMaterial material)
{
    return material.alphaMode == PBR_ALPHA_MASK && material.baseColor.a < material.alphaCutoff;
}

#endif
