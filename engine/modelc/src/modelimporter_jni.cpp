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

#include "modelimporter.h"

#include <jni.h> // JDK
#include <jni/jni_util.h> // defold

#include <Modelimporter_jni.h>

#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>
#include <dlib/hashtable.h>

namespace dmModelImporter
{

// ******************************************************************************************************************

template<typename DefoldType>
static void AddToCache(dmHashTable64<jobject>& cache, DefoldType* key, jobject obj)
{
    if (cache.Full())
    {
        uint32_t cap = cache.Capacity() + 256;
        cache.SetCapacity((cap*3/2), cap);
    }
    cache.Put((uintptr_t)key, obj);
}

template<typename DefoldType>
static jobject GetFromCache(dmHashTable64<jobject>& cache, DefoldType* key)
{
    jobject* pobj = cache.Get((uintptr_t)key);
    if (!pobj)
        return 0;
    return *pobj;
}


// TODO: Move to jni_util.h
static jobjectArray CreateObjectArray(JNIEnv* env, jclass cls, const dmArray<jobject>& values)
{
    uint32_t count = values.Size();
    jobjectArray arr = env->NewObjectArray(count, cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        env->SetObjectArrayElement(arr, i, values[i]);
    }
    return arr;
}

// **************************************************
//

static jobjectArray CreateImagesArray(JNIEnv* env, dmModelImporter::jni::TypeInfos* types,
                                    uint32_t count, const dmModelImporter::Image* images,
                                    dmHashTable64<jobject>& cache)
{
    jobjectArray arr = env->NewObjectArray(count, types->m_ImageJNI.cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        const Image* image = &images[i];

        jobject obj = env->AllocObject(types->m_ImageJNI.cls);
        dmJNI::SetString(env, obj, types->m_ImageJNI.name, image->m_Name);
        dmJNI::SetString(env, obj, types->m_ImageJNI.uri, image->m_Uri);
        dmJNI::SetString(env, obj, types->m_ImageJNI.mimeType, image->m_MimeType);
        dmJNI::SetUInt(env, obj, types->m_ImageJNI.index, image->m_Index);

        dmJNI::SetObject(env, obj, types->m_ImageJNI.buffer, GetFromCache(cache, image->m_Buffer));

        env->SetObjectArrayElement(arr, i, obj);

        AddToCache(cache, image, obj);
    }
    return arr;

}

static jobjectArray CreateSamplersArray(JNIEnv* env, dmModelImporter::jni::TypeInfos* types,
                                    uint32_t count, const dmModelImporter::Sampler* samplers,
                                    dmHashTable64<jobject>& cache)
{
    jobjectArray arr = env->NewObjectArray(count, types->m_SamplerJNI.cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        const Sampler* sampler = &samplers[i];

        jobject obj = C2J_CreateSampler(env, types, sampler);
        env->SetObjectArrayElement(arr, i, obj);

        AddToCache(cache, sampler, obj);
    }
    return arr;

}

static jobjectArray CreateTexturesArray(JNIEnv* env, dmModelImporter::jni::TypeInfos* types,
                                    uint32_t count, const dmModelImporter::Texture* textures,
                                    dmHashTable64<jobject>& cache)
{
    jobjectArray arr = env->NewObjectArray(count, types->m_TextureJNI.cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        const Texture* texture = &textures[i];

        jobject obj = env->AllocObject(types->m_TextureJNI.cls);
        dmJNI::SetString(env, obj, types->m_TextureJNI.name, texture->m_Name);
        dmJNI::SetUInt(env, obj, types->m_TextureJNI.index, texture->m_Index);

        dmJNI::SetObject(env, obj, types->m_TextureJNI.image, GetFromCache(cache, texture->m_Image));
        dmJNI::SetObject(env, obj, types->m_TextureJNI.sampler, GetFromCache(cache, texture->m_Sampler));
        dmJNI::SetObject(env, obj, types->m_TextureJNI.basisuImage, GetFromCache(cache, texture->m_BasisuImage));
        env->SetObjectArrayElement(arr, i, obj);

        AddToCache(cache, texture, obj);
    }
    return arr;

}

// **************************************************
// Material

static jobjectArray CreateMaterialsArray(JNIEnv* env, dmModelImporter::jni::TypeInfos* types,
                        uint32_t count, const dmModelImporter::Material* materials,
                        uint32_t dynamic_count, const dmModelImporter::Material* const * dynamic_materials,
                        dmArray<jobject>& nodes)
{
    uint32_t total_count = count + dynamic_count;
    nodes.SetCapacity(total_count);
    nodes.SetSize(total_count);

    jobjectArray arr = env->NewObjectArray(total_count, types->m_MaterialJNI.cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        const dmModelImporter::Material* material = &materials[i];
        jobject obj = C2J_CreateMaterial(env, types, material);
        nodes[material->m_Index] = obj;
        env->SetObjectArrayElement(arr, material->m_Index, obj);
    }
    for (uint32_t i = 0; i < dynamic_count; ++i)
    {
        const dmModelImporter::Material* material = dynamic_materials[i];
        jobject obj = C2J_CreateMaterial(env, types, material);
        nodes[material->m_Index] = obj;
        env->SetObjectArrayElement(arr, material->m_Index, obj);
    }

    return arr;
}

// **************************************************
// Nodes

static jobject CreateNode(JNIEnv* env, dmModelImporter::jni::TypeInfos* types, const dmModelImporter::Node* node)
{
    jobject obj = env->AllocObject(types->m_NodeJNI.cls);
    dmJNI::SetInt(env, obj, types->m_NodeJNI.index, node->m_Index);
    dmJNI::SetString(env, obj, types->m_NodeJNI.name, node->m_Name);
    dmJNI::SetObject(env, obj, types->m_NodeJNI.local, C2J_CreateTransform(env, types, &node->m_Local));
    dmJNI::SetObject(env, obj, types->m_NodeJNI.world, C2J_CreateTransform(env, types, &node->m_World));
    return obj;
}

static void CreateNodes(JNIEnv* env, dmModelImporter::jni::TypeInfos* types, const dmModelImporter::Scene* scene, dmArray<jobject>& nodes)
{
    uint32_t count = scene->m_Nodes.Size();
    nodes.SetCapacity(count);
    nodes.SetSize(count);

    for (uint32_t i = 0; i < count; ++i)
    {
        const Node* node = &scene->m_Nodes[i];
        nodes[node->m_Index] = CreateNode(env, types, node);
    }

    for (uint32_t i = 0; i < count; ++i)
    {
        const Node* node = &scene->m_Nodes[i];

        if (node->m_Parent != 0)
        {
            dmJNI::SetObject(env, nodes[node->m_Index], types->m_NodeJNI.parent, nodes[node->m_Parent->m_Index]);
        }

        jobjectArray childrenArray = env->NewObjectArray(node->m_Children.Size(), types->m_NodeJNI.cls, 0);
        for (uint32_t c = 0; c < node->m_Children.Size(); ++c)
        {
            dmModelImporter::Node* child = node->m_Children[c];
            env->SetObjectArrayElement(childrenArray, c, nodes[child->m_Index]);
        }
        env->SetObjectField(nodes[node->m_Index], types->m_NodeJNI.children, childrenArray);
        env->DeleteLocalRef(childrenArray);
    }
}

static void FixupNodeReferences(JNIEnv* env, dmModelImporter::jni::TypeInfos* types, const dmModelImporter::Scene* scene, const dmArray<jobject>& skins, const dmArray<jobject>& models, const dmArray<jobject>& nodes)
{
    uint32_t count = scene->m_Nodes.Size();
    for (uint32_t i = 0; i < count; ++i)
    {
        const Node* node = &scene->m_Nodes[i];
        if (node->m_Skin)
        {
            jobject node_obj = nodes[node->m_Index];
            jobject skin_obj = skins[node->m_Skin->m_Index];
            dmJNI::SetObject(env, node_obj, types->m_NodeJNI.skin, skin_obj);
        }

        if (node->m_Model)
        {
            jobject node_obj = nodes[node->m_Index];
            jobject model_obj = models[node->m_Model->m_Index];
            dmJNI::SetObject(env, node_obj, types->m_NodeJNI.model, model_obj);
        }

    }
}

static void FixupModelBones(JNIEnv* env, dmModelImporter::jni::TypeInfos* types, const dmModelImporter::Scene* scene,
                            const dmArray<jobject>& models, dmHashTable64<jobject>& cache)
{
    uint32_t count = scene->m_Models.Size();
    for (uint32_t i = 0; i < count; ++i)
    {
        const Model* model = &scene->m_Models[i];
        if (!model->m_ParentBone)
            continue;

        jobject model_obj = models[model->m_Index]; // TODO: SHould be able to use the object cache here as well.
        jobject* bone_obj_p = cache.Get((uintptr_t)model->m_ParentBone);
        if (bone_obj_p)
            dmJNI::SetObject(env, model_obj, types->m_ModelJNI.parentBone, *bone_obj_p);
        else
            dmLogError("Failed to find the bone parent pointer");
    }
}

// **************************************************
// Meshes

static jobject CreateMesh(JNIEnv* env, dmModelImporter::jni::TypeInfos* types, const dmArray<jobject>& materials, const dmModelImporter::Mesh* mesh)
{
    jobject obj = C2J_CreateMesh(env, types, mesh);
    // Note:
    // The step above create a material object.
    // But it might be good if they actually share the references
    // so we immediately set another material to the object

    if (mesh->m_Material)
        dmJNI::SetObject(env, obj, types->m_MeshJNI.material, materials[mesh->m_Material->m_Index]);

    return obj;
}

static jobjectArray CreateMeshesArray(JNIEnv* env, dmModelImporter::jni::TypeInfos* types, const dmArray<jobject>& materials, uint32_t count, const dmModelImporter::Mesh* meshes)
{
    jobjectArray arr = env->NewObjectArray(count, types->m_MeshJNI.cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        jobject o = CreateMesh(env, types, materials, &meshes[i]);
        env->SetObjectArrayElement(arr, i, o);
        env->DeleteLocalRef(o);
    }
    return arr;
}

static jobject CreateModel(JNIEnv* env, dmModelImporter::jni::TypeInfos* types, const dmArray<jobject>& materials, const dmModelImporter::Model* model)
{
    jobject obj = env->AllocObject(types->m_ModelJNI.cls);
    dmJNI::SetInt(env, obj, types->m_ModelJNI.index, model->m_Index);
    dmJNI::SetString(env, obj, types->m_ModelJNI.name, model->m_Name);
    // Bones aren't created yet. See FixupModelBones
    //dmJNI::SetObject(env, obj, types->m_ModelJNI.boneParent, model->m_ParentBone ? bones[model->m_ParentBone->m_Index] : 0);
    dmJNI::SetObjectDeref(env, obj, types->m_ModelJNI.meshes, CreateMeshesArray(env, types, materials, model->m_Meshes.Size(), model->m_Meshes.Begin()));
    return obj;
}

static void CreateModels(JNIEnv* env, dmModelImporter::jni::TypeInfos* types, const dmModelImporter::Scene* scene, const dmArray<jobject>& materials, dmArray<jobject>& models)
{
    uint32_t count = scene->m_Models.Size();
    models.SetCapacity(count);
    models.SetSize(count);

    for (uint32_t i = 0; i < count; ++i)
    {
        const Model* model = &scene->m_Models[i];
        models[model->m_Index] = CreateModel(env, types, materials, model);
    }
}

// **************************************************
// Animations

static jobjectArray CreateKeyFramesArray(JNIEnv* env, dmModelImporter::jni::TypeInfos* types, const dmModelImporter::KeyFrame* key_frames, uint32_t count)
{
    jobjectArray arr = env->NewObjectArray(count, types->m_KeyFrameJNI.cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        jobject o = C2J_CreateKeyFrame(env, types, &key_frames[i]);
        env->SetObjectArrayElement(arr, i, o);
        env->DeleteLocalRef(o);
    }
    return arr;
}

static jobject CreateNodeAnimation(JNIEnv* env, dmModelImporter::jni::TypeInfos* types, const dmModelImporter::NodeAnimation* node_anim, const dmArray<jobject>& nodes)
{
    jobject obj = env->AllocObject(types->m_NodeAnimationJNI.cls);
    // Currently neeeded to avoid generating new node instances
    dmJNI::SetObject(env, obj, types->m_NodeAnimationJNI.node, nodes[node_anim->m_Node->m_Index]);
    // from C2J_CreateNodeAnimation
    dmJNI::SetObjectDeref(env, obj, types->m_NodeAnimationJNI.translationKeys, CreateKeyFramesArray(env, types, node_anim->m_TranslationKeys.Begin(), node_anim->m_TranslationKeys.Size()));
    dmJNI::SetObjectDeref(env, obj, types->m_NodeAnimationJNI.rotationKeys, CreateKeyFramesArray(env, types, node_anim->m_RotationKeys.Begin(), node_anim->m_RotationKeys.Size()));
    dmJNI::SetObjectDeref(env, obj, types->m_NodeAnimationJNI.scaleKeys, CreateKeyFramesArray(env, types, node_anim->m_ScaleKeys.Begin(), node_anim->m_ScaleKeys.Size()));
    dmJNI::SetFloat(env, obj, types->m_NodeAnimationJNI.startTime, node_anim->m_StartTime);
    dmJNI::SetFloat(env, obj, types->m_NodeAnimationJNI.endTime, node_anim->m_EndTime);
    return obj;
}

static jobjectArray CreateNodeAnimationsArray(JNIEnv* env, dmModelImporter::jni::TypeInfos* types, uint32_t count, const dmModelImporter::NodeAnimation* node_anim, const dmArray<jobject>& nodes)
{
    jobjectArray arr = env->NewObjectArray(count, types->m_NodeAnimationJNI.cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        jobject o = CreateNodeAnimation(env, types, &node_anim[i], nodes);
        env->SetObjectArrayElement(arr, i, o);
        env->DeleteLocalRef(o);
    }
    return arr;
}

static jobject CreateAnimation(JNIEnv* env, dmModelImporter::jni::TypeInfos* types, const dmModelImporter::Animation* animation, const dmArray<jobject>& nodes)
{
    jobject obj = env->AllocObject(types->m_AnimationJNI.cls);
    dmJNI::SetString(env, obj, types->m_AnimationJNI.name, animation->m_Name);
    dmJNI::SetFloat(env, obj, types->m_AnimationJNI.duration, animation->m_Duration);
    dmJNI::SetObjectDeref(env, obj, types->m_AnimationJNI.nodeAnimations, CreateNodeAnimationsArray(env, types, animation->m_NodeAnimations.Size(), animation->m_NodeAnimations.Begin(), nodes));
    return obj;
}

static jobjectArray CreateAnimationsArray(JNIEnv* env, dmModelImporter::jni::TypeInfos* types, const dmModelImporter::Animation* animations, uint32_t count, const dmArray<jobject>& nodes)
{
    jobjectArray arr = env->NewObjectArray(count, types->m_AnimationJNI.cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        env->SetObjectArrayElement(arr, i, CreateAnimation(env, types, &animations[i], nodes));
    }
    return arr;
}

// **************************************************
// Bones

static jobject CreateBone(JNIEnv* env, dmModelImporter::jni::TypeInfos* types, const dmModelImporter::Bone* bone, const dmArray<jobject>& nodes)
{
    jobject obj = env->AllocObject(types->m_BoneJNI.cls);
    dmJNI::SetInt(env, obj, types->m_BoneJNI.index, bone->m_Index);
    dmJNI::SetString(env, obj, types->m_BoneJNI.name, bone->m_Name);
    dmJNI::SetObject(env, obj, types->m_BoneJNI.invBindPose, C2J_CreateTransform(env, types, &bone->m_InvBindPose));
    if (bone->m_Node != 0) // A generated root bone doesn't have a corresponding Node
        dmJNI::SetObject(env, obj, types->m_BoneJNI.node, nodes[bone->m_Node->m_Index]);
    else
        dmJNI::SetObject(env, obj, types->m_BoneJNI.node, 0);
    return obj;
}

static jobjectArray CreateBonesArray(JNIEnv* env, dmModelImporter::jni::TypeInfos* types,
                                    uint32_t count, const dmModelImporter::Bone* bones,
                                    const dmArray<jobject>& nodes,
                                    dmHashTable64<jobject>& cache)
{
    dmArray<jobject> tmp;
    tmp.SetCapacity(count);
    tmp.SetSize(count);

    jobjectArray arr = env->NewObjectArray(count, types->m_BoneJNI.cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        const Bone* bone = &bones[i];
        tmp[bone->m_Index] = CreateBone(env, types, bone, nodes);
        env->SetObjectArrayElement(arr, i, tmp[bone->m_Index]);

        if (cache.Full())
        {
            uint32_t cap = cache.Capacity() + 1024;
            cache.SetCapacity((cap*2)/3, cap);
        }
        cache.Put((uintptr_t)bone, tmp[bone->m_Index]);
    }

    for (uint32_t i = 0; i < count; ++i)
    {
        const Bone* bone = &bones[i];
        if (bone->m_ParentIndex != INVALID_INDEX)
            dmJNI::SetObject(env, tmp[bone->m_Index], types->m_BoneJNI.parent, tmp[bone->m_ParentIndex]);
        else
            dmJNI::SetObject(env, tmp[bone->m_Index], types->m_BoneJNI.parent, 0);
    }

    return arr;
}

static void CreateBones(JNIEnv* env, dmModelImporter::jni::TypeInfos* types, const dmModelImporter::Scene* scene,
                        const dmArray<jobject>& skins, const dmArray<jobject>& nodes,
                        dmHashTable64<jobject>& cache)
{
    uint32_t count = scene->m_Skins.Size();
    for (uint32_t i = 0; i < count; ++i)
    {
        const Skin* skin = &scene->m_Skins[i];
        jobject skin_obj = skins[skin->m_Index];

        dmJNI::SetObjectDeref(env, skin_obj, types->m_SkinJNI.bones, CreateBonesArray(env, types, skin->m_Bones.Size(), skin->m_Bones.Begin(), nodes, cache));
    }
}

// **************************************************
// Skins

static jobject CreateSkin(JNIEnv* env, dmModelImporter::jni::TypeInfos* types, const dmModelImporter::Skin* skin)
{
    jobject obj = env->AllocObject(types->m_SkinJNI.cls);
    dmJNI::SetInt(env, obj, types->m_SkinJNI.index, skin->m_Index);
    dmJNI::SetString(env, obj, types->m_SkinJNI.name, skin->m_Name);
    return obj;
}

static void CreateSkins(JNIEnv* env, dmModelImporter::jni::TypeInfos* types, const dmModelImporter::Scene* scene, dmArray<jobject>& skins)
{
    uint32_t count = scene->m_Skins.Size();
    skins.SetCapacity(count);
    skins.SetSize(count);

    for (uint32_t i = 0; i < count; ++i)
    {
        const Skin* skin = &scene->m_Skins[i];
        skins[skin->m_Index] = CreateSkin(env, types, skin);
    }
}

static void DeleteLocalRefs(JNIEnv* env, dmArray<jobject>& objects)
{
    for (uint32_t i = 0; i < objects.Size(); ++i)
    {
        env->DeleteLocalRef(objects[i]);
    }
}

// In our case we want to return array of length 0
static jobjectArray CreateBufferArray(JNIEnv* env, dmModelImporter::jni::TypeInfos* types, const Buffer* src, uint32_t src_count, dmHashTable64<jobject>& cache)
{
    uint32_t count = src ? src_count : 0;
    jobjectArray arr = env->NewObjectArray(count, types->m_BufferJNI.cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        jobject obj = C2J_CreateBuffer(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);

        AddToCache(cache, &src[i], obj);
    }
    return arr;
}

static jobject CreateJavaScene(JNIEnv* env, const dmModelImporter::Scene* scene)
{
    dmLogDebug("CreateJavaScene: env = %p\n", env);
    dmModelImporter::jni::ScopedContext jni_scope(env);
    dmModelImporter::jni::TypeInfos* types = &jni_scope.m_TypeInfos;

    jobject obj = env->AllocObject(types->m_SceneJNI.cls);

    dmArray<jobject> models;
    dmArray<jobject> skins;
    dmArray<jobject> nodes;
    dmArray<jobject> materials;
    dmArray<jobject> roots;
    dmHashTable64<jobject> object_cache;

    dmJNI::SetObjectDeref(env, obj, types->m_SceneJNI.buffers, CreateBufferArray(env, types, scene->m_Buffers.Begin(), scene->m_Buffers.Size(), object_cache));

    dmJNI::SetObjectDeref(env, obj, types->m_SceneJNI.images, CreateImagesArray(env, types, scene->m_Images.Size(), scene->m_Images.Begin(), object_cache));
    dmJNI::SetObjectDeref(env, obj, types->m_SceneJNI.samplers, CreateSamplersArray(env, types, scene->m_Samplers.Size(), scene->m_Samplers.Begin(), object_cache));
    dmJNI::SetObjectDeref(env, obj, types->m_SceneJNI.textures, CreateTexturesArray(env, types, scene->m_Textures.Size(), scene->m_Textures.Begin(), object_cache));

    dmJNI::SetObjectDeref(env, obj, types->m_SceneJNI.materials, CreateMaterialsArray(env, types,
                                                                        scene->m_Materials.Size(), scene->m_Materials.Begin(),
                                                                        scene->m_DynamicMaterials.Size(), scene->m_DynamicMaterials.Begin(),
                                                                        materials));


    // Creates all nodes, but doesn't set setting skins/models
    CreateNodes(env, types, scene, nodes);
    CreateSkins(env, types, scene, skins);
    CreateModels(env, types, scene, materials, models);
    CreateBones(env, types, scene, skins, nodes, object_cache);

    // Set the skin+model to the nodes
    FixupNodeReferences(env, types, scene, skins, models, nodes);
    FixupModelBones(env, types, scene, models, object_cache);

    //env->EnsureLocalCapacity(8192); // TODO: Set this in the "C2J_Create*Array" functions

    {
        uint32_t count = scene->m_RootNodes.Size();
        roots.SetCapacity(count);
        for (uint32_t i = 0; i < count; ++i)
        {
            const dmModelImporter::Node* root = scene->m_RootNodes[i];
            roots.Push(nodes[root->m_Index]);
        }
    }

    dmJNI::SetObjectDeref(env, obj, types->m_SceneJNI.rootNodes, CreateObjectArray(env, types->m_NodeJNI.cls, roots));
    dmJNI::SetObjectDeref(env, obj, types->m_SceneJNI.nodes, CreateObjectArray(env, types->m_NodeJNI.cls, nodes));
    dmJNI::SetObjectDeref(env, obj, types->m_SceneJNI.skins, CreateObjectArray(env, types->m_SkinJNI.cls, skins));
    dmJNI::SetObjectDeref(env, obj, types->m_SceneJNI.models, CreateObjectArray(env, types->m_ModelJNI.cls, models));
    dmJNI::SetObjectDeref(env, obj, types->m_SceneJNI.animations, CreateAnimationsArray(env, types, scene->m_Animations.Begin(), scene->m_Animations.Size(), nodes));

    DeleteLocalRefs(env, nodes);
    DeleteLocalRefs(env, skins);
    DeleteLocalRefs(env, models);
    DeleteLocalRefs(env, materials);

    return obj;
}

} // namespace

