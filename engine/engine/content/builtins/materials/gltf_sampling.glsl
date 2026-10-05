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
// so texture() filters sRGB-encoded texels before to_linear() decodes the result.
// Correct color filtering decodes each texel first; our ordering can darken
// transitions. The current 8-bit mipmap path also averages encoded values, which
// cannot be corrected by decoding the resulting mip texels in this shader.
// Fixing both requires sRGB-aware mipmap generation and texture sampling. Remove
// the manual decoding below when those samplers return linear RGB, to avoid
// decoding twice. Alpha and normal/metallic/roughness/occlusion data stay linear.
//
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

    // Missing textures leave the material factors unchanged. Unlit materials
    // use only base color and alpha, so lighting-only textures are not sampled.
    material.occlusion = 1.0;
    material.emissive = vec3(0.0);

    bool has_base_color_texture = pbrMetallicRoughness.metallicRoughnessTextures.x > 0.5;
    if (has_base_color_texture)
    {
        // Color textures are sRGB; factors and vertex colors are already linear.
        // to_linear() converts RGB while leaving alpha unchanged.
        material.baseColor *= to_linear(texture(PbrMetallicRoughness_baseColorTexture, uv));
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
            float occlusion_strength = saturate(pbrNormalScaleAndOcclusionStrength.y);
            material.occlusion = mix(1.0, sampled_occlusion, occlusion_strength);
        }

        material.emissive = pbrEmissiveFactorAndStrength.rgb * pbrEmissiveFactorAndStrength.w;
        bool has_emissive_texture = pbrCommonTextures.z > 0.5;
        if (has_emissive_texture)
        {
            // Emission has the same sRGB filtering limitation described above.
            // Decode before lighting, leaving alpha unchanged.
            material.emissive *= to_linear(texture(PbrMaterial_emissiveTexture, uv)).rgb;
        }
    }

    return material;
}

PBRSurface get_pbr_surface(PBRMaterial material)
{
    PBRSurface surface;
    surface.position = var_position.xyz;
    // The camera is at the origin in view space, so the view direction points
    // from the surface back toward that origin.
    surface.view = pbr_normalize(-surface.position, vec3(0.0, 0.0, 1.0));

    vec3 view_normal = pbr_normalize(var_normal, vec3(0.0, 0.0, 1.0));
    float facing_sign = material.doubleSided && !gl_FrontFacing ? -1.0 : 1.0;
    // Keep the unperturbed mesh normal available separately from the normal map.
    surface.geometricNormal = view_normal * facing_sign;

    // Tangent-space normal maps require a valid mesh tangent frame. Otherwise
    // keep the mesh normal; unlit materials do not need normal-map sampling.
    bool has_normal_texture = pbrCommonTextures.x > 0.5;
    bool has_tangent_frame = var_has_tangent > 0.5;
    if (!material.unlit && has_normal_texture && has_tangent_frame)
    {
        // Interpolation can make the tangent and normal non-orthogonal. Remove
        // the tangent's normal component before rebuilding the tangent frame.
        vec3 view_tangent = var_tangent - view_normal * dot(view_normal, var_tangent);
        view_tangent = pbr_normalize(view_tangent, vec3(0.0));

        // Recover the frame's handedness from the interpolated bitangent. This
        // preserves mirrored UVs and reflections introduced by model transforms.
        vec3 view_bitangent = cross(view_normal, view_tangent);
        float handedness = dot(view_bitangent, var_bitangent) < 0.0 ? -1.0 : 1.0;
        view_bitangent *= handedness;

        // Normal textures store tangent-space directions in [0, 1]. Decode to
        // [-1, 1], then apply glTF's normal scale to X/Y only, leaving Z unchanged.
        vec3 tangent_space_normal = texture(PbrMaterial_normalTexture, var_texcoord0).xyz * 2.0 - 1.0;
        tangent_space_normal.xy *= pbrNormalScaleAndOcclusionStrength.x;

        // The frame's columns convert the sampled direction into view space.
        // Normalize after scaling, retaining the mesh normal if the result degenerates.
        mat3 tangent_to_view = mat3(view_tangent, view_bitangent, view_normal);
        view_normal = pbr_normalize(tangent_to_view * tangent_space_normal, view_normal);
    }

    // Double-sided back faces need the final, normal-mapped direction flipped too.
    surface.normal = view_normal * facing_sign;

    // Defold camera views are rigid transforms: the inverse rotation is its
    // transpose. Positions also need the view translation removed first.
    mat3 inverse_view_rotation = transpose(mat3(var_view));
    surface.worldPosition = inverse_view_rotation * (surface.position - var_view[3].xyz);
    surface.worldNormal = inverse_view_rotation * surface.normal;
    surface.worldGeometricNormal = inverse_view_rotation * surface.geometricNormal;
    surface.worldView = inverse_view_rotation * surface.view;

    return surface;
}

#endif
