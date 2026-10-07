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
    // X metallic factor, Y roughness factor
    vec4 metallicAndRoughnessFactor;
    // Texture-presence flags: X base color, Y metallic-roughness
    vec4 metallicRoughnessTextures;
};

uniform PbrMaterial
{
    // X alpha cutoff, Y double-sided, Z unlit, W alpha mode (PBR_ALPHA_*)
    vec4 pbrAlphaCutoffAndDoubleSidedAndIsUnlit;
    // Texture-presence flags: X normal, Y occlusion, Z emissive
    vec4 pbrCommonTextures;
    // RGB emissive factor, W emissive strength
    vec4 pbrEmissiveFactorAndStrength;
    // X normal scale, Y occlusion strength
    vec4 pbrNormalScaleAndOcclusionStrength;
    PbrMetallicRoughness pbrMetallicRoughness;
};

// TODO: Base-color and emissive textures currently use ordinary texture formats,
// so texture() filters sRGB-encoded texels before pbr_srgb_to_linear() decodes the result.
// Correct color filtering decodes each texel first; our ordering can darken
// transitions. The current 8-bit mipmap path also averages encoded values, which
// cannot be corrected by decoding the resulting mip texels in this shader.
// Fixing both requires sRGB-aware mipmap generation and texture sampling. Remove
// the manual decoding below when those samplers return linear RGB, to avoid
// decoding twice. Alpha and normal/metallic/roughness/occlusion data stay linear.
//
// Sample once. Subsequent material/lighting functions consume explicit data.
PBRMaterial pbr_sample_material(vec2 uv, vec4 vertex_color)
{
    PBRMaterial material;
    material.baseColor = pbrMetallicRoughness.baseColorFactor * vertex_color;
    material.metallic = pbrMetallicRoughness.metallicAndRoughnessFactor.x;
    material.roughness = pbrMetallicRoughness.metallicAndRoughnessFactor.y;

    material.alphaCutoff = pbrAlphaCutoffAndDoubleSidedAndIsUnlit.x;
    material.doubleSided = pbrAlphaCutoffAndDoubleSidedAndIsUnlit.y > 0.5;
    material.unlit = pbrAlphaCutoffAndDoubleSidedAndIsUnlit.z > 0.5;
    material.alphaMode = int(pbrAlphaCutoffAndDoubleSidedAndIsUnlit.w);

    // Missing textures leave the material factors unchanged. Unlit materials
    // use only base color and alpha, so lighting-only textures are not sampled.
    // To turn an unlit input into a lit material, also supply the skipped values
    // procedurally or use a custom sampler; changing unlit alone cannot recover them.
    material.occlusion = 1.0;
    material.emissive = vec3(0.0);

    bool has_base_color_texture = pbrMetallicRoughness.metallicRoughnessTextures.x > 0.5;
    if (has_base_color_texture)
    {
        // Color textures are sRGB; factors and vertex colors are already linear.
        // pbr_srgb_to_linear() converts RGB while leaving alpha unchanged.
        material.baseColor *= pbr_srgb_to_linear(texture(PbrMetallicRoughness_baseColorTexture, uv));
    }

    if (!material.unlit)
    {
        bool has_metallic_roughness_texture = pbrMetallicRoughness.metallicRoughnessTextures.y > 0.5;
        if (has_metallic_roughness_texture)
        {
            // glTF packs roughness in G and metallic in B. These are linear
            // data channels, so they must not undergo sRGB decoding.
            vec4 metallic_roughness = texture(PbrMetallicRoughness_metallicRoughnessTexture, uv);
            material.metallic *= metallic_roughness.b;
            material.roughness *= metallic_roughness.g;
        }

        bool has_occlusion_texture = pbrCommonTextures.y > 0.5;
        if (has_occlusion_texture)
        {
            // Strength zero disables occlusion; strength one uses the sampled R channel.
            float sampled_occlusion = texture(PbrMaterial_occlusionTexture, uv).r;
            float occlusion_strength = pbr_saturate(pbrNormalScaleAndOcclusionStrength.y);
            material.occlusion = mix(1.0, sampled_occlusion, occlusion_strength);
        }

        material.emissive = pbrEmissiveFactorAndStrength.rgb * pbrEmissiveFactorAndStrength.w;
        bool has_emissive_texture = pbrCommonTextures.z > 0.5;
        if (has_emissive_texture)
        {
            // Emission has the same sRGB filtering limitation described above.
            // Decode before lighting, leaving alpha unchanged.
            material.emissive *= pbr_srgb_to_linear(texture(PbrMaterial_emissiveTexture, uv)).rgb;
        }
    }

    return material;
}

// Default adapter from glTF varyings/textures to the explicit surface helpers.
// normal_uv can differ from the UVs passed to pbr_sample_material(). For custom
// geometry or procedural normals, call pbr_create_surface() directly instead.
PBRSurface pbr_sample_surface(PBRMaterial material, vec2 normal_uv)
{
    vec3 geometric_normal = pbr_normalize(var_normal, vec3(0.0, 0.0, 1.0));
    vec3 shading_normal = geometric_normal;

    // Without a tangent frame, retain the mesh normal. Unlit materials do not
    // need the normal texture; all remaining sampling happens before discard.
    bool has_normal_texture = pbrCommonTextures.x > 0.5;
    bool has_tangent_frame = var_has_tangent > 0.5;
    if (!material.unlit && has_normal_texture && has_tangent_frame)
    {
        mat3 tangent_frame = pbr_create_tangent_frame(geometric_normal, var_tangent, var_bitangent);
        vec3 normal_texel = texture(PbrMaterial_normalTexture, normal_uv).xyz;
        vec3 tangent_space_normal = pbr_decode_normal(normal_texel, pbrNormalScaleAndOcclusionStrength.x);
        shading_normal = pbr_transform_normal(tangent_frame, tangent_space_normal);
    }

    // The perspective camera is at the origin in view space. Custom callers
    // can supply a different view direction through pbr_create_surface().
    return pbr_create_surface(var_position.xyz, geometric_normal, shading_normal, -var_position.xyz,
                              material.doubleSided, gl_FrontFacing, var_view);
}

#endif