static jobject LoadFromBufferInternal(JNIEnv* env, jclass cls, jstring _path, jbyteArray array, jobject data_resolver)
{
    dmLogDebug("CreateJavaScene: env = %p\n", env);

    dmJNI::ScopedString j_path(env, _path);
    const char* path = j_path.m_String;

    const char* suffix = strrchr(path, '.');
    if (!suffix) {
        dmLogError("No suffix found in path: %s", path);
        return 0;
    } else {
        suffix++; // skip the '.'
    }

    jsize file_size = env->GetArrayLength(array);
    jbyte* file_data = env->GetByteArrayElements(array, 0);

    dmModelImporter::Options options;
    dmModelImporter::Scene* scene = dmModelImporter::LoadFromBuffer(&options, suffix, (uint8_t*)file_data, file_size);

    if (!scene)
    {
        dmLogError("Failed to load %s", path);
        return 0;
    }

    bool resolved = false;
    if (data_resolver != 0 && dmModelImporter::NeedsResolve(scene))
    {
        jclass cls_resolver = env->GetObjectClass(data_resolver);
        jmethodID get_data = env->GetMethodID(cls_resolver, "getData", "(Ljava/lang/String;Ljava/lang/String;)[B");

        for (uint32_t i = 0; i < scene->m_Buffers.Size(); ++i)
        {
            if (scene->m_Buffers[i].m_Buffer)
                continue;

            const char* uri = scene->m_Buffers[i].m_Uri;
            jstring j_uri = env->NewStringUTF(uri);

            jbyteArray bytes = (jbyteArray)env->CallObjectMethod(data_resolver, get_data, _path, j_uri);
            if (env->ExceptionCheck()) {
                dmLogError("JNI ExceptionCheck failed:");
                env->ExceptionDescribe();
                env->ExceptionClear();
                return 0;
            }
            if (bytes)
            {
                dmLogDebug("Found buffer for %s!\n", uri);

                jsize buffer_size = env->GetArrayLength(bytes);
                jbyte* buffer_data = env->GetByteArrayElements(bytes, 0);
                dmModelImporter::ResolveBuffer(scene, scene->m_Buffers[i].m_Uri, buffer_data, buffer_size);
                resolved = true;

                env->ReleaseByteArrayElements(bytes, buffer_data, JNI_ABORT);
            }
            else {
                dmLogDebug("Found no buffer for uri '%s'\n", uri);
            }
            env->DeleteLocalRef(j_uri);
        }

        if(dmModelImporter::NeedsResolve(scene))
        {
            dmLogWarning("The model is still missing buffers!");
        }
    }

    if (resolved && !dmModelImporter::NeedsResolve(scene))
    {
        dmModelImporter::LoadFinalize(scene);
        dmModelImporter::Validate(scene);
    }

    if (dmLogGetLevel() == LOG_SEVERITY_DEBUG) // verbose mode
    {
        dmModelImporter::DebugScene(scene);
    }

    jobject jscene = 0;
    if (!dmModelImporter::NeedsResolve(scene))
    {
        jscene = dmModelImporter::CreateJavaScene(env, scene);
    }

    dmModelImporter::DestroyScene(scene);

    env->ReleaseByteArrayElements(array, file_data, JNI_ABORT);

    return jscene;
}

