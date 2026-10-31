// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <string.h>
#include <stddef.h>
#include <stdlib.h>

#include <dlib/log.h>
#include <dlib/static_assert.h>

#include <dlib/hash.h>
#include <dmsdk/dlib/vmath.h>

#include "../gamesys_private.h"
#include "../resources/res_meshset.h"
#include "../resources/res_model.h"
#include "../resources/res_rig_scene.h"

namespace dmGameSystem
{
    const dmhash_t PBR_METALLIC_ROUGHNESS_BASE_COLOR_FACTOR                         = dmHashString64("pbrMetallicRoughness.baseColorFactor");
    const dmhash_t PBR_METALLIC_ROUGHNESS_METALLIC_AND_ROUGHNESS_FACTOR             = dmHashString64("pbrMetallicRoughness.metallicAndRoughnessFactor");
    const dmhash_t PBR_METALLIC_ROUGHNESS_TEXTURES                                  = dmHashString64("pbrMetallicRoughness.metallicRoughnessTextures");
    const dmhash_t PBR_SPECULAR_GLOSSINESS_DIFFUSE_FACTOR                           = dmHashString64("pbrSpecularGlossiness.diffuseFactor");
    const dmhash_t PBR_SPECULAR_GLOSSINESS_SPECULAR_AND_SPECULAR_GLOSSINESS_FACTOR  = dmHashString64("pbrSpecularGlossiness.specularAndSpecularGlossinessFactor");
    const dmhash_t PBR_SPECULAR_GLOSSINESS_TEXTURES                                 = dmHashString64("pbrSpecularGlossiness.specularGlossinessTextures");
    const dmhash_t PBR_CLEAR_COAT_CLEAR_COAT_AND_CLEAR_COAT_ROUGHNESS_FACTOR        = dmHashString64("pbrClearCoat.clearCoatAndClearCoatRoughnessFactor");
    const dmhash_t PBR_CLEAR_COAT_TEXTURES                                          = dmHashString64("pbrClearCoat.clearCoatTextures");
    const dmhash_t PBR_TRANSMISSION_TRANSMISSION_FACTOR                             = dmHashString64("pbrTransmission.transmissionFactor");
    const dmhash_t PBR_TRANSMISSION_TEXTURES                                        = dmHashString64("pbrTransmission.transmissionTextures");
    const dmhash_t PBR_IOR_IOR_FACTOR                                               = dmHashString64("pbrIor.ior");
    const dmhash_t PBR_SPECULAR_SPECULAR_COLOR_AND_SPECULAR_FACTOR                  = dmHashString64("pbrSpecular.specularColorAndSpecularFactor");
    const dmhash_t PBR_SPECULAR_TEXTURES                                            = dmHashString64("pbrSpecular.specularTextures");
    const dmhash_t PBR_VOLUME_THICKNESS_FACTOR_AND_ATTENUATION_COLOR                = dmHashString64("pbrVolume.thicknessFactorAndAttenuationColor");
    const dmhash_t PBR_VOLUME_ATTENUATION_DISTANCE                                  = dmHashString64("pbrVolume.attenuationDistance");
    const dmhash_t PBR_VOLUME_TEXTURES                                              = dmHashString64("pbrVolume.volumeTextures");
    const dmhash_t PBR_SHEEN_SHEEN_COLOR_AND_SHEEN_ROUGHNESS_FACTOR                 = dmHashString64("pbrSheen.sheenColorAndRoughnessFactor");
    const dmhash_t PBR_SHEEN_TEXTURES                                               = dmHashString64("pbrSheen.sheenTextures");
    const dmhash_t PBR_EMISSIVE_STRENGTH_EMISSIVE_STRENGTH                          = dmHashString64("pbrEmissiveStrength.emissiveStrength");
    const dmhash_t PBR_IRIDESCENCE_IRIDESCENCE_FACTOR_AND_IOR_AND_THICKNESS_MIN_MAX = dmHashString64("pbrIridescence.iridescenceFactorAndIorAndThicknessMinMax");
    const dmhash_t PBR_IRIDESCENCE_TEXTURES                                         = dmHashString64("pbrIridescence.iridescenceTextures");
    const dmhash_t PBR_ALPHA_CUTOFF_AND_DOUBLE_SIDED_AND_IS_UNLIT                   = dmHashString64("pbrAlphaCutoffAndDoubleSidedAndIsUnlit");
    const dmhash_t PBR_COMMON_TEXTURES                                              = dmHashString64("pbrCommonTextures");
    const dmhash_t PBR_EMISSIVE_FACTOR_AND_STRENGTH                                 = dmHashString64("pbrEmissiveFactorAndStrength");
    const dmhash_t PBR_NORMAL_SCALE_AND_OCCLUSION_STRENGTH                          = dmHashString64("pbrNormalScaleAndOcclusionStrength");

