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

#ifndef DEFOLD_PBR_BRDF
#define DEFOLD_PBR_BRDF

#include "/builtins/materials/gltf_material.glsl"

/*
 * Microfacet BRDF helpers for metallic-roughness PBR.
 *
 * The public entry point is evaluate_brdf(), which separates diffuse and
 * specular light so downstream lighting extensions can add to either bucket
 * before final composition.
 */
vec3 fresnel_schlick(vec3 f0, vec3 f90, float v_dot_h)
{
    float x = saturate(1.0 - v_dot_h);
    float x2 = x * x;
    float x5 = x * x2 * x2;
    return f0 + (f90 - f0) * x5;
}

float visibility_ggx(float n_dot_l, float n_dot_v, float alpha_roughness)
{
    float alpha_roughness_sq = alpha_roughness * alpha_roughness;
    float ggx_v = n_dot_l * sqrt(n_dot_v * n_dot_v * (1.0 - alpha_roughness_sq) + alpha_roughness_sq);
    float ggx_l = n_dot_v * sqrt(n_dot_l * n_dot_l * (1.0 - alpha_roughness_sq) + alpha_roughness_sq);
    float ggx = ggx_v + ggx_l;
    return ggx > 0.0 ? 0.5 / ggx : 0.0;
}

float distribution_ggx(float n_dot_h, float alpha_roughness)
{
    float alpha_roughness_sq = alpha_roughness * alpha_roughness;
    float f = (n_dot_h * n_dot_h) * (alpha_roughness_sq - 1.0) + 1.0;
    return alpha_roughness_sq / (PBR_PI * f * f);
}

vec3 brdf_lambertian(MaterialInfo material, float v_dot_h)
{
    vec3 f = fresnel_schlick(material.f0, material.f90, v_dot_h);
    return (1.0 - material.specularWeight * f) * (material.diffuseColor / PBR_PI);
}

vec3 brdf_specular_ggx(MaterialInfo material, float v_dot_h, float n_dot_l, float n_dot_v, float n_dot_h)
{
    vec3 f = fresnel_schlick(material.f0, material.f90, v_dot_h);
    float vis = visibility_ggx(n_dot_l, n_dot_v, material.alphaRoughness);
    float d = distribution_ggx(n_dot_h, material.alphaRoughness);
    return material.specularWeight * f * vis * d;
}

/* Evaluates one light direction and returns diffuse/specular contributions. */
void evaluate_brdf(MaterialInfo material, vec3 n, vec3 v, vec3 l, vec3 light_color, out vec3 diffuse_light, out vec3 specular_light)
{
    diffuse_light = vec3(0.0);
    specular_light = vec3(0.0);

    vec3 h = normalize(l + v);
    float n_dot_l = clamped_dot(n, l);
    float n_dot_v = max(abs(dot(n, v)), PBR_EPSILON);
    float n_dot_h = clamped_dot(n, h);
    float v_dot_h = clamped_dot(v, h);

    if (n_dot_l <= 0.0)
    {
        return;
    }

    vec3 diffuse = brdf_lambertian(material, v_dot_h);
    vec3 specular = brdf_specular_ggx(material, v_dot_h, n_dot_l, n_dot_v, n_dot_h);
    diffuse_light = light_color * n_dot_l * diffuse;
    specular_light = light_color * n_dot_l * specular;
}

#endif
