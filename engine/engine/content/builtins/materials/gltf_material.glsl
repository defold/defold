/*
MIT License

Copyright (c) 2024 Defold

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef DEFOLD_PBR_MATERIAL
#define DEFOLD_PBR_MATERIAL

#include "/builtins/materials/gltf_inputs.glsl"
#include "/builtins/materials/gltf_common.glsl"

uniform sampler2D PbrMaterial_normalTexture;
uniform sampler2D PbrMaterial_occlusionTexture;
uniform sampler2D PbrMaterial_emissiveTexture;

uniform sampler2D PbrMetallicRoughness_baseColorTexture;
uniform sampler2D PbrMetallicRoughness_metallicRoughnessTexture;

/*
 * Defold's model pipeline writes glTF metallic-roughness material properties
 * into this uniform block. Boolean texture presence is encoded as numeric
 * flags so projects can share one material across textured and untextured
 * assets.
 */
struct PbrMetallicRoughness
{
    vec4 baseColorFactor;
    vec4 metallicAndRoughnessFactor;
    vec4 metallicRoughnessTextures;
};

uniform PbrMaterial
{
    vec4 pbrAlphaCutoffAndDoubleSidedAndIsUnlit;
    vec4 pbrCommonTextures;
    PbrMetallicRoughness pbrMetallicRoughness;
};

/*
 * Raw material parameters plus per-fragment shading vectors.
 *
 * get_pbr_params() gathers material flags, texture availability, view-space
 * normal, view direction, matching world-space vectors, and double-sided
 * normal correction into one struct. Extension code should prefer reading
 * this struct over sampling globals directly when possible.
 */
struct PBRParams
{
    vec4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    float alphaCutoff;
    bool doubleSided;
    bool unlit;
    bool hasBaseColorTexture;
    bool hasMetallicRoughnessTexture;
    bool hasNormalTexture;
    bool hasOcclusionTexture;
    bool hasEmissiveTexture;
    vec3 normal;
    vec3 view;
    vec3 worldPosition;
    vec3 worldNormal;
    vec3 worldView;
};

/*
 * Derived material properties used by BRDF and lighting code.
 *
 * baseColor is linear. diffuseColor, f0, roughness, and metallic are resolved
 * from factors and textures so lighting extensions can consume physically
 * meaningful values without knowing how the glTF inputs were packed.
 */
struct MaterialInfo
{
    vec4 baseColor;
    vec3 diffuseColor;
    vec3 f0;
    vec3 f90;
    float metallic;
    float perceptualRoughness;
    float alphaRoughness;
    float specularWeight;
};

float valid_or(float value, float fallback)
{
    return value == value ? value : fallback;
}

bool is_valid(float value)
{
    return value == value;
}

bool is_valid(vec4 value)
{
    return is_valid(value.x) && is_valid(value.y) && is_valid(value.z) && is_valid(value.w);
}

vec4 sample_base_color_texture()
{
    return texture(PbrMetallicRoughness_baseColorTexture, var_texcoord0);
}

vec4 sample_metallic_roughness_texture()
{
    return texture(PbrMetallicRoughness_metallicRoughnessTexture, var_texcoord0);
}

vec4 sample_normal_texture()
{
    return texture(PbrMaterial_normalTexture, var_texcoord0);
}

vec4 sample_occlusion_texture()
{
    return texture(PbrMaterial_occlusionTexture, var_texcoord0);
}

vec4 sample_emissive_texture()
{
    return texture(PbrMaterial_emissiveTexture, var_texcoord0);
}

/* Returns a tangent-space normal from the normal texture, or +Z if absent. */
vec3 get_tangent_space_normal(PBRParams params)
{
    if (params.hasNormalTexture)
    {
        return normalize(sample_normal_texture().xyz * 2.0 - 1.0);
    }
    return vec3(0.0, 0.0, 1.0);
}

/* Returns the final view-space normal used for all lighting calculations. */
vec3 get_normal(PBRParams params)
{
    vec3 n = normalize(var_normal);
    if (params.hasNormalTexture && var_has_tangent > 0.5)
    {
        vec3 tangent_normal = get_tangent_space_normal(params);
        n = normalize(mat3(normalize(var_tangent), normalize(var_bitangent), n) * tangent_normal);
    }
    return n;
}