    struct ModelMaterialBuffer
    {
        dmRender::UniformBufferBinding m_Binding;
        dmGraphics::HProgram           m_Program;
        uint64_t                       m_ProgramVersion;
        uint32_t                       m_MaterialIndex;
    };

    // Fixed std140 channel ABIs, validated by MaterialBuilder. Reflection selects
    // complete channels and their block offsets, never individual channel fields.
    struct PbrCommon
    {
        dmVMath::Vector4 m_AlphaCutoffAndDoubleSidedAndIsUnlit;
        dmVMath::Vector4 m_Textures;
        dmVMath::Vector4 m_EmissiveFactorAndStrength;
        dmVMath::Vector4 m_NormalScaleAndOcclusionStrength;
    };
    DM_STATIC_ASSERT(sizeof(PbrCommon) == 64, PbrCommonSize);

    struct PbrMetallicRoughness
    {
        dmVMath::Vector4 m_BaseColorFactor;
        dmVMath::Vector4 m_MetallicAndRoughnessFactor;
        dmVMath::Vector4 m_Textures;
    };
    DM_STATIC_ASSERT(sizeof(PbrMetallicRoughness) == 48, PbrMetallicRoughnessSize);

    struct PbrSpecularGlossiness
    {
        dmVMath::Vector4 m_DiffuseFactor;
        dmVMath::Vector4 m_SpecularAndGlossinessFactor;
        dmVMath::Vector4 m_Textures;
    };
    DM_STATIC_ASSERT(sizeof(PbrSpecularGlossiness) == 48, PbrSpecularGlossinessSize);

    struct PbrClearCoat
    {
        dmVMath::Vector4 m_Factor;
        dmVMath::Vector4 m_Textures;
    };
    DM_STATIC_ASSERT(sizeof(PbrClearCoat) == 32, PbrClearCoatSize);

    struct PbrTransmission
    {
        dmVMath::Vector4 m_Factor;
        dmVMath::Vector4 m_Textures;
    };
    DM_STATIC_ASSERT(sizeof(PbrTransmission) == 32, PbrTransmissionSize);

    struct PbrIor
    {
        dmVMath::Vector4 m_Factor;
    };
    DM_STATIC_ASSERT(sizeof(PbrIor) == 16, PbrIorSize);

    struct PbrSpecular
    {
        dmVMath::Vector4 m_Factor;
        dmVMath::Vector4 m_Textures;
    };
    DM_STATIC_ASSERT(sizeof(PbrSpecular) == 32, PbrSpecularSize);

    struct PbrVolume
    {
        dmVMath::Vector4 m_Factor;
        dmVMath::Vector4 m_Distance;
        dmVMath::Vector4 m_Textures;
    };
    DM_STATIC_ASSERT(sizeof(PbrVolume) == 48, PbrVolumeSize);

    struct PbrSheen
    {
        dmVMath::Vector4 m_Factor;
        dmVMath::Vector4 m_Textures;
    };
    DM_STATIC_ASSERT(sizeof(PbrSheen) == 32, PbrSheenSize);

    struct PbrEmissiveStrength
    {
        dmVMath::Vector4 m_Factor;
    };
    DM_STATIC_ASSERT(sizeof(PbrEmissiveStrength) == 16, PbrEmissiveStrengthSize);

    struct PbrIridescence
    {
        dmVMath::Vector4 m_Factor;
        dmVMath::Vector4 m_Textures;
    };
    DM_STATIC_ASSERT(sizeof(PbrIridescence) == 32, PbrIridescenceSize);

    struct PbrMaterialData
    {
        PbrCommon m_Common;
        PbrMetallicRoughness m_MetallicRoughness;
        PbrSpecularGlossiness m_SpecularGlossiness;
        PbrClearCoat m_ClearCoat;
        PbrTransmission m_Transmission;
        PbrIor m_Ior;
        PbrSpecular m_Specular;
        PbrVolume m_Volume;
        PbrSheen m_Sheen;
        PbrEmissiveStrength m_EmissiveStrength;
        PbrIridescence m_Iridescence;
    };

