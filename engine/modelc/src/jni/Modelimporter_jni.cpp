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

// generated, do not edit

#include <jni.h>

#include "Modelimporter_jni.h"
#include <jni/jni_util.h>
#include <dlib/array.h>

#define CLASS_NAME_FORMAT "com/dynamo/bob/pipeline/Modelimporter$%s"

namespace dmModelImporter {
namespace jni {
void InitializeJNITypes(JNIEnv* env, TypeInfos* infos) {
    #define SETUP_CLASS(TYPE, TYPE_NAME) \
        TYPE * obj = &infos->m_ ## TYPE ; \
        obj->cls = dmJNI::GetClass(env, CLASS_NAME, TYPE_NAME); \
        if (!obj->cls) { \
            printf("ERROR: Failed to get class %s$%s\n", CLASS_NAME, TYPE_NAME); \
        }

    #define GET_FLD_TYPESTR(NAME, FULL_TYPE_STR) \
        obj-> NAME = dmJNI::GetFieldFromString(env, obj->cls, # NAME, FULL_TYPE_STR);

    #define GET_FLD(NAME, TYPE_NAME) \
        obj-> NAME = dmJNI::GetFieldFromString(env, obj->cls, # NAME, "L" CLASS_NAME "$" TYPE_NAME ";")

    #define GET_FLD_ARRAY(NAME, TYPE_NAME) \
        obj-> NAME = dmJNI::GetFieldFromString(env, obj->cls, # NAME, "[L" CLASS_NAME "$" TYPE_NAME ";")

    {
        SETUP_CLASS(Vector3JNI, "Vector3");
        GET_FLD_TYPESTR(x, "F");
        GET_FLD_TYPESTR(y, "F");
        GET_FLD_TYPESTR(z, "F");
    }
    {
        SETUP_CLASS(QuatJNI, "Quat");
        GET_FLD_TYPESTR(x, "F");
        GET_FLD_TYPESTR(y, "F");
        GET_FLD_TYPESTR(z, "F");
        GET_FLD_TYPESTR(w, "F");
    }
    {
        SETUP_CLASS(TransformJNI, "Transform");
        GET_FLD(rotation, "Quat");
        GET_FLD(translation, "Vector3");
        GET_FLD(scale, "Vector3");
    }
    {
        SETUP_CLASS(AabbJNI, "Aabb");
        GET_FLD(min, "Vector3");
        GET_FLD(max, "Vector3");
    }
    {
        SETUP_CLASS(ImageJNI, "Image");
        GET_FLD_TYPESTR(name, "Ljava/lang/String;");
        GET_FLD_TYPESTR(uri, "Ljava/lang/String;");
        GET_FLD_TYPESTR(mimeType, "Ljava/lang/String;");
        GET_FLD(buffer, "Buffer");
        GET_FLD_TYPESTR(index, "I");
    }
    {
        SETUP_CLASS(SamplerJNI, "Sampler");
        GET_FLD_TYPESTR(name, "Ljava/lang/String;");
        GET_FLD_TYPESTR(index, "I");
        GET_FLD_TYPESTR(magFilter, "I");
        GET_FLD_TYPESTR(minFilter, "I");
        GET_FLD_TYPESTR(wrapS, "I");
        GET_FLD_TYPESTR(wrapT, "I");
    }
    {
        SETUP_CLASS(TextureJNI, "Texture");
        GET_FLD_TYPESTR(name, "Ljava/lang/String;");
        GET_FLD(image, "Image");
        GET_FLD(sampler, "Sampler");
        GET_FLD(basisuImage, "Image");
        GET_FLD_TYPESTR(index, "I");
    }
    {
        SETUP_CLASS(TextureTransformJNI, "TextureTransform");
        GET_FLD_TYPESTR(offset, "[F");
        GET_FLD_TYPESTR(rotation, "F");
        GET_FLD_TYPESTR(scale, "[F");
        GET_FLD_TYPESTR(texcoord, "I");
    }
    {
        SETUP_CLASS(TextureViewJNI, "TextureView");
        GET_FLD(texture, "Texture");
        GET_FLD_TYPESTR(texcoord, "I");
        GET_FLD_TYPESTR(scale, "F");
        GET_FLD_TYPESTR(hasTransform, "Z");
        GET_FLD(transform, "TextureTransform");
    }
    {
        SETUP_CLASS(PbrMetallicRoughnessJNI, "PbrMetallicRoughness");
        GET_FLD(baseColorTexture, "TextureView");
        GET_FLD(metallicRoughnessTexture, "TextureView");
        GET_FLD_TYPESTR(baseColorFactor, "[F");
        GET_FLD_TYPESTR(metallicFactor, "F");
        GET_FLD_TYPESTR(roughnessFactor, "F");
    }
    {
        SETUP_CLASS(PbrSpecularGlossinessJNI, "PbrSpecularGlossiness");
        GET_FLD(diffuseTexture, "TextureView");
        GET_FLD(specularGlossinessTexture, "TextureView");
        GET_FLD_TYPESTR(diffuseFactor, "[F");
        GET_FLD_TYPESTR(specularFactor, "[F");
        GET_FLD_TYPESTR(glossinessFactor, "F");
    }
    {
        SETUP_CLASS(ClearcoatJNI, "Clearcoat");
        GET_FLD(clearcoatTexture, "TextureView");
        GET_FLD(clearcoatRoughnessTexture, "TextureView");
        GET_FLD(clearcoatNormalTexture, "TextureView");
        GET_FLD_TYPESTR(clearcoatFactor, "F");
        GET_FLD_TYPESTR(clearcoatRoughnessFactor, "F");
    }
    {
        SETUP_CLASS(TransmissionJNI, "Transmission");
        GET_FLD(transmissionTexture, "TextureView");
        GET_FLD_TYPESTR(transmissionFactor, "F");
    }
    {
        SETUP_CLASS(IorJNI, "Ior");
        GET_FLD_TYPESTR(ior, "F");
    }
    {
        SETUP_CLASS(SpecularJNI, "Specular");
        GET_FLD(specularTexture, "TextureView");
        GET_FLD(specularColorTexture, "TextureView");
        GET_FLD_TYPESTR(specularColorFactor, "[F");
        GET_FLD_TYPESTR(specularFactor, "F");
    }
    {
        SETUP_CLASS(VolumeJNI, "Volume");
        GET_FLD(thicknessTexture, "TextureView");
        GET_FLD_TYPESTR(thicknessFactor, "F");
        GET_FLD_TYPESTR(attenuationColor, "[F");
        GET_FLD_TYPESTR(attenuationDistance, "F");
    }
    {
        SETUP_CLASS(SheenJNI, "Sheen");
        GET_FLD(sheenColorTexture, "TextureView");
        GET_FLD(sheenRoughnessTexture, "TextureView");
        GET_FLD_TYPESTR(sheenColorFactor, "[F");
        GET_FLD_TYPESTR(sheenRoughnessFactor, "F");
    }
    {
        SETUP_CLASS(EmissiveStrengthJNI, "EmissiveStrength");
        GET_FLD_TYPESTR(emissiveStrength, "F");
    }
    {
        SETUP_CLASS(IridescenceJNI, "Iridescence");
        GET_FLD_TYPESTR(iridescenceFactor, "F");
        GET_FLD(iridescenceTexture, "TextureView");
        GET_FLD_TYPESTR(iridescenceIor, "F");
        GET_FLD_TYPESTR(iridescenceThicknessMin, "F");
        GET_FLD_TYPESTR(iridescenceThicknessMax, "F");
        GET_FLD(iridescenceThicknessTexture, "TextureView");
    }
    {
        SETUP_CLASS(MaterialJNI, "Material");
        GET_FLD_TYPESTR(name, "Ljava/lang/String;");
        GET_FLD_TYPESTR(index, "I");
        GET_FLD_TYPESTR(isSkinned, "B");
        GET_FLD(pbrMetallicRoughness, "PbrMetallicRoughness");
        GET_FLD(pbrSpecularGlossiness, "PbrSpecularGlossiness");
        GET_FLD(clearcoat, "Clearcoat");
        GET_FLD(ior, "Ior");
        GET_FLD(specular, "Specular");
        GET_FLD(sheen, "Sheen");
        GET_FLD(transmission, "Transmission");
        GET_FLD(volume, "Volume");
        GET_FLD(emissiveStrength, "EmissiveStrength");
        GET_FLD(iridescence, "Iridescence");
        GET_FLD(normalTexture, "TextureView");
        GET_FLD(occlusionTexture, "TextureView");
        GET_FLD(emissiveTexture, "TextureView");
        GET_FLD_TYPESTR(emissiveFactor, "[F");
        GET_FLD_TYPESTR(alphaCutoff, "F");
        GET_FLD(alphaMode, "AlphaMode");
        GET_FLD_TYPESTR(doubleSided, "Z");
        GET_FLD_TYPESTR(unlit, "Z");
    }
    {
        SETUP_CLASS(MeshJNI, "Mesh");
        GET_FLD_TYPESTR(name, "Ljava/lang/String;");
        GET_FLD(material, "Material");
        GET_FLD_TYPESTR(positions, "[F");
        GET_FLD_TYPESTR(normals, "[F");
        GET_FLD_TYPESTR(tangents, "[F");
        GET_FLD_TYPESTR(colors, "[F");
        GET_FLD_TYPESTR(weights, "[F");
        GET_FLD_TYPESTR(bones, "[I");
        GET_FLD_TYPESTR(texCoords0NumComponents, "I");
        GET_FLD_TYPESTR(texCoords0, "[F");
        GET_FLD_TYPESTR(texCoords1NumComponents, "I");
        GET_FLD_TYPESTR(texCoords1, "[F");
        GET_FLD(aabb, "Aabb");
        GET_FLD_TYPESTR(indices, "[I");
        GET_FLD_TYPESTR(vertexCount, "I");
    }
    {
        SETUP_CLASS(ModelJNI, "Model");
        GET_FLD_TYPESTR(name, "Ljava/lang/String;");
        GET_FLD_ARRAY(meshes, "Mesh");
        GET_FLD_TYPESTR(index, "I");
        GET_FLD(parentBone, "Bone");
    }
    {
        SETUP_CLASS(BoneJNI, "Bone");
        GET_FLD(invBindPose, "Transform");
        GET_FLD_TYPESTR(name, "Ljava/lang/String;");
        GET_FLD(node, "Node");
        GET_FLD(parent, "Bone");
        GET_FLD_TYPESTR(parentIndex, "I");
        GET_FLD_TYPESTR(index, "I");
        GET_FLD_ARRAY(children, "Bone");
    }
    {
        SETUP_CLASS(SkinJNI, "Skin");
        GET_FLD_TYPESTR(name, "Ljava/lang/String;");
        GET_FLD_ARRAY(bones, "Bone");
        GET_FLD_TYPESTR(index, "I");
        GET_FLD_TYPESTR(boneRemap, "[I");
    }
    {
        SETUP_CLASS(NodeJNI, "Node");
        GET_FLD(local, "Transform");
        GET_FLD(world, "Transform");
        GET_FLD_TYPESTR(name, "Ljava/lang/String;");
        GET_FLD(model, "Model");
        GET_FLD(skin, "Skin");
        GET_FLD(parent, "Node");
        GET_FLD_ARRAY(children, "Node");
        GET_FLD_TYPESTR(index, "I");
        GET_FLD_TYPESTR(nameHash, "J");
    }
    {
        SETUP_CLASS(KeyFrameJNI, "KeyFrame");
        GET_FLD_TYPESTR(value, "[F");
        GET_FLD_TYPESTR(time, "F");
    }
    {
        SETUP_CLASS(NodeAnimationJNI, "NodeAnimation");
        GET_FLD(node, "Node");
        GET_FLD_ARRAY(translationKeys, "KeyFrame");
        GET_FLD_ARRAY(rotationKeys, "KeyFrame");
        GET_FLD_ARRAY(scaleKeys, "KeyFrame");
        GET_FLD_TYPESTR(startTime, "F");
        GET_FLD_TYPESTR(endTime, "F");
    }
    {
        SETUP_CLASS(AnimationJNI, "Animation");
        GET_FLD_TYPESTR(name, "Ljava/lang/String;");
        GET_FLD_ARRAY(nodeAnimations, "NodeAnimation");
        GET_FLD_TYPESTR(duration, "F");
    }
    {
        SETUP_CLASS(BufferJNI, "Buffer");
        GET_FLD_TYPESTR(uri, "Ljava/lang/String;");
        GET_FLD_TYPESTR(buffer, "[B");
    }
    {
        SETUP_CLASS(SceneJNI, "Scene");
        GET_FLD_TYPESTR(opaqueSceneData, "J");
        GET_FLD_ARRAY(nodes, "Node");
        GET_FLD_ARRAY(models, "Model");
        GET_FLD_ARRAY(skins, "Skin");
        GET_FLD_ARRAY(rootNodes, "Node");
        GET_FLD_ARRAY(animations, "Animation");
        GET_FLD_ARRAY(materials, "Material");
        GET_FLD_ARRAY(samplers, "Sampler");
        GET_FLD_ARRAY(images, "Image");
        GET_FLD_ARRAY(textures, "Texture");
        GET_FLD_ARRAY(buffers, "Buffer");
        GET_FLD_ARRAY(dynamicMaterials, "Material");
    }
    {
        SETUP_CLASS(OptionsJNI, "Options");
        GET_FLD_TYPESTR(dummy, "I");
    }
    #undef GET_FLD
    #undef GET_FLD_ARRAY
    #undef GET_FLD_TYPESTR
}

void FinalizeJNITypes(JNIEnv* env, TypeInfos* infos) {
    env->DeleteLocalRef(infos->m_Vector3JNI.cls);
    env->DeleteLocalRef(infos->m_QuatJNI.cls);
    env->DeleteLocalRef(infos->m_TransformJNI.cls);
    env->DeleteLocalRef(infos->m_AabbJNI.cls);
    env->DeleteLocalRef(infos->m_ImageJNI.cls);
    env->DeleteLocalRef(infos->m_SamplerJNI.cls);
    env->DeleteLocalRef(infos->m_TextureJNI.cls);
    env->DeleteLocalRef(infos->m_TextureTransformJNI.cls);
    env->DeleteLocalRef(infos->m_TextureViewJNI.cls);
    env->DeleteLocalRef(infos->m_PbrMetallicRoughnessJNI.cls);
    env->DeleteLocalRef(infos->m_PbrSpecularGlossinessJNI.cls);
    env->DeleteLocalRef(infos->m_ClearcoatJNI.cls);
    env->DeleteLocalRef(infos->m_TransmissionJNI.cls);
    env->DeleteLocalRef(infos->m_IorJNI.cls);
    env->DeleteLocalRef(infos->m_SpecularJNI.cls);
    env->DeleteLocalRef(infos->m_VolumeJNI.cls);
    env->DeleteLocalRef(infos->m_SheenJNI.cls);
    env->DeleteLocalRef(infos->m_EmissiveStrengthJNI.cls);
    env->DeleteLocalRef(infos->m_IridescenceJNI.cls);
    env->DeleteLocalRef(infos->m_MaterialJNI.cls);
    env->DeleteLocalRef(infos->m_MeshJNI.cls);
    env->DeleteLocalRef(infos->m_ModelJNI.cls);
    env->DeleteLocalRef(infos->m_BoneJNI.cls);
    env->DeleteLocalRef(infos->m_SkinJNI.cls);
    env->DeleteLocalRef(infos->m_NodeJNI.cls);
    env->DeleteLocalRef(infos->m_KeyFrameJNI.cls);
    env->DeleteLocalRef(infos->m_NodeAnimationJNI.cls);
    env->DeleteLocalRef(infos->m_AnimationJNI.cls);
    env->DeleteLocalRef(infos->m_BufferJNI.cls);
    env->DeleteLocalRef(infos->m_SceneJNI.cls);
    env->DeleteLocalRef(infos->m_OptionsJNI.cls);
}


//----------------------------------------
// From C to Jni
//----------------------------------------
jobject C2J_CreateVector3(JNIEnv* env, TypeInfos* types, const Vector3* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_Vector3JNI.cls);
    dmJNI::SetFloat(env, obj, types->m_Vector3JNI.x, src->x);
    dmJNI::SetFloat(env, obj, types->m_Vector3JNI.y, src->y);
    dmJNI::SetFloat(env, obj, types->m_Vector3JNI.z, src->z);
    return obj;
}

jobject C2J_CreateQuat(JNIEnv* env, TypeInfos* types, const Quat* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_QuatJNI.cls);
    dmJNI::SetFloat(env, obj, types->m_QuatJNI.x, src->x);
    dmJNI::SetFloat(env, obj, types->m_QuatJNI.y, src->y);
    dmJNI::SetFloat(env, obj, types->m_QuatJNI.z, src->z);
    dmJNI::SetFloat(env, obj, types->m_QuatJNI.w, src->w);
    return obj;
}

jobject C2J_CreateTransform(JNIEnv* env, TypeInfos* types, const Transform* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_TransformJNI.cls);
    dmJNI::SetObjectDeref(env, obj, types->m_TransformJNI.rotation, C2J_CreateQuat(env, types, &src->m_Rotation));
    dmJNI::SetObjectDeref(env, obj, types->m_TransformJNI.translation, C2J_CreateVector3(env, types, &src->m_Translation));
    dmJNI::SetObjectDeref(env, obj, types->m_TransformJNI.scale, C2J_CreateVector3(env, types, &src->m_Scale));
    return obj;
}

jobject C2J_CreateAabb(JNIEnv* env, TypeInfos* types, const Aabb* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_AabbJNI.cls);
    dmJNI::SetObjectDeref(env, obj, types->m_AabbJNI.min, C2J_CreateVector3(env, types, &src->m_Min));
    dmJNI::SetObjectDeref(env, obj, types->m_AabbJNI.max, C2J_CreateVector3(env, types, &src->m_Max));
    return obj;
}

jobject C2J_CreateImage(JNIEnv* env, TypeInfos* types, const Image* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_ImageJNI.cls);
    dmJNI::SetString(env, obj, types->m_ImageJNI.name, src->m_Name);
    dmJNI::SetString(env, obj, types->m_ImageJNI.uri, src->m_Uri);
    dmJNI::SetString(env, obj, types->m_ImageJNI.mimeType, src->m_MimeType);
    dmJNI::SetObjectDeref(env, obj, types->m_ImageJNI.buffer, C2J_CreateBuffer(env, types, src->m_Buffer));
    dmJNI::SetUInt(env, obj, types->m_ImageJNI.index, src->m_Index);
    return obj;
}

