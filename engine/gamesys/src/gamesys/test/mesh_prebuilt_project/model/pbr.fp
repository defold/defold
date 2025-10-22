#version 140

#define PBR_METALLIC_ROUGHNESS
#define PBR_SPECULAR_GLOSSINESS
#define PBR_CLEARCOAT
#define PBR_TRANSMISSION
#define PBR_IOR
#define PBR_SPECULAR
#define PBR_VOLUME
#define PBR_SHEEN
#define PBR_EMISSIVE_STRENGTH
#define PBR_IRIDESCENCE

////////////////////////
// COMMON
////////////////////////
uniform sampler2D PbrMaterial_normalTexture;
uniform sampler2D PbrMaterial_occlusionTexture;
uniform sampler2D PbrMaterial_emissiveTexture;

////////////////////////
// METALLIC ROUGHNESS
////////////////////////
uniform sampler2D PbrMetallicRoughness_baseColorTexture;
uniform sampler2D PbrMetallicRoughness_metallicRoughnessTexture;

struct PbrMetallicRoughness
{
	vec4 baseColorFactor;
	vec4 metallicAndRoughnessFactor; // R: metallic (Default=1.0), G: roughness (Default=1.0)
	vec4 metallicRoughnessTextures;  // R: use baseColorTexture, G: use metallicRoughnessTexture
};

////////////////////////
// SPECULAR GLOSSINESS
////////////////////////
uniform sampler2D PbrSpecularGlossiness_diffuseTexture;
uniform sampler2D PbrSpecularGlossiness_specularGlossinessTexture;

struct PbrSpecularGlossiness
{
	vec4 diffuseFactor;
	vec4 specularAndSpecularGlossinessFactor; // RGB: specular (Default=1.0), A: glossiness (Default=1.0)
	vec4 specularGlossinessTextures;          // R: use diffuseTexture, G: use specularGlossinessTexture
};

////////////////////////
// CLEARCOAT
////////////////////////
uniform sampler2D PbrClearcoat_clearcoatTexture;
uniform sampler2D PbrClearcoat_clearcoatRoughnessTexture;
uniform sampler2D PbrClearcoat_clearcoatNormalTexture;

struct PbrClearCoat
{
	vec4 clearCoatAndClearCoatRoughnessFactor; // R: clearCoat (Default=0.0), G: clearCoatRoughness (Default=0.0)
	vec4 clearCoatTextures;                    // R: use clearCoatTexture, G: use clearCoatRoughnessTexture, B: use clearCoatNormalTexture
};

////////////////////////
// TRANSMISSION
////////////////////////
uniform sampler2D PbrTransmission_transmissionTexture;

struct PbrTransmission
{
	vec4 transmissionFactor;   // R: transmission (Default=0.0)
	vec4 transmissionTextures; // R: use transmissionTexture
};

////////////////////////
// IOR
////////////////////////
struct PbrIor
{
	vec4 ior; // R: ior (Default=0.0)
};

////////////////////////
// SPECULAR
////////////////////////
uniform sampler2D PbrSpecular_specularTexture;
uniform sampler2D PbrSpecular_specularColorTexture;

struct PbrSpecular
{
	vec4 specularColorAndSpecularFactor; // RGB: specularColor, A: specularFactor (Default=1.0);
	vec4 specularTextures;               // R: use specularTexture, G: use specularColorTexture
};

////////////////////////
// VOLUME
////////////////////////
uniform sampler2D PbrVolume_thicknessTexture;

struct PbrVolume
{
	vec4 thicknessFactorAndAttenuationColor; // R: thicknessFactor (Default=0.0), RGB: attenuationColor
	vec4 attenuationDistance;                // R: attentuationDistance (Default=-1.0)
	vec4 volumeTextures;                     // R: use thicknessTexture
};

////////////////////////
// SHEEN
////////////////////////
uniform sampler2D PbrSheen_sheenColorTexture;
uniform sampler2D PbrSheen_sheenRoughnessTexture;

