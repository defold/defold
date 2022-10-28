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

static void OutputTransform(const dmTransform::Transform& transform)
{
    printf("    t: %f, %f, %f\n", transform.GetTranslation().getX(), transform.GetTranslation().getY(), transform.GetTranslation().getZ());
    printf("    r: %f, %f, %f, %f\n", transform.GetRotation().getX(), transform.GetRotation().getY(), transform.GetRotation().getZ(), transform.GetRotation().getW());
    printf("    s: %f, %f, %f\n", transform.GetScale().getX(), transform.GetScale().getY(), transform.GetScale().getZ());
}

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

// ******************************************************************************************************************

struct SceneJNI
{
    jclass      cls;
    jfieldID    nodes;
    jfieldID    models;
    jfieldID    skins;
    jfieldID    rootNodes;
    jfieldID    animations;
} g_SceneJNI;

struct SkinJNI
{
    jclass      cls;
    jfieldID    name;
    jfieldID    bones;
    jfieldID    index;
} g_SkinJNI;

struct BoneJNI
{
    jclass      cls;
    jfieldID    invBindPose;
    jfieldID    name;
    jfieldID    index;
    jfieldID    node;
    jfieldID    parent;
} g_BoneJNI;

struct NodeJNI
{
    jclass      cls;
    jfieldID    local;      // local transform
    jfieldID    world;      // world transform
    jfieldID    name;
    jfieldID    index;
    jfieldID    parent;
    jfieldID    children;
    jfieldID    model;
    jfieldID    skin;
} g_NodeJNI;

struct ModelJNI
{
    jclass      cls;
    jfieldID    name;
    jfieldID    meshes;
    jfieldID    index;
} g_ModelJNI;

struct MeshJNI
{
    jclass      cls;

    jfieldID    name;
    jfieldID    material;

    jfieldID    positions;
    jfieldID    normals;
    jfieldID    tangents;
    jfieldID    colors;
    jfieldID    weights;
    jfieldID    bones;

    jfieldID    texCoords0NumComponents;
    jfieldID    texCoords0;
    jfieldID    texCoords1NumComponents;
    jfieldID    texCoords1;

    jfieldID    indices;

    jfieldID    vertexCount;
    jfieldID    indexCount;

} g_MeshJNI;


struct Vec4JNI
{
    jclass      cls;
    jfieldID    x, y, z, w;
} g_Vec4JNI;

struct TransformJNI
{
    jclass      cls;
    jfieldID    translation;
    jfieldID    rotation;
    jfieldID    scale;
} g_TransformJNI;

struct KeyFrameJNI
{
    jclass      cls;
    jfieldID    value;
    jfieldID    time;
} g_KeyFrameJNI;

struct NodeAnimationJNI
{
    jclass      cls;
    jfieldID    node;
    jfieldID    translationKeys;
    jfieldID    rotationKeys;
    jfieldID    scaleKeys;
    jfieldID    startTime;
    jfieldID    endTime;
} g_NodeAnimationJNI;

struct AnimationJNI
{
    jclass      cls;
    jfieldID    name;
    jfieldID    nodeAnimations;
    jfieldID    duration;
} g_AnimationJNI;