jobject C2J_CreateSampler(JNIEnv* env, TypeInfos* types, const Sampler* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_SamplerJNI.cls);
    dmJNI::SetString(env, obj, types->m_SamplerJNI.name, src->m_Name);
    dmJNI::SetUInt(env, obj, types->m_SamplerJNI.index, src->m_Index);
    dmJNI::SetInt(env, obj, types->m_SamplerJNI.magFilter, src->m_MagFilter);
    dmJNI::SetInt(env, obj, types->m_SamplerJNI.minFilter, src->m_MinFilter);
    dmJNI::SetInt(env, obj, types->m_SamplerJNI.wrapS, src->m_WrapS);
    dmJNI::SetInt(env, obj, types->m_SamplerJNI.wrapT, src->m_WrapT);
    return obj;
}

jobject C2J_CreateTexture(JNIEnv* env, TypeInfos* types, const Texture* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_TextureJNI.cls);
    dmJNI::SetString(env, obj, types->m_TextureJNI.name, src->m_Name);
    dmJNI::SetObjectDeref(env, obj, types->m_TextureJNI.image, C2J_CreateImage(env, types, src->m_Image));
    dmJNI::SetObjectDeref(env, obj, types->m_TextureJNI.sampler, C2J_CreateSampler(env, types, src->m_Sampler));
    dmJNI::SetObjectDeref(env, obj, types->m_TextureJNI.basisuImage, C2J_CreateImage(env, types, src->m_BasisuImage));
    dmJNI::SetUInt(env, obj, types->m_TextureJNI.index, src->m_Index);
    return obj;
}

jobject C2J_CreateTextureTransform(JNIEnv* env, TypeInfos* types, const TextureTransform* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_TextureTransformJNI.cls);
    dmJNI::SetObjectDeref(env, obj, types->m_TextureTransformJNI.offset, dmJNI::C2J_CreateFloatArray(env, src->m_Offset, 2));
    dmJNI::SetFloat(env, obj, types->m_TextureTransformJNI.rotation, src->m_Rotation);
    dmJNI::SetObjectDeref(env, obj, types->m_TextureTransformJNI.scale, dmJNI::C2J_CreateFloatArray(env, src->m_Scale, 2));
    dmJNI::SetInt(env, obj, types->m_TextureTransformJNI.texcoord, src->m_Texcoord);
    return obj;
}

jobject C2J_CreateTextureView(JNIEnv* env, TypeInfos* types, const TextureView* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_TextureViewJNI.cls);
    dmJNI::SetObjectDeref(env, obj, types->m_TextureViewJNI.texture, C2J_CreateTexture(env, types, src->m_Texture));
    dmJNI::SetInt(env, obj, types->m_TextureViewJNI.texcoord, src->m_Texcoord);
    dmJNI::SetFloat(env, obj, types->m_TextureViewJNI.scale, src->m_Scale);
    dmJNI::SetBoolean(env, obj, types->m_TextureViewJNI.hasTransform, src->m_HasTransform);
    dmJNI::SetObjectDeref(env, obj, types->m_TextureViewJNI.transform, C2J_CreateTextureTransform(env, types, &src->m_Transform));
    return obj;
}

jobject C2J_CreatePbrMetallicRoughness(JNIEnv* env, TypeInfos* types, const PbrMetallicRoughness* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_PbrMetallicRoughnessJNI.cls);
    dmJNI::SetObjectDeref(env, obj, types->m_PbrMetallicRoughnessJNI.baseColorTexture, C2J_CreateTextureView(env, types, &src->m_BaseColorTexture));
    dmJNI::SetObjectDeref(env, obj, types->m_PbrMetallicRoughnessJNI.metallicRoughnessTexture, C2J_CreateTextureView(env, types, &src->m_MetallicRoughnessTexture));
    dmJNI::SetObjectDeref(env, obj, types->m_PbrMetallicRoughnessJNI.baseColorFactor, dmJNI::C2J_CreateFloatArray(env, src->m_BaseColorFactor, 4));
    dmJNI::SetFloat(env, obj, types->m_PbrMetallicRoughnessJNI.metallicFactor, src->m_MetallicFactor);
    dmJNI::SetFloat(env, obj, types->m_PbrMetallicRoughnessJNI.roughnessFactor, src->m_RoughnessFactor);
    return obj;
}

jobject C2J_CreatePbrSpecularGlossiness(JNIEnv* env, TypeInfos* types, const PbrSpecularGlossiness* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_PbrSpecularGlossinessJNI.cls);
    dmJNI::SetObjectDeref(env, obj, types->m_PbrSpecularGlossinessJNI.diffuseTexture, C2J_CreateTextureView(env, types, &src->m_DiffuseTexture));
    dmJNI::SetObjectDeref(env, obj, types->m_PbrSpecularGlossinessJNI.specularGlossinessTexture, C2J_CreateTextureView(env, types, &src->m_SpecularGlossinessTexture));
    dmJNI::SetObjectDeref(env, obj, types->m_PbrSpecularGlossinessJNI.diffuseFactor, dmJNI::C2J_CreateFloatArray(env, src->m_DiffuseFactor, 4));
    dmJNI::SetObjectDeref(env, obj, types->m_PbrSpecularGlossinessJNI.specularFactor, dmJNI::C2J_CreateFloatArray(env, src->m_SpecularFactor, 3));
    dmJNI::SetFloat(env, obj, types->m_PbrSpecularGlossinessJNI.glossinessFactor, src->m_GlossinessFactor);
    return obj;
}

