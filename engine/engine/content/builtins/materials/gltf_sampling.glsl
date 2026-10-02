#ifndef DEFOLD_PBR_SAMPLING
#define DEFOLD_PBR_SAMPLING

#include "/builtins/materials/gltf_inputs.glsl"
#include "/builtins/materials/gltf_material.glsl"

uniform sampler2D PbrMaterial_normalTexture;
uniform sampler2D PbrMaterial_occlusionTexture;
uniform sampler2D PbrMaterial_emissiveTexture;
uniform sampler2D PbrMetallicRoughness_baseColorTexture;
uniform sampler2D PbrMetallicRoughness_metallicRoughnessTexture;

struct PbrMetallicRoughness
{
    vec4 baseColorFactor;
    vec4 metallicAndRoughnessFactor;
    vec4 metallicRoughnessTextures;
};

uniform PbrMaterial
{
    // cutoff, double-sided, unlit, alpha mode (OPAQUE=0, MASK=1, BLEND=2)
    vec4 pbrAlphaCutoffAndDoubleSidedAndIsUnlit;
    vec4 pbrCommonTextures;
    // RGB emissive factor, W emissive strength
    vec4 pbrEmissiveFactorAndStrength;
    // X normal scale, Y occlusion strength
    vec4 pbrNormalScaleAndOcclusionStrength;
    PbrMetallicRoughness pbrMetallicRoughness;
};

// Sample once. Subsequent material/lighting functions consume explicit data.
PBRMaterial sample_pbr_material(vec2 uv, vec4 vertex_color)
{
    PBRMaterial material;
    material.baseColor = pbrMetallicRoughness.baseColorFactor * vertex_color;
    material.metallic = pbrMetallicRoughness.metallicAndRoughnessFactor.x;
    material.roughness = pbrMetallicRoughness.metallicAndRoughnessFactor.y;
    material.alphaCutoff = pbrAlphaCutoffAndDoubleSidedAndIsUnlit.x;
    material.doubleSided = pbrAlphaCutoffAndDoubleSidedAndIsUnlit.y > 0.5;
    material.unlit = pbrAlphaCutoffAndDoubleSidedAndIsUnlit.z > 0.5;
    material.alphaMode = int(pbrAlphaCutoffAndDoubleSidedAndIsUnlit.w);
    material.occlusion = 1.0;
    material.emissive = vec3(0.0);
    if (pbrMetallicRoughness.metallicRoughnessTextures.x > 0.5)
        material.baseColor *= to_linear(texture(PbrMetallicRoughness_baseColorTexture, uv));
    if (!material.unlit)
    {
        if (pbrMetallicRoughness.metallicRoughnessTextures.y > 0.5)
        {
            vec4 mr = texture(PbrMetallicRoughness_metallicRoughnessTexture, uv);
            material.metallic *= mr.b;
            material.roughness *= mr.g;
        }
        if (pbrCommonTextures.y > 0.5)
            material.occlusion = mix(1.0, texture(PbrMaterial_occlusionTexture, uv).r,
                                     saturate(pbrNormalScaleAndOcclusionStrength.y));
        material.emissive = pbrEmissiveFactorAndStrength.rgb * pbrEmissiveFactorAndStrength.w;
        if (pbrCommonTextures.z > 0.5)
            material.emissive *= to_linear(texture(PbrMaterial_emissiveTexture, uv)).rgb;
    }
    return material;
}

PBRSurface get_pbr_surface(PBRMaterial material)
{
    PBRSurface surface;
    surface.position = var_position.xyz;
    surface.view = pbr_normalize(-surface.position, vec3(0.0, 0.0, 1.0));
    vec3 normal = pbr_normalize(var_normal, vec3(0.0, 0.0, 1.0));
    float facing = material.doubleSided && !gl_FrontFacing ? -1.0 : 1.0;
    surface.geometricNormal = normal * facing;
    if (!material.unlit && pbrCommonTextures.x > 0.5 && var_has_tangent > 0.5)
    {
        vec3 tangent = pbr_normalize(var_tangent - normal * dot(normal, var_tangent), vec3(0.0));
        float handedness = dot(cross(normal, tangent), var_bitangent) < 0.0 ? -1.0 : 1.0;
        vec3 bitangent = cross(normal, tangent) * handedness;
        vec3 mapped = texture(PbrMaterial_normalTexture, var_texcoord0).xyz * 2.0 - 1.0;
        mapped.xy *= pbrNormalScaleAndOcclusionStrength.x;
        normal = pbr_normalize(mat3(tangent, bitangent, normal) * mapped, normal);
    }
    surface.normal = normal * facing;
    // Defold camera views are rigid transforms.
    mat3 inverse_view_rotation = transpose(mat3(var_view));
    surface.worldPosition = inverse_view_rotation * (surface.position - var_view[3].xyz);
    surface.worldNormal = inverse_view_rotation * surface.normal;
    surface.worldGeometricNormal = inverse_view_rotation * surface.geometricNormal;
    surface.worldView = inverse_view_rotation * surface.view;
    return surface;
}

#endif