    struct PbrChannelInfo
    {
        dmhash_t m_NameHash;
        uint32_t m_DataOffset;
        uint32_t m_Size;
        int32_t  m_TextureVector;
        dmhash_t m_Constants[4];
    };

    // Globals
    static dmRigDDF::Material   g_DefaultMaterial;
    static bool                 g_DefaultMaterialInitialized = false;
    static const PbrChannelInfo g_PbrChannels[] = {
        { dmHashString64("pbrCommon"), offsetof(PbrMaterialData, m_Common), sizeof(PbrCommon), 1, { PBR_ALPHA_CUTOFF_AND_DOUBLE_SIDED_AND_IS_UNLIT, PBR_COMMON_TEXTURES, PBR_EMISSIVE_FACTOR_AND_STRENGTH, PBR_NORMAL_SCALE_AND_OCCLUSION_STRENGTH } },
        { dmHashString64("pbrMetallicRoughness"), offsetof(PbrMaterialData, m_MetallicRoughness), sizeof(PbrMetallicRoughness), 2, { PBR_METALLIC_ROUGHNESS_BASE_COLOR_FACTOR, PBR_METALLIC_ROUGHNESS_METALLIC_AND_ROUGHNESS_FACTOR, PBR_METALLIC_ROUGHNESS_TEXTURES } },
        { dmHashString64("pbrSpecularGlossiness"), offsetof(PbrMaterialData, m_SpecularGlossiness), sizeof(PbrSpecularGlossiness), 2, { PBR_SPECULAR_GLOSSINESS_DIFFUSE_FACTOR, PBR_SPECULAR_GLOSSINESS_SPECULAR_AND_SPECULAR_GLOSSINESS_FACTOR, PBR_SPECULAR_GLOSSINESS_TEXTURES } },
        { dmHashString64("pbrClearCoat"), offsetof(PbrMaterialData, m_ClearCoat), sizeof(PbrClearCoat), 1, { PBR_CLEAR_COAT_CLEAR_COAT_AND_CLEAR_COAT_ROUGHNESS_FACTOR, PBR_CLEAR_COAT_TEXTURES } },
        { dmHashString64("pbrTransmission"), offsetof(PbrMaterialData, m_Transmission), sizeof(PbrTransmission), 1, { PBR_TRANSMISSION_TRANSMISSION_FACTOR, PBR_TRANSMISSION_TEXTURES } },
        { dmHashString64("pbrIor"), offsetof(PbrMaterialData, m_Ior), sizeof(PbrIor), -1, { PBR_IOR_IOR_FACTOR } },
        { dmHashString64("pbrSpecular"), offsetof(PbrMaterialData, m_Specular), sizeof(PbrSpecular), 1, { PBR_SPECULAR_SPECULAR_COLOR_AND_SPECULAR_FACTOR, PBR_SPECULAR_TEXTURES } },
        { dmHashString64("pbrVolume"), offsetof(PbrMaterialData, m_Volume), sizeof(PbrVolume), 2, { PBR_VOLUME_THICKNESS_FACTOR_AND_ATTENUATION_COLOR, PBR_VOLUME_ATTENUATION_DISTANCE, PBR_VOLUME_TEXTURES } },
        { dmHashString64("pbrSheen"), offsetof(PbrMaterialData, m_Sheen), sizeof(PbrSheen), 1, { PBR_SHEEN_SHEEN_COLOR_AND_SHEEN_ROUGHNESS_FACTOR, PBR_SHEEN_TEXTURES } },
        { dmHashString64("pbrEmissiveStrength"), offsetof(PbrMaterialData, m_EmissiveStrength), sizeof(PbrEmissiveStrength), -1, { PBR_EMISSIVE_STRENGTH_EMISSIVE_STRENGTH } },
        { dmHashString64("pbrIridescence"), offsetof(PbrMaterialData, m_Iridescence), sizeof(PbrIridescence), 1, { PBR_IRIDESCENCE_IRIDESCENCE_FACTOR_AND_IOR_AND_THICKNESS_MIN_MAX, PBR_IRIDESCENCE_TEXTURES } },
    };

    static inline void InitDefaultPBRTextureView(dmRigDDF::TextureView& texture_view)
    {
        texture_view.m_Texture.m_Index = -1;
        texture_view.m_Texcoord = -1;
        texture_view.m_Scale = 1.0f;
        texture_view.m_Transform.m_ScaleX = 1.0f;
        texture_view.m_Transform.m_ScaleY = 1.0f;
        texture_view.m_Transform.m_Texcoord = -1;
    }