jobject C2J_CreateClearcoat(JNIEnv* env, TypeInfos* types, const Clearcoat* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_ClearcoatJNI.cls);
    dmJNI::SetObjectDeref(env, obj, types->m_ClearcoatJNI.clearcoatTexture, C2J_CreateTextureView(env, types, &src->m_ClearcoatTexture));
    dmJNI::SetObjectDeref(env, obj, types->m_ClearcoatJNI.clearcoatRoughnessTexture, C2J_CreateTextureView(env, types, &src->m_ClearcoatRoughnessTexture));
    dmJNI::SetObjectDeref(env, obj, types->m_ClearcoatJNI.clearcoatNormalTexture, C2J_CreateTextureView(env, types, &src->m_ClearcoatNormalTexture));
    dmJNI::SetFloat(env, obj, types->m_ClearcoatJNI.clearcoatFactor, src->m_ClearcoatFactor);
    dmJNI::SetFloat(env, obj, types->m_ClearcoatJNI.clearcoatRoughnessFactor, src->m_ClearcoatRoughnessFactor);
    return obj;
}

jobject C2J_CreateTransmission(JNIEnv* env, TypeInfos* types, const Transmission* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_TransmissionJNI.cls);
    dmJNI::SetObjectDeref(env, obj, types->m_TransmissionJNI.transmissionTexture, C2J_CreateTextureView(env, types, &src->m_TransmissionTexture));
    dmJNI::SetFloat(env, obj, types->m_TransmissionJNI.transmissionFactor, src->m_TransmissionFactor);
    return obj;
}

jobject C2J_CreateIor(JNIEnv* env, TypeInfos* types, const Ior* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_IorJNI.cls);
    dmJNI::SetFloat(env, obj, types->m_IorJNI.ior, src->m_Ior);
    return obj;
}

jobject C2J_CreateSpecular(JNIEnv* env, TypeInfos* types, const Specular* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_SpecularJNI.cls);
    dmJNI::SetObjectDeref(env, obj, types->m_SpecularJNI.specularTexture, C2J_CreateTextureView(env, types, &src->m_SpecularTexture));
    dmJNI::SetObjectDeref(env, obj, types->m_SpecularJNI.specularColorTexture, C2J_CreateTextureView(env, types, &src->m_SpecularColorTexture));
    dmJNI::SetObjectDeref(env, obj, types->m_SpecularJNI.specularColorFactor, dmJNI::C2J_CreateFloatArray(env, src->m_SpecularColorFactor, 3));
    dmJNI::SetFloat(env, obj, types->m_SpecularJNI.specularFactor, src->m_SpecularFactor);
    return obj;
}

jobject C2J_CreateVolume(JNIEnv* env, TypeInfos* types, const Volume* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_VolumeJNI.cls);
    dmJNI::SetObjectDeref(env, obj, types->m_VolumeJNI.thicknessTexture, C2J_CreateTextureView(env, types, &src->m_ThicknessTexture));
    dmJNI::SetFloat(env, obj, types->m_VolumeJNI.thicknessFactor, src->m_ThicknessFactor);
    dmJNI::SetObjectDeref(env, obj, types->m_VolumeJNI.attenuationColor, dmJNI::C2J_CreateFloatArray(env, src->m_AttenuationColor, 3));
    dmJNI::SetFloat(env, obj, types->m_VolumeJNI.attenuationDistance, src->m_AttenuationDistance);
    return obj;
}

jobject C2J_CreateSheen(JNIEnv* env, TypeInfos* types, const Sheen* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_SheenJNI.cls);
    dmJNI::SetObjectDeref(env, obj, types->m_SheenJNI.sheenColorTexture, C2J_CreateTextureView(env, types, &src->m_SheenColorTexture));
    dmJNI::SetObjectDeref(env, obj, types->m_SheenJNI.sheenRoughnessTexture, C2J_CreateTextureView(env, types, &src->m_SheenRoughnessTexture));
    dmJNI::SetObjectDeref(env, obj, types->m_SheenJNI.sheenColorFactor, dmJNI::C2J_CreateFloatArray(env, src->m_SheenColorFactor, 3));
    dmJNI::SetFloat(env, obj, types->m_SheenJNI.sheenRoughnessFactor, src->m_SheenRoughnessFactor);
    return obj;
}

jobject C2J_CreateEmissiveStrength(JNIEnv* env, TypeInfos* types, const EmissiveStrength* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_EmissiveStrengthJNI.cls);
    dmJNI::SetFloat(env, obj, types->m_EmissiveStrengthJNI.emissiveStrength, src->m_EmissiveStrength);
    return obj;
}

jobject C2J_CreateIridescence(JNIEnv* env, TypeInfos* types, const Iridescence* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_IridescenceJNI.cls);
    dmJNI::SetFloat(env, obj, types->m_IridescenceJNI.iridescenceFactor, src->m_IridescenceFactor);
    dmJNI::SetObjectDeref(env, obj, types->m_IridescenceJNI.iridescenceTexture, C2J_CreateTextureView(env, types, &src->m_IridescenceTexture));
    dmJNI::SetFloat(env, obj, types->m_IridescenceJNI.iridescenceIor, src->m_IridescenceIor);
    dmJNI::SetFloat(env, obj, types->m_IridescenceJNI.iridescenceThicknessMin, src->m_IridescenceThicknessMin);
    dmJNI::SetFloat(env, obj, types->m_IridescenceJNI.iridescenceThicknessMax, src->m_IridescenceThicknessMax);
    dmJNI::SetObjectDeref(env, obj, types->m_IridescenceJNI.iridescenceThicknessTexture, C2J_CreateTextureView(env, types, &src->m_IridescenceThicknessTexture));
    return obj;
}

jobject C2J_CreateMaterial(JNIEnv* env, TypeInfos* types, const Material* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_MaterialJNI.cls);
    dmJNI::SetString(env, obj, types->m_MaterialJNI.name, src->m_Name);
    dmJNI::SetUInt(env, obj, types->m_MaterialJNI.index, src->m_Index);
    dmJNI::SetUByte(env, obj, types->m_MaterialJNI.isSkinned, src->m_IsSkinned);
    dmJNI::SetObjectDeref(env, obj, types->m_MaterialJNI.pbrMetallicRoughness, C2J_CreatePbrMetallicRoughness(env, types, src->m_PbrMetallicRoughness));
    dmJNI::SetObjectDeref(env, obj, types->m_MaterialJNI.pbrSpecularGlossiness, C2J_CreatePbrSpecularGlossiness(env, types, src->m_PbrSpecularGlossiness));
    dmJNI::SetObjectDeref(env, obj, types->m_MaterialJNI.clearcoat, C2J_CreateClearcoat(env, types, src->m_Clearcoat));
    dmJNI::SetObjectDeref(env, obj, types->m_MaterialJNI.ior, C2J_CreateIor(env, types, src->m_Ior));
    dmJNI::SetObjectDeref(env, obj, types->m_MaterialJNI.specular, C2J_CreateSpecular(env, types, src->m_Specular));
    dmJNI::SetObjectDeref(env, obj, types->m_MaterialJNI.sheen, C2J_CreateSheen(env, types, src->m_Sheen));
    dmJNI::SetObjectDeref(env, obj, types->m_MaterialJNI.transmission, C2J_CreateTransmission(env, types, src->m_Transmission));
    dmJNI::SetObjectDeref(env, obj, types->m_MaterialJNI.volume, C2J_CreateVolume(env, types, src->m_Volume));
    dmJNI::SetObjectDeref(env, obj, types->m_MaterialJNI.emissiveStrength, C2J_CreateEmissiveStrength(env, types, src->m_EmissiveStrength));
    dmJNI::SetObjectDeref(env, obj, types->m_MaterialJNI.iridescence, C2J_CreateIridescence(env, types, src->m_Iridescence));
    dmJNI::SetObjectDeref(env, obj, types->m_MaterialJNI.normalTexture, C2J_CreateTextureView(env, types, &src->m_NormalTexture));
    dmJNI::SetObjectDeref(env, obj, types->m_MaterialJNI.occlusionTexture, C2J_CreateTextureView(env, types, &src->m_OcclusionTexture));
    dmJNI::SetObjectDeref(env, obj, types->m_MaterialJNI.emissiveTexture, C2J_CreateTextureView(env, types, &src->m_EmissiveTexture));
    dmJNI::SetObjectDeref(env, obj, types->m_MaterialJNI.emissiveFactor, dmJNI::C2J_CreateFloatArray(env, src->m_EmissiveFactor, 3));
    dmJNI::SetFloat(env, obj, types->m_MaterialJNI.alphaCutoff, src->m_AlphaCutoff);
    dmJNI::SetEnum(env, obj, types->m_MaterialJNI.alphaMode, src->m_AlphaMode);
    dmJNI::SetBoolean(env, obj, types->m_MaterialJNI.doubleSided, src->m_DoubleSided);
    dmJNI::SetBoolean(env, obj, types->m_MaterialJNI.unlit, src->m_Unlit);
    return obj;
}

jobject C2J_CreateMesh(JNIEnv* env, TypeInfos* types, const Mesh* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_MeshJNI.cls);
    dmJNI::SetString(env, obj, types->m_MeshJNI.name, src->m_Name);
    dmJNI::SetObjectDeref(env, obj, types->m_MeshJNI.material, C2J_CreateMaterial(env, types, src->m_Material));
    dmJNI::SetObjectDeref(env, obj, types->m_MeshJNI.positions, dmJNI::C2J_CreateFloatArray(env, src->m_Positions.Begin(), src->m_Positions.Size()));
    dmJNI::SetObjectDeref(env, obj, types->m_MeshJNI.normals, dmJNI::C2J_CreateFloatArray(env, src->m_Normals.Begin(), src->m_Normals.Size()));
    dmJNI::SetObjectDeref(env, obj, types->m_MeshJNI.tangents, dmJNI::C2J_CreateFloatArray(env, src->m_Tangents.Begin(), src->m_Tangents.Size()));
    dmJNI::SetObjectDeref(env, obj, types->m_MeshJNI.colors, dmJNI::C2J_CreateFloatArray(env, src->m_Colors.Begin(), src->m_Colors.Size()));
    dmJNI::SetObjectDeref(env, obj, types->m_MeshJNI.weights, dmJNI::C2J_CreateFloatArray(env, src->m_Weights.Begin(), src->m_Weights.Size()));
    dmJNI::SetObjectDeref(env, obj, types->m_MeshJNI.bones, dmJNI::C2J_CreateUIntArray(env, src->m_Bones.Begin(), src->m_Bones.Size()));
    dmJNI::SetUInt(env, obj, types->m_MeshJNI.texCoords0NumComponents, src->m_TexCoords0NumComponents);
    dmJNI::SetObjectDeref(env, obj, types->m_MeshJNI.texCoords0, dmJNI::C2J_CreateFloatArray(env, src->m_TexCoords0.Begin(), src->m_TexCoords0.Size()));
    dmJNI::SetUInt(env, obj, types->m_MeshJNI.texCoords1NumComponents, src->m_TexCoords1NumComponents);
    dmJNI::SetObjectDeref(env, obj, types->m_MeshJNI.texCoords1, dmJNI::C2J_CreateFloatArray(env, src->m_TexCoords1.Begin(), src->m_TexCoords1.Size()));
    dmJNI::SetObjectDeref(env, obj, types->m_MeshJNI.aabb, C2J_CreateAabb(env, types, &src->m_Aabb));
    dmJNI::SetObjectDeref(env, obj, types->m_MeshJNI.indices, dmJNI::C2J_CreateUIntArray(env, src->m_Indices.Begin(), src->m_Indices.Size()));
    dmJNI::SetUInt(env, obj, types->m_MeshJNI.vertexCount, src->m_VertexCount);
    return obj;
}

jobject C2J_CreateModel(JNIEnv* env, TypeInfos* types, const Model* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_ModelJNI.cls);
    dmJNI::SetString(env, obj, types->m_ModelJNI.name, src->m_Name);
    dmJNI::SetObjectDeref(env, obj, types->m_ModelJNI.meshes, C2J_CreateMeshArray(env, types, src->m_Meshes.Begin(), src->m_Meshes.Size()));
    dmJNI::SetUInt(env, obj, types->m_ModelJNI.index, src->m_Index);
    dmJNI::SetObjectDeref(env, obj, types->m_ModelJNI.parentBone, C2J_CreateBone(env, types, src->m_ParentBone));
    return obj;
}

