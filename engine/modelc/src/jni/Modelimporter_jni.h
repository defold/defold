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
#include "modelimporter.h"

#define JAVA_PACKAGE_NAME "com/dynamo/bob/pipeline"
#define CLASS_NAME "com/dynamo/bob/pipeline/Modelimporter"

namespace dmModelImporter {
namespace jni {
struct Vector3JNI {
    jclass cls;
    jfieldID x;
    jfieldID y;
    jfieldID z;
};
struct QuatJNI {
    jclass cls;
    jfieldID x;
    jfieldID y;
    jfieldID z;
    jfieldID w;
};
struct TransformJNI {
    jclass cls;
    jfieldID rotation;
    jfieldID translation;
    jfieldID scale;
};
struct AabbJNI {
    jclass cls;
    jfieldID min;
    jfieldID max;
};
struct ImageJNI {
    jclass cls;
    jfieldID name;
    jfieldID uri;
    jfieldID mimeType;
    jfieldID buffer;
    jfieldID index;
};
struct SamplerJNI {
    jclass cls;
    jfieldID name;
    jfieldID index;
    jfieldID magFilter;
    jfieldID minFilter;
    jfieldID wrapS;
    jfieldID wrapT;
};
struct TextureJNI {
    jclass cls;
    jfieldID name;
    jfieldID image;
    jfieldID sampler;
    jfieldID basisuImage;
    jfieldID index;
};
struct TextureTransformJNI {
    jclass cls;
    jfieldID offset;
    jfieldID rotation;
    jfieldID scale;
    jfieldID texcoord;
};
struct TextureViewJNI {
    jclass cls;
    jfieldID texture;
    jfieldID texcoord;
    jfieldID scale;
    jfieldID hasTransform;
    jfieldID transform;
};
struct PbrMetallicRoughnessJNI {
    jclass cls;
    jfieldID baseColorTexture;
    jfieldID metallicRoughnessTexture;
    jfieldID baseColorFactor;
    jfieldID metallicFactor;
    jfieldID roughnessFactor;
};
struct PbrSpecularGlossinessJNI {
    jclass cls;
    jfieldID diffuseTexture;
    jfieldID specularGlossinessTexture;
    jfieldID diffuseFactor;
    jfieldID specularFactor;
    jfieldID glossinessFactor;
};
struct ClearcoatJNI {
    jclass cls;
    jfieldID clearcoatTexture;
    jfieldID clearcoatRoughnessTexture;
    jfieldID clearcoatNormalTexture;
    jfieldID clearcoatFactor;
    jfieldID clearcoatRoughnessFactor;
};
struct TransmissionJNI {
    jclass cls;
    jfieldID transmissionTexture;
    jfieldID transmissionFactor;
};
struct IorJNI {
    jclass cls;
    jfieldID ior;
};
struct SpecularJNI {
    jclass cls;
    jfieldID specularTexture;
    jfieldID specularColorTexture;
    jfieldID specularColorFactor;
    jfieldID specularFactor;
};
struct VolumeJNI {
    jclass cls;
    jfieldID thicknessTexture;
    jfieldID thicknessFactor;
    jfieldID attenuationColor;
    jfieldID attenuationDistance;
};
struct SheenJNI {
    jclass cls;
    jfieldID sheenColorTexture;
    jfieldID sheenRoughnessTexture;
    jfieldID sheenColorFactor;
    jfieldID sheenRoughnessFactor;
};
struct EmissiveStrengthJNI {
    jclass cls;
    jfieldID emissiveStrength;
};
struct IridescenceJNI {
    jclass cls;
    jfieldID iridescenceFactor;
    jfieldID iridescenceTexture;
    jfieldID iridescenceIor;
    jfieldID iridescenceThicknessMin;
    jfieldID iridescenceThicknessMax;
    jfieldID iridescenceThicknessTexture;
};
struct MaterialJNI {
    jclass cls;
    jfieldID name;
    jfieldID index;
    jfieldID isSkinned;
    jfieldID pbrMetallicRoughness;
    jfieldID pbrSpecularGlossiness;
    jfieldID clearcoat;
    jfieldID ior;
    jfieldID specular;
    jfieldID sheen;
    jfieldID transmission;
    jfieldID volume;
    jfieldID emissiveStrength;
    jfieldID iridescence;
    jfieldID normalTexture;
    jfieldID occlusionTexture;
    jfieldID emissiveTexture;
    jfieldID emissiveFactor;
    jfieldID alphaCutoff;
    jfieldID alphaMode;
    jfieldID doubleSided;
    jfieldID unlit;
};
struct MeshJNI {
    jclass cls;
    jfieldID name;
    jfieldID material;
    jfieldID positions;
    jfieldID normals;
    jfieldID tangents;
    jfieldID colors;
    jfieldID weights;
    jfieldID bones;
    jfieldID texCoords0NumComponents;
    jfieldID texCoords0;
    jfieldID texCoords1NumComponents;
    jfieldID texCoords1;
    jfieldID aabb;
    jfieldID indices;
    jfieldID vertexCount;
};
struct ModelJNI {
    jclass cls;
    jfieldID name;
    jfieldID meshes;
    jfieldID index;
    jfieldID parentBone;
};
struct BoneJNI {
    jclass cls;
    jfieldID invBindPose;
    jfieldID name;
    jfieldID node;
    jfieldID parent;
    jfieldID parentIndex;
    jfieldID index;
    jfieldID children;
};
struct SkinJNI {
    jclass cls;
    jfieldID name;
    jfieldID bones;
    jfieldID index;
    jfieldID boneRemap;
};
struct NodeJNI {
    jclass cls;
    jfieldID local;
    jfieldID world;
    jfieldID name;
    jfieldID model;
    jfieldID skin;
    jfieldID parent;
    jfieldID children;
    jfieldID index;
    jfieldID nameHash;
};
struct KeyFrameJNI {
    jclass cls;
    jfieldID value;
    jfieldID time;
};
struct NodeAnimationJNI {
    jclass cls;
    jfieldID node;
    jfieldID translationKeys;
    jfieldID rotationKeys;
    jfieldID scaleKeys;
    jfieldID startTime;
    jfieldID endTime;
};
struct AnimationJNI {
    jclass cls;
    jfieldID name;
    jfieldID nodeAnimations;
    jfieldID duration;
};
struct BufferJNI {
    jclass cls;
    jfieldID uri;
    jfieldID buffer;
};
struct SceneJNI {
    jclass cls;
    jfieldID opaqueSceneData;
    jfieldID loadFinalizeFn;
    jfieldID validateFn;
    jfieldID destroyFn;
    jfieldID nodes;
    jfieldID models;
    jfieldID skins;
    jfieldID rootNodes;
    jfieldID animations;
    jfieldID materials;
    jfieldID samplers;
    jfieldID images;
    jfieldID textures;
    jfieldID buffers;
    jfieldID dynamicMaterials;
};
struct OptionsJNI {
    jclass cls;
    jfieldID dummy;
};
struct TypeInfos {
    Vector3JNI m_Vector3JNI;
    QuatJNI m_QuatJNI;
    TransformJNI m_TransformJNI;
    AabbJNI m_AabbJNI;
    ImageJNI m_ImageJNI;
    SamplerJNI m_SamplerJNI;
    TextureJNI m_TextureJNI;
    TextureTransformJNI m_TextureTransformJNI;
    TextureViewJNI m_TextureViewJNI;
    PbrMetallicRoughnessJNI m_PbrMetallicRoughnessJNI;
    PbrSpecularGlossinessJNI m_PbrSpecularGlossinessJNI;
    ClearcoatJNI m_ClearcoatJNI;
    TransmissionJNI m_TransmissionJNI;
    IorJNI m_IorJNI;
    SpecularJNI m_SpecularJNI;
    VolumeJNI m_VolumeJNI;
    SheenJNI m_SheenJNI;
    EmissiveStrengthJNI m_EmissiveStrengthJNI;
    IridescenceJNI m_IridescenceJNI;
    MaterialJNI m_MaterialJNI;
    MeshJNI m_MeshJNI;
    ModelJNI m_ModelJNI;
    BoneJNI m_BoneJNI;
    SkinJNI m_SkinJNI;
    NodeJNI m_NodeJNI;
    KeyFrameJNI m_KeyFrameJNI;
    NodeAnimationJNI m_NodeAnimationJNI;
    AnimationJNI m_AnimationJNI;
    BufferJNI m_BufferJNI;
    SceneJNI m_SceneJNI;
    OptionsJNI m_OptionsJNI;
};
void InitializeJNITypes(JNIEnv* env, TypeInfos* infos);
void FinalizeJNITypes(JNIEnv* env, TypeInfos* infos);

struct ScopedContext {
    JNIEnv*   m_Env;
    TypeInfos m_TypeInfos;
    ScopedContext(JNIEnv* env) : m_Env(env) {
        InitializeJNITypes(m_Env, &m_TypeInfos);
    }
    ~ScopedContext() {
        FinalizeJNITypes(m_Env, &m_TypeInfos);
    }
};


//----------------------------------------
// From C to Jni
//----------------------------------------
jobject C2J_CreateVector3(JNIEnv* env, TypeInfos* types, const Vector3* src);
jobject C2J_CreateQuat(JNIEnv* env, TypeInfos* types, const Quat* src);
jobject C2J_CreateTransform(JNIEnv* env, TypeInfos* types, const Transform* src);
jobject C2J_CreateAabb(JNIEnv* env, TypeInfos* types, const Aabb* src);
jobject C2J_CreateImage(JNIEnv* env, TypeInfos* types, const Image* src);
jobject C2J_CreateSampler(JNIEnv* env, TypeInfos* types, const Sampler* src);
jobject C2J_CreateTexture(JNIEnv* env, TypeInfos* types, const Texture* src);
jobject C2J_CreateTextureTransform(JNIEnv* env, TypeInfos* types, const TextureTransform* src);
jobject C2J_CreateTextureView(JNIEnv* env, TypeInfos* types, const TextureView* src);
jobject C2J_CreatePbrMetallicRoughness(JNIEnv* env, TypeInfos* types, const PbrMetallicRoughness* src);
jobject C2J_CreatePbrSpecularGlossiness(JNIEnv* env, TypeInfos* types, const PbrSpecularGlossiness* src);
jobject C2J_CreateClearcoat(JNIEnv* env, TypeInfos* types, const Clearcoat* src);
jobject C2J_CreateTransmission(JNIEnv* env, TypeInfos* types, const Transmission* src);
jobject C2J_CreateIor(JNIEnv* env, TypeInfos* types, const Ior* src);
jobject C2J_CreateSpecular(JNIEnv* env, TypeInfos* types, const Specular* src);
jobject C2J_CreateVolume(JNIEnv* env, TypeInfos* types, const Volume* src);
jobject C2J_CreateSheen(JNIEnv* env, TypeInfos* types, const Sheen* src);
jobject C2J_CreateEmissiveStrength(JNIEnv* env, TypeInfos* types, const EmissiveStrength* src);
jobject C2J_CreateIridescence(JNIEnv* env, TypeInfos* types, const Iridescence* src);
jobject C2J_CreateMaterial(JNIEnv* env, TypeInfos* types, const Material* src);
jobject C2J_CreateMesh(JNIEnv* env, TypeInfos* types, const Mesh* src);
jobject C2J_CreateModel(JNIEnv* env, TypeInfos* types, const Model* src);
jobject C2J_CreateBone(JNIEnv* env, TypeInfos* types, const Bone* src);
jobject C2J_CreateSkin(JNIEnv* env, TypeInfos* types, const Skin* src);
jobject C2J_CreateNode(JNIEnv* env, TypeInfos* types, const Node* src);
jobject C2J_CreateKeyFrame(JNIEnv* env, TypeInfos* types, const KeyFrame* src);
jobject C2J_CreateNodeAnimation(JNIEnv* env, TypeInfos* types, const NodeAnimation* src);
jobject C2J_CreateAnimation(JNIEnv* env, TypeInfos* types, const Animation* src);
jobject C2J_CreateBuffer(JNIEnv* env, TypeInfos* types, const Buffer* src);
jobject C2J_CreateScene(JNIEnv* env, TypeInfos* types, const Scene* src);
jobject C2J_CreateOptions(JNIEnv* env, TypeInfos* types, const Options* src);
jobjectArray C2J_CreateVector3Array(JNIEnv* env, TypeInfos* types, const Vector3* src, uint32_t src_count);
jobjectArray C2J_CreateVector3PtrArray(JNIEnv* env, TypeInfos* types, const Vector3* const* src, uint32_t src_count);
jobjectArray C2J_CreateQuatArray(JNIEnv* env, TypeInfos* types, const Quat* src, uint32_t src_count);
jobjectArray C2J_CreateQuatPtrArray(JNIEnv* env, TypeInfos* types, const Quat* const* src, uint32_t src_count);
jobjectArray C2J_CreateTransformArray(JNIEnv* env, TypeInfos* types, const Transform* src, uint32_t src_count);
jobjectArray C2J_CreateTransformPtrArray(JNIEnv* env, TypeInfos* types, const Transform* const* src, uint32_t src_count);
jobjectArray C2J_CreateAabbArray(JNIEnv* env, TypeInfos* types, const Aabb* src, uint32_t src_count);
jobjectArray C2J_CreateAabbPtrArray(JNIEnv* env, TypeInfos* types, const Aabb* const* src, uint32_t src_count);
jobjectArray C2J_CreateImageArray(JNIEnv* env, TypeInfos* types, const Image* src, uint32_t src_count);
jobjectArray C2J_CreateImagePtrArray(JNIEnv* env, TypeInfos* types, const Image* const* src, uint32_t src_count);
jobjectArray C2J_CreateSamplerArray(JNIEnv* env, TypeInfos* types, const Sampler* src, uint32_t src_count);
jobjectArray C2J_CreateSamplerPtrArray(JNIEnv* env, TypeInfos* types, const Sampler* const* src, uint32_t src_count);
jobjectArray C2J_CreateTextureArray(JNIEnv* env, TypeInfos* types, const Texture* src, uint32_t src_count);
jobjectArray C2J_CreateTexturePtrArray(JNIEnv* env, TypeInfos* types, const Texture* const* src, uint32_t src_count);
jobjectArray C2J_CreateTextureTransformArray(JNIEnv* env, TypeInfos* types, const TextureTransform* src, uint32_t src_count);
jobjectArray C2J_CreateTextureTransformPtrArray(JNIEnv* env, TypeInfos* types, const TextureTransform* const* src, uint32_t src_count);
jobjectArray C2J_CreateTextureViewArray(JNIEnv* env, TypeInfos* types, const TextureView* src, uint32_t src_count);
jobjectArray C2J_CreateTextureViewPtrArray(JNIEnv* env, TypeInfos* types, const TextureView* const* src, uint32_t src_count);
jobjectArray C2J_CreatePbrMetallicRoughnessArray(JNIEnv* env, TypeInfos* types, const PbrMetallicRoughness* src, uint32_t src_count);
jobjectArray C2J_CreatePbrMetallicRoughnessPtrArray(JNIEnv* env, TypeInfos* types, const PbrMetallicRoughness* const* src, uint32_t src_count);
jobjectArray C2J_CreatePbrSpecularGlossinessArray(JNIEnv* env, TypeInfos* types, const PbrSpecularGlossiness* src, uint32_t src_count);
jobjectArray C2J_CreatePbrSpecularGlossinessPtrArray(JNIEnv* env, TypeInfos* types, const PbrSpecularGlossiness* const* src, uint32_t src_count);
jobjectArray C2J_CreateClearcoatArray(JNIEnv* env, TypeInfos* types, const Clearcoat* src, uint32_t src_count);
jobjectArray C2J_CreateClearcoatPtrArray(JNIEnv* env, TypeInfos* types, const Clearcoat* const* src, uint32_t src_count);
jobjectArray C2J_CreateTransmissionArray(JNIEnv* env, TypeInfos* types, const Transmission* src, uint32_t src_count);
jobjectArray C2J_CreateTransmissionPtrArray(JNIEnv* env, TypeInfos* types, const Transmission* const* src, uint32_t src_count);
jobjectArray C2J_CreateIorArray(JNIEnv* env, TypeInfos* types, const Ior* src, uint32_t src_count);
jobjectArray C2J_CreateIorPtrArray(JNIEnv* env, TypeInfos* types, const Ior* const* src, uint32_t src_count);
jobjectArray C2J_CreateSpecularArray(JNIEnv* env, TypeInfos* types, const Specular* src, uint32_t src_count);
jobjectArray C2J_CreateSpecularPtrArray(JNIEnv* env, TypeInfos* types, const Specular* const* src, uint32_t src_count);
jobjectArray C2J_CreateVolumeArray(JNIEnv* env, TypeInfos* types, const Volume* src, uint32_t src_count);
jobjectArray C2J_CreateVolumePtrArray(JNIEnv* env, TypeInfos* types, const Volume* const* src, uint32_t src_count);
jobjectArray C2J_CreateSheenArray(JNIEnv* env, TypeInfos* types, const Sheen* src, uint32_t src_count);
jobjectArray C2J_CreateSheenPtrArray(JNIEnv* env, TypeInfos* types, const Sheen* const* src, uint32_t src_count);
jobjectArray C2J_CreateEmissiveStrengthArray(JNIEnv* env, TypeInfos* types, const EmissiveStrength* src, uint32_t src_count);
jobjectArray C2J_CreateEmissiveStrengthPtrArray(JNIEnv* env, TypeInfos* types, const EmissiveStrength* const* src, uint32_t src_count);
jobjectArray C2J_CreateIridescenceArray(JNIEnv* env, TypeInfos* types, const Iridescence* src, uint32_t src_count);
jobjectArray C2J_CreateIridescencePtrArray(JNIEnv* env, TypeInfos* types, const Iridescence* const* src, uint32_t src_count);
jobjectArray C2J_CreateMaterialArray(JNIEnv* env, TypeInfos* types, const Material* src, uint32_t src_count);
jobjectArray C2J_CreateMaterialPtrArray(JNIEnv* env, TypeInfos* types, const Material* const* src, uint32_t src_count);
jobjectArray C2J_CreateMeshArray(JNIEnv* env, TypeInfos* types, const Mesh* src, uint32_t src_count);
jobjectArray C2J_CreateMeshPtrArray(JNIEnv* env, TypeInfos* types, const Mesh* const* src, uint32_t src_count);
jobjectArray C2J_CreateModelArray(JNIEnv* env, TypeInfos* types, const Model* src, uint32_t src_count);
jobjectArray C2J_CreateModelPtrArray(JNIEnv* env, TypeInfos* types, const Model* const* src, uint32_t src_count);
jobjectArray C2J_CreateBoneArray(JNIEnv* env, TypeInfos* types, const Bone* src, uint32_t src_count);
jobjectArray C2J_CreateBonePtrArray(JNIEnv* env, TypeInfos* types, const Bone* const* src, uint32_t src_count);
jobjectArray C2J_CreateSkinArray(JNIEnv* env, TypeInfos* types, const Skin* src, uint32_t src_count);
jobjectArray C2J_CreateSkinPtrArray(JNIEnv* env, TypeInfos* types, const Skin* const* src, uint32_t src_count);
jobjectArray C2J_CreateNodeArray(JNIEnv* env, TypeInfos* types, const Node* src, uint32_t src_count);
jobjectArray C2J_CreateNodePtrArray(JNIEnv* env, TypeInfos* types, const Node* const* src, uint32_t src_count);
jobjectArray C2J_CreateKeyFrameArray(JNIEnv* env, TypeInfos* types, const KeyFrame* src, uint32_t src_count);
jobjectArray C2J_CreateKeyFramePtrArray(JNIEnv* env, TypeInfos* types, const KeyFrame* const* src, uint32_t src_count);
jobjectArray C2J_CreateNodeAnimationArray(JNIEnv* env, TypeInfos* types, const NodeAnimation* src, uint32_t src_count);
jobjectArray C2J_CreateNodeAnimationPtrArray(JNIEnv* env, TypeInfos* types, const NodeAnimation* const* src, uint32_t src_count);
jobjectArray C2J_CreateAnimationArray(JNIEnv* env, TypeInfos* types, const Animation* src, uint32_t src_count);
jobjectArray C2J_CreateAnimationPtrArray(JNIEnv* env, TypeInfos* types, const Animation* const* src, uint32_t src_count);
jobjectArray C2J_CreateBufferArray(JNIEnv* env, TypeInfos* types, const Buffer* src, uint32_t src_count);
jobjectArray C2J_CreateBufferPtrArray(JNIEnv* env, TypeInfos* types, const Buffer* const* src, uint32_t src_count);
jobjectArray C2J_CreateSceneArray(JNIEnv* env, TypeInfos* types, const Scene* src, uint32_t src_count);
jobjectArray C2J_CreateScenePtrArray(JNIEnv* env, TypeInfos* types, const Scene* const* src, uint32_t src_count);
jobjectArray C2J_CreateOptionsArray(JNIEnv* env, TypeInfos* types, const Options* src, uint32_t src_count);
jobjectArray C2J_CreateOptionsPtrArray(JNIEnv* env, TypeInfos* types, const Options* const* src, uint32_t src_count);
//----------------------------------------
// From Jni to C
//----------------------------------------
bool J2C_CreateVector3(JNIEnv* env, TypeInfos* types, jobject obj, Vector3* out);
bool J2C_CreateQuat(JNIEnv* env, TypeInfos* types, jobject obj, Quat* out);
bool J2C_CreateTransform(JNIEnv* env, TypeInfos* types, jobject obj, Transform* out);
bool J2C_CreateAabb(JNIEnv* env, TypeInfos* types, jobject obj, Aabb* out);
bool J2C_CreateImage(JNIEnv* env, TypeInfos* types, jobject obj, Image* out);
bool J2C_CreateSampler(JNIEnv* env, TypeInfos* types, jobject obj, Sampler* out);
bool J2C_CreateTexture(JNIEnv* env, TypeInfos* types, jobject obj, Texture* out);
bool J2C_CreateTextureTransform(JNIEnv* env, TypeInfos* types, jobject obj, TextureTransform* out);
bool J2C_CreateTextureView(JNIEnv* env, TypeInfos* types, jobject obj, TextureView* out);
bool J2C_CreatePbrMetallicRoughness(JNIEnv* env, TypeInfos* types, jobject obj, PbrMetallicRoughness* out);
bool J2C_CreatePbrSpecularGlossiness(JNIEnv* env, TypeInfos* types, jobject obj, PbrSpecularGlossiness* out);
bool J2C_CreateClearcoat(JNIEnv* env, TypeInfos* types, jobject obj, Clearcoat* out);
bool J2C_CreateTransmission(JNIEnv* env, TypeInfos* types, jobject obj, Transmission* out);
bool J2C_CreateIor(JNIEnv* env, TypeInfos* types, jobject obj, Ior* out);
bool J2C_CreateSpecular(JNIEnv* env, TypeInfos* types, jobject obj, Specular* out);
bool J2C_CreateVolume(JNIEnv* env, TypeInfos* types, jobject obj, Volume* out);
bool J2C_CreateSheen(JNIEnv* env, TypeInfos* types, jobject obj, Sheen* out);
bool J2C_CreateEmissiveStrength(JNIEnv* env, TypeInfos* types, jobject obj, EmissiveStrength* out);
bool J2C_CreateIridescence(JNIEnv* env, TypeInfos* types, jobject obj, Iridescence* out);
bool J2C_CreateMaterial(JNIEnv* env, TypeInfos* types, jobject obj, Material* out);
bool J2C_CreateMesh(JNIEnv* env, TypeInfos* types, jobject obj, Mesh* out);
bool J2C_CreateModel(JNIEnv* env, TypeInfos* types, jobject obj, Model* out);
bool J2C_CreateBone(JNIEnv* env, TypeInfos* types, jobject obj, Bone* out);
bool J2C_CreateSkin(JNIEnv* env, TypeInfos* types, jobject obj, Skin* out);
bool J2C_CreateNode(JNIEnv* env, TypeInfos* types, jobject obj, Node* out);
bool J2C_CreateKeyFrame(JNIEnv* env, TypeInfos* types, jobject obj, KeyFrame* out);
bool J2C_CreateNodeAnimation(JNIEnv* env, TypeInfos* types, jobject obj, NodeAnimation* out);
bool J2C_CreateAnimation(JNIEnv* env, TypeInfos* types, jobject obj, Animation* out);
bool J2C_CreateBuffer(JNIEnv* env, TypeInfos* types, jobject obj, Buffer* out);
bool J2C_CreateScene(JNIEnv* env, TypeInfos* types, jobject obj, Scene* out);
bool J2C_CreateOptions(JNIEnv* env, TypeInfos* types, jobject obj, Options* out);
Vector3* J2C_CreateVector3Array(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateVector3ArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Vector3* dst, uint32_t dst_count);
Vector3** J2C_CreateVector3PtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateVector3PtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Vector3** dst, uint32_t dst_count);
Quat* J2C_CreateQuatArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateQuatArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Quat* dst, uint32_t dst_count);
Quat** J2C_CreateQuatPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateQuatPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Quat** dst, uint32_t dst_count);
Transform* J2C_CreateTransformArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateTransformArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Transform* dst, uint32_t dst_count);
Transform** J2C_CreateTransformPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateTransformPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Transform** dst, uint32_t dst_count);
Aabb* J2C_CreateAabbArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateAabbArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Aabb* dst, uint32_t dst_count);
Aabb** J2C_CreateAabbPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateAabbPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Aabb** dst, uint32_t dst_count);
Image* J2C_CreateImageArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateImageArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Image* dst, uint32_t dst_count);
Image** J2C_CreateImagePtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateImagePtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Image** dst, uint32_t dst_count);
Sampler* J2C_CreateSamplerArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateSamplerArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Sampler* dst, uint32_t dst_count);
Sampler** J2C_CreateSamplerPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateSamplerPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Sampler** dst, uint32_t dst_count);
Texture* J2C_CreateTextureArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateTextureArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Texture* dst, uint32_t dst_count);
Texture** J2C_CreateTexturePtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateTexturePtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Texture** dst, uint32_t dst_count);
TextureTransform* J2C_CreateTextureTransformArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateTextureTransformArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, TextureTransform* dst, uint32_t dst_count);
TextureTransform** J2C_CreateTextureTransformPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateTextureTransformPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, TextureTransform** dst, uint32_t dst_count);
TextureView* J2C_CreateTextureViewArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateTextureViewArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, TextureView* dst, uint32_t dst_count);
TextureView** J2C_CreateTextureViewPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateTextureViewPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, TextureView** dst, uint32_t dst_count);
PbrMetallicRoughness* J2C_CreatePbrMetallicRoughnessArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreatePbrMetallicRoughnessArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, PbrMetallicRoughness* dst, uint32_t dst_count);
PbrMetallicRoughness** J2C_CreatePbrMetallicRoughnessPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreatePbrMetallicRoughnessPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, PbrMetallicRoughness** dst, uint32_t dst_count);
PbrSpecularGlossiness* J2C_CreatePbrSpecularGlossinessArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreatePbrSpecularGlossinessArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, PbrSpecularGlossiness* dst, uint32_t dst_count);
PbrSpecularGlossiness** J2C_CreatePbrSpecularGlossinessPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreatePbrSpecularGlossinessPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, PbrSpecularGlossiness** dst, uint32_t dst_count);
Clearcoat* J2C_CreateClearcoatArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateClearcoatArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Clearcoat* dst, uint32_t dst_count);
Clearcoat** J2C_CreateClearcoatPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateClearcoatPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Clearcoat** dst, uint32_t dst_count);
Transmission* J2C_CreateTransmissionArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateTransmissionArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Transmission* dst, uint32_t dst_count);
Transmission** J2C_CreateTransmissionPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateTransmissionPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Transmission** dst, uint32_t dst_count);
Ior* J2C_CreateIorArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateIorArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Ior* dst, uint32_t dst_count);
Ior** J2C_CreateIorPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateIorPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Ior** dst, uint32_t dst_count);
Specular* J2C_CreateSpecularArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateSpecularArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Specular* dst, uint32_t dst_count);
Specular** J2C_CreateSpecularPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateSpecularPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Specular** dst, uint32_t dst_count);
Volume* J2C_CreateVolumeArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateVolumeArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Volume* dst, uint32_t dst_count);
Volume** J2C_CreateVolumePtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateVolumePtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Volume** dst, uint32_t dst_count);
Sheen* J2C_CreateSheenArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateSheenArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Sheen* dst, uint32_t dst_count);
Sheen** J2C_CreateSheenPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateSheenPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Sheen** dst, uint32_t dst_count);
EmissiveStrength* J2C_CreateEmissiveStrengthArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateEmissiveStrengthArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, EmissiveStrength* dst, uint32_t dst_count);
EmissiveStrength** J2C_CreateEmissiveStrengthPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateEmissiveStrengthPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, EmissiveStrength** dst, uint32_t dst_count);
Iridescence* J2C_CreateIridescenceArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateIridescenceArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Iridescence* dst, uint32_t dst_count);
Iridescence** J2C_CreateIridescencePtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateIridescencePtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Iridescence** dst, uint32_t dst_count);
Material* J2C_CreateMaterialArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateMaterialArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Material* dst, uint32_t dst_count);
Material** J2C_CreateMaterialPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateMaterialPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Material** dst, uint32_t dst_count);
Mesh* J2C_CreateMeshArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateMeshArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Mesh* dst, uint32_t dst_count);
Mesh** J2C_CreateMeshPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateMeshPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Mesh** dst, uint32_t dst_count);
Model* J2C_CreateModelArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateModelArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Model* dst, uint32_t dst_count);
Model** J2C_CreateModelPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateModelPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Model** dst, uint32_t dst_count);
Bone* J2C_CreateBoneArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateBoneArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Bone* dst, uint32_t dst_count);
Bone** J2C_CreateBonePtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateBonePtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Bone** dst, uint32_t dst_count);
Skin* J2C_CreateSkinArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateSkinArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Skin* dst, uint32_t dst_count);
Skin** J2C_CreateSkinPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateSkinPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Skin** dst, uint32_t dst_count);
Node* J2C_CreateNodeArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateNodeArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Node* dst, uint32_t dst_count);
Node** J2C_CreateNodePtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateNodePtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Node** dst, uint32_t dst_count);
KeyFrame* J2C_CreateKeyFrameArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateKeyFrameArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, KeyFrame* dst, uint32_t dst_count);
KeyFrame** J2C_CreateKeyFramePtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateKeyFramePtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, KeyFrame** dst, uint32_t dst_count);
NodeAnimation* J2C_CreateNodeAnimationArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateNodeAnimationArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, NodeAnimation* dst, uint32_t dst_count);
NodeAnimation** J2C_CreateNodeAnimationPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateNodeAnimationPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, NodeAnimation** dst, uint32_t dst_count);
Animation* J2C_CreateAnimationArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateAnimationArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Animation* dst, uint32_t dst_count);
Animation** J2C_CreateAnimationPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateAnimationPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Animation** dst, uint32_t dst_count);
Buffer* J2C_CreateBufferArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateBufferArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Buffer* dst, uint32_t dst_count);
Buffer** J2C_CreateBufferPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateBufferPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Buffer** dst, uint32_t dst_count);
Scene* J2C_CreateSceneArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateSceneArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Scene* dst, uint32_t dst_count);
Scene** J2C_CreateScenePtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateScenePtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Scene** dst, uint32_t dst_count);
Options* J2C_CreateOptionsArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateOptionsArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Options* dst, uint32_t dst_count);
Options** J2C_CreateOptionsPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateOptionsPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Options** dst, uint32_t dst_count);
} // jni
} // dmModelImporter