    static void InitDefaultPBRMaterial(dmRigDDF::Material& material)
    {
        memset((void*) &material, 0, sizeof(dmRigDDF::Material));

        InitDefaultPBRTextureView(material.m_Pbrmetallicroughness.m_Basecolortexture);
        InitDefaultPBRTextureView(material.m_Pbrmetallicroughness.m_Metallicroughnesstexture);
        InitDefaultPBRTextureView(material.m_Pbrspecularglossiness.m_Diffusetexture);
        InitDefaultPBRTextureView(material.m_Pbrspecularglossiness.m_Specularglossinesstexture);
        InitDefaultPBRTextureView(material.m_Clearcoat.m_Clearcoattexture);
        InitDefaultPBRTextureView(material.m_Clearcoat.m_Clearcoatroughnesstexture);
        InitDefaultPBRTextureView(material.m_Clearcoat.m_Clearcoatnormaltexture);
        InitDefaultPBRTextureView(material.m_Transmission.m_Transmissiontexture);
        InitDefaultPBRTextureView(material.m_Specular.m_Speculartexture);
        InitDefaultPBRTextureView(material.m_Specular.m_Specularcolortexture);
        InitDefaultPBRTextureView(material.m_Volume.m_Thicknesstexture);
        InitDefaultPBRTextureView(material.m_Sheen.m_Sheencolortexture);
        InitDefaultPBRTextureView(material.m_Sheen.m_Sheenroughnesstexture);
        InitDefaultPBRTextureView(material.m_Iridescence.m_Iridescencetexture);
        InitDefaultPBRTextureView(material.m_Iridescence.m_Iridescencethicknesstexture);
        InitDefaultPBRTextureView(material.m_Normaltexture);
        InitDefaultPBRTextureView(material.m_Occlusiontexture);
        InitDefaultPBRTextureView(material.m_Emissivetexture);

        material.m_Pbrmetallicroughness.m_Basecolorfactor = dmVMath::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        material.m_Pbrmetallicroughness.m_Metallicfactor = 1.0f;
        material.m_Pbrmetallicroughness.m_Roughnessfactor = 1.0f;
        material.m_Pbrspecularglossiness.m_Diffusefactor = dmVMath::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        material.m_Pbrspecularglossiness.m_Specularfactor = dmVMath::Vector3(1.0f, 1.0f, 1.0f);
        material.m_Pbrspecularglossiness.m_Glossinessfactor = 1.0f;
        material.m_Specular.m_Specularcolorfactor = dmVMath::Vector3(1.0f, 1.0f, 1.0f);
        material.m_Specular.m_Specularfactor = 1.0f;
        material.m_Volume.m_Attenuationcolor = dmVMath::Vector3(1.0f, 1.0f, 1.0f);
        material.m_Volume.m_Attenuationdistance = -1.0f;
        material.m_Emissivestrength.m_Emissivestrength = 1.0f;
        material.m_Iridescence.m_Iridescenceior = 1.3f;
        material.m_Iridescence.m_Iridescencethicknessmin = 100.0f;
        material.m_Iridescence.m_Iridescencethicknessmax = 400.0f;
        material.m_Alphacutoff = 0.5f;

        g_DefaultMaterialInitialized = true;
    }

    static const dmRigDDF::Material& GetPBRMaterial(ModelResource* resource, uint32_t material_index)
    {
        dmRigDDF::MeshSet* mesh_set = resource->m_RigScene->m_MeshSetRes->m_MeshSet;
        const dmRigDDF::Material* ddf_material_ptr = 0;

        if (material_index < mesh_set->m_Materials.m_Count)
        {
            ddf_material_ptr = &mesh_set->m_Materials[material_index];
        }
        else
        {
            // If there is no material available from the glTF file,
            // we need to use a default material so we can render at least something.
            if (!g_DefaultMaterialInitialized)
            {
                InitDefaultPBRMaterial(g_DefaultMaterial);
            }
            ddf_material_ptr = &g_DefaultMaterial;
        }

        return *ddf_material_ptr;

    }