jobject C2J_CreateBone(JNIEnv* env, TypeInfos* types, const Bone* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_BoneJNI.cls);
    dmJNI::SetObjectDeref(env, obj, types->m_BoneJNI.invBindPose, C2J_CreateTransform(env, types, &src->m_InvBindPose));
    dmJNI::SetString(env, obj, types->m_BoneJNI.name, src->m_Name);
    dmJNI::SetObjectDeref(env, obj, types->m_BoneJNI.node, C2J_CreateNode(env, types, src->m_Node));
    dmJNI::SetObjectDeref(env, obj, types->m_BoneJNI.parent, C2J_CreateBone(env, types, src->m_Parent));
    dmJNI::SetUInt(env, obj, types->m_BoneJNI.parentIndex, src->m_ParentIndex);
    dmJNI::SetUInt(env, obj, types->m_BoneJNI.index, src->m_Index);
    dmJNI::SetObjectDeref(env, obj, types->m_BoneJNI.children, C2J_CreateBonePtrArray(env, types, src->m_Children.Begin(), src->m_Children.Size()));
    return obj;
}

jobject C2J_CreateSkin(JNIEnv* env, TypeInfos* types, const Skin* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_SkinJNI.cls);
    dmJNI::SetString(env, obj, types->m_SkinJNI.name, src->m_Name);
    dmJNI::SetObjectDeref(env, obj, types->m_SkinJNI.bones, C2J_CreateBoneArray(env, types, src->m_Bones.Begin(), src->m_Bones.Size()));
    dmJNI::SetUInt(env, obj, types->m_SkinJNI.index, src->m_Index);
    dmJNI::SetObjectDeref(env, obj, types->m_SkinJNI.boneRemap, dmJNI::C2J_CreateUIntArray(env, src->m_BoneRemap.Begin(), src->m_BoneRemap.Size()));
    return obj;
}

jobject C2J_CreateNode(JNIEnv* env, TypeInfos* types, const Node* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_NodeJNI.cls);
    dmJNI::SetObjectDeref(env, obj, types->m_NodeJNI.local, C2J_CreateTransform(env, types, &src->m_Local));
    dmJNI::SetObjectDeref(env, obj, types->m_NodeJNI.world, C2J_CreateTransform(env, types, &src->m_World));
    dmJNI::SetString(env, obj, types->m_NodeJNI.name, src->m_Name);
    dmJNI::SetObjectDeref(env, obj, types->m_NodeJNI.model, C2J_CreateModel(env, types, src->m_Model));
    dmJNI::SetObjectDeref(env, obj, types->m_NodeJNI.skin, C2J_CreateSkin(env, types, src->m_Skin));
    dmJNI::SetObjectDeref(env, obj, types->m_NodeJNI.parent, C2J_CreateNode(env, types, src->m_Parent));
    dmJNI::SetObjectDeref(env, obj, types->m_NodeJNI.children, C2J_CreateNodePtrArray(env, types, src->m_Children.Begin(), src->m_Children.Size()));
    dmJNI::SetUInt(env, obj, types->m_NodeJNI.index, src->m_Index);
    dmJNI::SetULong(env, obj, types->m_NodeJNI.nameHash, src->m_NameHash);
    return obj;
}

jobject C2J_CreateKeyFrame(JNIEnv* env, TypeInfos* types, const KeyFrame* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_KeyFrameJNI.cls);
    dmJNI::SetObjectDeref(env, obj, types->m_KeyFrameJNI.value, dmJNI::C2J_CreateFloatArray(env, src->m_Value, 4));
    dmJNI::SetFloat(env, obj, types->m_KeyFrameJNI.time, src->m_Time);
    return obj;
}

jobject C2J_CreateNodeAnimation(JNIEnv* env, TypeInfos* types, const NodeAnimation* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_NodeAnimationJNI.cls);
    dmJNI::SetObjectDeref(env, obj, types->m_NodeAnimationJNI.node, C2J_CreateNode(env, types, src->m_Node));
    dmJNI::SetObjectDeref(env, obj, types->m_NodeAnimationJNI.translationKeys, C2J_CreateKeyFrameArray(env, types, src->m_TranslationKeys.Begin(), src->m_TranslationKeys.Size()));
    dmJNI::SetObjectDeref(env, obj, types->m_NodeAnimationJNI.rotationKeys, C2J_CreateKeyFrameArray(env, types, src->m_RotationKeys.Begin(), src->m_RotationKeys.Size()));
    dmJNI::SetObjectDeref(env, obj, types->m_NodeAnimationJNI.scaleKeys, C2J_CreateKeyFrameArray(env, types, src->m_ScaleKeys.Begin(), src->m_ScaleKeys.Size()));
    dmJNI::SetFloat(env, obj, types->m_NodeAnimationJNI.startTime, src->m_StartTime);
    dmJNI::SetFloat(env, obj, types->m_NodeAnimationJNI.endTime, src->m_EndTime);
    return obj;
}

jobject C2J_CreateAnimation(JNIEnv* env, TypeInfos* types, const Animation* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_AnimationJNI.cls);
    dmJNI::SetString(env, obj, types->m_AnimationJNI.name, src->m_Name);
    dmJNI::SetObjectDeref(env, obj, types->m_AnimationJNI.nodeAnimations, C2J_CreateNodeAnimationArray(env, types, src->m_NodeAnimations.Begin(), src->m_NodeAnimations.Size()));
    dmJNI::SetFloat(env, obj, types->m_AnimationJNI.duration, src->m_Duration);
    return obj;
}

jobject C2J_CreateBuffer(JNIEnv* env, TypeInfos* types, const Buffer* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_BufferJNI.cls);
    dmJNI::SetString(env, obj, types->m_BufferJNI.uri, src->m_Uri);
    dmJNI::SetObjectDeref(env, obj, types->m_BufferJNI.buffer, dmJNI::C2J_CreateUByteArray(env, src->m_Buffer, src->m_BufferCount));
    return obj;
}

jobject C2J_CreateScene(JNIEnv* env, TypeInfos* types, const Scene* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_SceneJNI.cls);
    dmJNI::SetLong(env, obj, types->m_SceneJNI.opaqueSceneData, (uintptr_t)src->m_OpaqueSceneData);
    dmJNI::SetObjectDeref(env, obj, types->m_SceneJNI.nodes, C2J_CreateNodeArray(env, types, src->m_Nodes.Begin(), src->m_Nodes.Size()));
    dmJNI::SetObjectDeref(env, obj, types->m_SceneJNI.models, C2J_CreateModelArray(env, types, src->m_Models.Begin(), src->m_Models.Size()));
    dmJNI::SetObjectDeref(env, obj, types->m_SceneJNI.skins, C2J_CreateSkinArray(env, types, src->m_Skins.Begin(), src->m_Skins.Size()));
    dmJNI::SetObjectDeref(env, obj, types->m_SceneJNI.rootNodes, C2J_CreateNodePtrArray(env, types, src->m_RootNodes.Begin(), src->m_RootNodes.Size()));
    dmJNI::SetObjectDeref(env, obj, types->m_SceneJNI.animations, C2J_CreateAnimationArray(env, types, src->m_Animations.Begin(), src->m_Animations.Size()));
    dmJNI::SetObjectDeref(env, obj, types->m_SceneJNI.materials, C2J_CreateMaterialArray(env, types, src->m_Materials.Begin(), src->m_Materials.Size()));
    dmJNI::SetObjectDeref(env, obj, types->m_SceneJNI.samplers, C2J_CreateSamplerArray(env, types, src->m_Samplers.Begin(), src->m_Samplers.Size()));
    dmJNI::SetObjectDeref(env, obj, types->m_SceneJNI.images, C2J_CreateImageArray(env, types, src->m_Images.Begin(), src->m_Images.Size()));
    dmJNI::SetObjectDeref(env, obj, types->m_SceneJNI.textures, C2J_CreateTextureArray(env, types, src->m_Textures.Begin(), src->m_Textures.Size()));
    dmJNI::SetObjectDeref(env, obj, types->m_SceneJNI.buffers, C2J_CreateBufferArray(env, types, src->m_Buffers.Begin(), src->m_Buffers.Size()));
    dmJNI::SetObjectDeref(env, obj, types->m_SceneJNI.dynamicMaterials, C2J_CreateMaterialPtrArray(env, types, src->m_DynamicMaterials.Begin(), src->m_DynamicMaterials.Size()));
    return obj;
}

jobject C2J_CreateOptions(JNIEnv* env, TypeInfos* types, const Options* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_OptionsJNI.cls);
    dmJNI::SetInt(env, obj, types->m_OptionsJNI.dummy, src->dummy);
    return obj;
}

