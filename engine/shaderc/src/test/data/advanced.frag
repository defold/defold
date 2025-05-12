#version 140

in vec4 var_position;
in vec3 var_normal;
in vec2 var_texcoord0;
in vec4 var_light;

out vec4 color_out;

uniform sampler2D PbrMetallicRoughness_baseColorTexture;
uniform sampler2D PbrMetallicRoughness_metallicRoughnessTexture;

struct PbrMetallicRoughness
{
    vec4 baseColorFactor;
    vec4 metallicRoughnessFactor;
};

uniform sampler2D PbrSpecularGlossiness_diffuseTexture;
uniform sampler2D PbrSpecularGlossiness_specularGlossinessTexture;

struct PbrSpecularGlossiness
{
    vec4 diffuseFactor;
    vec4 specularGlossinessFactor;
};

struct PBRMaterial
{
    PbrMetallicRoughness metallicRoughness;
    PbrSpecularGlossiness specularGlossiness;
};

uniform fs_uniforms
{
    PBRMaterial material;
};

uniform PBRMaterialUniform
{
    PBRMaterial material;
} my_material;

void main()
{
    vec4 color = material.metallicRoughness.baseColorFactor + my_material.material.metallicRoughness.baseColorFactor;
    
    // Diffuse light calculations
    vec3 ambient_light = vec3(0.2);
    vec3 diff_light = vec3(normalize(var_light.xyz - var_position.xyz));
    diff_light = max(dot(var_normal,diff_light), 0.0) + ambient_light;
    diff_light = clamp(diff_light, 0.0, 1.0);

    color_out = vec4(color.rgb*diff_light,1.0);
}

