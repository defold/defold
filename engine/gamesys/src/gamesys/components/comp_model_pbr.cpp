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
    // Stores a material's uniform buffer so model instances can share it.
    // Render objects point to the binding here, so its address must stay fixed.
    // The material index tells us which material the buffer belongs to, and the
    // program version tells us when a shader reload requires a new buffer.
    struct ModelMaterialBuffer
    {
        dmRender::UniformBufferBinding m_Binding;
        uint64_t                       m_ProgramVersion;
        dmGraphics::HProgram           m_Program;
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
        PbrCommon             m_Common;
        PbrMetallicRoughness  m_MetallicRoughness;
        PbrSpecularGlossiness m_SpecularGlossiness;
        PbrVolume             m_Volume;
        PbrClearCoat          m_ClearCoat;
        PbrTransmission       m_Transmission;
        PbrSpecular           m_Specular;
        PbrSheen              m_Sheen;
        PbrIridescence        m_Iridescence;
        PbrIor                m_Ior;
        PbrEmissiveStrength   m_EmissiveStrength;
    };

    struct PbrChannelInfo
    {
        dmhash_t m_Constants[4];
        dmhash_t m_NameHash;
        uint32_t m_DataOffset;
        uint32_t m_Size;
        int32_t  m_TextureVector;
    };

    struct ModelPBRContext
    {
        dmRigDDF::Material m_DefaultMaterial;
        PbrChannelInfo     m_Channels[11];
        dmhash_t           m_MaterialNameHash;
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

    static ModelPBRContext InitModelPBRContext()
    {
        ModelPBRContext context;
        memset((void*) &context, 0, sizeof(context));
        context.m_MaterialNameHash = dmHashString64("PbrMaterial");

        PbrChannelInfo& common = context.m_Channels[0];
        common.m_Constants[0] = dmHashString64("pbrAlphaCutoffAndDoubleSidedAndIsUnlit");
        common.m_Constants[1] = dmHashString64("pbrCommonTextures");
        common.m_Constants[2] = dmHashString64("pbrEmissiveFactorAndStrength");
        common.m_Constants[3] = dmHashString64("pbrNormalScaleAndOcclusionStrength");
        common.m_NameHash = dmHashString64("pbrCommon");
        common.m_DataOffset = offsetof(PbrMaterialData, m_Common);
        common.m_Size = sizeof(PbrCommon);
        common.m_TextureVector = 1;

        PbrChannelInfo& metallic_roughness = context.m_Channels[1];
        metallic_roughness.m_Constants[0] = dmHashString64("pbrMetallicRoughness.baseColorFactor");
        metallic_roughness.m_Constants[1] = dmHashString64("pbrMetallicRoughness.metallicAndRoughnessFactor");
        metallic_roughness.m_Constants[2] = dmHashString64("pbrMetallicRoughness.metallicRoughnessTextures");
        metallic_roughness.m_NameHash = dmHashString64("pbrMetallicRoughness");
        metallic_roughness.m_DataOffset = offsetof(PbrMaterialData, m_MetallicRoughness);
        metallic_roughness.m_Size = sizeof(PbrMetallicRoughness);
        metallic_roughness.m_TextureVector = 2;

        PbrChannelInfo& specular_glossiness = context.m_Channels[2];
        specular_glossiness.m_Constants[0] = dmHashString64("pbrSpecularGlossiness.diffuseFactor");
        specular_glossiness.m_Constants[1] = dmHashString64("pbrSpecularGlossiness.specularAndSpecularGlossinessFactor");
        specular_glossiness.m_Constants[2] = dmHashString64("pbrSpecularGlossiness.specularGlossinessTextures");
        specular_glossiness.m_NameHash = dmHashString64("pbrSpecularGlossiness");
        specular_glossiness.m_DataOffset = offsetof(PbrMaterialData, m_SpecularGlossiness);
        specular_glossiness.m_Size = sizeof(PbrSpecularGlossiness);
        specular_glossiness.m_TextureVector = 2;

        PbrChannelInfo& clear_coat = context.m_Channels[3];
        clear_coat.m_Constants[0] = dmHashString64("pbrClearCoat.clearCoatAndClearCoatRoughnessFactor");
        clear_coat.m_Constants[1] = dmHashString64("pbrClearCoat.clearCoatTextures");
        clear_coat.m_NameHash = dmHashString64("pbrClearCoat");
        clear_coat.m_DataOffset = offsetof(PbrMaterialData, m_ClearCoat);
        clear_coat.m_Size = sizeof(PbrClearCoat);
        clear_coat.m_TextureVector = 1;

        PbrChannelInfo& transmission = context.m_Channels[4];
        transmission.m_Constants[0] = dmHashString64("pbrTransmission.transmissionFactor");
        transmission.m_Constants[1] = dmHashString64("pbrTransmission.transmissionTextures");
        transmission.m_NameHash = dmHashString64("pbrTransmission");
        transmission.m_DataOffset = offsetof(PbrMaterialData, m_Transmission);
        transmission.m_Size = sizeof(PbrTransmission);
        transmission.m_TextureVector = 1;

        PbrChannelInfo& ior = context.m_Channels[5];
        ior.m_Constants[0] = dmHashString64("pbrIor.ior");
        ior.m_NameHash = dmHashString64("pbrIor");
        ior.m_DataOffset = offsetof(PbrMaterialData, m_Ior);
        ior.m_Size = sizeof(PbrIor);
        ior.m_TextureVector = -1;

        PbrChannelInfo& specular = context.m_Channels[6];
        specular.m_Constants[0] = dmHashString64("pbrSpecular.specularColorAndSpecularFactor");
        specular.m_Constants[1] = dmHashString64("pbrSpecular.specularTextures");
        specular.m_NameHash = dmHashString64("pbrSpecular");
        specular.m_DataOffset = offsetof(PbrMaterialData, m_Specular);
        specular.m_Size = sizeof(PbrSpecular);
        specular.m_TextureVector = 1;

        PbrChannelInfo& volume = context.m_Channels[7];
        volume.m_Constants[0] = dmHashString64("pbrVolume.thicknessFactorAndAttenuationColor");
        volume.m_Constants[1] = dmHashString64("pbrVolume.attenuationDistance");
        volume.m_Constants[2] = dmHashString64("pbrVolume.volumeTextures");
        volume.m_NameHash = dmHashString64("pbrVolume");
        volume.m_DataOffset = offsetof(PbrMaterialData, m_Volume);
        volume.m_Size = sizeof(PbrVolume);
        volume.m_TextureVector = 2;

        PbrChannelInfo& sheen = context.m_Channels[8];
        sheen.m_Constants[0] = dmHashString64("pbrSheen.sheenColorAndRoughnessFactor");
        sheen.m_Constants[1] = dmHashString64("pbrSheen.sheenTextures");
        sheen.m_NameHash = dmHashString64("pbrSheen");
        sheen.m_DataOffset = offsetof(PbrMaterialData, m_Sheen);
        sheen.m_Size = sizeof(PbrSheen);
        sheen.m_TextureVector = 1;

        PbrChannelInfo& emissive_strength = context.m_Channels[9];
        emissive_strength.m_Constants[0] = dmHashString64("pbrEmissiveStrength.emissiveStrength");
        emissive_strength.m_NameHash = dmHashString64("pbrEmissiveStrength");
        emissive_strength.m_DataOffset = offsetof(PbrMaterialData, m_EmissiveStrength);
        emissive_strength.m_Size = sizeof(PbrEmissiveStrength);
        emissive_strength.m_TextureVector = -1;

        PbrChannelInfo& iridescence = context.m_Channels[10];
        iridescence.m_Constants[0] = dmHashString64("pbrIridescence.iridescenceFactorAndIorAndThicknessMinMax");
        iridescence.m_Constants[1] = dmHashString64("pbrIridescence.iridescenceTextures");
        iridescence.m_NameHash = dmHashString64("pbrIridescence");
        iridescence.m_DataOffset = offsetof(PbrMaterialData, m_Iridescence);
        iridescence.m_Size = sizeof(PbrIridescence);
        iridescence.m_TextureVector = 1;

        dmRigDDF::Material& material = context.m_DefaultMaterial;

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

        return context;
    }

    static const ModelPBRContext& GetModelPBRContext()
    {
        static const ModelPBRContext context = InitModelPBRContext();
        return context;
    }

    static const dmRigDDF::Material& GetPBRMaterial(const ModelPBRContext& context, ModelResource* resource, uint32_t material_index)
    {
        const dmRigDDF::MeshSet* mesh_set = resource->m_RigScene->m_MeshSetRes->m_MeshSet;
        if (material_index < mesh_set->m_Materials.m_Count)
        {
            return mesh_set->m_Materials[material_index];
        }

        // Models without imported material data use neutral PBR defaults.
        return context.m_DefaultMaterial;
    }

    static void FillPBRData(const dmRigDDF::Material& ddf_material, PbrMaterialData* data)
    {
        memset(data, 0, sizeof(*data));
        data->m_MetallicRoughness.m_BaseColorFactor = ddf_material.m_Pbrmetallicroughness.m_Basecolorfactor;
        data->m_MetallicRoughness.m_MetallicAndRoughnessFactor = dmVMath::Vector4(ddf_material.m_Pbrmetallicroughness.m_Metallicfactor, ddf_material.m_Pbrmetallicroughness.m_Roughnessfactor, 0.0f, 0.0f);
        data->m_MetallicRoughness.m_Textures = dmVMath::Vector4(
            ddf_material.m_Pbrmetallicroughness.m_Basecolortexture.m_Texture.m_Index != -1 ? 1.0f : 0.0f,
            ddf_material.m_Pbrmetallicroughness.m_Metallicroughnesstexture.m_Texture.m_Index != -1 ? 1.0f : 0.0f, 0.0f, 0.0f);

        data->m_SpecularGlossiness.m_DiffuseFactor = ddf_material.m_Pbrspecularglossiness.m_Diffusefactor;
        data->m_SpecularGlossiness.m_SpecularAndGlossinessFactor = dmVMath::Vector4(ddf_material.m_Pbrspecularglossiness.m_Specularfactor.getX(), ddf_material.m_Pbrspecularglossiness.m_Specularfactor.getY(), ddf_material.m_Pbrspecularglossiness.m_Specularfactor.getZ(), ddf_material.m_Pbrspecularglossiness.m_Glossinessfactor);
        data->m_SpecularGlossiness.m_Textures = dmVMath::Vector4(
            ddf_material.m_Pbrspecularglossiness.m_Diffusetexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f,
            ddf_material.m_Pbrspecularglossiness.m_Specularglossinesstexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f, 0.0f, 0.0f);

        data->m_ClearCoat.m_Factor = dmVMath::Vector4(ddf_material.m_Clearcoat.m_Clearcoatfactor, ddf_material.m_Clearcoat.m_Clearcoatroughnessfactor, 0.0f, 0.0f);
        data->m_ClearCoat.m_Textures = dmVMath::Vector4(
            ddf_material.m_Clearcoat.m_Clearcoattexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f,
            ddf_material.m_Clearcoat.m_Clearcoatroughnesstexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f,
            ddf_material.m_Clearcoat.m_Clearcoatnormaltexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f, 0.0f);

        data->m_Transmission.m_Factor = dmVMath::Vector4(ddf_material.m_Transmission.m_Transmissionfactor, 0.0f, 0.0f, 0.0f);
        data->m_Transmission.m_Textures = dmVMath::Vector4(
            ddf_material.m_Transmission.m_Transmissiontexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);

        data->m_Ior.m_Factor = dmVMath::Vector4(ddf_material.m_Ior.m_Ior, 0.0f, 0.0f, 0.0f);

        data->m_Specular.m_Factor = dmVMath::Vector4(ddf_material.m_Specular.m_Specularcolorfactor.getX(), ddf_material.m_Specular.m_Specularcolorfactor.getY(), ddf_material.m_Specular.m_Specularcolorfactor.getZ(), ddf_material.m_Specular.m_Specularfactor);
        data->m_Specular.m_Textures = dmVMath::Vector4(
            ddf_material.m_Specular.m_Speculartexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f,
            ddf_material.m_Specular.m_Specularcolortexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f, 0.0f, 0.0f);

        data->m_Volume.m_Factor = dmVMath::Vector4(ddf_material.m_Volume.m_Thicknessfactor, ddf_material.m_Volume.m_Attenuationcolor.getX(), ddf_material.m_Volume.m_Attenuationcolor.getY(), ddf_material.m_Volume.m_Attenuationcolor.getZ());
        data->m_Volume.m_Distance = dmVMath::Vector4(ddf_material.m_Volume.m_Attenuationdistance, 0.0f, 0.0f, 0.0f);
        data->m_Volume.m_Textures = dmVMath::Vector4(
            ddf_material.m_Volume.m_Thicknesstexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);

        data->m_Sheen.m_Factor = dmVMath::Vector4(ddf_material.m_Sheen.m_Sheencolorfactor.getX(), ddf_material.m_Sheen.m_Sheencolorfactor.getY(), ddf_material.m_Sheen.m_Sheencolorfactor.getZ(), ddf_material.m_Sheen.m_Sheenroughnessfactor);
        data->m_Sheen.m_Textures = dmVMath::Vector4(
            ddf_material.m_Sheen.m_Sheencolortexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f,
            ddf_material.m_Sheen.m_Sheenroughnesstexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f, 0.0f, 0.0f);

        data->m_EmissiveStrength.m_Factor = dmVMath::Vector4(ddf_material.m_Emissivestrength.m_Emissivestrength, 0.0f, 0.0f, 0.0f);

        data->m_Iridescence.m_Factor = dmVMath::Vector4(ddf_material.m_Iridescence.m_Iridescencefactor, ddf_material.m_Iridescence.m_Iridescenceior, ddf_material.m_Iridescence.m_Iridescencethicknessmin, ddf_material.m_Iridescence.m_Iridescencethicknessmax);
        data->m_Iridescence.m_Textures = dmVMath::Vector4(
            ddf_material.m_Iridescence.m_Iridescencetexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f,
            ddf_material.m_Iridescence.m_Iridescencethicknesstexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f, 0.0f, 0.0f);

        data->m_Common.m_AlphaCutoffAndDoubleSidedAndIsUnlit = dmVMath::Vector4(ddf_material.m_Alphacutoff, ddf_material.m_Doublesided, ddf_material.m_Unlit, (float) ddf_material.m_Alphamode);
        data->m_Common.m_EmissiveFactorAndStrength = dmVMath::Vector4(ddf_material.m_Emissivefactor.getX(), ddf_material.m_Emissivefactor.getY(),
                                                                      ddf_material.m_Emissivefactor.getZ(), ddf_material.m_Emissivestrength.m_Emissivestrength);
        // Absent texture views can contain zero-initialized importer values.
        // Use neutral defaults only when the texture is absent; an authored
        // zero scale/strength on an existing texture must remain zero.
        float normal_scale = ddf_material.m_Normaltexture.m_Texture.m_Index != -1 ? ddf_material.m_Normaltexture.m_Scale : 1.0f;
        float occlusion_strength = ddf_material.m_Occlusiontexture.m_Texture.m_Index != -1 ? ddf_material.m_Occlusiontexture.m_Scale : 1.0f;
        data->m_Common.m_NormalScaleAndOcclusionStrength = dmVMath::Vector4(normal_scale, occlusion_strength, 0.0f, 0.0f);
        data->m_Common.m_Textures = dmVMath::Vector4(
            ddf_material.m_Normaltexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f,
            ddf_material.m_Occlusiontexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f,
            ddf_material.m_Emissivetexture.m_Texture.m_Index > -1 ? 1.0f : 0.0f, 0.0f);
    }

    bool FillPBRConstants(ModelResource* resource, HComponentRenderConstants* render_constants, dmRender::HMaterial material, uint32_t material_index)
    {
        dmRenderDDF::MaterialDesc::PbrParameters parameters;
        dmRender::GetMaterialPBRParameters(material, &parameters);

        if (!parameters.m_HasParameters)
        {
            return false;
        }

        const ModelPBRContext& pbr_context = GetModelPBRContext();
        PbrMaterialData data;
        FillPBRData(GetPBRMaterial(pbr_context, resource, material_index), &data);

        if (!*render_constants)
        {
            *render_constants = CreateRenderConstants();
        }

        const bool enabled[] = {
            true, // The common data will always be set.
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

        for (uint32_t channel = 0; channel < DM_ARRAY_SIZE(pbr_context.m_Channels); ++channel)
        {
            if (!enabled[channel])
            {
                continue;
            }

            const PbrChannelInfo& info = pbr_context.m_Channels[channel];
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

    static void DeleteModelMaterialBuffer(dmGraphics::HContext context, ModelMaterialBuffer* buffer)
    {
        dmGraphics::DeleteUniformBuffer(context, buffer->m_Binding.m_Buffer);
        delete buffer;
    }

    void DeleteModelMaterialBuffers(dmGraphics::HContext context, ModelResource* resource)
    {
        for (uint32_t i = 0; i < resource->m_MaterialBuffers.Size(); ++i)
        {
            DeleteModelMaterialBuffer(context, resource->m_MaterialBuffers[i]);
        }
        resource->m_MaterialBuffers.SetSize(0);
    }

    static bool ValidatePBRMembers(const dmGraphics::ShaderResourceTypeInfo& type, const PbrChannelInfo& channel, const char* prefix)
    {
        uint32_t count = channel.m_Size / sizeof(dmVMath::Vector4);
        if (type.m_MemberCount < count)
        {
            return false;
        }
        for (uint32_t i = 0; i < count; ++i)
        {
            const dmGraphics::ShaderResourceMember& member = type.m_Members[i];
            dmhash_t name_hash = member.m_NameHash;
            if (prefix)
            {
                HashState64 hash;
                dmHashInit64(&hash, false);
                dmHashUpdateBuffer64(&hash, prefix, strlen(prefix));
                dmHashUpdateBuffer64(&hash, ".", 1);
                dmHashUpdateBuffer64(&hash, member.m_Name, strlen(member.m_Name));
                name_hash = dmHashFinal64(&hash);
            }
            if (name_hash != channel.m_Constants[i] || member.m_Type.m_UseTypeIndex ||
                member.m_Type.m_ShaderType != dmGraphics::ShaderDesc::SHADER_TYPE_VEC4 ||
                member.m_ElementCount > 1 || member.m_Offset != i * sizeof(dmVMath::Vector4))
            {
                return false;
            }
        }
        return true;
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

        if (!parameters.m_HasFixedLayout)
        {
            return false;
        }

        dmGraphics::HProgram program = dmRender::GetMaterialProgram(material);
        uint64_t version = dmGraphics::GetProgramVersion(program);
        for (uint32_t i = 0; i < resource->m_MaterialBuffers.Size(); ++i)
        {
            ModelMaterialBuffer* buffer = resource->m_MaterialBuffers[i];
            if (buffer->m_Program == program && buffer->m_ProgramVersion != version)
            {
                DeleteModelMaterialBuffer(context, buffer);
                resource->m_MaterialBuffers.EraseSwap(i--);
                continue;
            }
            if (buffer->m_ProgramVersion == version && buffer->m_MaterialIndex == material_index)
            {
                ro->m_UniformBuffers[0] = &buffer->m_Binding;
                return true;
            }
        }

        const ModelPBRContext& pbr_context = GetModelPBRContext();
        dmGraphics::UniformBufferBindingInfo layout;
        if (!dmGraphics::GetUniformBufferBinding(program, pbr_context.m_MaterialNameHash, &layout))
        {
            dmLogError("PBR material has no PbrMaterial uniform block. Rebuild its shaders for this graphics context.");
            return false;
        }

        PbrMaterialData data;
        FillPBRData(GetPBRMaterial(pbr_context, resource, material_index), &data);
        const dmGraphics::ShaderResourceTypeInfo& root = layout.m_Types[layout.m_RootTypeIndex];

        const uint32_t common_member_count = sizeof(PbrCommon) / sizeof(dmVMath::Vector4);
        // A shader can reload without its material. The compiled fixed-layout flag
        // therefore cannot validate the current program's common fields or channels.
        if (layout.m_Size < sizeof(PbrCommon) || !ValidatePBRMembers(root, pbr_context.m_Channels[0], 0))
        {
            dmLogError("PbrMaterial common layout changed. Rebuild and reload the material; skipping draw.");
            return false;
        }

        uint8_t* bytes = (uint8_t*) calloc(1, layout.m_Size);
        memcpy(bytes, &data.m_Common, sizeof(data.m_Common));

        uint32_t channel_offsets[DM_ARRAY_SIZE(pbr_context.m_Channels)] = {};
        uint32_t channels = 0;
        // Whole channels may move, but their internal layouts must still match the native structs.
        for (uint32_t member_index = common_member_count; member_index < root.m_MemberCount; ++member_index)
        {
            const dmGraphics::ShaderResourceMember& member = root.m_Members[member_index];
            const PbrChannelInfo* channel = 0;
            uint32_t channel_index = 0;

            for (uint32_t i = 1; i < DM_ARRAY_SIZE(pbr_context.m_Channels); ++i)
            {
                if (pbr_context.m_Channels[i].m_NameHash == member.m_NameHash)
                {
                    channel = &pbr_context.m_Channels[i];
                    channel_index = i;
                    break;
                }
            }

            if (!channel || !member.m_Type.m_UseTypeIndex || member.m_ElementCount > 1 ||
                (channels & (1u << channel_index)) || member.m_Offset < sizeof(PbrCommon) ||
                member.m_Offset % sizeof(dmVMath::Vector4) != 0 || member.m_Offset > layout.m_Size ||
                channel->m_Size > layout.m_Size - member.m_Offset ||
                layout.m_Types[member.m_Type.m_TypeIndex].m_MemberCount != channel->m_Size / sizeof(dmVMath::Vector4) ||
                !ValidatePBRMembers(layout.m_Types[member.m_Type.m_TypeIndex], *channel, member.m_Name))
            {
                dmLogError("PbrMaterial channel '%s' layout changed. Rebuild and reload the material; skipping draw.", member.m_Name);
                free(bytes);
                return false;
            }

            for (uint32_t i = 1; i < DM_ARRAY_SIZE(pbr_context.m_Channels); ++i)
            {
                if ((channels & (1u << i)) && member.m_Offset < channel_offsets[i] + pbr_context.m_Channels[i].m_Size &&
                    member.m_Offset + channel->m_Size > channel_offsets[i])
                {
                    dmLogError("PbrMaterial channels overlap. Rebuild and reload the material; skipping draw.");
                    free(bytes);
                    return false;
                }
            }
            channels |= 1u << channel_index;
            channel_offsets[channel_index] = member.m_Offset;
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
        buffer->m_Binding.m_NameHash = pbr_context.m_MaterialNameHash;
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

        ro->m_UniformBuffers[0] = &buffer->m_Binding;
        return true;
    }
}
