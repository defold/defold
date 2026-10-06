#ifndef DEFOLD_PBR_TYPES
#define DEFOLD_PBR_TYPES

#define PBR_ALPHA_OPAQUE 0
#define PBR_ALPHA_MASK   1
#define PBR_ALPHA_BLEND  2

// Resolved, linear material inputs. Modify these before pbr_create_material_info().
struct PBRMaterial
{
    vec4 baseColor;
    vec3 emissive;
    float metallic;
    float roughness;
    float occlusion;
    float alphaCutoff;
    int alphaMode;
    bool doubleSided;
    bool unlit;
};

// View-space position and unit directions are authoritative; world* are derived.
// After editing view-space fields, call pbr_finalize_surface() to synchronize them.
struct PBRSurface
{
    vec3 position;
    vec3 normal;
    vec3 geometricNormal; // Interpolated mesh normal before normal mapping.
    vec3 view;
    vec3 worldPosition;
    vec3 worldNormal;
    vec3 worldGeometricNormal;
    vec3 worldView;
};

// Derived BRDF values. Rebuild after changing PBRMaterial's base inputs.
struct MaterialInfo
{
    vec4 baseColor;
    vec3 diffuseColor;
    vec3 dielectricF0; // Nonmetal reflectance, kept separate for diffuse energy conservation.
    vec3 f0;  // Specular reflectance at normal incidence.
    vec3 f90; // Specular reflectance at grazing angles.
    float metallic;
    float perceptualRoughness;
    float alphaRoughness;
    float specularWeight;
};

struct PBRLightSample
{
    vec3 direction; // Unit surface-to-light direction, in view space.
    vec3 radiance;  // Linear light color times intensity and attenuation.
};

// Linear outgoing light, already weighted by the material response.
// IBL/GI providers must not apply albedo or the BRDF again when accumulating.
struct PBRLightContribution
{
    vec3 diffuse;
    vec3 specular;
};

struct PBRLighting
{
    PBRLightContribution direct;
    PBRLightContribution indirect;
};

#endif
