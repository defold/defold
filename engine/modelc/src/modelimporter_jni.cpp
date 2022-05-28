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

// jobject CreateInstance(JNIEnv* env, const char* clsname)
// {
//     jclass cls = GetClass(env, clsname);
//     jobject obj = env->AllocObject( cls );
//     return obj;
// }

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
static jfieldID GetFieldMesh(JNIEnv* env, jclass cls, const char* field_name)
{
    return env->GetFieldID(cls, field_name, "Lcom/dynamo/bob/pipeline/ModelImporter$Mesh;");
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
static void SetFieldNode(JNIEnv* env, jclass cls, jobject obj, const char* field_name, jobject node)
{
    jfieldID field = GetFieldNode(env, cls, field_name);
    env->SetObjectField(obj, field, node);
}

static void SetFieldObject(JNIEnv* env, jobject obj, jfieldID field, jobject value)
{
    env->SetObjectField(obj, field, value);
}


// ******************************************************************************************************************

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

static jobject CreateNode(JNIEnv* env, jclass cls, dmModelImporter::Node* node)
{
    jobject obj = env->AllocObject(cls);
    SetFieldInt(env, cls, obj, "index", node->m_Index);
    SetFieldString(env, cls, obj, "name", node->m_Name);
    SetFieldObject(env, obj, GetFieldTransform(env, cls, "transform"), CreateTransform(env, &node->m_Transform));
    return obj;
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
    // jfieldID fModel = GetFieldModel(env, cls, "model");
    // jfieldID fSkin = GetFieldSkin(env, cls, "skin");
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

static void CreateModels(JNIEnv* env, const dmModelImporter::Scene* scene, dmArray<jobject>& models)
{

}

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

static void FixupNodeReferences(JNIEnv* env, const dmModelImporter::Scene* scene, const dmArray<jobject>& skins, const dmArray<jobject>& nodes)
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
    }
}

static jobject CreateJavaScene(JNIEnv* env, const dmModelImporter::Scene* scene)
{
    jclass cls = GetClass(env, "Scene");
    jobject obj = env->AllocObject(cls);

    dmArray<jobject> models;
    dmArray<jobject> skins;
    dmArray<jobject> nodes;
    //CreateModels(env, scene, models);
    CreateNodes(env, scene, nodes);
    CreateSkins(env, scene, skins);

    CreateBones(env, scene, skins, nodes);

    FixupNodeReferences(env, scene, skins, nodes);

    jclass nodeCls = GetClass(env, "Node");
    jclass skinCls = GetClass(env, "Skin");

    ///
    {
        uint32_t count = nodes.Size();
        jobjectArray arr = env->NewObjectArray(count, nodeCls, 0);
        for (uint32_t i = 0; i < count; ++i)
        {
            env->SetObjectArrayElement(arr, i, nodes[i]);
        }
        jfieldID field = env->GetFieldID(cls, "nodes", "[Lcom/dynamo/bob/pipeline/ModelImporter$Node;");
        env->SetObjectField(obj, field, arr);
    }

    {
        uint32_t count = scene->m_RootNodesCount;
        jobjectArray arr = env->NewObjectArray(count, nodeCls, 0);
        for (uint32_t i = 0; i < count; ++i)
        {
            dmModelImporter::Node* root = scene->m_RootNodes[i];
            env->SetObjectArrayElement(arr, i, nodes[root->m_Index]);
        }
        jfieldID field = env->GetFieldID(cls, "rootNodes", "[Lcom/dynamo/bob/pipeline/ModelImporter$Node;");
        env->SetObjectField(obj, field, arr);
    }

    {
        uint32_t count = scene->m_SkinsCount;
        jobjectArray arr = env->NewObjectArray(count, skinCls, 0);
        for (uint32_t i = 0; i < count; ++i)
        {
            env->SetObjectArrayElement(arr, i, skins[i]);
        }
        jfieldID field = env->GetFieldID(cls, "skins", "[Lcom/dynamo/bob/pipeline/ModelImporter$Skin;");
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