    static void FillPBRData(const dmRigDDF::Material& ddf_material, PbrMaterialData* data)
    {
        memset(data, 0, sizeof(*data));
        {
            dmVMath::Vector4 metallic_roughness = dmVMath::Vector4(ddf_material.m_Pbrmetallicroughness.m_Metallicfactor, ddf_material.m_Pbrmetallicroughness.m_Roughnessfactor, 0.0f, 0.0f);
            data->m_MetallicRoughness.m_BaseColorFactor = ddf_material.m_Pbrmetallicroughness.m_Basecolorfactor;
            data->m_MetallicRoughness.m_MetallicAndRoughnessFactor = metallic_roughness;

            if (ddf_material.m_Pbrmetallicroughness.m_Basecolortexture.m_Texture.m_Index != -1 ||
                ddf_material.m_Pbrmetallicroughness.m_Metallicroughnesstexture.m_Texture.m_Index != -1)
            {
                dmVMath::Vector4 metallic_roughness_textures = dmVMath::Vector4(
                    ddf_material.m_Pbrmetallicroughness.m_Basecolortexture.m_Texture.m_Index != -1 ? 1.0f : 0.0f,
                    ddf_material.m_Pbrmetallicroughness.m_Metallicroughnesstexture.m_Texture.m_Index != -1 ? 1.0f : 0.0f, 0.0f, 0.0f);

                data->m_MetallicRoughness.m_Textures = metallic_roughness_textures;
            }
        }
        {
            dmVMath::Vector4 specular_glossiness = dmVMath::Vector4(ddf_material.m_Pbrspecularglossiness.m_Specularfactor.getX(), ddf_material.m_Pbrspecularglossiness.m_Specularfactor.getY(), ddf_material.m_Pbrspecularglossiness.m_Specularfactor.getZ(), ddf_material.m_Pbrspecularglossiness.m_Glossinessfactor);
            data->m_SpecularGlossiness.m_DiffuseFactor = ddf_material.m_Pbrspecularglossiness.m_Diffusefactor;
            data->m_SpecularGlossiness.m_SpecularAndGlossinessFactor = specular_glossiness;

            if (ddf_material.m_Pbrspecularglossiness.m_Diffusetexture.m_Texture.m_Index != -1 ||
                ddf_material.m_Pbrspecularglossiness.m_Specularglossinesstexture.m_Texture.m_Index != -1)
            {
                dmVMath::Vector4 specular_glossiness_textures = dmVMath::Vector4(
                    ddf_material.m_Pbrspecularglossiness.m_Diffusetexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f,
                    ddf_material.m_Pbrspecularglossiness.m_Specularglossinesstexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f, 0.0f, 0.0f);
                data->m_SpecularGlossiness.m_Textures = specular_glossiness_textures;
            }

        }
        {
            dmVMath::Vector4 clear_coat = dmVMath::Vector4(ddf_material.m_Clearcoat.m_Clearcoatfactor, ddf_material.m_Clearcoat.m_Clearcoatroughnessfactor, 0.0f, 0.0f);
            data->m_ClearCoat.m_Factor = clear_coat;

            if (ddf_material.m_Clearcoat.m_Clearcoattexture.m_Texture.m_Index != -1 ||
                ddf_material.m_Clearcoat.m_Clearcoatroughnesstexture.m_Texture.m_Index != -1 ||
                ddf_material.m_Clearcoat.m_Clearcoatnormaltexture.m_Texture.m_Index != -1)
            {
                dmVMath::Vector4 clear_coat_textures = dmVMath::Vector4(
                    ddf_material.m_Clearcoat.m_Clearcoattexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f,
                    ddf_material.m_Clearcoat.m_Clearcoatroughnesstexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f,
                    ddf_material.m_Clearcoat.m_Clearcoatnormaltexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f, 0.0f);
                data->m_ClearCoat.m_Textures = clear_coat_textures;
            }

        }
        {
            dmVMath::Vector4 transmission = dmVMath::Vector4(ddf_material.m_Transmission.m_Transmissionfactor, 0.0f, 0.0f, 0.0f);
            data->m_Transmission.m_Factor = transmission;

            if (ddf_material.m_Transmission.m_Transmissiontexture.m_Texture.m_Index != -1)
            {
                dmVMath::Vector4 transmission_textures = dmVMath::Vector4(
                    ddf_material.m_Transmission.m_Transmissiontexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
                data->m_Transmission.m_Textures = transmission_textures;
            }
        }
        {
            dmVMath::Vector4 ior = dmVMath::Vector4(ddf_material.m_Ior.m_Ior, 0.0f, 0.0f, 0.0f);
            data->m_Ior.m_Factor = ior;
        }
        {
            dmVMath::Vector4 specular = dmVMath::Vector4(ddf_material.m_Specular.m_Specularcolorfactor.getX(), ddf_material.m_Specular.m_Specularcolorfactor.getY(), ddf_material.m_Specular.m_Specularcolorfactor.getZ(), ddf_material.m_Specular.m_Specularfactor);
            data->m_Specular.m_Factor = specular;

            if (ddf_material.m_Specular.m_Speculartexture.m_Texture.m_Index != -1 ||
                ddf_material.m_Specular.m_Specularcolortexture.m_Texture.m_Index != -1)
            {
                dmVMath::Vector4 specular_textures = dmVMath::Vector4(
                    ddf_material.m_Specular.m_Speculartexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f,
                    ddf_material.m_Specular.m_Specularcolortexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f, 0.0f, 0.0f);
                data->m_Specular.m_Textures = specular_textures;
            }
        }
        {
            dmVMath::Vector4 thicknessFactorAndAttenuationColor = dmVMath::Vector4(ddf_material.m_Volume.m_Thicknessfactor, ddf_material.m_Volume.m_Attenuationcolor.getX(), ddf_material.m_Volume.m_Attenuationcolor.getY(), ddf_material.m_Volume.m_Attenuationcolor.getZ());
            dmVMath::Vector4 attenuationDistance = dmVMath::Vector4(ddf_material.m_Volume.m_Attenuationdistance, 0.0f, 0.0f, 0.0f);
            data->m_Volume.m_Factor = thicknessFactorAndAttenuationColor;
            data->m_Volume.m_Distance = attenuationDistance;

            if (ddf_material.m_Volume.m_Thicknesstexture.m_Texture.m_Index != -1)
            {
                dmVMath::Vector4 volume_textures = dmVMath::Vector4(
                    ddf_material.m_Volume.m_Thicknesstexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
                data->m_Volume.m_Textures = volume_textures;
            }
        }
        {
            dmVMath::Vector4 sheenColorAndRoughnessFactor = dmVMath::Vector4(ddf_material.m_Sheen.m_Sheencolorfactor.getX(), ddf_material.m_Sheen.m_Sheencolorfactor.getY(), ddf_material.m_Sheen.m_Sheencolorfactor.getZ(), ddf_material.m_Sheen.m_Sheenroughnessfactor);
            data->m_Sheen.m_Factor = sheenColorAndRoughnessFactor;

            if (ddf_material.m_Sheen.m_Sheencolortexture.m_Texture.m_Index != -1 ||
                ddf_material.m_Sheen.m_Sheenroughnesstexture.m_Texture.m_Index != -1)
            {
                dmVMath::Vector4 sheen_textures = dmVMath::Vector4(
                    ddf_material.m_Sheen.m_Sheencolortexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f,
                    ddf_material.m_Sheen.m_Sheenroughnesstexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f, 0.0f, 0.0f);
                data->m_Sheen.m_Textures = sheen_textures;
            }
        }
        {
            dmVMath::Vector4 emissiveFactorAndStrength = dmVMath::Vector4(ddf_material.m_Emissivestrength.m_Emissivestrength, 0.0f, 0.0f, 0.0f);
            data->m_EmissiveStrength.m_Factor = emissiveFactorAndStrength;
        }
        {
            dmVMath::Vector4 iridescenceFactorAndIorAndThicknessMinMax = dmVMath::Vector4(ddf_material.m_Iridescence.m_Iridescencefactor, ddf_material.m_Iridescence.m_Iridescenceior, ddf_material.m_Iridescence.m_Iridescencethicknessmin, ddf_material.m_Iridescence.m_Iridescencethicknessmax);
            data->m_Iridescence.m_Factor = iridescenceFactorAndIorAndThicknessMinMax;

            if (ddf_material.m_Iridescence.m_Iridescencetexture.m_Texture.m_Index != -1 ||
                ddf_material.m_Iridescence.m_Iridescencethicknesstexture.m_Texture.m_Index != -1)
            {
                dmVMath::Vector4 iridescence_textures = dmVMath::Vector4(
                    ddf_material.m_Iridescence.m_Iridescencetexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f,
                    ddf_material.m_Iridescence.m_Iridescencethicknesstexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f, 0.0f, 0.0f);
                data->m_Iridescence.m_Textures = iridescence_textures;
            }
        }
        dmVMath::Vector4 alphaCutoffAndDoubleSidedAndIsUnlit = dmVMath::Vector4(ddf_material.m_Alphacutoff, ddf_material.m_Doublesided, ddf_material.m_Unlit, (float) ddf_material.m_Alphamode);
        data->m_Common.m_AlphaCutoffAndDoubleSidedAndIsUnlit = alphaCutoffAndDoubleSidedAndIsUnlit;

        dmVMath::Vector4 emissive_factor(ddf_material.m_Emissivefactor.getX(), ddf_material.m_Emissivefactor.getY(),
                                       ddf_material.m_Emissivefactor.getZ(), ddf_material.m_Emissivestrength.m_Emissivestrength);
        data->m_Common.m_EmissiveFactorAndStrength = emissive_factor;
        // Absent texture views can contain zero-initialized importer values.
        // Use neutral defaults only when the texture is absent; an authored
        // zero scale/strength on an existing texture must remain zero.
        float normal_scale = ddf_material.m_Normaltexture.m_Texture.m_Index != -1 ? ddf_material.m_Normaltexture.m_Scale : 1.0f;
        float occlusion_strength = ddf_material.m_Occlusiontexture.m_Texture.m_Index != -1 ? ddf_material.m_Occlusiontexture.m_Scale : 1.0f;
        dmVMath::Vector4 texture_factors(normal_scale, occlusion_strength, 0.0f, 0.0f);
        data->m_Common.m_NormalScaleAndOcclusionStrength = texture_factors;

        if (ddf_material.m_Normaltexture.m_Texture.m_Index != -1 ||
            ddf_material.m_Occlusiontexture.m_Texture.m_Index != -1 ||
            ddf_material.m_Emissivetexture.m_Texture.m_Index != -1)
        {
            dmVMath::Vector4 common_textures = dmVMath::Vector4(
                ddf_material.m_Normaltexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f,
                ddf_material.m_Occlusiontexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f,
                ddf_material.m_Emissivetexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f, 0.0f);
            data->m_Common.m_Textures = common_textures;
        }

    }

    bool FillPBRConstants(ModelResource* resource, const MaterialInfo* material_info, HComponentRenderConstants* render_constants, dmRender::HMaterial material, uint32_t material_index)
    {
        dmRenderDDF::MaterialDesc::PbrParameters parameters;
        dmRender::GetMaterialPBRParameters(material, &parameters);

        if (!parameters.m_HasParameters)
        {
            return false;
        }

        PbrMaterialData data;
        FillPBRData(GetPBRMaterial(resource, material_index), &data);

        if (!*render_constants)
        {
            *render_constants = CreateRenderConstants();
        }

        const bool enabled[] = {
            true,
            parameters.m_HasMetallicRoughness,
            parameters.m_HasSpecularGlossiness,
            parameters.m_HasClearcoat,
            parameters.m_HasTransmission,
            parameters.m_HasIor,
            parameters.m_HasSpecular,
            parameters.m_HasVolume,
            parameters.m_HasSheen,
            parameters.m_HasEmissiveStrength,
            parameters.m_HasIridescence
        };

        for (uint32_t channel = 0; channel < DM_ARRAY_SIZE(g_PbrChannels); ++channel)
        {
            if (!enabled[channel])
            {
                continue;
            }

            const PbrChannelInfo& info = g_PbrChannels[channel];
            dmVMath::Vector4* values = (dmVMath::Vector4*) ((uint8_t*) &data + info.m_DataOffset);

            for (uint32_t i = 0; i < info.m_Size / sizeof(dmVMath::Vector4); ++i)
            {
                // Keep the legacy representation of absent texture flags.
                if ((int32_t) i == info.m_TextureVector && values[i].getX() == 0 && values[i].getY() == 0 && values[i].getZ() == 0)
                {
                    ClearRenderConstant(*render_constants, info.m_Constants[i]);
                }
                else
                {
                    SetRenderConstant(*render_constants, info.m_Constants[i], values + i, 1);
                }
            }
        }
        return true;
    }

    void DeleteModelMaterialBuffers(dmGraphics::HContext context, ModelResource* resource)
    {
        for (uint32_t i = 0; i < resource->m_MaterialBuffers.Size(); ++i)
        {
            ModelMaterialBuffer* buffer = resource->m_MaterialBuffers[i];
            dmGraphics::DeleteUniformBuffer(context, buffer->m_Binding.m_Buffer);
            delete buffer;
        }
        resource->m_MaterialBuffers.SetSize(0);
    }

    bool SetModelMaterialBuffer(dmGraphics::HContext context, ModelResource* resource, dmRender::HMaterial material, uint32_t material_index, dmRender::RenderObject* ro)
    {
        assert(dmGraphics::IsContextFeatureSupported(context, dmGraphics::CONTEXT_FEATURE_UNIFORM_BUFFER));
        dmRenderDDF::MaterialDesc::PbrParameters parameters;
        dmRender::GetMaterialPBRParameters(material, &parameters);

        if (!parameters.m_HasParameters)
        {
            return true;
        }

        dmGraphics::HProgram program = dmRender::GetMaterialProgram(material);
        uint64_t version = dmGraphics::GetProgramVersion(program);
        for (uint32_t i = 0; i < resource->m_MaterialBuffers.Size(); ++i)
        {
            ModelMaterialBuffer* buffer = resource->m_MaterialBuffers[i];
            if (buffer->m_Program == program && buffer->m_ProgramVersion != version)
            {
                dmGraphics::DeleteUniformBuffer(context, buffer->m_Binding.m_Buffer);
                delete buffer;
                resource->m_MaterialBuffers.EraseSwap(i--);
                continue;
            }
            if (buffer->m_ProgramVersion == version && buffer->m_MaterialIndex == material_index)
            {
                ro->m_UniformBuffers = &buffer->m_Binding;
                ro->m_UniformBufferCount = 1;
                return true;
            }
        }

        dmGraphics::UniformBufferBindingInfo layout;
        if (!dmGraphics::GetUniformBufferBinding(program, dmHashString64("PbrMaterial"), &layout))
        {
            dmLogError("PBR material has no PbrMaterial uniform block. Rebuild its shaders for this graphics context.");
            return false;
        }

        PbrMaterialData data;
        FillPBRData(GetPBRMaterial(resource, material_index), &data);
        const dmGraphics::ShaderResourceTypeInfo& root = layout.m_Types[layout.m_RootTypeIndex];

        if (root.m_MemberCount < 4 || layout.m_Size < sizeof(PbrCommon))
        {
            dmLogError("PbrMaterial must use the built-in common fields and channel structs.");
            return false;
        }

        uint8_t* bytes = (uint8_t*) calloc(1, layout.m_Size);
        memcpy(bytes, &data.m_Common, sizeof(data.m_Common));

        // Bob validates the fixed channel ABIs. Only top-level offsets vary by shader.
        for (uint32_t member_index = 4; member_index < root.m_MemberCount; ++member_index)
        {
            const dmGraphics::ShaderResourceMember& member = root.m_Members[member_index];
            const PbrChannelInfo* channel = 0;

            for (uint32_t i = 1; i < DM_ARRAY_SIZE(g_PbrChannels); ++i)
            {
                if (g_PbrChannels[i].m_NameHash == member.m_NameHash)
                {
                    channel = &g_PbrChannels[i];
                    break;
                }
            }

            if (!channel || member.m_Offset > layout.m_Size || channel->m_Size > layout.m_Size - member.m_Offset)
            {
                dmLogError("Invalid PbrMaterial channel '%s'. Rebuild the material with the current shader contract.", member.m_Name);
                free(bytes);
                return false;
            }

            memcpy(bytes + member.m_Offset, (const uint8_t*) &data + channel->m_DataOffset, channel->m_Size);
        }

        dmGraphics::HUniformBuffer ubo = dmGraphics::NewUniformBuffer(context, layout.m_Layout, layout.m_Size);
        if (!ubo)
        {
            dmLogError("Unable to allocate PbrMaterial uniform buffer (%u bytes).", layout.m_Size);
            free(bytes);
            return false;
        }
        dmGraphics::SetUniformBuffer(context, ubo, 0, layout.m_Size, bytes);
        free(bytes);

        ModelMaterialBuffer* buffer = new ModelMaterialBuffer;
        buffer->m_Binding.m_NameHash = dmHashString64("PbrMaterial");
        buffer->m_Binding.m_Buffer = ubo;
        buffer->m_Binding.m_Layout = layout.m_Layout;
        buffer->m_Binding.m_Size = layout.m_Size;
        buffer->m_Program = program;
        buffer->m_ProgramVersion = version;
        buffer->m_MaterialIndex = material_index;

        if (resource->m_MaterialBuffers.Full())
        {
            resource->m_MaterialBuffers.OffsetCapacity(4);
        }
        resource->m_MaterialBuffers.Push(buffer);

        ro->m_UniformBuffers = &buffer->m_Binding;
        ro->m_UniformBufferCount = 1;
        return true;
    }
}
