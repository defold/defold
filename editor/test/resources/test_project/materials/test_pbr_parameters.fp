#version 140

struct PbrMetallicRoughness
{
	vec4 baseColorFactor;
	vec4 metallicAndRoughnessFactor;
	vec4 metallicRoughnessTextures;
};

struct PbrSpecularGlossiness
{
	vec4 diffuseFactor;
	vec4 specularAndSpecularGlossinessFactor;
	vec4 specularGlossinessTextures;
};

struct PbrClearCoat
{
	vec4 clearCoatAndClearCoatRoughnessFactor;
	vec4 clearCoatTextures;
};

struct PbrTransmission
{
	vec4 transmissionFactor;
	vec4 transmissionTextures;
};

struct PbrIor
{
	vec4 ior;
};

struct PbrSpecular
{
	vec4 specularColorAndSpecularFactor;
	vec4 specularTextures;
};

struct PbrVolume
{
	vec4 thicknessFactorAndAttenuationColor;
	vec4 attenuationDistance;
	vec4 volumeTextures;
};

struct PbrSheen
{
	vec4 sheenColorAndRoughnessFactor;
	vec4 sheenTextures;
};

struct PbrEmissiveStrength
{
	vec4 emissiveStrength;
};

struct PbrIridescence
{
	vec4 iridescenceFactorAndIorAndThicknessMinMax;
	vec4 iridescenceTextures;
};

uniform PbrMaterial
{
	vec4 pbrAlphaCutoffAndDoubleSidedAndIsUnlit;
	vec4 pbrCommonTextures;
	PbrMetallicRoughness  pbrMetallicRoughness;
	PbrSpecularGlossiness pbrSpecularGlossiness;
	PbrClearCoat pbrClearCoat;
	PbrTransmission pbrTransmission;
	PbrIor pbrIor;
	PbrSpecular pbrSpecular;
	PbrVolume pbrVolume;
	PbrSheen pbrSheen;
	PbrEmissiveStrength pbrEmissiveStrength;
	PbrIridescence pbrIridescence;
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

