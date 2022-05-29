// Copyright 2020-2022 The Defold Foundation
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

#include <jni.h>

#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>

#define CLASS_SCENE  "com/dynamo/bob/pipeline/ModelImporter$Scene"

struct ScopedString
{
    JNIEnv* m_Env;
    jstring m_JString;
    const char* m_String;
    ScopedString(JNIEnv* env, jstring str)
    : m_Env(env)
    , m_JString(str)
    , m_String(env->GetStringUTFChars(str, JNI_FALSE))
    {
    }
    ~ScopedString()
    {
        if (m_String)
        {
            m_Env->ReleaseStringUTFChars(m_JString, m_String);
        }
    }
};


namespace dmModelImporter
{

static const char* CLASS_NAME_FORMAT="com/dynamo/bob/pipeline/ModelImporter$%s";

// ******************************************************************************************************************

jclass GetClass(JNIEnv* env, const char* clsname)
{
    char buffer[128];
    dmSnPrintf(buffer, sizeof(buffer), CLASS_NAME_FORMAT, clsname);
    return env->FindClass(buffer);
}

static jfieldID GetFieldInt(JNIEnv* env, jclass cls, const char* field_name)
{
    return env->GetFieldID(cls, field_name, "I");
}
static jfieldID GetFieldString(JNIEnv* env, jclass cls, const char* field_name)
{
    return env->GetFieldID(cls, field_name, "Ljava/lang/String;");
}
static jfieldID GetFieldNode(JNIEnv* env, jclass cls, const char* field_name)
{
    return env->GetFieldID(cls, field_name, "Lcom/dynamo/bob/pipeline/ModelImporter$Node;");
}
static jfieldID GetFieldVec4(JNIEnv* env, jclass cls, const char* field_name)
{
    return env->GetFieldID(cls, field_name, "Lcom/dynamo/bob/pipeline/ModelImporter$Vec4;");
}
static jfieldID GetFieldTransform(JNIEnv* env, jclass cls, const char* field_name)
{
    return env->GetFieldID(cls, field_name, "Lcom/dynamo/bob/pipeline/ModelImporter$Transform;");
}
static jfieldID GetFieldModel(JNIEnv* env, jclass cls, const char* field_name)
{
    return env->GetFieldID(cls, field_name, "Lcom/dynamo/bob/pipeline/ModelImporter$Model;");
}
static jfieldID GetFieldSkin(JNIEnv* env, jclass cls, const char* field_name)
{
    return env->GetFieldID(cls, field_name, "Lcom/dynamo/bob/pipeline/ModelImporter$Skin;");
}
static jfieldID GetFieldBone(JNIEnv* env, jclass cls, const char* field_name)
{
    return env->GetFieldID(cls, field_name, "Lcom/dynamo/bob/pipeline/ModelImporter$Bone;");
}

// ******************************************************************************************************************

static void SetFieldInt(JNIEnv* env, jclass cls, jobject obj, const char* field_name, int value)
{
    jfieldID field = GetFieldInt(env, cls, field_name);
    env->SetIntField(obj, field, value);
}
static void SetFieldString(JNIEnv* env, jclass cls, jobject obj, const char* field_name, const char* value)
{
    jfieldID field = GetFieldString(env, cls, field_name);
    jstring str = env->NewStringUTF(value);
    env->SetObjectField(obj, field, str);
}
static void SetFieldObject(JNIEnv* env, jobject obj, jfieldID field, jobject value)
{
    env->SetObjectField(obj, field, value);
}


// ******************************************************************************************************************

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

static jobject CreateVec4(JNIEnv* env, const dmVMath::Vector4& value)
{
    jclass cls = GetClass(env, "Vec4");
    jobject obj = env->AllocObject(cls);
    jfieldID fx = env->GetFieldID(cls, "x", "F");
    jfieldID fy = env->GetFieldID(cls, "y", "F");
    jfieldID fz = env->GetFieldID(cls, "z", "F");
    jfieldID fw = env->GetFieldID(cls, "w", "F");
    env->SetFloatField(obj, fx, value.getX());
    env->SetFloatField(obj, fy, value.getY());
    env->SetFloatField(obj, fz, value.getZ());
    env->SetFloatField(obj, fw, value.getW());
    return obj;
}

static jobject CreateTransform(JNIEnv* env, dmTransform::Transform* transform)
{
    jclass cls = GetClass(env, "Transform");
    jobject obj = env->AllocObject(cls);
    jfieldID ft = GetFieldVec4(env, cls, "translation");
    jfieldID fr = GetFieldVec4(env, cls, "rotation");
    jfieldID fs = GetFieldVec4(env, cls, "scale");
    SetFieldObject(env, obj, ft, CreateVec4(env, dmVMath::Vector4(transform->GetTranslation())));
    SetFieldObject(env, obj, fr, CreateVec4(env, dmVMath::Vector4(transform->GetRotation())));
    SetFieldObject(env, obj, fs, CreateVec4(env, dmVMath::Vector4(transform->GetScale())));
    return obj;
}


// **************************************************
// Nodes

static jobject CreateNode(JNIEnv* env, jclass cls, dmModelImporter::Node* node)
{
    jobject obj = env->AllocObject(cls);
    SetFieldInt(env, cls, obj, "index", node->m_Index);
    SetFieldString(env, cls, obj, "name", node->m_Name);
    SetFieldObject(env, obj, GetFieldTransform(env, cls, "transform"), CreateTransform(env, &node->m_Transform));
    return obj;
}

static jfloatArray CreateFloatArray(JNIEnv* env, uint32_t count, const float* values)
{
    jfloatArray arr = env->NewFloatArray(count);
    env->SetFloatArrayRegion(arr, 0, count, values);
    return arr;
}

static jintArray CreateIntArray(JNIEnv* env, uint32_t count, const int* values)
{
    jintArray arr = env->NewIntArray(count);
    env->SetIntArrayRegion(arr, 0, count, values);
    return arr;
}

static void CreateNodes(JNIEnv* env, const dmModelImporter::Scene* scene, dmArray<jobject>& nodes)
{
    uint32_t count = scene->m_NodesCount;
    nodes.SetCapacity(count);
    nodes.SetSize(count);

    jclass cls = GetClass(env, "Node");

    for (uint32_t i = 0; i < count; ++i)
    {
        Node* node = &scene->m_Nodes[i];
        nodes[node->m_Index] = CreateNode(env, cls, node);
    }

    jfieldID fChildren = env->GetFieldID(cls, "children", "[Lcom/dynamo/bob/pipeline/ModelImporter$Node;");
    jfieldID fParent = GetFieldNode(env, cls, "parent");
    for (uint32_t i = 0; i < count; ++i)
    {
        Node* node = &scene->m_Nodes[i];

        if (node->m_Parent != 0)
        {
            SetFieldObject(env, nodes[i], fParent, nodes[node->m_Parent->m_Index]);
        }

        jobjectArray childrenArray = env->NewObjectArray(node->m_ChildrenCount, cls, 0);
        for (uint32_t i = 0; i < node->m_ChildrenCount; ++i)
        {
            dmModelImporter::Node* child = node->m_Children[i];
            env->SetObjectArrayElement(childrenArray, i, nodes[child->m_Index]);
        }
        env->SetObjectField(nodes[i], fChildren, childrenArray);
    }
}

static void FixupNodeReferences(JNIEnv* env, const dmModelImporter::Scene* scene, const dmArray<jobject>& skins, const dmArray<jobject>& models, const dmArray<jobject>& nodes)
{
    jclass nodeCls = GetClass(env, "Node");
    jfieldID fSkin = GetFieldSkin(env, nodeCls, "skin");
    jfieldID fModel = GetFieldModel(env, nodeCls, "model");

    uint32_t count = scene->m_NodesCount;
    for (uint32_t i = 0; i < count; ++i)
    {
        dmModelImporter::Node* node = &scene->m_Nodes[i];
        if (node->m_Skin)
        {
            jobject node_obj = nodes[node->m_Index];
            jobject skin_obj = skins[node->m_Skin->m_Index];
            SetFieldObject(env, node_obj, fSkin, skin_obj);
        }

        if (node->m_Model)
        {
            jobject node_obj = nodes[node->m_Index];
            jobject model_obj = models[node->m_Model->m_Index];
            SetFieldObject(env, node_obj, fModel, model_obj);
        }

    }
}

// **************************************************
// Meshes

static jobject CreateMesh(JNIEnv* env, jclass cls, const dmModelImporter::Mesh* mesh)
{
    jobject obj = env->AllocObject(cls);
    SetFieldString(env, cls, obj, "name", mesh->m_Name);
    SetFieldString(env, cls, obj, "material", mesh->m_Material);
    SetFieldInt(env, cls, obj, "vertexCount", mesh->m_VertexCount);
    SetFieldInt(env, cls, obj, "indexCount", mesh->m_IndexCount);
    SetFieldInt(env, cls, obj, "texCoords0NumComponents", mesh->m_TexCoord0NumComponents);
    SetFieldInt(env, cls, obj, "texCoords1NumComponents", mesh->m_TexCoord1NumComponents);

#define SET_FARRAY(OBJ, FIELD, COUNT, VALUES) \
    { \
        jfieldID f ## FIELD  = env->GetFieldID(cls, # FIELD, "[F"); \
        jfloatArray arr = CreateFloatArray(env, COUNT, VALUES); \
        env->SetObjectField(OBJ, f ## FIELD, arr); \
    }
#define SET_IARRAY(OBJ, FIELD, COUNT, VALUES) \
    { \
        jfieldID f ## FIELD  = env->GetFieldID(cls, # FIELD, "[I"); \
        jintArray arr = CreateIntArray(env, COUNT, (const int*)VALUES); \
        env->SetObjectField(OBJ, f ## FIELD, arr); \
    }

    uint32_t vcount = mesh->m_VertexCount;
    uint32_t icount = mesh->m_IndexCount;
    SET_FARRAY(obj, positions, vcount * 3, mesh->m_Positions);
    SET_FARRAY(obj, normals, vcount * 3, mesh->m_Normals);
    SET_FARRAY(obj, tangents, vcount * 3, mesh->m_Tangents);
    SET_FARRAY(obj, colors, vcount * 4, mesh->m_Color);
    SET_FARRAY(obj, weights, vcount * 4, mesh->m_Weights);
    SET_FARRAY(obj, texCoords0, vcount * mesh->m_TexCoord0NumComponents, mesh->m_TexCoord0);
    SET_FARRAY(obj, texCoords1, vcount * mesh->m_TexCoord1NumComponents, mesh->m_TexCoord1);

    SET_IARRAY(obj, bones, vcount * 4, mesh->m_Bones);
    SET_IARRAY(obj, indices, icount, mesh->m_Indices);

#undef SET_FARRAY
#undef SET_UARRAY

    return obj;
}

static jobjectArray CreateMeshesArray(JNIEnv* env, jclass cls, uint32_t count, const dmModelImporter::Mesh* meshes)
{
    jobjectArray arr = env->NewObjectArray(count, cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        env->SetObjectArrayElement(arr, i, CreateMesh(env, cls, &meshes[i]));
    }
    return arr;
}

static jobject CreateModel(JNIEnv* env, jclass cls, const dmModelImporter::Model* model)
{
    jobject obj = env->AllocObject(cls);
    SetFieldInt(env, cls, obj, "index", model->m_Index);
    SetFieldString(env, cls, obj, "name", model->m_Name);

    jclass meshCls = GetClass(env, "Mesh");
    jobjectArray arr = CreateMeshesArray(env, meshCls, model->m_MeshesCount, model->m_Meshes);

    jfieldID fMeshes = env->GetFieldID(cls, "meshes", "[Lcom/dynamo/bob/pipeline/ModelImporter$Mesh;");
    env->SetObjectField(obj, fMeshes, arr);

    return obj;
}

static void CreateModels(JNIEnv* env, const dmModelImporter::Scene* scene, dmArray<jobject>& models)
{
    uint32_t count = scene->m_ModelsCount;
    models.SetCapacity(count);
    models.SetSize(count);

    jclass cls = GetClass(env, "Model");
    for (uint32_t i = 0; i < count; ++i)
    {
        Model* model = &scene->m_Models[i];
        models[model->m_Index] = CreateModel(env, cls, model);
    }
}

// **************************************************
// Animations

static jobject CreateKeyFrame(JNIEnv* env, jclass cls, const dmModelImporter::KeyFrame* key_frame)
{
    jobject obj = env->AllocObject(cls);

    jfieldID ftime = env->GetFieldID(cls, "time", "F");
    jfieldID fvalue = env->GetFieldID(cls, "value", "[F");

    jfloatArray arr = env->NewFloatArray(4);
    env->SetFloatArrayRegion(arr, 0, 3, key_frame->m_Value);
    env->SetObjectField(obj, fvalue, arr);
    env->SetFloatField(obj, ftime, key_frame->m_Time);
    return obj;
}

static jobjectArray CreateKeyFramesArray(JNIEnv* env, jclass cls, uint32_t count, const dmModelImporter::KeyFrame* key_frames)
{
    jobjectArray arr = env->NewObjectArray(count, cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        env->SetObjectArrayElement(arr, i, CreateKeyFrame(env, cls, &key_frames[i]));
    }
    return arr;
}

static jobject CreateNodeAnimation(JNIEnv* env, jclass cls, const dmModelImporter::NodeAnimation* node_anim, const dmArray<jobject>& nodes)
{
    jobject obj = env->AllocObject(cls);
    SetFieldObject(env, obj, GetFieldNode(env, cls, "node"), nodes[node_anim->m_Node->m_Index]);

    jfieldID ftranslationKeys = env->GetFieldID(cls, "translationKeys", "[Lcom/dynamo/bob/pipeline/ModelImporter$KeyFrame;");
    jfieldID frotationKeys = env->GetFieldID(cls, "rotationKeys", "[Lcom/dynamo/bob/pipeline/ModelImporter$KeyFrame;");
    jfieldID fscaleKeys = env->GetFieldID(cls, "scaleKeys", "[Lcom/dynamo/bob/pipeline/ModelImporter$KeyFrame;");

    jclass keyframeCls = GetClass(env, "KeyFrame");
    env->SetObjectField(obj, ftranslationKeys, CreateKeyFramesArray(env, keyframeCls, node_anim->m_TranslationKeysCount, node_anim->m_TranslationKeys));
    env->SetObjectField(obj, frotationKeys, CreateKeyFramesArray(env, keyframeCls, node_anim->m_RotationKeysCount, node_anim->m_RotationKeys));
    env->SetObjectField(obj, fscaleKeys, CreateKeyFramesArray(env, keyframeCls, node_anim->m_ScaleKeysCount, node_anim->m_ScaleKeys));
    return obj;
}

static jobjectArray CreateNodeAnimationsArray(JNIEnv* env, jclass cls, uint32_t count, const dmModelImporter::NodeAnimation* node_anim, const dmArray<jobject>& nodes)
{
    jobjectArray arr = env->NewObjectArray(count, cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        env->SetObjectArrayElement(arr, i, CreateNodeAnimation(env, cls, &node_anim[i], nodes));
    }
    return arr;
}

static jobject CreateAnimation(JNIEnv* env, jclass cls, const dmModelImporter::Animation* animation, const dmArray<jobject>& nodes)
{
    jobject obj = env->AllocObject(cls);
    SetFieldString(env, cls, obj, "name", animation->m_Name);

    jclass nodeAnimCls = GetClass(env, "NodeAnimation");
    jobjectArray arr = CreateNodeAnimationsArray(env, nodeAnimCls, animation->m_NodeAnimationsCount, animation->m_NodeAnimations, nodes);
    jfieldID field = env->GetFieldID(cls, "nodeAnimations", "[Lcom/dynamo/bob/pipeline/ModelImporter$NodeAnimation;");
    env->SetObjectField(obj, field, arr);
    return obj;
}

static jobjectArray CreateAnimationsArray(JNIEnv* env, jclass cls, uint32_t count, const dmModelImporter::Animation* animations, const dmArray<jobject>& nodes)
{
    jobjectArray arr = env->NewObjectArray(count, cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        env->SetObjectArrayElement(arr, i, CreateAnimation(env, cls, &animations[i], nodes));
    }
    return arr;
}

// **************************************************
// Bones

static jobject CreateBone(JNIEnv* env, jclass cls, dmModelImporter::Bone* bone, const dmArray<jobject>& nodes)
{
    jobject obj = env->AllocObject(cls);
    SetFieldInt(env, cls, obj, "index", bone->m_Index);
    SetFieldString(env, cls, obj, "name", bone->m_Name);
    SetFieldObject(env, obj, GetFieldTransform(env, cls, "invBindPose"), CreateTransform(env, &bone->m_InvBindPose));
    SetFieldObject(env, obj, GetFieldNode(env, cls, "node"), nodes[bone->m_Node->m_Index]);
    return obj;
}

static jobjectArray CreateBonesArray(JNIEnv* env, jclass cls, uint32_t count, dmModelImporter::Bone* bones, const dmArray<jobject>& nodes)
{
    jfieldID fParent = GetFieldBone(env, cls, "parent");

    dmArray<jobject> tmp;
    tmp.SetCapacity(count);
    tmp.SetSize(count);

    jobjectArray bonesArray = env->NewObjectArray(count, cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        dmModelImporter::Bone* bone = &bones[i];
        tmp[bone->m_Index] = CreateBone(env, cls, bone, nodes);
        env->SetObjectArrayElement(bonesArray, i, tmp[bone->m_Index]);
    }

    for (uint32_t i = 0; i < count; ++i)
    {
        dmModelImporter::Bone* bone = &bones[i];
        if (bone->m_ParentIndex == 0xFFFFFFFF)
            continue;
        env->SetObjectField(tmp[bone->m_Index], fParent, tmp[bone->m_ParentIndex]);
    }

    return bonesArray;
}

static void CreateBones(JNIEnv* env, const dmModelImporter::Scene* scene, const dmArray<jobject>& skins, const dmArray<jobject>& nodes)
{
    jclass skinCls = GetClass(env, "Skin");
    jclass boneCls = GetClass(env, "Bone");
    jfieldID fBones = env->GetFieldID(skinCls, "bones", "[Lcom/dynamo/bob/pipeline/ModelImporter$Bone;");

    uint32_t count = scene->m_SkinsCount;
    for (uint32_t i = 0; i < count; ++i)
    {
        Skin* skin = &scene->m_Skins[i];
        jobject skin_obj = skins[skin->m_Index];

        jobjectArray bonesArray = CreateBonesArray(env, boneCls, skin->m_BonesCount, skin->m_Bones, nodes);
        env->SetObjectField(skin_obj, fBones, bonesArray);
    }
}

// **************************************************
// SKins

static jobject CreateSkin(JNIEnv* env, jclass cls, dmModelImporter::Skin* skin)
{
    jobject obj = env->AllocObject(cls);
    SetFieldInt(env, cls, obj, "index", skin->m_Index);
    SetFieldString(env, cls, obj, "name", skin->m_Name);
    return obj;
}

static void CreateSkins(JNIEnv* env, const dmModelImporter::Scene* scene, dmArray<jobject>& skins)
{
    uint32_t count = scene->m_SkinsCount;
    skins.SetCapacity(count);
    skins.SetSize(count);

    jclass cls = GetClass(env, "Skin");
    for (uint32_t i = 0; i < count; ++i)
    {
        Skin* skin = &scene->m_Skins[i];
        skins[skin->m_Index] = CreateSkin(env, cls, skin);
    }
}


static jobject CreateJavaScene(JNIEnv* env, const dmModelImporter::Scene* scene)
{
    jclass cls = GetClass(env, "Scene");
    jobject obj = env->AllocObject(cls);

    dmArray<jobject> models;
    dmArray<jobject> skins;
    dmArray<jobject> nodes;

    // Creates all nodes, and leaves out setting skins/models
    CreateNodes(env, scene, nodes);

    CreateSkins(env, scene, skins);
    CreateModels(env, scene, models);
    CreateBones(env, scene, skins, nodes);

    // Set the skin+model to the nodes
    FixupNodeReferences(env, scene, skins, models, nodes);

    ///
    {
        jobjectArray arr = CreateObjectArray(env, GetClass(env, "Node"), nodes);
        jfieldID field = env->GetFieldID(cls, "nodes", "[Lcom/dynamo/bob/pipeline/ModelImporter$Node;");
        env->SetObjectField(obj, field, arr);
    }

    {
        uint32_t count = scene->m_RootNodesCount;
        jobjectArray arr = env->NewObjectArray(count, GetClass(env, "Node"), 0);
        for (uint32_t i = 0; i < count; ++i)
        {
            dmModelImporter::Node* root = scene->m_RootNodes[i];
            env->SetObjectArrayElement(arr, i, nodes[root->m_Index]);
        }
        jfieldID field = env->GetFieldID(cls, "rootNodes", "[Lcom/dynamo/bob/pipeline/ModelImporter$Node;");
        env->SetObjectField(obj, field, arr);
    }

    {
        jobjectArray arr = CreateObjectArray(env, GetClass(env, "Skin"), skins);
        jfieldID field = env->GetFieldID(cls, "skins", "[Lcom/dynamo/bob/pipeline/ModelImporter$Skin;");
        env->SetObjectField(obj, field, arr);
    }

    {
        jobjectArray arr = CreateObjectArray(env, GetClass(env, "Model"), models);
        jfieldID field = env->GetFieldID(cls, "models", "[Lcom/dynamo/bob/pipeline/ModelImporter$Model;");
        env->SetObjectField(obj, field, arr);
    }

    {
        jobjectArray arr = CreateAnimationsArray(env, GetClass(env, "Animation"),
                                                    scene->m_AnimationsCount, scene->m_Animations, nodes);
        jfieldID field = env->GetFieldID(cls, "animations", "[Lcom/dynamo/bob/pipeline/ModelImporter$Animation;");
        env->SetObjectField(obj, field, arr);
    }



    ///

    return obj;
}

}

JNIEXPORT jobject JNICALL Java_ModelImporter_LoadFromBufferInternal(JNIEnv* env, jclass cls, jstring _path, jbyteArray array)
{
    ScopedString j_path(env, _path);
    const char* path = j_path.m_String;

    const char* suffix = strrchr(path, '.');
    if (!suffix) {
        dmLogWarning("No suffix found in path: %s", path);
        dmLogWarning("Assuming glb format!");
        suffix = "glb";
    } else {
        suffix++; // skip the '.'
    }

    jsize file_size = env->GetArrayLength(array);
    jbyte* file_data = env->GetByteArrayElements(array, 0);

    dmLogDebug("LoadFromBufferInternal: %s suffix: %s bytes: %d\n", path, suffix, file_size);

    dmModelImporter::Options options;
    dmModelImporter::Scene* scene = dmModelImporter::LoadFromBuffer(&options, suffix, (uint8_t*)file_data, file_size);
    if (!scene)
    {
        dmLogError("Failed to load %s", path);
        return 0;
    }

    if (dmLog::Getlevel() == dmLog::LOG_SEVERITY_DEBUG) // verbose mode
    {
        dmModelImporter::DebugScene(scene);
    }

    dmLogDebug("    CreateJavaScene: ->\n");

    jobject test = dmModelImporter::CreateJavaScene(env, scene);

    dmLogDebug("    CreateJavaScene: end.\n");

    dmModelImporter::DestroyScene(scene);

    dmLogDebug("LoadFromBufferInternal: end.\n");

    return test;
}

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {

    printf("JNI_OnLoad ->\n");

    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        printf("JNI_OnLoad GetEnv error\n");
        return JNI_ERR;
    }

    // Find your class. JNI_OnLoad is called from the correct class loader context for this to work.
    jclass c = env->FindClass("com/dynamo/bob/pipeline/ModelImporter");
    printf("JNI_OnLoad: c = %p\n", c);
    if (c == 0)
      return JNI_ERR;

    // Register your class' native methods.
    static const JNINativeMethod methods[] = {
        {"LoadFromBufferInternal", "(Ljava/lang/String;[B)L" CLASS_SCENE ";", reinterpret_cast<void*>(Java_ModelImporter_LoadFromBufferInternal)},
    };
    int rc = env->RegisterNatives(c, methods, sizeof(methods)/sizeof(JNINativeMethod));
    if (rc != JNI_OK) return rc;

    printf("JNI_OnLoad return.\n");
    return JNI_VERSION_1_6;
}

namespace dmModelImporter
{

}
