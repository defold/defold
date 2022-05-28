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

static jobject CreateNode(JNIEnv* env, dmModelImporter::Node* node)
{
    jclass cls = GetClass(env, "Node");
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

    jclass cls = GetClass(env, "Node");

    for (uint32_t i = 0; i < count; ++i)
    {
        Node* node = &scene->m_Nodes[i];
        nodes.Push(CreateNode(env, node));
    }

    for (uint32_t i = 0; i < count; ++i)
    {
        Node* node = &scene->m_Nodes[i];

        if (node->m_Parent != 0)
        {
            SetFieldNode(env, cls, nodes[i], "parent", nodes[node->m_Parent->m_Index]);
        }

        jobjectArray childrenArray = env->NewObjectArray(node->m_ChildrenCount, cls, 0);
        for (uint32_t i = 0; i < node->m_ChildrenCount; ++i)
        {
            dmModelImporter::Node* child = node->m_Children[i];
            env->SetObjectArrayElement(childrenArray, i, nodes[child->m_Index]);
        }
        jfieldID fieldNodes = env->GetFieldID(cls, "children", "[Lcom/dynamo/bob/pipeline/ModelImporter$Node;");
        env->SetObjectField(nodes[i], fieldNodes, childrenArray);
    }
}

static jobject CreateJavaScene(JNIEnv* env, const dmModelImporter::Scene* scene)
{
    jclass cls = env->FindClass(CLASS_SCENE);
    jobject obj = env->AllocObject(cls);

    dmArray<jobject> nodes;
    CreateNodes(env, scene, nodes);

    jclass nodeCls = GetClass(env, "Node");

    ///
    {
        jobjectArray nodesArray = env->NewObjectArray(nodes.Size(), nodeCls, 0);
        for (uint32_t i = 0; i < nodes.Size(); ++i)
        {
            env->SetObjectArrayElement(nodesArray, i, nodes[i]);
        }
        jfieldID fieldNodes = env->GetFieldID(cls, "nodes", "[Lcom/dynamo/bob/pipeline/ModelImporter$Node;");
        env->SetObjectField(obj, fieldNodes, nodesArray);
    }

    {
        jobjectArray rootNodesArray = env->NewObjectArray(scene->m_RootNodesCount, nodeCls, 0);
        for (uint32_t i = 0; i < scene->m_RootNodesCount; ++i)
        {
            dmModelImporter::Node* root = scene->m_RootNodes[i];
            env->SetObjectArrayElement(rootNodesArray, i, nodes[root->m_Index]);
        }
        jfieldID fieldNodes = env->GetFieldID(cls, "rootNodes", "[Lcom/dynamo/bob/pipeline/ModelImporter$Node;");
        env->SetObjectField(obj, fieldNodes, rootNodesArray);
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
