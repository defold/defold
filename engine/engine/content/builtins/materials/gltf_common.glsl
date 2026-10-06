#ifndef DEFOLD_PBR_COMMON
#define DEFOLD_PBR_COMMON

/*
 * Common scalar, color-space, and clamping helpers shared by the PBR shader
 * modules. Color textures contain sRGB-encoded values decoded here after sampling;
 * see gltf_sampling.glsl for filtering limitations. Output encoding preserves HDR
 * values; tone mapping belongs to the application.
 */

const float PBR_PI = 3.1415926535897932384626433832795;
const float PBR_EPSILON = 0.00001;

float pbr_saturate(float value)
{
    return clamp(value, 0.0, 1.0);
}

vec3 pbr_saturate(vec3 value)
{
    return clamp(value, vec3(0.0), vec3(1.0));
}

vec4 pbr_srgb_to_linear(vec4 color)
{
    // sRGB has a linear segment near black. Alpha is already linear.
    vec3 low = color.rgb / 12.92;
    vec3 high = pow(max((color.rgb + 0.055) / 1.055, vec3(0.0)), vec3(2.4));
    return vec4(mix(high, low, lessThanEqual(color.rgb, vec3(0.04045))), color.a);
}

vec3 pbr_linear_to_srgb(vec3 color)
{
    // Encode linear RGB as sRGB without clipping highlights above one.
    color = max(color, vec3(0.0));
    vec3 low = color * 12.92;
    vec3 high = 1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055;
    return mix(high, low, lessThanEqual(color, vec3(0.0031308)));
}

float pbr_clamped_dot(vec3 a, vec3 b)
{
    return pbr_saturate(dot(a, b));
}

vec3 pbr_normalize(vec3 value, vec3 fallback)
{
    // Degenerate mesh inputs must not introduce NaNs into the lighting calculation.
    float length_squared = dot(value, value);
    return length_squared > PBR_EPSILON * PBR_EPSILON ? value * inversesqrt(length_squared) : fallback;
}

#endif
