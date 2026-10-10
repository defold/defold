#ifndef DEFOLD_PBR_BRDF
#define DEFOLD_PBR_BRDF

#include "/builtins/materials/gltf_types.glsl"
#include "/builtins/materials/gltf_common.glsl"

/*
 * Microfacet BRDF helpers for metallic-roughness PBR.
 *
 * The public entry point is pbr_evaluate_brdf(), which returns separate diffuse
 * and specular BRDF values. The lighting layer applies radiance and N dot L.
 */

// Approximates the fraction of light reflected by a microfacet at the viewing
// angle given by V dot H, from f0 at normal incidence to f90 at grazing angles.
vec3 pbr_evaluate_fresnel_schlick(vec3 f0, vec3 f90, float v_dot_h)
{
    // Schlick's approximation transitions from f0 at normal incidence to f90
    // at grazing angles, using the fifth power of (1 - V dot H).
    float grazing = pbr_saturate(1.0 - v_dot_h);
    float grazing_squared = grazing * grazing;
    float grazing_fifth = grazing * grazing_squared * grazing_squared;
    return f0 + (f90 - f0) * grazing_fifth;
}

// Height-correlated Smith visibility accounts for microfacets hiding one another.
float pbr_evaluate_visibility_ggx(float n_dot_l, float n_dot_v, float alpha_roughness)
{
    float alpha_roughness_sq = alpha_roughness * alpha_roughness;
    float ggx_v = n_dot_l * sqrt(n_dot_v * n_dot_v * (1.0 - alpha_roughness_sq) + alpha_roughness_sq);
    float ggx_l = n_dot_v * sqrt(n_dot_l * n_dot_l * (1.0 - alpha_roughness_sq) + alpha_roughness_sq);
    float ggx = ggx_v + ggx_l;
    return ggx > 0.0 ? 0.5 / ggx : 0.0;
}

// GGX describes how many microfacets are aligned with the light/view half vector.
float pbr_evaluate_distribution_ggx(float n_dot_h, float alpha_roughness)
{
    float alpha_roughness_sq = alpha_roughness * alpha_roughness;
    float denominator = (n_dot_h * n_dot_h) * (alpha_roughness_sq - 1.0) + 1.0;
    return alpha_roughness_sq / (PBR_PI * denominator * denominator);
}

vec3 pbr_evaluate_brdf_lambertian(MaterialInfo material, float v_dot_h)
{
    // Only the dielectric part scatters light diffusely. Its Fresnel must stay
    // independent of the metallic specular color; diffuseColor already includes
    // the (1 - metallic) weight used to blend dielectric and metal responses.
    vec3 fresnel = pbr_evaluate_fresnel_schlick(material.dielectricF0, material.f90, v_dot_h);
    return (1.0 - material.specularWeight * fresnel) * (material.diffuseColor / PBR_PI);
}

vec3 pbr_evaluate_brdf_specular_ggx(MaterialInfo material, float v_dot_h, float n_dot_l, float n_dot_v, float n_dot_h)
{
    vec3 fresnel = pbr_evaluate_fresnel_schlick(material.f0, material.f90, v_dot_h);
    float visibility = pbr_evaluate_visibility_ggx(n_dot_l, n_dot_v, material.alphaRoughness);
    float distribution = pbr_evaluate_distribution_ggx(n_dot_h, material.alphaRoughness);
    return material.specularWeight * fresnel * visibility * distribution;
}

// All directions are unit vectors in the same coordinate space. Outputs are
// BRDF values, without incoming radiance or the projected-area factor N dot L.
void pbr_evaluate_brdf(MaterialInfo material, vec3 normal, vec3 view_direction, vec3 light_direction,
                       out vec3 diffuse_brdf, out vec3 specular_brdf)
{
    diffuse_brdf = vec3(0.0);
    specular_brdf = vec3(0.0);

    // A microfacet reflects this light toward the viewer when its normal is
    // aligned with the halfway direction between the light and view vectors.
    vec3 half_direction = pbr_normalize(light_direction + view_direction, normal);
    float n_dot_l = pbr_clamped_dot(normal, light_direction);
    float n_dot_v = max(abs(dot(normal, view_direction)), PBR_EPSILON);
    float n_dot_h = pbr_clamped_dot(normal, half_direction);
    float v_dot_h = pbr_clamped_dot(view_direction, half_direction);

    // Lights behind the shading normal contribute neither diffuse nor specular light.
    if (n_dot_l <= 0.0)
    {
        return;
    }

    diffuse_brdf = pbr_evaluate_brdf_lambertian(material, v_dot_h);
    specular_brdf = pbr_evaluate_brdf_specular_ggx(material, v_dot_h, n_dot_l, n_dot_v, n_dot_h);
}

#endif