jobjectArray C2J_CreateVector3Array(JNIEnv* env, TypeInfos* types, const Vector3* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_Vector3JNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateVector3(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateVector3PtrArray(JNIEnv* env, TypeInfos* types, const Vector3* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_Vector3JNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateVector3(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateQuatArray(JNIEnv* env, TypeInfos* types, const Quat* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_QuatJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateQuat(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateQuatPtrArray(JNIEnv* env, TypeInfos* types, const Quat* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_QuatJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateQuat(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateTransformArray(JNIEnv* env, TypeInfos* types, const Transform* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_TransformJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateTransform(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateTransformPtrArray(JNIEnv* env, TypeInfos* types, const Transform* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_TransformJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateTransform(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateAabbArray(JNIEnv* env, TypeInfos* types, const Aabb* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_AabbJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateAabb(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateAabbPtrArray(JNIEnv* env, TypeInfos* types, const Aabb* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_AabbJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateAabb(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateImageArray(JNIEnv* env, TypeInfos* types, const Image* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_ImageJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateImage(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateImagePtrArray(JNIEnv* env, TypeInfos* types, const Image* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_ImageJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateImage(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateSamplerArray(JNIEnv* env, TypeInfos* types, const Sampler* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_SamplerJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateSampler(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateSamplerPtrArray(JNIEnv* env, TypeInfos* types, const Sampler* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_SamplerJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateSampler(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateTextureArray(JNIEnv* env, TypeInfos* types, const Texture* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_TextureJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateTexture(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateTexturePtrArray(JNIEnv* env, TypeInfos* types, const Texture* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_TextureJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateTexture(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateTextureTransformArray(JNIEnv* env, TypeInfos* types, const TextureTransform* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_TextureTransformJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateTextureTransform(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateTextureTransformPtrArray(JNIEnv* env, TypeInfos* types, const TextureTransform* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_TextureTransformJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateTextureTransform(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateTextureViewArray(JNIEnv* env, TypeInfos* types, const TextureView* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_TextureViewJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateTextureView(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateTextureViewPtrArray(JNIEnv* env, TypeInfos* types, const TextureView* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_TextureViewJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateTextureView(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreatePbrMetallicRoughnessArray(JNIEnv* env, TypeInfos* types, const PbrMetallicRoughness* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_PbrMetallicRoughnessJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreatePbrMetallicRoughness(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreatePbrMetallicRoughnessPtrArray(JNIEnv* env, TypeInfos* types, const PbrMetallicRoughness* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_PbrMetallicRoughnessJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreatePbrMetallicRoughness(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreatePbrSpecularGlossinessArray(JNIEnv* env, TypeInfos* types, const PbrSpecularGlossiness* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_PbrSpecularGlossinessJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreatePbrSpecularGlossiness(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreatePbrSpecularGlossinessPtrArray(JNIEnv* env, TypeInfos* types, const PbrSpecularGlossiness* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_PbrSpecularGlossinessJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreatePbrSpecularGlossiness(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateClearcoatArray(JNIEnv* env, TypeInfos* types, const Clearcoat* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_ClearcoatJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateClearcoat(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateClearcoatPtrArray(JNIEnv* env, TypeInfos* types, const Clearcoat* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_ClearcoatJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateClearcoat(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateTransmissionArray(JNIEnv* env, TypeInfos* types, const Transmission* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_TransmissionJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateTransmission(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateTransmissionPtrArray(JNIEnv* env, TypeInfos* types, const Transmission* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_TransmissionJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateTransmission(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateIorArray(JNIEnv* env, TypeInfos* types, const Ior* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_IorJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateIor(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateIorPtrArray(JNIEnv* env, TypeInfos* types, const Ior* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_IorJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateIor(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateSpecularArray(JNIEnv* env, TypeInfos* types, const Specular* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_SpecularJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateSpecular(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateSpecularPtrArray(JNIEnv* env, TypeInfos* types, const Specular* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_SpecularJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateSpecular(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateVolumeArray(JNIEnv* env, TypeInfos* types, const Volume* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_VolumeJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateVolume(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateVolumePtrArray(JNIEnv* env, TypeInfos* types, const Volume* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_VolumeJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateVolume(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateSheenArray(JNIEnv* env, TypeInfos* types, const Sheen* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_SheenJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateSheen(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateSheenPtrArray(JNIEnv* env, TypeInfos* types, const Sheen* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_SheenJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateSheen(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateEmissiveStrengthArray(JNIEnv* env, TypeInfos* types, const EmissiveStrength* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_EmissiveStrengthJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateEmissiveStrength(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateEmissiveStrengthPtrArray(JNIEnv* env, TypeInfos* types, const EmissiveStrength* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_EmissiveStrengthJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateEmissiveStrength(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateIridescenceArray(JNIEnv* env, TypeInfos* types, const Iridescence* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_IridescenceJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateIridescence(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateIridescencePtrArray(JNIEnv* env, TypeInfos* types, const Iridescence* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_IridescenceJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateIridescence(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateMaterialArray(JNIEnv* env, TypeInfos* types, const Material* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_MaterialJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateMaterial(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateMaterialPtrArray(JNIEnv* env, TypeInfos* types, const Material* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_MaterialJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateMaterial(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateMeshArray(JNIEnv* env, TypeInfos* types, const Mesh* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_MeshJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateMesh(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateMeshPtrArray(JNIEnv* env, TypeInfos* types, const Mesh* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_MeshJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateMesh(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateModelArray(JNIEnv* env, TypeInfos* types, const Model* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_ModelJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateModel(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateModelPtrArray(JNIEnv* env, TypeInfos* types, const Model* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_ModelJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateModel(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateBoneArray(JNIEnv* env, TypeInfos* types, const Bone* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_BoneJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateBone(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateBonePtrArray(JNIEnv* env, TypeInfos* types, const Bone* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_BoneJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateBone(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateSkinArray(JNIEnv* env, TypeInfos* types, const Skin* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_SkinJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateSkin(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateSkinPtrArray(JNIEnv* env, TypeInfos* types, const Skin* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_SkinJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateSkin(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateNodeArray(JNIEnv* env, TypeInfos* types, const Node* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_NodeJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateNode(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateNodePtrArray(JNIEnv* env, TypeInfos* types, const Node* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_NodeJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateNode(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateKeyFrameArray(JNIEnv* env, TypeInfos* types, const KeyFrame* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_KeyFrameJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateKeyFrame(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateKeyFramePtrArray(JNIEnv* env, TypeInfos* types, const KeyFrame* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_KeyFrameJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateKeyFrame(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateNodeAnimationArray(JNIEnv* env, TypeInfos* types, const NodeAnimation* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_NodeAnimationJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateNodeAnimation(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateNodeAnimationPtrArray(JNIEnv* env, TypeInfos* types, const NodeAnimation* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_NodeAnimationJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateNodeAnimation(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateAnimationArray(JNIEnv* env, TypeInfos* types, const Animation* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_AnimationJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateAnimation(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateAnimationPtrArray(JNIEnv* env, TypeInfos* types, const Animation* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_AnimationJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateAnimation(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateBufferArray(JNIEnv* env, TypeInfos* types, const Buffer* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_BufferJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateBuffer(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateBufferPtrArray(JNIEnv* env, TypeInfos* types, const Buffer* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_BufferJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateBuffer(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateSceneArray(JNIEnv* env, TypeInfos* types, const Scene* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_SceneJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateScene(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateScenePtrArray(JNIEnv* env, TypeInfos* types, const Scene* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_SceneJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateScene(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateOptionsArray(JNIEnv* env, TypeInfos* types, const Options* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_OptionsJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateOptions(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateOptionsPtrArray(JNIEnv* env, TypeInfos* types, const Options* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_OptionsJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateOptions(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
//----------------------------------------
// From Jni to C
//----------------------------------------
bool J2C_CreateVector3(JNIEnv* env, TypeInfos* types, jobject obj, Vector3* out) {
    if (out == 0) return false;
    out->x = dmJNI::GetFloat(env, obj, types->m_Vector3JNI.x);
    out->y = dmJNI::GetFloat(env, obj, types->m_Vector3JNI.y);
    out->z = dmJNI::GetFloat(env, obj, types->m_Vector3JNI.z);
    return true;
}

bool J2C_CreateQuat(JNIEnv* env, TypeInfos* types, jobject obj, Quat* out) {
    if (out == 0) return false;
    out->x = dmJNI::GetFloat(env, obj, types->m_QuatJNI.x);
    out->y = dmJNI::GetFloat(env, obj, types->m_QuatJNI.y);
    out->z = dmJNI::GetFloat(env, obj, types->m_QuatJNI.z);
    out->w = dmJNI::GetFloat(env, obj, types->m_QuatJNI.w);
    return true;
}

bool J2C_CreateTransform(JNIEnv* env, TypeInfos* types, jobject obj, Transform* out) {
    if (out == 0) return false;
    {
        jobject field_object = env->GetObjectField(obj, types->m_TransformJNI.rotation);
        if (field_object) {
            J2C_CreateQuat(env, types, field_object, &out->m_Rotation);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_TransformJNI.translation);
        if (field_object) {
            J2C_CreateVector3(env, types, field_object, &out->m_Translation);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_TransformJNI.scale);
        if (field_object) {
            J2C_CreateVector3(env, types, field_object, &out->m_Scale);
            env->DeleteLocalRef(field_object);
        }
    }
    return true;
}

bool J2C_CreateAabb(JNIEnv* env, TypeInfos* types, jobject obj, Aabb* out) {
    if (out == 0) return false;
    {
        jobject field_object = env->GetObjectField(obj, types->m_AabbJNI.min);
        if (field_object) {
            J2C_CreateVector3(env, types, field_object, &out->m_Min);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_AabbJNI.max);
        if (field_object) {
            J2C_CreateVector3(env, types, field_object, &out->m_Max);
            env->DeleteLocalRef(field_object);
        }
    }
    return true;
}

bool J2C_CreateImage(JNIEnv* env, TypeInfos* types, jobject obj, Image* out) {
    if (out == 0) return false;
    out->m_Name = dmJNI::GetString(env, obj, types->m_ImageJNI.name);
    out->m_Uri = dmJNI::GetString(env, obj, types->m_ImageJNI.uri);
    out->m_MimeType = dmJNI::GetString(env, obj, types->m_ImageJNI.mimeType);
    {
        jobject field_object = env->GetObjectField(obj, types->m_ImageJNI.buffer);
        if (field_object) {
            out->m_Buffer = new Buffer();
            J2C_CreateBuffer(env, types, field_object, out->m_Buffer);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_Index = dmJNI::GetUInt(env, obj, types->m_ImageJNI.index);
    return true;
}

bool J2C_CreateSampler(JNIEnv* env, TypeInfos* types, jobject obj, Sampler* out) {
    if (out == 0) return false;
    out->m_Name = dmJNI::GetString(env, obj, types->m_SamplerJNI.name);
    out->m_Index = dmJNI::GetUInt(env, obj, types->m_SamplerJNI.index);
    out->m_MagFilter = dmJNI::GetInt(env, obj, types->m_SamplerJNI.magFilter);
    out->m_MinFilter = dmJNI::GetInt(env, obj, types->m_SamplerJNI.minFilter);
    out->m_WrapS = dmJNI::GetInt(env, obj, types->m_SamplerJNI.wrapS);
    out->m_WrapT = dmJNI::GetInt(env, obj, types->m_SamplerJNI.wrapT);
    return true;
}

bool J2C_CreateTexture(JNIEnv* env, TypeInfos* types, jobject obj, Texture* out) {
    if (out == 0) return false;
    out->m_Name = dmJNI::GetString(env, obj, types->m_TextureJNI.name);
    {
        jobject field_object = env->GetObjectField(obj, types->m_TextureJNI.image);
        if (field_object) {
            out->m_Image = new Image();
            J2C_CreateImage(env, types, field_object, out->m_Image);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_TextureJNI.sampler);
        if (field_object) {
            out->m_Sampler = new Sampler();
            J2C_CreateSampler(env, types, field_object, out->m_Sampler);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_TextureJNI.basisuImage);
        if (field_object) {
            out->m_BasisuImage = new Image();
            J2C_CreateImage(env, types, field_object, out->m_BasisuImage);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_Index = dmJNI::GetUInt(env, obj, types->m_TextureJNI.index);
    return true;
}

bool J2C_CreateTextureTransform(JNIEnv* env, TypeInfos* types, jobject obj, TextureTransform* out) {
    if (out == 0) return false;
    {
        jobject field_object = env->GetObjectField(obj, types->m_TextureTransformJNI.offset);
        if (field_object) {
            dmJNI::J2C_CopyFloatArray(env, (jfloatArray)field_object, out->m_Offset, 2);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_Rotation = dmJNI::GetFloat(env, obj, types->m_TextureTransformJNI.rotation);
    {
        jobject field_object = env->GetObjectField(obj, types->m_TextureTransformJNI.scale);
        if (field_object) {
            dmJNI::J2C_CopyFloatArray(env, (jfloatArray)field_object, out->m_Scale, 2);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_Texcoord = dmJNI::GetInt(env, obj, types->m_TextureTransformJNI.texcoord);
    return true;
}

bool J2C_CreateTextureView(JNIEnv* env, TypeInfos* types, jobject obj, TextureView* out) {
    if (out == 0) return false;
    {
        jobject field_object = env->GetObjectField(obj, types->m_TextureViewJNI.texture);
        if (field_object) {
            out->m_Texture = new Texture();
            J2C_CreateTexture(env, types, field_object, out->m_Texture);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_Texcoord = dmJNI::GetInt(env, obj, types->m_TextureViewJNI.texcoord);
    out->m_Scale = dmJNI::GetFloat(env, obj, types->m_TextureViewJNI.scale);
    out->m_HasTransform = dmJNI::GetBoolean(env, obj, types->m_TextureViewJNI.hasTransform);
    {
        jobject field_object = env->GetObjectField(obj, types->m_TextureViewJNI.transform);
        if (field_object) {
            J2C_CreateTextureTransform(env, types, field_object, &out->m_Transform);
            env->DeleteLocalRef(field_object);
        }
    }
    return true;
}

bool J2C_CreatePbrMetallicRoughness(JNIEnv* env, TypeInfos* types, jobject obj, PbrMetallicRoughness* out) {
    if (out == 0) return false;
    {
        jobject field_object = env->GetObjectField(obj, types->m_PbrMetallicRoughnessJNI.baseColorTexture);
        if (field_object) {
            J2C_CreateTextureView(env, types, field_object, &out->m_BaseColorTexture);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_PbrMetallicRoughnessJNI.metallicRoughnessTexture);
        if (field_object) {
            J2C_CreateTextureView(env, types, field_object, &out->m_MetallicRoughnessTexture);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_PbrMetallicRoughnessJNI.baseColorFactor);
        if (field_object) {
            dmJNI::J2C_CopyFloatArray(env, (jfloatArray)field_object, out->m_BaseColorFactor, 4);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_MetallicFactor = dmJNI::GetFloat(env, obj, types->m_PbrMetallicRoughnessJNI.metallicFactor);
    out->m_RoughnessFactor = dmJNI::GetFloat(env, obj, types->m_PbrMetallicRoughnessJNI.roughnessFactor);
    return true;
}

bool J2C_CreatePbrSpecularGlossiness(JNIEnv* env, TypeInfos* types, jobject obj, PbrSpecularGlossiness* out) {
    if (out == 0) return false;
    {
        jobject field_object = env->GetObjectField(obj, types->m_PbrSpecularGlossinessJNI.diffuseTexture);
        if (field_object) {
            J2C_CreateTextureView(env, types, field_object, &out->m_DiffuseTexture);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_PbrSpecularGlossinessJNI.specularGlossinessTexture);
        if (field_object) {
            J2C_CreateTextureView(env, types, field_object, &out->m_SpecularGlossinessTexture);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_PbrSpecularGlossinessJNI.diffuseFactor);
        if (field_object) {
            dmJNI::J2C_CopyFloatArray(env, (jfloatArray)field_object, out->m_DiffuseFactor, 4);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_PbrSpecularGlossinessJNI.specularFactor);
        if (field_object) {
            dmJNI::J2C_CopyFloatArray(env, (jfloatArray)field_object, out->m_SpecularFactor, 3);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_GlossinessFactor = dmJNI::GetFloat(env, obj, types->m_PbrSpecularGlossinessJNI.glossinessFactor);
    return true;
}

bool J2C_CreateClearcoat(JNIEnv* env, TypeInfos* types, jobject obj, Clearcoat* out) {
    if (out == 0) return false;
    {
        jobject field_object = env->GetObjectField(obj, types->m_ClearcoatJNI.clearcoatTexture);
        if (field_object) {
            J2C_CreateTextureView(env, types, field_object, &out->m_ClearcoatTexture);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_ClearcoatJNI.clearcoatRoughnessTexture);
        if (field_object) {
            J2C_CreateTextureView(env, types, field_object, &out->m_ClearcoatRoughnessTexture);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_ClearcoatJNI.clearcoatNormalTexture);
        if (field_object) {
            J2C_CreateTextureView(env, types, field_object, &out->m_ClearcoatNormalTexture);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_ClearcoatFactor = dmJNI::GetFloat(env, obj, types->m_ClearcoatJNI.clearcoatFactor);
    out->m_ClearcoatRoughnessFactor = dmJNI::GetFloat(env, obj, types->m_ClearcoatJNI.clearcoatRoughnessFactor);
    return true;
}

bool J2C_CreateTransmission(JNIEnv* env, TypeInfos* types, jobject obj, Transmission* out) {
    if (out == 0) return false;
    {
        jobject field_object = env->GetObjectField(obj, types->m_TransmissionJNI.transmissionTexture);
        if (field_object) {
            J2C_CreateTextureView(env, types, field_object, &out->m_TransmissionTexture);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_TransmissionFactor = dmJNI::GetFloat(env, obj, types->m_TransmissionJNI.transmissionFactor);
    return true;
}

bool J2C_CreateIor(JNIEnv* env, TypeInfos* types, jobject obj, Ior* out) {
    if (out == 0) return false;
    out->m_Ior = dmJNI::GetFloat(env, obj, types->m_IorJNI.ior);
    return true;
}

bool J2C_CreateSpecular(JNIEnv* env, TypeInfos* types, jobject obj, Specular* out) {
    if (out == 0) return false;
    {
        jobject field_object = env->GetObjectField(obj, types->m_SpecularJNI.specularTexture);
        if (field_object) {
            J2C_CreateTextureView(env, types, field_object, &out->m_SpecularTexture);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_SpecularJNI.specularColorTexture);
        if (field_object) {
            J2C_CreateTextureView(env, types, field_object, &out->m_SpecularColorTexture);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_SpecularJNI.specularColorFactor);
        if (field_object) {
            dmJNI::J2C_CopyFloatArray(env, (jfloatArray)field_object, out->m_SpecularColorFactor, 3);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_SpecularFactor = dmJNI::GetFloat(env, obj, types->m_SpecularJNI.specularFactor);
    return true;
}

bool J2C_CreateVolume(JNIEnv* env, TypeInfos* types, jobject obj, Volume* out) {
    if (out == 0) return false;
    {
        jobject field_object = env->GetObjectField(obj, types->m_VolumeJNI.thicknessTexture);
        if (field_object) {
            J2C_CreateTextureView(env, types, field_object, &out->m_ThicknessTexture);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_ThicknessFactor = dmJNI::GetFloat(env, obj, types->m_VolumeJNI.thicknessFactor);
    {
        jobject field_object = env->GetObjectField(obj, types->m_VolumeJNI.attenuationColor);
        if (field_object) {
            dmJNI::J2C_CopyFloatArray(env, (jfloatArray)field_object, out->m_AttenuationColor, 3);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_AttenuationDistance = dmJNI::GetFloat(env, obj, types->m_VolumeJNI.attenuationDistance);
    return true;
}

bool J2C_CreateSheen(JNIEnv* env, TypeInfos* types, jobject obj, Sheen* out) {
    if (out == 0) return false;
    {
        jobject field_object = env->GetObjectField(obj, types->m_SheenJNI.sheenColorTexture);
        if (field_object) {
            J2C_CreateTextureView(env, types, field_object, &out->m_SheenColorTexture);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_SheenJNI.sheenRoughnessTexture);
        if (field_object) {
            J2C_CreateTextureView(env, types, field_object, &out->m_SheenRoughnessTexture);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_SheenJNI.sheenColorFactor);
        if (field_object) {
            dmJNI::J2C_CopyFloatArray(env, (jfloatArray)field_object, out->m_SheenColorFactor, 3);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_SheenRoughnessFactor = dmJNI::GetFloat(env, obj, types->m_SheenJNI.sheenRoughnessFactor);
    return true;
}

bool J2C_CreateEmissiveStrength(JNIEnv* env, TypeInfos* types, jobject obj, EmissiveStrength* out) {
    if (out == 0) return false;
    out->m_EmissiveStrength = dmJNI::GetFloat(env, obj, types->m_EmissiveStrengthJNI.emissiveStrength);
    return true;
}

bool J2C_CreateIridescence(JNIEnv* env, TypeInfos* types, jobject obj, Iridescence* out) {
    if (out == 0) return false;
    out->m_IridescenceFactor = dmJNI::GetFloat(env, obj, types->m_IridescenceJNI.iridescenceFactor);
    {
        jobject field_object = env->GetObjectField(obj, types->m_IridescenceJNI.iridescenceTexture);
        if (field_object) {
            J2C_CreateTextureView(env, types, field_object, &out->m_IridescenceTexture);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_IridescenceIor = dmJNI::GetFloat(env, obj, types->m_IridescenceJNI.iridescenceIor);
    out->m_IridescenceThicknessMin = dmJNI::GetFloat(env, obj, types->m_IridescenceJNI.iridescenceThicknessMin);
    out->m_IridescenceThicknessMax = dmJNI::GetFloat(env, obj, types->m_IridescenceJNI.iridescenceThicknessMax);
    {
        jobject field_object = env->GetObjectField(obj, types->m_IridescenceJNI.iridescenceThicknessTexture);
        if (field_object) {
            J2C_CreateTextureView(env, types, field_object, &out->m_IridescenceThicknessTexture);
            env->DeleteLocalRef(field_object);
        }
    }
    return true;
}

bool J2C_CreateMaterial(JNIEnv* env, TypeInfos* types, jobject obj, Material* out) {
    if (out == 0) return false;
    out->m_Name = dmJNI::GetString(env, obj, types->m_MaterialJNI.name);
    out->m_Index = dmJNI::GetUInt(env, obj, types->m_MaterialJNI.index);
    out->m_IsSkinned = dmJNI::GetUByte(env, obj, types->m_MaterialJNI.isSkinned);
    {
        jobject field_object = env->GetObjectField(obj, types->m_MaterialJNI.pbrMetallicRoughness);
        if (field_object) {
            out->m_PbrMetallicRoughness = new PbrMetallicRoughness();
            J2C_CreatePbrMetallicRoughness(env, types, field_object, out->m_PbrMetallicRoughness);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_MaterialJNI.pbrSpecularGlossiness);
        if (field_object) {
            out->m_PbrSpecularGlossiness = new PbrSpecularGlossiness();
            J2C_CreatePbrSpecularGlossiness(env, types, field_object, out->m_PbrSpecularGlossiness);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_MaterialJNI.clearcoat);
        if (field_object) {
            out->m_Clearcoat = new Clearcoat();
            J2C_CreateClearcoat(env, types, field_object, out->m_Clearcoat);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_MaterialJNI.ior);
        if (field_object) {
            out->m_Ior = new Ior();
            J2C_CreateIor(env, types, field_object, out->m_Ior);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_MaterialJNI.specular);
        if (field_object) {
            out->m_Specular = new Specular();
            J2C_CreateSpecular(env, types, field_object, out->m_Specular);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_MaterialJNI.sheen);
        if (field_object) {
            out->m_Sheen = new Sheen();
            J2C_CreateSheen(env, types, field_object, out->m_Sheen);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_MaterialJNI.transmission);
        if (field_object) {
            out->m_Transmission = new Transmission();
            J2C_CreateTransmission(env, types, field_object, out->m_Transmission);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_MaterialJNI.volume);
        if (field_object) {
            out->m_Volume = new Volume();
            J2C_CreateVolume(env, types, field_object, out->m_Volume);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_MaterialJNI.emissiveStrength);
        if (field_object) {
            out->m_EmissiveStrength = new EmissiveStrength();
            J2C_CreateEmissiveStrength(env, types, field_object, out->m_EmissiveStrength);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_MaterialJNI.iridescence);
        if (field_object) {
            out->m_Iridescence = new Iridescence();
            J2C_CreateIridescence(env, types, field_object, out->m_Iridescence);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_MaterialJNI.normalTexture);
        if (field_object) {
            J2C_CreateTextureView(env, types, field_object, &out->m_NormalTexture);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_MaterialJNI.occlusionTexture);
        if (field_object) {
            J2C_CreateTextureView(env, types, field_object, &out->m_OcclusionTexture);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_MaterialJNI.emissiveTexture);
        if (field_object) {
            J2C_CreateTextureView(env, types, field_object, &out->m_EmissiveTexture);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_MaterialJNI.emissiveFactor);
        if (field_object) {
            dmJNI::J2C_CopyFloatArray(env, (jfloatArray)field_object, out->m_EmissiveFactor, 3);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_AlphaCutoff = dmJNI::GetFloat(env, obj, types->m_MaterialJNI.alphaCutoff);
    out->m_AlphaMode = (AlphaMode)dmJNI::GetEnum(env, obj, types->m_MaterialJNI.alphaMode);
    out->m_DoubleSided = dmJNI::GetBoolean(env, obj, types->m_MaterialJNI.doubleSided);
    out->m_Unlit = dmJNI::GetBoolean(env, obj, types->m_MaterialJNI.unlit);
    return true;
}

bool J2C_CreateMesh(JNIEnv* env, TypeInfos* types, jobject obj, Mesh* out) {
    if (out == 0) return false;
    out->m_Name = dmJNI::GetString(env, obj, types->m_MeshJNI.name);
    {
        jobject field_object = env->GetObjectField(obj, types->m_MeshJNI.material);
        if (field_object) {
            out->m_Material = new Material();
            J2C_CreateMaterial(env, types, field_object, out->m_Material);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_MeshJNI.positions);
        if (field_object) {
            uint32_t tmp_count;
            float* tmp = dmJNI::J2C_CreateFloatArray(env, (jfloatArray)field_object, &tmp_count);
            out->m_Positions.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_MeshJNI.normals);
        if (field_object) {
            uint32_t tmp_count;
            float* tmp = dmJNI::J2C_CreateFloatArray(env, (jfloatArray)field_object, &tmp_count);
            out->m_Normals.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_MeshJNI.tangents);
        if (field_object) {
            uint32_t tmp_count;
            float* tmp = dmJNI::J2C_CreateFloatArray(env, (jfloatArray)field_object, &tmp_count);
            out->m_Tangents.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_MeshJNI.colors);
        if (field_object) {
            uint32_t tmp_count;
            float* tmp = dmJNI::J2C_CreateFloatArray(env, (jfloatArray)field_object, &tmp_count);
            out->m_Colors.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_MeshJNI.weights);
        if (field_object) {
            uint32_t tmp_count;
            float* tmp = dmJNI::J2C_CreateFloatArray(env, (jfloatArray)field_object, &tmp_count);
            out->m_Weights.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_MeshJNI.bones);
        if (field_object) {
            uint32_t tmp_count;
            uint32_t* tmp = dmJNI::J2C_CreateUIntArray(env, (jintArray)field_object, &tmp_count);
            out->m_Bones.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_TexCoords0NumComponents = dmJNI::GetUInt(env, obj, types->m_MeshJNI.texCoords0NumComponents);
    {
        jobject field_object = env->GetObjectField(obj, types->m_MeshJNI.texCoords0);
        if (field_object) {
            uint32_t tmp_count;
            float* tmp = dmJNI::J2C_CreateFloatArray(env, (jfloatArray)field_object, &tmp_count);
            out->m_TexCoords0.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_TexCoords1NumComponents = dmJNI::GetUInt(env, obj, types->m_MeshJNI.texCoords1NumComponents);
    {
        jobject field_object = env->GetObjectField(obj, types->m_MeshJNI.texCoords1);
        if (field_object) {
            uint32_t tmp_count;
            float* tmp = dmJNI::J2C_CreateFloatArray(env, (jfloatArray)field_object, &tmp_count);
            out->m_TexCoords1.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_MeshJNI.aabb);
        if (field_object) {
            J2C_CreateAabb(env, types, field_object, &out->m_Aabb);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_MeshJNI.indices);
        if (field_object) {
            uint32_t tmp_count;
            uint32_t* tmp = dmJNI::J2C_CreateUIntArray(env, (jintArray)field_object, &tmp_count);
            out->m_Indices.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_VertexCount = dmJNI::GetUInt(env, obj, types->m_MeshJNI.vertexCount);
    return true;
}

bool J2C_CreateModel(JNIEnv* env, TypeInfos* types, jobject obj, Model* out) {
    if (out == 0) return false;
    out->m_Name = dmJNI::GetString(env, obj, types->m_ModelJNI.name);
    {
        jobject field_object = env->GetObjectField(obj, types->m_ModelJNI.meshes);
        if (field_object) {
            uint32_t tmp_count;
            Mesh* tmp = J2C_CreateMeshArray(env, types, (jobjectArray)field_object, &tmp_count);
            out->m_Meshes.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_Index = dmJNI::GetUInt(env, obj, types->m_ModelJNI.index);
    {
        jobject field_object = env->GetObjectField(obj, types->m_ModelJNI.parentBone);
        if (field_object) {
            out->m_ParentBone = new Bone();
            J2C_CreateBone(env, types, field_object, out->m_ParentBone);
            env->DeleteLocalRef(field_object);
        }
    }
    return true;
}

bool J2C_CreateBone(JNIEnv* env, TypeInfos* types, jobject obj, Bone* out) {
    if (out == 0) return false;
    {
        jobject field_object = env->GetObjectField(obj, types->m_BoneJNI.invBindPose);
        if (field_object) {
            J2C_CreateTransform(env, types, field_object, &out->m_InvBindPose);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_Name = dmJNI::GetString(env, obj, types->m_BoneJNI.name);
    {
        jobject field_object = env->GetObjectField(obj, types->m_BoneJNI.node);
        if (field_object) {
            out->m_Node = new Node();
            J2C_CreateNode(env, types, field_object, out->m_Node);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_BoneJNI.parent);
        if (field_object) {
            out->m_Parent = new Bone();
            J2C_CreateBone(env, types, field_object, out->m_Parent);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_ParentIndex = dmJNI::GetUInt(env, obj, types->m_BoneJNI.parentIndex);
    out->m_Index = dmJNI::GetUInt(env, obj, types->m_BoneJNI.index);
    {
        jobject field_object = env->GetObjectField(obj, types->m_BoneJNI.children);
        if (field_object) {
            uint32_t tmp_count;
            Bone** tmp = J2C_CreateBonePtrArray(env, types, (jobjectArray)field_object, &tmp_count);
            out->m_Children.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    return true;
}

bool J2C_CreateSkin(JNIEnv* env, TypeInfos* types, jobject obj, Skin* out) {
    if (out == 0) return false;
    out->m_Name = dmJNI::GetString(env, obj, types->m_SkinJNI.name);
    {
        jobject field_object = env->GetObjectField(obj, types->m_SkinJNI.bones);
        if (field_object) {
            uint32_t tmp_count;
            Bone* tmp = J2C_CreateBoneArray(env, types, (jobjectArray)field_object, &tmp_count);
            out->m_Bones.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_Index = dmJNI::GetUInt(env, obj, types->m_SkinJNI.index);
    {
        jobject field_object = env->GetObjectField(obj, types->m_SkinJNI.boneRemap);
        if (field_object) {
            uint32_t tmp_count;
            uint32_t* tmp = dmJNI::J2C_CreateUIntArray(env, (jintArray)field_object, &tmp_count);
            out->m_BoneRemap.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    return true;
}

bool J2C_CreateNode(JNIEnv* env, TypeInfos* types, jobject obj, Node* out) {
    if (out == 0) return false;
    {
        jobject field_object = env->GetObjectField(obj, types->m_NodeJNI.local);
        if (field_object) {
            J2C_CreateTransform(env, types, field_object, &out->m_Local);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_NodeJNI.world);
        if (field_object) {
            J2C_CreateTransform(env, types, field_object, &out->m_World);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_Name = dmJNI::GetString(env, obj, types->m_NodeJNI.name);
    {
        jobject field_object = env->GetObjectField(obj, types->m_NodeJNI.model);
        if (field_object) {
            out->m_Model = new Model();
            J2C_CreateModel(env, types, field_object, out->m_Model);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_NodeJNI.skin);
        if (field_object) {
            out->m_Skin = new Skin();
            J2C_CreateSkin(env, types, field_object, out->m_Skin);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_NodeJNI.parent);
        if (field_object) {
            out->m_Parent = new Node();
            J2C_CreateNode(env, types, field_object, out->m_Parent);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_NodeJNI.children);
        if (field_object) {
            uint32_t tmp_count;
            Node** tmp = J2C_CreateNodePtrArray(env, types, (jobjectArray)field_object, &tmp_count);
            out->m_Children.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_Index = dmJNI::GetUInt(env, obj, types->m_NodeJNI.index);
    out->m_NameHash = dmJNI::GetULong(env, obj, types->m_NodeJNI.nameHash);
    return true;
}

bool J2C_CreateKeyFrame(JNIEnv* env, TypeInfos* types, jobject obj, KeyFrame* out) {
    if (out == 0) return false;
    {
        jobject field_object = env->GetObjectField(obj, types->m_KeyFrameJNI.value);
        if (field_object) {
            dmJNI::J2C_CopyFloatArray(env, (jfloatArray)field_object, out->m_Value, 4);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_Time = dmJNI::GetFloat(env, obj, types->m_KeyFrameJNI.time);
    return true;
}

bool J2C_CreateNodeAnimation(JNIEnv* env, TypeInfos* types, jobject obj, NodeAnimation* out) {
    if (out == 0) return false;
    {
        jobject field_object = env->GetObjectField(obj, types->m_NodeAnimationJNI.node);
        if (field_object) {
            out->m_Node = new Node();
            J2C_CreateNode(env, types, field_object, out->m_Node);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_NodeAnimationJNI.translationKeys);
        if (field_object) {
            uint32_t tmp_count;
            KeyFrame* tmp = J2C_CreateKeyFrameArray(env, types, (jobjectArray)field_object, &tmp_count);
            out->m_TranslationKeys.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_NodeAnimationJNI.rotationKeys);
        if (field_object) {
            uint32_t tmp_count;
            KeyFrame* tmp = J2C_CreateKeyFrameArray(env, types, (jobjectArray)field_object, &tmp_count);
            out->m_RotationKeys.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_NodeAnimationJNI.scaleKeys);
        if (field_object) {
            uint32_t tmp_count;
            KeyFrame* tmp = J2C_CreateKeyFrameArray(env, types, (jobjectArray)field_object, &tmp_count);
            out->m_ScaleKeys.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_StartTime = dmJNI::GetFloat(env, obj, types->m_NodeAnimationJNI.startTime);
    out->m_EndTime = dmJNI::GetFloat(env, obj, types->m_NodeAnimationJNI.endTime);
    return true;
}

bool J2C_CreateAnimation(JNIEnv* env, TypeInfos* types, jobject obj, Animation* out) {
    if (out == 0) return false;
    out->m_Name = dmJNI::GetString(env, obj, types->m_AnimationJNI.name);
    {
        jobject field_object = env->GetObjectField(obj, types->m_AnimationJNI.nodeAnimations);
        if (field_object) {
            uint32_t tmp_count;
            NodeAnimation* tmp = J2C_CreateNodeAnimationArray(env, types, (jobjectArray)field_object, &tmp_count);
            out->m_NodeAnimations.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_Duration = dmJNI::GetFloat(env, obj, types->m_AnimationJNI.duration);
    return true;
}

bool J2C_CreateBuffer(JNIEnv* env, TypeInfos* types, jobject obj, Buffer* out) {
    if (out == 0) return false;
    out->m_Uri = dmJNI::GetString(env, obj, types->m_BufferJNI.uri);
    {
        jobject field_object = env->GetObjectField(obj, types->m_BufferJNI.buffer);
        if (field_object) {
            out->m_Buffer = dmJNI::J2C_CreateUByteArray(env, (jbyteArray)field_object, &out->m_BufferCount);
            env->DeleteLocalRef(field_object);
        }
    }
    return true;
}

bool J2C_CreateScene(JNIEnv* env, TypeInfos* types, jobject obj, Scene* out) {
    if (out == 0) return false;
    out->m_OpaqueSceneData = (void*)(uintptr_t)dmJNI::GetLong(env, obj, types->m_SceneJNI.opaqueSceneData);
    {
        jobject field_object = env->GetObjectField(obj, types->m_SceneJNI.nodes);
        if (field_object) {
            uint32_t tmp_count;
            Node* tmp = J2C_CreateNodeArray(env, types, (jobjectArray)field_object, &tmp_count);
            out->m_Nodes.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_SceneJNI.models);
        if (field_object) {
            uint32_t tmp_count;
            Model* tmp = J2C_CreateModelArray(env, types, (jobjectArray)field_object, &tmp_count);
            out->m_Models.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_SceneJNI.skins);
        if (field_object) {
            uint32_t tmp_count;
            Skin* tmp = J2C_CreateSkinArray(env, types, (jobjectArray)field_object, &tmp_count);
            out->m_Skins.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_SceneJNI.rootNodes);
        if (field_object) {
            uint32_t tmp_count;
            Node** tmp = J2C_CreateNodePtrArray(env, types, (jobjectArray)field_object, &tmp_count);
            out->m_RootNodes.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_SceneJNI.animations);
        if (field_object) {
            uint32_t tmp_count;
            Animation* tmp = J2C_CreateAnimationArray(env, types, (jobjectArray)field_object, &tmp_count);
            out->m_Animations.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_SceneJNI.materials);
        if (field_object) {
            uint32_t tmp_count;
            Material* tmp = J2C_CreateMaterialArray(env, types, (jobjectArray)field_object, &tmp_count);
            out->m_Materials.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_SceneJNI.samplers);
        if (field_object) {
            uint32_t tmp_count;
            Sampler* tmp = J2C_CreateSamplerArray(env, types, (jobjectArray)field_object, &tmp_count);
            out->m_Samplers.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_SceneJNI.images);
        if (field_object) {
            uint32_t tmp_count;
            Image* tmp = J2C_CreateImageArray(env, types, (jobjectArray)field_object, &tmp_count);
            out->m_Images.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_SceneJNI.textures);
        if (field_object) {
            uint32_t tmp_count;
            Texture* tmp = J2C_CreateTextureArray(env, types, (jobjectArray)field_object, &tmp_count);
            out->m_Textures.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_SceneJNI.buffers);
        if (field_object) {
            uint32_t tmp_count;
            Buffer* tmp = J2C_CreateBufferArray(env, types, (jobjectArray)field_object, &tmp_count);
            out->m_Buffers.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_SceneJNI.dynamicMaterials);
        if (field_object) {
            uint32_t tmp_count;
            Material** tmp = J2C_CreateMaterialPtrArray(env, types, (jobjectArray)field_object, &tmp_count);
            out->m_DynamicMaterials.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    return true;
}

bool J2C_CreateOptions(JNIEnv* env, TypeInfos* types, jobject obj, Options* out) {
    if (out == 0) return false;
    out->dummy = dmJNI::GetInt(env, obj, types->m_OptionsJNI.dummy);
    return true;
}

void J2C_CreateVector3ArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Vector3* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateVector3(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Vector3* J2C_CreateVector3Array(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Vector3* out = new Vector3[len];
    J2C_CreateVector3ArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateVector3PtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Vector3** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new Vector3();
        J2C_CreateVector3(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Vector3** J2C_CreateVector3PtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Vector3** out = new Vector3*[len];
    J2C_CreateVector3PtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateQuatArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Quat* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateQuat(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Quat* J2C_CreateQuatArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Quat* out = new Quat[len];
    J2C_CreateQuatArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateQuatPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Quat** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new Quat();
        J2C_CreateQuat(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Quat** J2C_CreateQuatPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Quat** out = new Quat*[len];
    J2C_CreateQuatPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateTransformArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Transform* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateTransform(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Transform* J2C_CreateTransformArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Transform* out = new Transform[len];
    J2C_CreateTransformArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateTransformPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Transform** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new Transform();
        J2C_CreateTransform(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Transform** J2C_CreateTransformPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Transform** out = new Transform*[len];
    J2C_CreateTransformPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateAabbArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Aabb* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateAabb(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Aabb* J2C_CreateAabbArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Aabb* out = new Aabb[len];
    J2C_CreateAabbArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateAabbPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Aabb** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new Aabb();
        J2C_CreateAabb(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Aabb** J2C_CreateAabbPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Aabb** out = new Aabb*[len];
    J2C_CreateAabbPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateImageArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Image* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateImage(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Image* J2C_CreateImageArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Image* out = new Image[len];
    J2C_CreateImageArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateImagePtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Image** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new Image();
        J2C_CreateImage(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Image** J2C_CreateImagePtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Image** out = new Image*[len];
    J2C_CreateImagePtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateSamplerArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Sampler* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateSampler(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Sampler* J2C_CreateSamplerArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Sampler* out = new Sampler[len];
    J2C_CreateSamplerArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateSamplerPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Sampler** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new Sampler();
        J2C_CreateSampler(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Sampler** J2C_CreateSamplerPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Sampler** out = new Sampler*[len];
    J2C_CreateSamplerPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateTextureArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Texture* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateTexture(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Texture* J2C_CreateTextureArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Texture* out = new Texture[len];
    J2C_CreateTextureArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateTexturePtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Texture** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new Texture();
        J2C_CreateTexture(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Texture** J2C_CreateTexturePtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Texture** out = new Texture*[len];
    J2C_CreateTexturePtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateTextureTransformArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, TextureTransform* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateTextureTransform(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
TextureTransform* J2C_CreateTextureTransformArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    TextureTransform* out = new TextureTransform[len];
    J2C_CreateTextureTransformArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateTextureTransformPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, TextureTransform** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new TextureTransform();
        J2C_CreateTextureTransform(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
TextureTransform** J2C_CreateTextureTransformPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    TextureTransform** out = new TextureTransform*[len];
    J2C_CreateTextureTransformPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateTextureViewArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, TextureView* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateTextureView(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
TextureView* J2C_CreateTextureViewArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    TextureView* out = new TextureView[len];
    J2C_CreateTextureViewArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateTextureViewPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, TextureView** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new TextureView();
        J2C_CreateTextureView(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
TextureView** J2C_CreateTextureViewPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    TextureView** out = new TextureView*[len];
    J2C_CreateTextureViewPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreatePbrMetallicRoughnessArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, PbrMetallicRoughness* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreatePbrMetallicRoughness(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
PbrMetallicRoughness* J2C_CreatePbrMetallicRoughnessArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    PbrMetallicRoughness* out = new PbrMetallicRoughness[len];
    J2C_CreatePbrMetallicRoughnessArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreatePbrMetallicRoughnessPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, PbrMetallicRoughness** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new PbrMetallicRoughness();
        J2C_CreatePbrMetallicRoughness(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
PbrMetallicRoughness** J2C_CreatePbrMetallicRoughnessPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    PbrMetallicRoughness** out = new PbrMetallicRoughness*[len];
    J2C_CreatePbrMetallicRoughnessPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreatePbrSpecularGlossinessArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, PbrSpecularGlossiness* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreatePbrSpecularGlossiness(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
PbrSpecularGlossiness* J2C_CreatePbrSpecularGlossinessArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    PbrSpecularGlossiness* out = new PbrSpecularGlossiness[len];
    J2C_CreatePbrSpecularGlossinessArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreatePbrSpecularGlossinessPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, PbrSpecularGlossiness** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new PbrSpecularGlossiness();
        J2C_CreatePbrSpecularGlossiness(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
PbrSpecularGlossiness** J2C_CreatePbrSpecularGlossinessPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    PbrSpecularGlossiness** out = new PbrSpecularGlossiness*[len];
    J2C_CreatePbrSpecularGlossinessPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateClearcoatArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Clearcoat* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateClearcoat(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Clearcoat* J2C_CreateClearcoatArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Clearcoat* out = new Clearcoat[len];
    J2C_CreateClearcoatArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateClearcoatPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Clearcoat** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new Clearcoat();
        J2C_CreateClearcoat(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Clearcoat** J2C_CreateClearcoatPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Clearcoat** out = new Clearcoat*[len];
    J2C_CreateClearcoatPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateTransmissionArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Transmission* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateTransmission(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Transmission* J2C_CreateTransmissionArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Transmission* out = new Transmission[len];
    J2C_CreateTransmissionArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateTransmissionPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Transmission** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new Transmission();
        J2C_CreateTransmission(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Transmission** J2C_CreateTransmissionPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Transmission** out = new Transmission*[len];
    J2C_CreateTransmissionPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateIorArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Ior* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateIor(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Ior* J2C_CreateIorArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Ior* out = new Ior[len];
    J2C_CreateIorArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateIorPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Ior** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new Ior();
        J2C_CreateIor(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Ior** J2C_CreateIorPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Ior** out = new Ior*[len];
    J2C_CreateIorPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateSpecularArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Specular* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateSpecular(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Specular* J2C_CreateSpecularArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Specular* out = new Specular[len];
    J2C_CreateSpecularArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateSpecularPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Specular** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new Specular();
        J2C_CreateSpecular(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Specular** J2C_CreateSpecularPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Specular** out = new Specular*[len];
    J2C_CreateSpecularPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateVolumeArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Volume* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateVolume(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Volume* J2C_CreateVolumeArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Volume* out = new Volume[len];
    J2C_CreateVolumeArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateVolumePtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Volume** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new Volume();
        J2C_CreateVolume(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Volume** J2C_CreateVolumePtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Volume** out = new Volume*[len];
    J2C_CreateVolumePtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateSheenArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Sheen* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateSheen(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Sheen* J2C_CreateSheenArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Sheen* out = new Sheen[len];
    J2C_CreateSheenArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateSheenPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Sheen** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new Sheen();
        J2C_CreateSheen(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Sheen** J2C_CreateSheenPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Sheen** out = new Sheen*[len];
    J2C_CreateSheenPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateEmissiveStrengthArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, EmissiveStrength* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateEmissiveStrength(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
EmissiveStrength* J2C_CreateEmissiveStrengthArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    EmissiveStrength* out = new EmissiveStrength[len];
    J2C_CreateEmissiveStrengthArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateEmissiveStrengthPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, EmissiveStrength** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new EmissiveStrength();
        J2C_CreateEmissiveStrength(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
EmissiveStrength** J2C_CreateEmissiveStrengthPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    EmissiveStrength** out = new EmissiveStrength*[len];
    J2C_CreateEmissiveStrengthPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateIridescenceArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Iridescence* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateIridescence(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Iridescence* J2C_CreateIridescenceArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Iridescence* out = new Iridescence[len];
    J2C_CreateIridescenceArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateIridescencePtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Iridescence** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new Iridescence();
        J2C_CreateIridescence(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Iridescence** J2C_CreateIridescencePtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Iridescence** out = new Iridescence*[len];
    J2C_CreateIridescencePtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateMaterialArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Material* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateMaterial(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Material* J2C_CreateMaterialArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Material* out = new Material[len];
    J2C_CreateMaterialArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateMaterialPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Material** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new Material();
        J2C_CreateMaterial(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Material** J2C_CreateMaterialPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Material** out = new Material*[len];
    J2C_CreateMaterialPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateMeshArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Mesh* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateMesh(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Mesh* J2C_CreateMeshArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Mesh* out = new Mesh[len];
    J2C_CreateMeshArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateMeshPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Mesh** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new Mesh();
        J2C_CreateMesh(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Mesh** J2C_CreateMeshPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Mesh** out = new Mesh*[len];
    J2C_CreateMeshPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateModelArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Model* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateModel(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Model* J2C_CreateModelArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Model* out = new Model[len];
    J2C_CreateModelArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateModelPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Model** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new Model();
        J2C_CreateModel(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Model** J2C_CreateModelPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Model** out = new Model*[len];
    J2C_CreateModelPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateBoneArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Bone* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateBone(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Bone* J2C_CreateBoneArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Bone* out = new Bone[len];
    J2C_CreateBoneArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateBonePtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Bone** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new Bone();
        J2C_CreateBone(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Bone** J2C_CreateBonePtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Bone** out = new Bone*[len];
    J2C_CreateBonePtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateSkinArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Skin* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateSkin(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Skin* J2C_CreateSkinArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Skin* out = new Skin[len];
    J2C_CreateSkinArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateSkinPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Skin** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new Skin();
        J2C_CreateSkin(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Skin** J2C_CreateSkinPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Skin** out = new Skin*[len];
    J2C_CreateSkinPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateNodeArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Node* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateNode(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Node* J2C_CreateNodeArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Node* out = new Node[len];
    J2C_CreateNodeArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateNodePtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Node** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new Node();
        J2C_CreateNode(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Node** J2C_CreateNodePtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Node** out = new Node*[len];
    J2C_CreateNodePtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateKeyFrameArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, KeyFrame* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateKeyFrame(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
KeyFrame* J2C_CreateKeyFrameArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    KeyFrame* out = new KeyFrame[len];
    J2C_CreateKeyFrameArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateKeyFramePtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, KeyFrame** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new KeyFrame();
        J2C_CreateKeyFrame(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
KeyFrame** J2C_CreateKeyFramePtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    KeyFrame** out = new KeyFrame*[len];
    J2C_CreateKeyFramePtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateNodeAnimationArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, NodeAnimation* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateNodeAnimation(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
NodeAnimation* J2C_CreateNodeAnimationArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    NodeAnimation* out = new NodeAnimation[len];
    J2C_CreateNodeAnimationArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateNodeAnimationPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, NodeAnimation** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new NodeAnimation();
        J2C_CreateNodeAnimation(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
NodeAnimation** J2C_CreateNodeAnimationPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    NodeAnimation** out = new NodeAnimation*[len];
    J2C_CreateNodeAnimationPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateAnimationArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Animation* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateAnimation(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Animation* J2C_CreateAnimationArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Animation* out = new Animation[len];
    J2C_CreateAnimationArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateAnimationPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Animation** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new Animation();
        J2C_CreateAnimation(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Animation** J2C_CreateAnimationPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Animation** out = new Animation*[len];
    J2C_CreateAnimationPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateBufferArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Buffer* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateBuffer(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Buffer* J2C_CreateBufferArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Buffer* out = new Buffer[len];
    J2C_CreateBufferArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateBufferPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Buffer** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new Buffer();
        J2C_CreateBuffer(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Buffer** J2C_CreateBufferPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Buffer** out = new Buffer*[len];
    J2C_CreateBufferPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateSceneArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Scene* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateScene(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Scene* J2C_CreateSceneArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Scene* out = new Scene[len];
    J2C_CreateSceneArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateScenePtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Scene** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new Scene();
        J2C_CreateScene(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Scene** J2C_CreateScenePtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Scene** out = new Scene*[len];
    J2C_CreateScenePtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateOptionsArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Options* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateOptions(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Options* J2C_CreateOptionsArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Options* out = new Options[len];
    J2C_CreateOptionsArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateOptionsPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Options** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new Options();
        J2C_CreateOptions(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Options** J2C_CreateOptionsPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Options** out = new Options*[len];
    J2C_CreateOptionsPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
} // jni
} // dmModelImporter