JNIEXPORT jobject JNICALL Java_ModelImporterJni_LoadFromBufferInternal(JNIEnv* env, jclass cls, jstring _path, jbyteArray array, jobject data_resolver)
{
    dmLogDebug("Java_ModelImporterJni_LoadFromBufferInternal: env = %p\n", env);
    //DM_SCOPED_SIGNAL_CONTEXT(env, return 0;);

    jobject jscene;
    DM_JNI_GUARD_SCOPE_BEGIN();
        jscene = LoadFromBufferInternal(env, cls, _path, array, data_resolver);
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return jscene;
}

// JNIEXPORT jint JNICALL Java_ModelImporterJni_AddressOf(JNIEnv* env, jclass cls, jobject object)
// {
//     return dmModelImporter::AddressOf(object);
// }

JNIEXPORT void JNICALL Java_ModelImporterJni_TestException(JNIEnv* env, jclass cls, jstring j_message)
{
    //DM_SCOPED_SIGNAL_CONTEXT(env, return;);
    dmJNI::ScopedString s_message(env, j_message);
    const char* message = s_message.m_String;
    printf("Received message: %s\n", message);
    dmJNI::TestSignalFromString(message);
}

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    dmLogDebug("JNI_OnLoad ->\n");
    //dmJNI::EnableDefaultSignalHandlers(vm);

    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8) != JNI_OK) {
        printf("JNI_OnLoad GetEnv error\n");
        return JNI_ERR;
    }

    // Find your class. JNI_OnLoad is called from the correct class loader context for this to work.
    jclass c = env->FindClass( JAVA_PACKAGE_NAME "/ModelImporterJni");
    dmLogDebug("JNI_OnLoad: c = %p\n", c);
    if (c == 0)
        return JNI_ERR;

    // Register your class' native methods.
    // Don't forget to add them to the corresponding java file (e.g. ModelImporter.java)
    static const JNINativeMethod methods[] = {
        {(char*)"LoadFromBufferInternal", (char*)"(Ljava/lang/String;[BLjava/lang/Object;)L" CLASS_NAME "$Scene;", reinterpret_cast<void*>(Java_ModelImporterJni_LoadFromBufferInternal)},
        //{"AddressOf", "(Ljava/lang/Object;)I", reinterpret_cast<void*>(Java_ModelImporterJni_AddressOf)},
        {(char*)"TestException", (char*)"(Ljava/lang/String;)V", reinterpret_cast<void*>(Java_ModelImporterJni_TestException)},
    };
    int rc = env->RegisterNatives(c, methods, sizeof(methods)/sizeof(JNINativeMethod));
    env->DeleteLocalRef(c);

    if (rc != JNI_OK) return rc;

    dmLogDebug("JNI_OnLoad return.\n");
    return JNI_VERSION_1_8;
}

JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved)
{
    dmLogDebug("JNI_OnUnload ->\n");
}

namespace dmModelImporter
{

}