static void InitializeJNITypes(JNIEnv* env)
{
#define SETUP_CLASS(TYPE, TYPE_NAME) \
    TYPE * obj = &g_ ## TYPE ; \
    obj->cls = GetClass(env, TYPE_NAME);

#define GET_FLD_TYPESTR(NAME, FULL_TYPE_STR) \
    obj-> NAME = env->GetFieldID(obj->cls, # NAME, FULL_TYPE_STR);

#define GET_FLD(NAME, TYPE_NAME) \
    obj-> NAME = env->GetFieldID(obj->cls, # NAME, "Lcom/dynamo/bob/pipeline/ModelImporter$" TYPE_NAME ";");

#define GET_FLD_ARRAY(NAME, TYPE_NAME) \
    obj-> NAME = env->GetFieldID(obj->cls, # NAME, "[Lcom/dynamo/bob/pipeline/ModelImporter$" TYPE_NAME ";");


    {
        SETUP_CLASS(Vec4JNI, "Vec4");
        GET_FLD_TYPESTR(x, "F");
        GET_FLD_TYPESTR(y, "F");
        GET_FLD_TYPESTR(z, "F");
        GET_FLD_TYPESTR(w, "F");
    }
    {
        SETUP_CLASS(TransformJNI, "Transform");
        GET_FLD(translation, "Vec4");
        GET_FLD(rotation, "Vec4");
        GET_FLD(scale, "Vec4");
    }
    {
        SETUP_CLASS(SceneJNI, "Scene");
        GET_FLD_ARRAY(nodes, "Node");
        GET_FLD_ARRAY(models, "Model");
        GET_FLD_ARRAY(skins, "Skin");
        GET_FLD_ARRAY(rootNodes, "Node");
        GET_FLD_ARRAY(animations, "Animation");
    }
    {
        SETUP_CLASS(SkinJNI, "Skin");
        GET_FLD_TYPESTR(name, "Ljava/lang/String;");
        GET_FLD_TYPESTR(index, "I");
        GET_FLD_ARRAY(bones, "Bone");
    }
    {
        SETUP_CLASS(BoneJNI, "Bone");
        GET_FLD(invBindPose, "Transform");
        GET_FLD_TYPESTR(name, "Ljava/lang/String;");
        GET_FLD_TYPESTR(index, "I");
        GET_FLD(node, "Node");
        GET_FLD(parent, "Bone");
    }
    {
        SETUP_CLASS(NodeJNI, "Node");
        GET_FLD_TYPESTR(name, "Ljava/lang/String;");
        GET_FLD_TYPESTR(index, "I");
        GET_FLD(local, "Transform");
        GET_FLD(world, "Transform");
        GET_FLD(parent, "Node");
        GET_FLD_ARRAY(children, "Node");
        GET_FLD(model, "Model");
        GET_FLD(skin, "Skin");
    }
    {
        SETUP_CLASS(ModelJNI, "Model");
        GET_FLD_TYPESTR(name, "Ljava/lang/String;");
        GET_FLD_TYPESTR(index, "I");
        GET_FLD_ARRAY(meshes, "Mesh");
    }
    {
        SETUP_CLASS(MeshJNI, "Mesh");
        GET_FLD_TYPESTR(name, "Ljava/lang/String;");
        GET_FLD_TYPESTR(material, "Ljava/lang/String;");

        GET_FLD_TYPESTR(positions, "[F");
        GET_FLD_TYPESTR(normals, "[F");
        GET_FLD_TYPESTR(tangents, "[F");
        GET_FLD_TYPESTR(colors, "[F");
        GET_FLD_TYPESTR(weights, "[F");

        GET_FLD_TYPESTR(bones, "[I");
        GET_FLD_TYPESTR(indices, "[I");

        GET_FLD_TYPESTR(vertexCount, "I");
        GET_FLD_TYPESTR(indexCount, "I");

        GET_FLD_TYPESTR(texCoords0NumComponents, "I");
        GET_FLD_TYPESTR(texCoords1NumComponents, "I");
        GET_FLD_TYPESTR(texCoords0, "[F");
        GET_FLD_TYPESTR(texCoords1, "[F");
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

#undef GET_FLD
#undef GET_FLD_ARRAY
#undef GET_FLD_TYPESTR
}


static void FinalizeJNITypes(JNIEnv* env)
{
}

static int AddressOf(jobject object)
{
    uint64_t a = *(uint64_t*)(uintptr_t)object;
    return a;
}

static int GetAddressOfField(JNIEnv* env, jobject object, jfieldID field)
{
    jobject field_object = env->GetObjectField(object, field);
    int id = AddressOf(field_object);
    env->DeleteLocalRef(field_object);
    return id;
}

// ******************************************************************************************************************


static void SetFieldString(JNIEnv* env, jclass cls, jobject obj, const char* field_name, const char* value)
{
    jfieldID field = GetFieldString(env, cls, field_name);
    jstring str = env->NewStringUTF(value);
    env->SetObjectField(obj, field, str);
    env->DeleteLocalRef(str);
}

static void SetFieldInt(JNIEnv* env, jobject obj, jfieldID field, int value)
{
    env->SetIntField(obj, field, value);
}
static void SetFieldString(JNIEnv* env, jobject obj, jfieldID field, const char* value)
{
    jstring str = env->NewStringUTF(value);
    env->SetObjectField(obj, field, str);
    env->DeleteLocalRef(str);
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
    jobject obj = env->AllocObject(g_Vec4JNI.cls);
    env->SetFloatField(obj, g_Vec4JNI.x, value.getX());
    env->SetFloatField(obj, g_Vec4JNI.y, value.getY());
    env->SetFloatField(obj, g_Vec4JNI.z, value.getZ());
    env->SetFloatField(obj, g_Vec4JNI.w, value.getW());
    return obj;
}

static jobject CreateTransform(JNIEnv* env, dmTransform::Transform* transform)
{
    jobject obj = env->AllocObject(g_TransformJNI.cls);
    SetFieldObject(env, obj, g_TransformJNI.translation, CreateVec4(env, dmVMath::Vector4(transform->GetTranslation())));
    SetFieldObject(env, obj, g_TransformJNI.rotation, CreateVec4(env, dmVMath::Vector4(transform->GetRotation())));
    SetFieldObject(env, obj, g_TransformJNI.scale, CreateVec4(env, dmVMath::Vector4(transform->GetScale())));
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
    env->SetIntArrayRegion(arr, 0, count, (const jint*)values);
    return arr;
}

// For debugging the set values
static void GetVec4(JNIEnv* env, jobject object, float* vec4)
{
    vec4[0] = env->GetFloatField(object, g_Vec4JNI.x);
    vec4[1] = env->GetFloatField(object, g_Vec4JNI.y);
    vec4[2] = env->GetFloatField(object, g_Vec4JNI.z);
    vec4[3] = env->GetFloatField(object, g_Vec4JNI.w);
}

static void GetTransform(JNIEnv* env, jobject object, jfieldID field, dmTransform::Transform* out)
{
    jobject xform = env->GetObjectField(object, field);
    // TODO: check if it's a transform class!

    jobject xform_pos = env->GetObjectField(xform, g_TransformJNI.translation);
    jobject xform_rot = env->GetObjectField(xform, g_TransformJNI.rotation);
    jobject xform_scl = env->GetObjectField(xform, g_TransformJNI.scale);

    float v[4] = {};
    GetVec4(env, xform_pos, v);
    out->SetTranslation(dmVMath::Vector3(v[0], v[1], v[2]));
    GetVec4(env, xform_rot, v);
    out->SetRotation(dmVMath::Quat(v[0], v[1], v[2], v[3]));
    GetVec4(env, xform_scl, v);
    out->SetScale(dmVMath::Vector3(v[0], v[1], v[2]));

    env->DeleteLocalRef(xform_pos);
    env->DeleteLocalRef(xform_rot);
    env->DeleteLocalRef(xform_scl);
    env->DeleteLocalRef(xform);
}

// **************************************************
// Nodes

static jobject CreateNode(JNIEnv* env, jclass cls, dmModelImporter::Node* node)
{
    jobject obj = env->AllocObject(g_NodeJNI.cls);
    SetFieldInt(env, obj, g_NodeJNI.index, node->m_Index);
    SetFieldString(env, obj, g_NodeJNI.name, node->m_Name);
    SetFieldObject(env, obj, g_NodeJNI.local, CreateTransform(env, &node->m_Local));
    SetFieldObject(env, obj, g_NodeJNI.world, CreateTransform(env, &node->m_World));
    return obj;
}

static void CreateNodes(JNIEnv* env, const dmModelImporter::Scene* scene, dmArray<jobject>& nodes)
{
    uint32_t count = scene->m_NodesCount;
    nodes.SetCapacity(count);
    nodes.SetSize(count);

    for (uint32_t i = 0; i < count; ++i)
    {
        Node* node = &scene->m_Nodes[i];
        nodes[node->m_Index] = CreateNode(env, g_NodeJNI.cls, node);
    }

    for (uint32_t i = 0; i < count; ++i)
    {
        Node* node = &scene->m_Nodes[i];

        if (node->m_Parent != 0)
        {
            SetFieldObject(env, nodes[i], g_NodeJNI.parent, nodes[node->m_Parent->m_Index]);
        }

        jobjectArray childrenArray = env->NewObjectArray(node->m_ChildrenCount, g_NodeJNI.cls, 0);
        for (uint32_t i = 0; i < node->m_ChildrenCount; ++i)
        {
            dmModelImporter::Node* child = node->m_Children[i];
            env->SetObjectArrayElement(childrenArray, i, nodes[child->m_Index]);
        }
        env->SetObjectField(nodes[i], g_NodeJNI.children, childrenArray);
        env->DeleteLocalRef(childrenArray);
    }
}

static void FixupNodeReferences(JNIEnv* env, const dmModelImporter::Scene* scene, const dmArray<jobject>& skins, const dmArray<jobject>& models, const dmArray<jobject>& nodes)
{
    uint32_t count = scene->m_NodesCount;
    for (uint32_t i = 0; i < count; ++i)
    {
        dmModelImporter::Node* node = &scene->m_Nodes[i];
        if (node->m_Skin)
        {
            jobject node_obj = nodes[node->m_Index];
            jobject skin_obj = skins[node->m_Skin->m_Index];
            SetFieldObject(env, node_obj, g_NodeJNI.skin, skin_obj);
        }

        if (node->m_Model)
        {
            jobject node_obj = nodes[node->m_Index];
            jobject model_obj = models[node->m_Model->m_Index];
            SetFieldObject(env, node_obj, g_NodeJNI.model, model_obj);
        }

    }
}

// **************************************************
// Meshes

static jobject CreateMesh(JNIEnv* env, const dmModelImporter::Mesh* mesh)
{
    jobject obj = env->AllocObject(g_MeshJNI.cls);
    SetFieldString(env, obj, g_MeshJNI.name, mesh->m_Name);
    SetFieldString(env, obj, g_MeshJNI.material, mesh->m_Material);
    SetFieldInt(env, obj, g_MeshJNI.vertexCount, mesh->m_VertexCount);
    SetFieldInt(env, obj, g_MeshJNI.indexCount, mesh->m_IndexCount);
    SetFieldInt(env, obj, g_MeshJNI.texCoords0NumComponents, mesh->m_TexCoord0NumComponents);
    SetFieldInt(env, obj, g_MeshJNI.texCoords1NumComponents, mesh->m_TexCoord1NumComponents);

#define SET_FARRAY(OBJ, FIELD, COUNT, VALUES) \
    { \
        if (VALUES) { \
            jfloatArray arr = CreateFloatArray(env, COUNT, VALUES); \
            env->SetObjectField(OBJ, g_MeshJNI. FIELD, arr); \
            env->DeleteLocalRef(arr); \
        } \
    }
#define SET_IARRAY(OBJ, FIELD, COUNT, VALUES) \
    { \
        if (VALUES) { \
            jintArray arr = CreateIntArray(env, COUNT, (const int*)VALUES); \
            env->SetObjectField(OBJ, g_MeshJNI. FIELD, arr); \
            env->DeleteLocalRef(arr); \
        } \
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

static jobjectArray CreateMeshesArray(JNIEnv* env, uint32_t count, const dmModelImporter::Mesh* meshes)
{
    jobjectArray arr = env->NewObjectArray(count, g_MeshJNI.cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        jobject o = CreateMesh(env, &meshes[i]);
        env->SetObjectArrayElement(arr, i, o);
        env->DeleteLocalRef(o);
    }
    return arr;
}

static jobject CreateModel(JNIEnv* env, const dmModelImporter::Model* model)
{
    jobject obj = env->AllocObject(g_ModelJNI.cls);
    SetFieldInt(env, obj, g_ModelJNI.index, model->m_Index);
    SetFieldString(env, obj, g_ModelJNI.name, model->m_Name);

    jobjectArray arr = CreateMeshesArray(env, model->m_MeshesCount, model->m_Meshes);
    env->SetObjectField(obj, g_ModelJNI.meshes, arr);
    env->DeleteLocalRef(arr);
    return obj;
}

static void CreateModels(JNIEnv* env, const dmModelImporter::Scene* scene, dmArray<jobject>& models)
{
    uint32_t count = scene->m_ModelsCount;
    models.SetCapacity(count);
    models.SetSize(count);

    for (uint32_t i = 0; i < count; ++i)
    {
        Model* model = &scene->m_Models[i];
        models[model->m_Index] = CreateModel(env, model);
    }
}

// **************************************************
// Animations

static jobject CreateKeyFrame(JNIEnv* env, const dmModelImporter::KeyFrame* key_frame)
{
    jobject obj = env->AllocObject(g_KeyFrameJNI.cls);
    jfloatArray arr = env->NewFloatArray(4);
    env->SetFloatArrayRegion(arr, 0, 4, key_frame->m_Value);
    env->SetObjectField(obj, g_KeyFrameJNI.value, arr);
    env->SetFloatField(obj, g_KeyFrameJNI.time, key_frame->m_Time);
    env->DeleteLocalRef(arr);
    return obj;
}

static jobjectArray CreateKeyFramesArray(JNIEnv* env, uint32_t count, const dmModelImporter::KeyFrame* key_frames)
{
    jobjectArray arr = env->NewObjectArray(count, g_KeyFrameJNI.cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        jobject o = CreateKeyFrame(env, &key_frames[i]);
        env->SetObjectArrayElement(arr, i, o);
        env->DeleteLocalRef(o);
    }
    return arr;
}

static jobject CreateNodeAnimation(JNIEnv* env, const dmModelImporter::NodeAnimation* node_anim, const dmArray<jobject>& nodes)
{
    jobject obj = env->AllocObject(g_NodeAnimationJNI.cls);
    SetFieldObject(env, obj, g_NodeAnimationJNI.node, nodes[node_anim->m_Node->m_Index]);

    jobjectArray arr = CreateKeyFramesArray(env, node_anim->m_TranslationKeysCount, node_anim->m_TranslationKeys);
    env->SetObjectField(obj, g_NodeAnimationJNI.translationKeys, arr);
    env->DeleteLocalRef(arr);

    arr = CreateKeyFramesArray(env, node_anim->m_RotationKeysCount, node_anim->m_RotationKeys);
    env->SetObjectField(obj, g_NodeAnimationJNI.rotationKeys, arr);
    env->DeleteLocalRef(arr);

    arr = CreateKeyFramesArray(env, node_anim->m_ScaleKeysCount, node_anim->m_ScaleKeys);
    env->SetObjectField(obj, g_NodeAnimationJNI.scaleKeys, arr);
    env->DeleteLocalRef(arr);

    env->SetFloatField(obj, g_NodeAnimationJNI.startTime, node_anim->m_StartTime);
    env->SetFloatField(obj, g_NodeAnimationJNI.endTime,  node_anim->m_EndTime);
    return obj;
}

static jobjectArray CreateNodeAnimationsArray(JNIEnv* env, uint32_t count, const dmModelImporter::NodeAnimation* node_anim, const dmArray<jobject>& nodes)
{
    jobjectArray arr = env->NewObjectArray(count, g_NodeAnimationJNI.cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        jobject o = CreateNodeAnimation(env, &node_anim[i], nodes);
        env->SetObjectArrayElement(arr, i, o);
        env->DeleteLocalRef(o);
    }
    return arr;
}

static jobject CreateAnimation(JNIEnv* env, const dmModelImporter::Animation* animation, const dmArray<jobject>& nodes)
{
    jobject obj = env->AllocObject(g_AnimationJNI.cls);
    SetFieldString(env, obj, g_AnimationJNI.name, animation->m_Name);

    jobjectArray arr = CreateNodeAnimationsArray(env, animation->m_NodeAnimationsCount, animation->m_NodeAnimations, nodes);
    env->SetObjectField(obj, g_AnimationJNI.nodeAnimations, arr);
    env->DeleteLocalRef(arr);

    env->SetFloatField(obj, g_AnimationJNI.duration, animation->m_Duration);

    return obj;
}

static jobjectArray CreateAnimationsArray(JNIEnv* env, uint32_t count, const dmModelImporter::Animation* animations, const dmArray<jobject>& nodes)
{
    jobjectArray arr = env->NewObjectArray(count, g_AnimationJNI.cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        env->SetObjectArrayElement(arr, i, CreateAnimation(env, &animations[i], nodes));
    }
    return arr;
}

// **************************************************
// Bones

static jobject CreateBone(JNIEnv* env, dmModelImporter::Bone* bone, const dmArray<jobject>& nodes)
{
    jobject obj = env->AllocObject(g_BoneJNI.cls);
    SetFieldInt(env, obj, g_BoneJNI.index, bone->m_Index);
    SetFieldString(env, obj, g_BoneJNI.name, bone->m_Name);
    SetFieldObject(env, obj, g_BoneJNI.invBindPose, CreateTransform(env, &bone->m_InvBindPose));
    if (bone->m_Node != 0) // A generated root bone doesn't have a corresponding Node
        SetFieldObject(env, obj, g_BoneJNI.node, nodes[bone->m_Node->m_Index]);
    else
        SetFieldObject(env, obj, g_BoneJNI.node, 0);
    return obj;
}

static jobjectArray CreateBonesArray(JNIEnv* env, uint32_t count, dmModelImporter::Bone* bones, const dmArray<jobject>& nodes)
{
    dmArray<jobject> tmp;
    tmp.SetCapacity(count);
    tmp.SetSize(count);

    jobjectArray arr = env->NewObjectArray(count, g_BoneJNI.cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        dmModelImporter::Bone* bone = &bones[i];
        tmp[bone->m_Index] = CreateBone(env, bone, nodes);
        env->SetObjectArrayElement(arr, i, tmp[bone->m_Index]);
    }

    for (uint32_t i = 0; i < count; ++i)
    {
        dmModelImporter::Bone* bone = &bones[i];
        if (bone->m_ParentIndex != INVALID_INDEX)
            SetFieldObject(env, tmp[bone->m_Index], g_BoneJNI.parent, tmp[bone->m_ParentIndex]);
        else
            SetFieldObject(env, tmp[bone->m_Index], g_BoneJNI.parent, 0);
    }

    return arr;
}

static void CreateBones(JNIEnv* env, const dmModelImporter::Scene* scene, const dmArray<jobject>& skins, const dmArray<jobject>& nodes)
{
    uint32_t count = scene->m_SkinsCount;
    for (uint32_t i = 0; i < count; ++i)
    {
        Skin* skin = &scene->m_Skins[i];
        jobject skin_obj = skins[skin->m_Index];

        jobjectArray arr = CreateBonesArray(env, skin->m_BonesCount, skin->m_Bones, nodes);
        SetFieldObject(env, skin_obj, g_SkinJNI.bones, arr);
        env->DeleteLocalRef(arr);
    }
}

// **************************************************
// Skins

static jobject CreateSkin(JNIEnv* env, dmModelImporter::Skin* skin)
{
    jobject obj = env->AllocObject(g_SkinJNI.cls);
    SetFieldInt(env, obj, g_SkinJNI.index, skin->m_Index);
    SetFieldString(env, obj, g_SkinJNI.name, skin->m_Name);
    return obj;
}

static void CreateSkins(JNIEnv* env, const dmModelImporter::Scene* scene, dmArray<jobject>& skins)
{
    uint32_t count = scene->m_SkinsCount;
    skins.SetCapacity(count);
    skins.SetSize(count);

    for (uint32_t i = 0; i < count; ++i)
    {
        Skin* skin = &scene->m_Skins[i];
        skins[skin->m_Index] = CreateSkin(env, skin);
    }
}


static jobject CreateJavaScene(JNIEnv* env, const dmModelImporter::Scene* scene)
{
    InitializeJNITypes(env);

    jobject obj = env->AllocObject(g_SceneJNI.cls);

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
        jobjectArray arr = CreateObjectArray(env, g_NodeJNI.cls, nodes);
        env->SetObjectField(obj, g_SceneJNI.nodes, arr);
        env->DeleteLocalRef(arr);
    }

    {
        uint32_t count = scene->m_RootNodesCount;
        jobjectArray arr = env->NewObjectArray(count, g_NodeJNI.cls, 0);
        for (uint32_t i = 0; i < count; ++i)
        {
            dmModelImporter::Node* root = scene->m_RootNodes[i];
            env->SetObjectArrayElement(arr, i, nodes[root->m_Index]);
        }
        env->SetObjectField(obj, g_SceneJNI.rootNodes, arr);
        env->DeleteLocalRef(arr);
    }

    {
        jobjectArray arr = CreateObjectArray(env, g_SkinJNI.cls, skins);
        env->SetObjectField(obj, g_SceneJNI.skins, arr);
        env->DeleteLocalRef(arr);
    }

    {
        jobjectArray arr = CreateObjectArray(env, g_ModelJNI.cls, models);
        env->SetObjectField(obj, g_SceneJNI.models, arr);
        env->DeleteLocalRef(arr);
    }

    {
        jobjectArray arr = CreateAnimationsArray(env, scene->m_AnimationsCount, scene->m_Animations, nodes);
        env->SetObjectField(obj, g_SceneJNI.animations, arr);
        env->DeleteLocalRef(arr);
    }


    for (uint32_t i = 0; i < nodes.Size(); ++i)
    {
        env->DeleteLocalRef(nodes[i]);
    }
    for (uint32_t i = 0; i < skins.Size(); ++i)
    {
        env->DeleteLocalRef(skins[i]);
    }
    for (uint32_t i = 0; i < models.Size(); ++i)
    {
        env->DeleteLocalRef(models[i]);
    }

    FinalizeJNITypes(env);

    return obj;
}

} // namespace

JNIEXPORT jobject JNICALL Java_ModelImporter_LoadFromBufferInternal(JNIEnv* env, jclass cls, jstring _path, jbyteArray array)
{
    ScopedString j_path(env, _path);
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

    dmLogDebug("LoadFromBufferInternal: %s suffix: %s bytes: %d\n", path, suffix, file_size);

    dmModelImporter::Options options;
    dmModelImporter::Scene* scene = dmModelImporter::LoadFromBuffer(&options, suffix, (uint8_t*)file_data, file_size);
    if (!scene)
    {
        dmLogError("Failed to load %s", path);
        return 0;
    }

    if (dmLogGetLevel() == LOG_SEVERITY_DEBUG) // verbose mode
    {
        dmModelImporter::DebugScene(scene);
    }

    jobject jscene = dmModelImporter::CreateJavaScene(env, scene);

    dmModelImporter::DestroyScene(scene);

    return jscene;
}

JNIEXPORT jint JNICALL Java_ModelImporter_AddressOf(JNIEnv* env, jclass cls, jobject object)
{
    return dmModelImporter::AddressOf(object);
}

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {

    dmLogDebug("JNI_OnLoad ->\n");

    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        printf("JNI_OnLoad GetEnv error\n");
        return JNI_ERR;
    }

    // Find your class. JNI_OnLoad is called from the correct class loader context for this to work.
    jclass c = env->FindClass("com/dynamo/bob/pipeline/ModelImporter");
    dmLogDebug("JNI_OnLoad: c = %p\n", c);
    if (c == 0)
      return JNI_ERR;

    // Register your class' native methods.
    static const JNINativeMethod methods[] = {
        {"LoadFromBufferInternal", "(Ljava/lang/String;[B)L" CLASS_SCENE ";", reinterpret_cast<void*>(Java_ModelImporter_LoadFromBufferInternal)},
        {"AddressOf", "(Ljava/lang/Object;)I", reinterpret_cast<void*>(Java_ModelImporter_AddressOf)},
    };
    int rc = env->RegisterNatives(c, methods, sizeof(methods)/sizeof(JNINativeMethod));
    env->DeleteLocalRef(c);

    if (rc != JNI_OK) return rc;

    dmLogDebug("JNI_OnLoad return.\n");
    return JNI_VERSION_1_6;
}

namespace dmModelImporter
{

}