/* Builds the per-fragment parameter bundle consumed by the PBR pipeline. */
PBRParams get_pbr_params()
{
    PBRParams params;
    vec4 base_color_factor = pbrMetallicRoughness.baseColorFactor;
    params.baseColorFactor = is_valid(base_color_factor) && base_color_factor.a > 0.0 ? base_color_factor : vec4(1.0);
    params.metallicFactor = valid_or(pbrMetallicRoughness.metallicAndRoughnessFactor.x, 1.0);
    params.roughnessFactor = valid_or(pbrMetallicRoughness.metallicAndRoughnessFactor.y, 1.0);
    params.alphaCutoff = max(valid_or(pbrAlphaCutoffAndDoubleSidedAndIsUnlit.x, 0.0), 0.0);
    params.doubleSided = pbrAlphaCutoffAndDoubleSidedAndIsUnlit.y > 0.5;
    params.unlit = pbrAlphaCutoffAndDoubleSidedAndIsUnlit.z > 0.5;
    params.hasBaseColorTexture = pbrMetallicRoughness.metallicRoughnessTextures.x > 0.5;
    params.hasMetallicRoughnessTexture = pbrMetallicRoughness.metallicRoughnessTextures.y > 0.5;
    params.hasNormalTexture = pbrCommonTextures.x > 0.5;
    params.hasOcclusionTexture = pbrCommonTextures.y > 0.5;
    params.hasEmissiveTexture = pbrCommonTextures.z > 0.5;
    params.view = normalize(-var_position.xyz);
    params.normal = get_normal(params);

    if (params.doubleSided && dot(params.normal, params.view) < 0.0)
    {
        params.normal = -params.normal;
    }

    mat3 view_rotation_inverse = transpose(mat3(var_view));
    vec3 view_translation = var_view[3].xyz;
    params.worldPosition = view_rotation_inverse * (var_position.xyz - view_translation);
    params.worldNormal = normalize(view_rotation_inverse * params.normal);
    params.worldView = normalize(view_rotation_inverse * params.view);

    return params;
}

/* Resolves linear base color from factor, texture, and vertex color. */
vec4 get_base_color(PBRParams params)
{
    vec4 base_color = params.baseColorFactor;
    if (params.hasBaseColorTexture)
    {
        base_color *= to_linear(sample_base_color_texture());
    }
    return base_color * var_color;
}

/* Resolves the BRDF-ready material values from PBRParams and textures. */
MaterialInfo get_material_info(PBRParams params)
{
    MaterialInfo material;
    material.baseColor = get_base_color(params);
    material.metallic = clamp(params.metallicFactor, 0.0, 1.0);
    material.perceptualRoughness = clamp(params.roughnessFactor, 0.04, 1.0);

    if (params.hasMetallicRoughnessTexture)
    {
        vec4 metallic_roughness = sample_metallic_roughness_texture();
        material.perceptualRoughness *= metallic_roughness.g;
        material.metallic *= metallic_roughness.b;
    }

    material.perceptualRoughness = clamp(material.perceptualRoughness, 0.04, 1.0);
    material.alphaRoughness = material.perceptualRoughness * material.perceptualRoughness;
    material.f0 = mix(vec3(0.04), material.baseColor.rgb, material.metallic);
    material.f90 = vec3(1.0);
    material.diffuseColor = material.baseColor.rgb * (1.0 - material.metallic);
    material.specularWeight = 1.0;
    return material;
}

/* Returns scalar ambient occlusion. A missing occlusion texture is neutral. */
float get_occlusion(PBRParams params)
{
    if (params.hasOcclusionTexture)
    {
        return sample_occlusion_texture().r;
    }
    return 1.0;
}

/* Returns linear emissive color. A missing emissive texture is black. */
vec3 get_emissive(PBRParams params)
{
    if (params.hasEmissiveTexture)
    {
        return to_linear(sample_emissive_texture()).rgb;
    }
    return vec3(0.0);
}

#endif
