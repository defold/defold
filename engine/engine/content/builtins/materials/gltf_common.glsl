#ifndef DEFOLD_PBR_COMMON
#define DEFOLD_PBR_COMMON

#include "/builtins/materials/gltf_types.glsl"

/*
 * Pure math, color-space, and surface construction helpers shared by the PBR
 * shader modules. No uniforms, varyings, or texture reads are required here.
 * Color textures contain sRGB-encoded values decoded here after sampling;
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

// The normal must be a unit vector. Rebuild an orthogonal tangent frame after
// interpolation, recovering mirrored UV/transform handedness from the bitangent.
mat3 pbr_create_tangent_frame(vec3 normal, vec3 tangent, vec3 bitangent)
{
    tangent = pbr_normalize(tangent - normal * dot(normal, tangent), vec3(0.0));
    vec3 orthogonal_bitangent = cross(normal, tangent);
    float handedness = dot(orthogonal_bitangent, bitangent) < 0.0 ? -1.0 : 1.0;
    return mat3(tangent, orthogonal_bitangent * handedness, normal);
}

// Decode a glTF normal texel from [0, 1] to [-1, 1]. Scale affects X/Y only.
// Procedural normals already in tangent space can bypass this step.
vec3 pbr_decode_normal(vec3 normal_texel, float normal_scale)
{
    vec3 normal = normal_texel * 2.0 - 1.0;
    normal.xy *= normal_scale;
    return normal;
}

// Transform a decoded or procedural tangent-space normal, retaining the mesh
// normal (the frame's third column) when the result degenerates.
vec3 pbr_transform_normal(mat3 tangent_frame, vec3 tangent_space_normal)
{
    return pbr_normalize(tangent_frame * tangent_space_normal, tangent_frame[2]);
}

// View-space fields are authoritative. Call after editing position, normal,
// geometricNormal or view to normalize directions and refresh world-space data.
// Normals must already face the desired side; finalization never flips them.
// If position changes, update view for the chosen camera model as needed.
void pbr_finalize_surface(inout PBRSurface surface, mat4 view_matrix)
{
    surface.geometricNormal = pbr_normalize(surface.geometricNormal, vec3(0.0, 0.0, 1.0));
    surface.normal = pbr_normalize(surface.normal, surface.geometricNormal);
    surface.view = pbr_normalize(surface.view, vec3(0.0, 0.0, 1.0));

    // Defold camera views are rigid transforms. Invert the rotation by
    // transposing it, and remove the translation when transforming positions.
    mat3 inverse_view_rotation = transpose(mat3(view_matrix));
    surface.worldPosition = inverse_view_rotation * (surface.position - view_matrix[3].xyz);
    surface.worldNormal = inverse_view_rotation * surface.normal;
    surface.worldGeometricNormal = inverse_view_rotation * surface.geometricNormal;
    surface.worldView = inverse_view_rotation * surface.view;
}

// Construct from explicit view-space geometry, without shader bindings. Supply
// normals before the back-face flip and a surface-to-camera view direction.
PBRSurface pbr_create_surface(vec3 position, vec3 geometric_normal, vec3 shading_normal,
                              vec3 view_direction, bool double_sided, bool front_facing, mat4 view_matrix)
{
    PBRSurface surface;
    float facing_sign = double_sided && !front_facing ? -1.0 : 1.0;
    surface.position = position;
    surface.geometricNormal = geometric_normal * facing_sign;
    surface.normal = shading_normal * facing_sign;
    surface.view = view_direction;
    pbr_finalize_surface(surface, view_matrix);
    return surface;
}

#endif