struct PbrSheen
{
	vec4 sheenColorAndRoughnessFactor; // RGB: sheenColor, A: sheenRoughnessFactor (Default=0.0)
	vec4 sheenTextures;                // R: use sheenColorTexture, G: use sheenRoughnessTexture
};

////////////////////////
// EMISSIVE STRENGTH
////////////////////////
struct PbrEmissiveStrength
{
	vec4 emissiveStrength; // R: emissiveStrength (Default=1.0)
};

////////////////////////
// IRIDESCENSE
////////////////////////
uniform sampler2D PbrEmissive_iridescenceTexture;
uniform sampler2D PbrEmissive_iridescenceThicknessTexture;

struct PbrIridescence
{
	vec4 iridescenceFactorAndIorAndThicknessMinMax; // R: iridescenceFactor (Default=0.0), G: iridescenceIor (Default=1.3), B: iridescenceThicknessMin (Default=100.0), A: iridescenceThicknessMax (Default=400.0)
	vec4 iridescenceTextures;                       // R: use iridescenceTexture, G: use iridescenceThicknessTexture
};

uniform PbrMaterial
{
	// Common properties
	vec4 pbrAlphaCutoffAndDoubleSidedAndIsUnlit; // R: alphaCutoff (Default=0.5), G: doubleSided (Default=false), B: unlit (Default=false)
	vec4 pbrCommonTextures;                      // R: use normalTexture, G: use occlusionTexture, B: use emissiveTexture
#ifdef PBR_METALLIC_ROUGHNESS
	PbrMetallicRoughness  pbrMetallicRoughness;
#endif
#ifdef PBR_SPECULAR_GLOSSINESS
	PbrSpecularGlossiness pbrSpecularGlossiness;
#endif
#ifdef PBR_CLEARCOAT
	PbrClearCoat pbrClearCoat;
#endif
#ifdef PBR_TRANSMISSION
	PbrTransmission pbrTransmission;
#endif
#ifdef PBR_IOR
	PbrIor pbrIor;
#endif
#ifdef PBR_SPECULAR
	PbrSpecular pbrSpecular;
#endif
#ifdef PBR_VOLUME
	PbrVolume pbrVolume;
#endif
#ifdef PBR_SHEEN
	PbrSheen pbrSheen;
#endif
#ifdef PBR_EMISSIVE_STRENGTH
	PbrEmissiveStrength pbrEmissiveStrength;
#endif
#ifdef PBR_IRIDESCENCE
	PbrIridescence pbrIridescence;
#endif
};

out vec4 FragColor;

void main()
{
    FragColor =
        pbrAlphaCutoffAndDoubleSidedAndIsUnlit + pbrCommonTextures +
        pbrMetallicRoughness.baseColorFactor + pbrMetallicRoughness.metallicAndRoughnessFactor + pbrMetallicRoughness.metallicRoughnessTextures +
        pbrSpecularGlossiness.diffuseFactor + pbrSpecularGlossiness.specularAndSpecularGlossinessFactor + pbrSpecularGlossiness.specularGlossinessTextures +
        pbrClearCoat.clearCoatAndClearCoatRoughnessFactor + pbrClearCoat.clearCoatTextures +
        pbrTransmission.transmissionFactor + pbrTransmission.transmissionTextures +
        pbrIor.ior +
        pbrSpecular.specularColorAndSpecularFactor + pbrSpecular.specularTextures +
        pbrVolume.thicknessFactorAndAttenuationColor + pbrVolume.attenuationDistance + pbrVolume.volumeTextures +
        pbrSheen.sheenColorAndRoughnessFactor + pbrSheen.sheenTextures +
        pbrEmissiveStrength.emissiveStrength +
        pbrIridescence.iridescenceFactorAndIorAndThicknessMinMax + pbrIridescence.iridescenceTextures;
}

