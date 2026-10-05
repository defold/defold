#ifndef DEFOLD_PBR_BRDF
#define DEFOLD_PBR_BRDF

#include "/builtins/materials/gltf_types.glsl"
#include "/builtins/materials/gltf_common.glsl"

/*
 * Microfacet BRDF helpers for metallic-roughness PBR.
 *
 * The public entry point is evaluate_brdf(), which separates diffuse and
 * specular light so downstream lighting extensions can add to either contribution
 * before final composition.
 */
vec3 fresnel_schlick(vec3 f0, vec3 f90, float v_dot_h)
{
    // Schlick's approximation transitions from f0 at normal incidence to f90
    // at grazing angles, using the fifth power of (1 - V dot H).
    float grazing = saturate(1.0 - v_dot_h);
    float grazing_squared = grazing * grazing;
    float grazing_fifth = grazing * grazing_squared * grazing_squared;
    return f0 + (f90 - f0) * grazing_fifth;
}

// Height-correlated Smith visibility accounts for microfacets hiding one another.
float visibility_ggx(float n_dot_l, float n_dot_v, float alpha_roughness)
{
    float alpha_roughness_sq = alpha_roughness * alpha_roughness;
    float ggx_v = n_dot_l * sqrt(n_dot_v * n_dot_v * (1.0 - alpha_roughness_sq) + alpha_roughness_sq);
    float ggx_l = n_dot_v * sqrt(n_dot_l * n_dot_l * (1.0 - alpha_roughness_sq) + alpha_roughness_sq);
    float ggx = ggx_v + ggx_l;
    return ggx > 0.0 ? 0.5 / ggx : 0.0;
}

// GGX describes how many microfacets are aligned with the light/view half vector.
float distribution_ggx(float n_dot_h, float alpha_roughness)
{
    float alpha_roughness_sq = alpha_roughness * alpha_roughness;
    float denominator = (n_dot_h * n_dot_h) * (alpha_roughness_sq - 1.0) + 1.0;
    return alpha_roughness_sq / (PBR_PI * denominator * denominator);
}

vec3 brdf_lambertian(MaterialInfo material, float v_dot_h)
{
    // Only the dielectric part scatters light diffusely. Its Fresnel must stay
    // independent of the metallic specular color; diffuseColor already includes
    // the (1 - metallic) weight used to blend dielectric and metal responses.
    vec3 fresnel = fresnel_schlick(material.dielectricF0, material.f90, v_dot_h);
    return (1.0 - material.specularWeight * fresnel) * (material.diffuseColor / PBR_PI);
}

vec3 brdf_specular_ggx(MaterialInfo material, float v_dot_h, float n_dot_l, float n_dot_v, float n_dot_h)
{
    vec3 fresnel = fresnel_schlick(material.f0, material.f90, v_dot_h);
    float visibility = visibility_ggx(n_dot_l, n_dot_v, material.alphaRoughness);
    float distribution = distribution_ggx(n_dot_h, material.alphaRoughness);
    return material.specularWeight * fresnel * visibility * distribution;
}

// All directions are unit vectors in view space. Radiance already includes
// the light's color, intensity and distance/spot attenuation.
void evaluate_brdf(MaterialInfo material, vec3 normal, vec3 view_direction, vec3 light_direction,
                   vec3 radiance, out vec3 diffuse_light, out vec3 specular_light)
{
    diffuse_light = vec3(0.0);
    specular_light = vec3(0.0);

    // A microfacet reflects this light toward the viewer when its normal is
    // aligned with the halfway direction between the light and view vectors.
    vec3 half_direction = pbr_normalize(light_direction + view_direction, normal);
    float n_dot_l = clamped_dot(normal, light_direction);
    float n_dot_v = max(abs(dot(normal, view_direction)), PBR_EPSILON);
    float n_dot_h = clamped_dot(normal, half_direction);
    float v_dot_h = clamped_dot(view_direction, half_direction);

    // Lights behind the shading normal contribute neither diffuse nor specular light.
    if (n_dot_l <= 0.0)
    {
        return;
    }

    vec3 diffuse = brdf_lambertian(material, v_dot_h);
    vec3 specular = brdf_specular_ggx(material, v_dot_h, n_dot_l, n_dot_v, n_dot_h);
    diffuse_light = radiance * n_dot_l * diffuse;
    specular_light = radiance * n_dot_l * specular;
}

#endif
