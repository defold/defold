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

#include "comp_model_pbr.h"

#include <string.h>

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

    static const dmhash_t PBR_METALLIC_ROUGHNESS_BASE_COLOR_TEXTURE_SAMPLER         = dmHashString64("PbrMetallicRoughness_baseColorTexture");
    static const dmhash_t PBR_METALLIC_ROUGHNESS_METALLIC_ROUGHNESS_TEXTURE_SAMPLER = dmHashString64("PbrMetallicRoughness_metallicRoughnessTexture");

    static bool HasModelTexture(const MaterialInfo* material_info, dmhash_t sampler_name_hash)
    {
        if (!material_info)
        {
            return false;
        }

        for (uint32_t i = 0; i < material_info->m_TexturesCount; ++i)
        {
            if (material_info->m_Textures[i].m_SamplerNameHash == sampler_name_hash && material_info->m_Textures[i].m_Texture)
            {
                return true;
            }
        }

        return false;
    }

    static void InitDefaultPBRTextureView(dmRigDDF::TextureView& texture_view)
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
    }

    bool FillPBRConstants(ModelResource* resource, const MaterialInfo* material_info, HComponentRenderConstants* render_constants, dmRender::HMaterial material, uint32_t material_index)
    {
        dmRenderDDF::MaterialDesc::PbrParameters parameters;
        dmRender::GetMaterialPBRParameters(material, &parameters);

        if (!parameters.m_HasParameters)
        {
            return false;
        }

        dmRigDDF::MeshSet* mesh_set = resource->m_RigScene->m_MeshSetRes->m_MeshSet;
        dmRigDDF::Material default_material;
        const dmRigDDF::Material* ddf_material_ptr = 0;
        bool using_default_material = false;

        if (material_index < mesh_set->m_Materials.m_Count)
        {
            ddf_material_ptr = &mesh_set->m_Materials[material_index];
        }
        else
        {
            InitDefaultPBRMaterial(default_material);
            ddf_material_ptr = &default_material;
            using_default_material = true;
        }

        const dmRigDDF::Material& ddf_material = *ddf_material_ptr;

        HComponentRenderConstants constants = *render_constants;
        if (!constants)
        {
            constants = dmGameSystem::CreateRenderConstants();
            *render_constants = constants;
        }

        if (parameters.m_HasMetallicRoughness)
        {
            /*******************************************
            * uniform sampler2D PbrMetallicRoughness_baseColorTexture;
            * uniform sampler2D PbrMetallicRoughness_metallicRoughnessTexture;
            * struct PbrMetallicRoughness
            * {
            *     vec4 baseColorFactor;
            *     vec4 metallicAndRoughnessFactor; // R: metallic (Default=1.0), G: roughness (Default=1.0)
            *     vec4 metallicRoughnessTextures;  // R: use baseColorTexture, G: use metallicRoughnessTexture
            * };
            *******************************************/
            bool has_base_color_texture = ddf_material.m_Pbrmetallicroughness.m_Basecolortexture.m_Texture.m_Index != -1;
            bool has_metallic_roughness_texture = ddf_material.m_Pbrmetallicroughness.m_Metallicroughnesstexture.m_Texture.m_Index != -1;

            if (using_default_material)
            {
                has_base_color_texture = has_base_color_texture || HasModelTexture(material_info, PBR_METALLIC_ROUGHNESS_BASE_COLOR_TEXTURE_SAMPLER);
                has_metallic_roughness_texture = has_metallic_roughness_texture || HasModelTexture(material_info, PBR_METALLIC_ROUGHNESS_METALLIC_ROUGHNESS_TEXTURE_SAMPLER);
            }

            dmVMath::Vector4 metallic_roughness = dmVMath::Vector4(ddf_material.m_Pbrmetallicroughness.m_Metallicfactor, ddf_material.m_Pbrmetallicroughness.m_Roughnessfactor, 0.0f, 0.0f);
            dmGameSystem::SetRenderConstant(constants, PBR_METALLIC_ROUGHNESS_BASE_COLOR_FACTOR, (dmVMath::Vector4*) &ddf_material.m_Pbrmetallicroughness.m_Basecolorfactor, 1);
            dmGameSystem::SetRenderConstant(constants, PBR_METALLIC_ROUGHNESS_METALLIC_AND_ROUGHNESS_FACTOR, &metallic_roughness, 1);

            if (has_base_color_texture || has_metallic_roughness_texture)
            {
                dmVMath::Vector4 metallic_roughness_textures = dmVMath::Vector4(
                    has_base_color_texture ? 1.0f : 0.0f,
                    has_metallic_roughness_texture ? 1.0f : 0.0f, 0.0f, 0.0f);

                dmGameSystem::SetRenderConstant(constants, PBR_METALLIC_ROUGHNESS_TEXTURES, &metallic_roughness_textures, 1);
            }
        }
        if (parameters.m_HasSpecularGlossiness)
        {
            /*******************************************
            * uniform sampler2D PbrSpecularGlossiness_diffuseTexture;
            * uniform sampler2D PbrSpecularGlossiness_specularGlossinessTexture;
            * struct PbrSpecularGlossiness
            * {
            *     vec4 diffuseFactor;
            *     vec4 specularAndSpecularGlossinessFactor; // RGB: specular (Default=1.0), A: glossiness (Default=1.0)
            *     vec4 specularGlossinessTextures;          // R: use diffuseTexture, G: use specularGlossinessTexture
            * };
            *******************************************/
            dmVMath::Vector4 specular_glossiness = dmVMath::Vector4(ddf_material.m_Pbrspecularglossiness.m_Specularfactor.getX(), ddf_material.m_Pbrspecularglossiness.m_Specularfactor.getY(), ddf_material.m_Pbrspecularglossiness.m_Specularfactor.getZ(), ddf_material.m_Pbrspecularglossiness.m_Glossinessfactor);
            dmGameSystem::SetRenderConstant(constants, PBR_SPECULAR_GLOSSINESS_DIFFUSE_FACTOR, (dmVMath::Vector4*) &ddf_material.m_Pbrspecularglossiness.m_Diffusefactor, 1);
            dmGameSystem::SetRenderConstant(constants, PBR_SPECULAR_GLOSSINESS_SPECULAR_AND_SPECULAR_GLOSSINESS_FACTOR, &specular_glossiness, 1);

            if (ddf_material.m_Pbrspecularglossiness.m_Diffusetexture.m_Texture.m_Index != -1 ||
                ddf_material.m_Pbrspecularglossiness.m_Specularglossinesstexture.m_Texture.m_Index != -1)
            {
                dmVMath::Vector4 specular_glossiness_textures = dmVMath::Vector4(
                    ddf_material.m_Pbrspecularglossiness.m_Diffusetexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f,
                    ddf_material.m_Pbrspecularglossiness.m_Specularglossinesstexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f, 0.0f, 0.0f);
                dmGameSystem::SetRenderConstant(constants, PBR_SPECULAR_GLOSSINESS_TEXTURES, &specular_glossiness_textures, 1);
            }

        }
        if (parameters.m_HasClearcoat)
        {
            /*******************************************
            * uniform sampler2D PbrClearcoat_clearcoatTexture;
            * uniform sampler2D PbrClearcoat_clearcoatRoughnessTexture;
            * uniform sampler2D PbrClearcoat_clearcoatNormalTexture;
            * struct PbrClearCoat
            * {
            *     vec4 clearCoatAndClearCoatRoughnessFactor; // R: clearCoat (Default=0.0), G: clearCoatRoughness (Default=0.0)
            *     vec4 clearCoatTextures;                    // R: use clearCoatTexture, G: use clearCoatRoughnessTexture, B: use clearCoatNormalTexture
            * };
            *******************************************/
            dmVMath::Vector4 clear_coat = dmVMath::Vector4(ddf_material.m_Clearcoat.m_Clearcoatfactor, ddf_material.m_Clearcoat.m_Clearcoatroughnessfactor, 0.0f, 0.0f);
            dmGameSystem::SetRenderConstant(constants, PBR_CLEAR_COAT_CLEAR_COAT_AND_CLEAR_COAT_ROUGHNESS_FACTOR, &clear_coat, 1);

            if (ddf_material.m_Clearcoat.m_Clearcoattexture.m_Texture.m_Index != -1 ||
                ddf_material.m_Clearcoat.m_Clearcoatroughnesstexture.m_Texture.m_Index != -1 ||
                ddf_material.m_Clearcoat.m_Clearcoatnormaltexture.m_Texture.m_Index != -1)
            {
                dmVMath::Vector4 clear_coat_textures = dmVMath::Vector4(
                    ddf_material.m_Clearcoat.m_Clearcoattexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f,
                    ddf_material.m_Clearcoat.m_Clearcoatroughnesstexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f,
                    ddf_material.m_Clearcoat.m_Clearcoatnormaltexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f, 0.0f);
                dmGameSystem::SetRenderConstant(constants, PBR_CLEAR_COAT_TEXTURES, &clear_coat_textures, 1);
            }

        }
        if (parameters.m_HasTransmission)
        {
            /*******************************************
            * uniform sampler2D PbrTransmission_transmissionTexture;
            * struct PbrTransmission
            * {
            *     vec4 transmissionFactor;   // R: transmission (Default=0.0)
            *     vec4 transmissionTextures; // R: use transmissionTexture
            * };
            *******************************************/
            dmVMath::Vector4 transmission = dmVMath::Vector4(ddf_material.m_Transmission.m_Transmissionfactor, 0.0f, 0.0f, 0.0f);
            dmGameSystem::SetRenderConstant(constants, PBR_TRANSMISSION_TRANSMISSION_FACTOR, &transmission, 1);

            if (ddf_material.m_Transmission.m_Transmissiontexture.m_Texture.m_Index != -1)
            {
                dmVMath::Vector4 transmission_textures = dmVMath::Vector4(
                    ddf_material.m_Transmission.m_Transmissiontexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
                dmGameSystem::SetRenderConstant(constants, PBR_TRANSMISSION_TEXTURES, &transmission_textures, 1);
            }
        }
        if (parameters.m_HasIor)
        {
            /*
            struct Ior
            {
                vec4 ior; // R: ior (Default=0.0)
            };
            */
            dmVMath::Vector4 ior = dmVMath::Vector4(ddf_material.m_Ior.m_Ior, 0.0f, 0.0f, 0.0f);
            dmGameSystem::SetRenderConstant(constants, PBR_IOR_IOR_FACTOR, &ior, 1);
        }
        if (parameters.m_HasSpecular)
        {
            /***********************************
            * uniform sampler2D PbrSpecular_specularTexture;
            * uniform sampler2D PbrSpecular_specularColorTexture;
            * struct PbrSpecular
            * {
            *     vec4 specularColorAndSpecularFactor; // RGB: specularColor, A: specularFactor (Default=1.0);
            *     vec4 specularTextures;               // R: use specularTexture, G: use specularColorTexture
            * };
            ***********************************/
            dmVMath::Vector4 specular = dmVMath::Vector4(ddf_material.m_Specular.m_Specularcolorfactor.getX(), ddf_material.m_Specular.m_Specularcolorfactor.getY(), ddf_material.m_Specular.m_Specularcolorfactor.getZ(), ddf_material.m_Specular.m_Specularfactor);
            dmGameSystem::SetRenderConstant(constants, PBR_SPECULAR_SPECULAR_COLOR_AND_SPECULAR_FACTOR, &specular, 1);

            if (ddf_material.m_Specular.m_Speculartexture.m_Texture.m_Index != -1 ||
                ddf_material.m_Specular.m_Specularcolortexture.m_Texture.m_Index != -1)
            {
                dmVMath::Vector4 specular_textures = dmVMath::Vector4(
                    ddf_material.m_Specular.m_Speculartexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f,
                    ddf_material.m_Specular.m_Specularcolortexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f, 0.0f, 0.0f);
                dmGameSystem::SetRenderConstant(constants, PBR_SPECULAR_TEXTURES, &specular_textures, 1);
            }
        }
        if (parameters.m_HasVolume)
        {
            /***********************************
            * uniform sampler2D PbrVolume_thicknessTexture;
            * struct PbrVolume
            * {
            *     vec4 thicknessFactorAndAttenuationColor; // R: thicknessFactor (Default=0.0), RGB: attenuationColor
            *     vec4 attenuationDistance;                // R: attentuationDistance (Default=-1.0)
            *     vec4 volumeTextures;                     // R: use thicknessTexture
            * };
            ***********************************/
            dmVMath::Vector4 thicknessFactorAndAttenuationColor = dmVMath::Vector4(ddf_material.m_Volume.m_Thicknessfactor, ddf_material.m_Volume.m_Attenuationcolor.getX(), ddf_material.m_Volume.m_Attenuationcolor.getY(), ddf_material.m_Volume.m_Attenuationcolor.getZ());
            dmVMath::Vector4 attenuationDistance = dmVMath::Vector4(ddf_material.m_Volume.m_Attenuationdistance, 0.0f, 0.0f, 0.0f);
            dmGameSystem::SetRenderConstant(constants, PBR_VOLUME_THICKNESS_FACTOR_AND_ATTENUATION_COLOR, &thicknessFactorAndAttenuationColor, 1);
            dmGameSystem::SetRenderConstant(constants, PBR_VOLUME_ATTENUATION_DISTANCE, &attenuationDistance, 1);

            if (ddf_material.m_Volume.m_Thicknesstexture.m_Texture.m_Index != -1)
            {
                dmVMath::Vector4 volume_textures = dmVMath::Vector4(
                    ddf_material.m_Volume.m_Thicknesstexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
                dmGameSystem::SetRenderConstant(constants, PBR_VOLUME_TEXTURES, &volume_textures, 1);
            }
        }
        if (parameters.m_HasSheen)
        {
            /***********************************
            * uniform sampler2D PbrSheen_sheenColorTexture;
            * uniform sampler2D PbrSheen_sheenRoughnessTexture;
            * struct PbrSheen
            * {
            *     vec4 sheenColorAndRoughnessFactor; // RGB: sheenColor, A: sheenRoughnessFactor (Default=0.0)
            *     vec4 sheenTextures;                // R: use sheenColorTexture, G: use sheenRoughnessTexture
            * };
            ***********************************/
            dmVMath::Vector4 sheenColorAndRoughnessFactor = dmVMath::Vector4(ddf_material.m_Sheen.m_Sheencolorfactor.getX(), ddf_material.m_Sheen.m_Sheencolorfactor.getY(), ddf_material.m_Sheen.m_Sheencolorfactor.getZ(), ddf_material.m_Sheen.m_Sheenroughnessfactor);
            dmGameSystem::SetRenderConstant(constants, PBR_SHEEN_SHEEN_COLOR_AND_SHEEN_ROUGHNESS_FACTOR, &sheenColorAndRoughnessFactor, 1);

            if (ddf_material.m_Sheen.m_Sheencolortexture.m_Texture.m_Index != -1 ||
                ddf_material.m_Sheen.m_Sheenroughnesstexture.m_Texture.m_Index != -1)
            {
                dmVMath::Vector4 sheen_textures = dmVMath::Vector4(
                    ddf_material.m_Sheen.m_Sheencolortexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f,
                    ddf_material.m_Sheen.m_Sheenroughnesstexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f, 0.0f, 0.0f);
                dmGameSystem::SetRenderConstant(constants, PBR_SHEEN_TEXTURES, &sheen_textures, 1);
            }
        }
        if (parameters.m_HasEmissiveStrength)
        {
            /***********************************
            * struct PbrEmissiveStrength
            * {
            *     vec4 emissiveStrength; // R: emissiveStrength (Default=1.0)
            * };
            ***********************************/
            dmVMath::Vector4 emissiveFactorAndStrength = dmVMath::Vector4(ddf_material.m_Emissivestrength.m_Emissivestrength, 0.0f, 0.0f, 0.0f);
            dmGameSystem::SetRenderConstant(constants, PBR_EMISSIVE_STRENGTH_EMISSIVE_STRENGTH, &emissiveFactorAndStrength, 1);
        }
        if (parameters.m_HasIridescence)
        {
            /***********************************
            * uniform sampler2D PbrEmissive_iridescenceTexture;
            * uniform sampler2D PbrEmissive_iridescenceThicknessTexture;
            * struct PbrIridescence
            * {
            *     vec4 iridescenceFactorAndIorAndThicknessMinMax; // R: iridescenceFactor (Default=0.0), G: iridescenceIor (Default=1.3), B: iridescenceThicknessMin (Default=100.0), A: iridescenceThicknessMax (Default=400.0)
            *     vec4 iridescenceTextures;                       // R: use iridescenceTexture, G: use iridescenceThicknessTexture
            * };
            ***********************************/
            dmVMath::Vector4 iridescenceFactorAndIorAndThicknessMinMax = dmVMath::Vector4(ddf_material.m_Iridescence.m_Iridescencefactor, ddf_material.m_Iridescence.m_Iridescenceior, ddf_material.m_Iridescence.m_Iridescencethicknessmin, ddf_material.m_Iridescence.m_Iridescencethicknessmax);
            dmGameSystem::SetRenderConstant(constants, PBR_IRIDESCENCE_IRIDESCENCE_FACTOR_AND_IOR_AND_THICKNESS_MIN_MAX, &iridescenceFactorAndIorAndThicknessMinMax, 1);

            if (ddf_material.m_Iridescence.m_Iridescencetexture.m_Texture.m_Index != -1 ||
                ddf_material.m_Iridescence.m_Iridescencethicknesstexture.m_Texture.m_Index != -1)
            {
                dmVMath::Vector4 iridescence_textures = dmVMath::Vector4(
                    ddf_material.m_Iridescence.m_Iridescencetexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f,
                    ddf_material.m_Iridescence.m_Iridescencethicknesstexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f, 0.0f, 0.0f);
                dmGameSystem::SetRenderConstant(constants, PBR_IRIDESCENCE_TEXTURES, &iridescence_textures, 1);
            }
        }

        /***********************************
        * vec4 pbrAlphaCutoffAndDoubleSidedAndIsUnlit; // R: alphaCutoff (Default=0.5), G: doubleSided (Default=false), B: unlit (Default=false)
        * vec4 pbrCommonTextures;                      // R: use normalTexture, G: use occlusionTexture, B: use emissiveTexture
        ***********************************/
        dmVMath::Vector4 alphaCutoffAndDoubleSidedAndIsUnlit = dmVMath::Vector4(ddf_material.m_Alphacutoff, ddf_material.m_Doublesided, ddf_material.m_Unlit, 0.0f);
        dmGameSystem::SetRenderConstant(constants, PBR_ALPHA_CUTOFF_AND_DOUBLE_SIDED_AND_IS_UNLIT, &alphaCutoffAndDoubleSidedAndIsUnlit, 1);

        if (ddf_material.m_Normaltexture.m_Texture.m_Index != -1 ||
            ddf_material.m_Occlusiontexture.m_Texture.m_Index != -1 ||
            ddf_material.m_Emissivetexture.m_Texture.m_Index != -1)
        {
            dmVMath::Vector4 common_textures = dmVMath::Vector4(
                ddf_material.m_Normaltexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f,
                ddf_material.m_Occlusiontexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f,
                ddf_material.m_Emissivetexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f, 0.0f);
            dmGameSystem::SetRenderConstant(constants, PBR_COMMON_TEXTURES, &common_textures, 1);
        }

        return true;
    }
}
