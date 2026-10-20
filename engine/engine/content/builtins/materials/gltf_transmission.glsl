#ifndef DEFOLD_PBR_TRANSMISSION
#define DEFOLD_PBR_TRANSMISSION

#include "/builtins/materials/gltf_lighting.glsl"

uniform sampler2D tex_scene_color;

uniform pbr_transmission_uniforms
{
    highp mat4 mtx_projection;
    // X enables transmission only while an opaque scene capture is bound.
    vec4 transmission_params;
};

vec3 pbr_volume_ray(PBRMaterial material, PBRSurface surface, mat3 world_to_local)
{
    // Thin-walled materials do not trace an interior ray.
    if (material.thickness <= 0.0)
        return vec3(0.0);
    // Zero is glTF's infinite-IOR compatibility mode.
    float inverse_ior = material.ior == 0.0 ? 0.0 : 1.0 / material.ior;
    vec3  ray = refract(-surface.worldView, surface.worldNormal, inverse_ior);
    ray = pbr_normalize(ray, -surface.worldView);
    // Thickness is in mesh space. Convert its distance without changing the
    // world-space direction selected by refraction, including nonuniform scale.
    float local_distance = length(world_to_local * ray);
    return ray * (material.thickness / local_distance);
}

vec3 pbr_volume_transmittance(PBRMaterial material, float distance_travelled)
{
    if (distance_travelled <= 0.0 || material.attenuationDistance <= 0.0)
    {
        return vec3(1.0);
    }
    return pow(clamp(material.attenuationColor, vec3(0.0), vec3(1.0)),
               vec3(distance_travelled / material.attenuationDistance));
}

// The opaque capture and final draw use matching viewports. Compute derivatives
// before any discard or material-dependent branches, using the unrefracted UVs.
vec2 pbr_transmission_pixel_size(vec3 position, mat4 projection)
{
    vec4 clip = projection * vec4(position, 1.0);
    vec2 uv = clip.xy / clip.w * 0.5 + 0.5;
    return vec2(abs(dFdx(uv.x)), abs(dFdy(uv.y)));
}

// Approximate the rough transmission lobe on single-level render targets with
// a fixed disk footprint. Decode each sample before accumulating linear radiance.
// IOR 1 has no refraction, including at the microfacet level.
vec3 pbr_sample_transmission(vec2 uv, vec2 pixel_size, float roughness, float ior)
{
    vec2 size = 1.0 / pixel_size;
    float adjusted_roughness = pbr_saturate(roughness) * (ior == 0.0 ? 1.0 : clamp(2.0 * (ior - 1.0), 0.0, 1.0));
    float lod = log2(max(size.x, size.y)) * adjusted_roughness;
    if (lod <= 0.0)
    {
        return pbr_srgb_to_linear(texture(tex_scene_color, uv)).rgb;
    }

    vec2 footprint = vec2(0.5 * (exp2(lod) - 1.0)) / size;
    // Equal-area golden-angle samples avoid retaining an unblurred center tap.
    const vec2 offsets[16] = vec2[16](
        vec2(0.17677670, 0.00000000),
        vec2(-0.22577219, 0.20682582),
        vec2(0.03455805, -0.39377118),
        vec2(0.28457122, 0.37117276),
        vec2(-0.52222319, -0.09237393),
        vec2(0.49469539, -0.31468471),
        vec2(-0.16546593, 0.61552500),
        vec2(-0.31556147, -0.60759440),
        vec2(0.68464216, 0.25003022),
        vec2(-0.71225609, 0.29400896),
        vec2(0.34335450, -0.73372862),
        vec2(0.25373024, 0.80893199),
        vec2(-0.76474589, -0.44318588),
        vec2(0.89713398, -0.19723239),
        vec2(-0.54750690, 0.77877223),
        vec2(-0.12648677, -0.97608970));
    vec3 color = vec3(0.0);
    for (int i = 0; i < 16; ++i)
    {
        color += pbr_srgb_to_linear(texture(tex_scene_color, uv + offsets[i] * footprint)).rgb;
    }
    return color / 16.0;
}

// Compose after accumulating direct and indirect lighting. The caller supplies
// linear scene color; the opaque capture must exclude transmissive geometry.
vec3 pbr_compose_transmission(PBRMaterial inputs, MaterialInfo material, PBRSurface surface, PBRLighting lighting, float transmission, vec3 background, float distance_travelled, float diffuse_visibility, float specular_visibility)
{
    lighting.direct.diffuse *= 1.0 - transmission;
    lighting.indirect.diffuse *= 1.0 - transmission;
    // Diffuse already contains (1 - metallic). Transmission replaces only the
    // dielectric base layer, so apply that weight once, after dielectric Fresnel.
    vec3 fresnel = pbr_evaluate_fresnel_schlick(material.dielectricF0, material.f90, pbr_clamped_dot(surface.normal, surface.view));
    vec3 transmitted = background * material.baseColor.rgb * pbr_volume_transmittance(inputs, distance_travelled) * (1.0 - fresnel);
    return pbr_compose_lighting(inputs, lighting, diffuse_visibility, specular_visibility) + transmitted * transmission * (1.0 - material.metallic);
}

#endif
