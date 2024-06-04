// Copyright 2020-2024 The Defold Foundation
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
#include "jni_util.h"

#include <jni.h>

#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>

#define CLASS_SCENE     "com/dynamo/bob/pipeline/ModelImporter$Scene"

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

// static void OutputTransform(const dmTransform::Transform& transform)
// {
//     printf("    t: %f, %f, %f\n", transform.GetTranslation().getX(), transform.GetTranslation().getY(), transform.GetTranslation().getZ());
//     printf("    r: %f, %f, %f, %f\n", transform.GetRotation().getX(), transform.GetRotation().getY(), transform.GetRotation().getZ(), transform.GetRotation().getW());
//     printf("    s: %f, %f, %f\n", transform.GetScale().getX(), transform.GetScale().getY(), transform.GetScale().getZ());
// }

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

// static jfieldID GetFieldInt(JNIEnv* env, jclass cls, const char* field_name)
// {
//     return env->GetFieldID(cls, field_name, "I");
// }
// static jfieldID GetFieldString(JNIEnv* env, jclass cls, const char* field_name)
// {
//     return env->GetFieldID(cls, field_name, "Ljava/lang/String;");
// }
// static jfieldID GetFieldNode(JNIEnv* env, jclass cls, const char* field_name)
// {
//     return env->GetFieldID(cls, field_name, "Lcom/dynamo/bob/pipeline/ModelImporter$Node;");
// }

// ******************************************************************************************************************

struct SceneJNI
{
    jclass      cls;
    jfieldID    nodes;
    jfieldID    models;
    jfieldID    skins;
    jfieldID    rootNodes;
    jfieldID    animations;
    jfieldID    materials;
    jfieldID    buffers;
};

struct MaterialJNI
{
    jclass      cls;
    jfieldID    name;
    jfieldID    index;
};

struct SkinJNI
{
    jclass      cls;
    jfieldID    name;
    jfieldID    bones;
    jfieldID    index;
};

struct BoneJNI
{
    jclass      cls;
    jfieldID    invBindPose;
    jfieldID    name;
    jfieldID    index;
    jfieldID    node;
    jfieldID    parent;
};

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
};

struct ModelJNI
{
    jclass      cls;
    jfieldID    name;
    jfieldID    meshes;
    jfieldID    index;
    jfieldID    boneParentName; // String
};

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

    jfieldID    aabb;
};

struct AabbJNI
{
    jclass      cls;
    jfieldID    min;
    jfieldID    max;
};

struct Vec4JNI
{
    jclass      cls;
    jfieldID    x, y, z, w;
};

struct TransformJNI
{
    jclass      cls;
    jfieldID    translation;
    jfieldID    rotation;
    jfieldID    scale;
};

struct KeyFrameJNI
{
    jclass      cls;
    jfieldID    value;
    jfieldID    time;
};

struct NodeAnimationJNI
{
    jclass      cls;
    jfieldID    node;
    jfieldID    translationKeys;
    jfieldID    rotationKeys;
    jfieldID    scaleKeys;
    jfieldID    startTime;
    jfieldID    endTime;
} ;

struct AnimationJNI
{
    jclass      cls;
    jfieldID    name;
    jfieldID    nodeAnimations;
    jfieldID    duration;
};

struct BufferJNI // GLTF format
{
    jclass      cls;
    jfieldID    uri;
    jfieldID    buffer;
};

struct TypeInfos
{
#define MEMBER(TYPE) TYPE m_ ## TYPE
    MEMBER(SceneJNI);
    MEMBER(SkinJNI);
    MEMBER(BoneJNI);
    MEMBER(NodeJNI);
    MEMBER(ModelJNI);
    MEMBER(MeshJNI);
    MEMBER(AabbJNI);
    MEMBER(Vec4JNI);
    MEMBER(TransformJNI);
    MEMBER(KeyFrameJNI);
    MEMBER(NodeAnimationJNI);
    MEMBER(AnimationJNI);
    MEMBER(BufferJNI);
    MEMBER(MaterialJNI);

#undef MEMBER
};

static void InitializeJNITypes(JNIEnv* env, TypeInfos* infos)
{
#define SETUP_CLASS(TYPE, TYPE_NAME) \
    TYPE * obj = &infos->m_ ## TYPE ; \
    obj->cls = GetClass(env, TYPE_NAME); \
    if (!obj->cls) { \
        char fullname[128]; \
        dmSnPrintf(fullname, sizeof(fullname), CLASS_NAME_FORMAT, TYPE_NAME); \
        printf("ERROR: Failed to get class %s\n", fullname); \
    } \
    assert(obj->cls);

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
        SETUP_CLASS(AabbJNI, "Aabb");
        GET_FLD(min, "Vec4");
        GET_FLD(max, "Vec4");
    }
    {
        SETUP_CLASS(BufferJNI, "Buffer");
        GET_FLD_TYPESTR(uri, "Ljava/lang/String;");
        GET_FLD_TYPESTR(buffer, "[B");
    }
    {
        SETUP_CLASS(SceneJNI, "Scene");
        GET_FLD_ARRAY(nodes, "Node");
        GET_FLD_ARRAY(models, "Model");
        GET_FLD_ARRAY(skins, "Skin");
        GET_FLD_ARRAY(rootNodes, "Node");
        GET_FLD_ARRAY(animations, "Animation");
        GET_FLD_ARRAY(materials, "Material");
        GET_FLD_ARRAY(buffers, "Buffer");
    }
    {
        SETUP_CLASS(MaterialJNI, "Material");
        GET_FLD_TYPESTR(name, "Ljava/lang/String;");
        GET_FLD_TYPESTR(index, "I");
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
        GET_FLD_TYPESTR(boneParentName, "Ljava/lang/String;");
    }
    {
        SETUP_CLASS(MeshJNI, "Mesh");
        GET_FLD_TYPESTR(name, "Ljava/lang/String;");

        GET_FLD(material, "Material");
        GET_FLD(aabb, "Aabb");

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

static int AddressOf(jobject object)
{
    uint64_t a = *(uint64_t*)(uintptr_t)object;
    return a;
}

// static int GetAddressOfField(JNIEnv* env, jobject object, jfieldID field)
// {
//     jobject field_object = env->GetObjectField(object, field);
//     int id = AddressOf(field_object);
//     env->DeleteLocalRef(field_object);
//     return id;
// }

// ******************************************************************************************************************


// static void SetFieldString(JNIEnv* env, jclass cls, jobject obj, const char* field_name, const char* value)
// {
//     jfieldID field = GetFieldString(env, cls, field_name);
//     jstring str = env->NewStringUTF(value);
//     env->SetObjectField(obj, field, str);
//     env->DeleteLocalRef(str);
// }

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

static jobject CreateVec4(JNIEnv* env, const TypeInfos* types, const dmVMath::Vector4& value)
{
    jobject obj = env->AllocObject(types->m_Vec4JNI.cls);
    env->SetFloatField(obj, types->m_Vec4JNI.x, value.getX());
    env->SetFloatField(obj, types->m_Vec4JNI.y, value.getY());
    env->SetFloatField(obj, types->m_Vec4JNI.z, value.getZ());
    env->SetFloatField(obj, types->m_Vec4JNI.w, value.getW());
    return obj;
}

static jobject CreateAabb(JNIEnv* env, const TypeInfos* types, const Aabb& value)
{
    jobject obj = env->AllocObject(types->m_AabbJNI.cls);
    SetFieldObject(env, obj, types->m_AabbJNI.min, CreateVec4(env, types, dmVMath::Vector4(value.m_Min[0], value.m_Min[1], value.m_Min[2], 1.0f)));
    SetFieldObject(env, obj, types->m_AabbJNI.max, CreateVec4(env, types, dmVMath::Vector4(value.m_Max[0], value.m_Max[1], value.m_Max[2], 1.0f)));
    return obj;
}

static jobject CreateTransform(JNIEnv* env, const TypeInfos* types, const dmTransform::Transform* transform)
{
    jobject obj = env->AllocObject(types->m_TransformJNI.cls);
    SetFieldObject(env, obj, types->m_TransformJNI.translation, CreateVec4(env, types, dmVMath::Vector4(transform->GetTranslation())));
    SetFieldObject(env, obj, types->m_TransformJNI.rotation, CreateVec4(env, types, dmVMath::Vector4(transform->GetRotation())));
    SetFieldObject(env, obj, types->m_TransformJNI.scale, CreateVec4(env, types, dmVMath::Vector4(transform->GetScale())));
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

static jbyteArray CreateByteArray(JNIEnv* env, uint32_t count, const uint8_t* values)
{
    jbyteArray arr = env->NewByteArray(count);
    env->SetByteArrayRegion(arr, 0, count, (const jbyte*)values);
    return arr;
}

// For debugging the set values
// static void GetVec4(JNIEnv* env, const TypeInfos* types, jobject object, float* vec4)
// {
//     vec4[0] = env->GetFloatField(object, types->m_Vec4JNI.x);
//     vec4[1] = env->GetFloatField(object, types->m_Vec4JNI.y);
//     vec4[2] = env->GetFloatField(object, types->m_Vec4JNI.z);
//     vec4[3] = env->GetFloatField(object, types->m_Vec4JNI.w);
// }

// static void GetTransform(JNIEnv* env, const TypeInfos* types, jobject object, jfieldID field, dmTransform::Transform* out)
// {
//     jobject xform = env->GetObjectField(object, field);
//     // TODO: check if it's a transform class!

//     jobject xform_pos = env->GetObjectField(xform, types->m_TransformJNI.translation);
//     jobject xform_rot = env->GetObjectField(xform, types->m_TransformJNI.rotation);
//     jobject xform_scl = env->GetObjectField(xform, types->m_TransformJNI.scale);

//     float v[4] = {};
//     GetVec4(env, types, xform_pos, v);
//     out->SetTranslation(dmVMath::Vector3(v[0], v[1], v[2]));
//     GetVec4(env, types, xform_rot, v);
//     out->SetRotation(dmVMath::Quat(v[0], v[1], v[2], v[3]));
//     GetVec4(env, types, xform_scl, v);
//     out->SetScale(dmVMath::Vector3(v[0], v[1], v[2]));

//     env->DeleteLocalRef(xform_pos);
//     env->DeleteLocalRef(xform_rot);
//     env->DeleteLocalRef(xform_scl);
//     env->DeleteLocalRef(xform);
// }

// **************************************************
// Material

static jobject CreateMaterial(JNIEnv* env, const TypeInfos* types, const dmModelImporter::Material* material)
{
    jobject obj = env->AllocObject(types->m_MaterialJNI.cls);
    SetFieldInt(env, obj, types->m_MaterialJNI.index, material->m_Index);
    SetFieldString(env, obj, types->m_MaterialJNI.name, material->m_Name);
    return obj;
}

static jobjectArray CreateMaterialsArray(JNIEnv* env, const TypeInfos* types,
                        uint32_t count, const dmModelImporter::Material* materials,
                        uint32_t dynamic_count, dmModelImporter::Material** const dynamic_materials,
                        dmArray<jobject>& nodes)
{
    uint32_t total_count = count + dynamic_count;
    nodes.SetCapacity(total_count);
    nodes.SetSize(total_count);

    jobjectArray arr = env->NewObjectArray(total_count, types->m_MaterialJNI.cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        const dmModelImporter::Material* material = &materials[i];
        jobject obj = CreateMaterial(env, types, material);
        nodes[material->m_Index] = obj;
        env->SetObjectArrayElement(arr, material->m_Index, obj);
    }
    for (uint32_t i = 0; i < dynamic_count; ++i)
    {
        const dmModelImporter::Material* material = dynamic_materials[i];
        jobject obj = CreateMaterial(env, types, material);
        nodes[material->m_Index] = obj;
        env->SetObjectArrayElement(arr, material->m_Index, obj);
    }

    return arr;
}

// Buffer

static jobject CreateBuffer(JNIEnv* env, const TypeInfos* types, const dmModelImporter::Buffer* buffer)
{
    jobject obj = env->AllocObject(types->m_BufferJNI.cls);
    SetFieldString(env, obj, types->m_BufferJNI.uri, buffer->m_Uri);

    if (buffer->m_Buffer)
    {
        jbyteArray arr = CreateByteArray(env, buffer->m_BufferSize, (uint8_t*)buffer->m_Buffer);
        env->SetObjectField(obj, types->m_BufferJNI.buffer, arr);
        env->DeleteLocalRef(arr);
    }
    return obj;
}

static jobjectArray CreateBuffersArray(JNIEnv* env, const TypeInfos* types, uint32_t count, const dmModelImporter::Buffer* buffers)
{
    jobjectArray arr = env->NewObjectArray(count, types->m_BufferJNI.cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        jobject obj = CreateBuffer(env, types, &buffers[i]);
        env->SetObjectArrayElement(arr, i, obj);
    }
    return arr;
}

// **************************************************
// Nodes

static jobject CreateNode(JNIEnv* env, const TypeInfos* types, const dmModelImporter::Node* node)
{
    jobject obj = env->AllocObject(types->m_NodeJNI.cls);
    SetFieldInt(env, obj, types->m_NodeJNI.index, node->m_Index);
    SetFieldString(env, obj, types->m_NodeJNI.name, node->m_Name);
    SetFieldObject(env, obj, types->m_NodeJNI.local, CreateTransform(env, types, &node->m_Local));
    SetFieldObject(env, obj, types->m_NodeJNI.world, CreateTransform(env, types, &node->m_World));
    return obj;
}

static void CreateNodes(JNIEnv* env, const TypeInfos* types, const dmModelImporter::Scene* scene, dmArray<jobject>& nodes)
{
    uint32_t count = scene->m_NodesCount;
    nodes.SetCapacity(count);
    nodes.SetSize(count);

    for (uint32_t i = 0; i < count; ++i)
    {
        Node* node = &scene->m_Nodes[i];
        nodes[node->m_Index] = CreateNode(env, types, node);
    }

    for (uint32_t i = 0; i < count; ++i)
    {
        Node* node = &scene->m_Nodes[i];

        if (node->m_Parent != 0)
        {
            SetFieldObject(env, nodes[i], types->m_NodeJNI.parent, nodes[node->m_Parent->m_Index]);
        }

        jobjectArray childrenArray = env->NewObjectArray(node->m_ChildrenCount, types->m_NodeJNI.cls, 0);
        for (uint32_t i = 0; i < node->m_ChildrenCount; ++i)
        {
            dmModelImporter::Node* child = node->m_Children[i];
            env->SetObjectArrayElement(childrenArray, i, nodes[child->m_Index]);
        }
        env->SetObjectField(nodes[i], types->m_NodeJNI.children, childrenArray);
        env->DeleteLocalRef(childrenArray);
    }
}

static void FixupNodeReferences(JNIEnv* env, const TypeInfos* types, const dmModelImporter::Scene* scene, const dmArray<jobject>& skins, const dmArray<jobject>& models, const dmArray<jobject>& nodes)
{
    uint32_t count = scene->m_NodesCount;
    for (uint32_t i = 0; i < count; ++i)
    {
        dmModelImporter::Node* node = &scene->m_Nodes[i];
        if (node->m_Skin)
        {
            jobject node_obj = nodes[node->m_Index];
            jobject skin_obj = skins[node->m_Skin->m_Index];
            SetFieldObject(env, node_obj, types->m_NodeJNI.skin, skin_obj);
        }

        if (node->m_Model)
        {
            jobject node_obj = nodes[node->m_Index];
            jobject model_obj = models[node->m_Model->m_Index];
            SetFieldObject(env, node_obj, types->m_NodeJNI.model, model_obj);
        }

    }
}

// **************************************************
// Meshes

static jobject CreateMesh(JNIEnv* env, const TypeInfos* types, const dmArray<jobject>& materials, const dmModelImporter::Mesh* mesh)
{
    jobject obj = env->AllocObject(types->m_MeshJNI.cls);
    SetFieldString(env, obj, types->m_MeshJNI.name, mesh->m_Name);
    if (mesh->m_Material)
        SetFieldObject(env, obj, types->m_MeshJNI.material, materials[mesh->m_Material->m_Index]);

    SetFieldInt(env, obj, types->m_MeshJNI.vertexCount, mesh->m_VertexCount);
    SetFieldInt(env, obj, types->m_MeshJNI.indexCount, mesh->m_IndexCount);
    SetFieldInt(env, obj, types->m_MeshJNI.texCoords0NumComponents, mesh->m_TexCoord0NumComponents);
    SetFieldInt(env, obj, types->m_MeshJNI.texCoords1NumComponents, mesh->m_TexCoord1NumComponents);

#define SET_FARRAY(OBJ, FIELD, COUNT, VALUES) \
    { \
        if (VALUES) { \
            jfloatArray arr = CreateFloatArray(env, COUNT, VALUES); \
            env->SetObjectField(OBJ, types->m_MeshJNI. FIELD, arr); \
            env->DeleteLocalRef(arr); \
        } \
    }
#define SET_IARRAY(OBJ, FIELD, COUNT, VALUES) \
    { \
        if (VALUES) { \
            jintArray arr = CreateIntArray(env, COUNT, (const int*)VALUES); \
            env->SetObjectField(OBJ, types->m_MeshJNI. FIELD, arr); \
            env->DeleteLocalRef(arr); \
        } \
    }

    uint32_t vcount = mesh->m_VertexCount;
    uint32_t icount = mesh->m_IndexCount;
    SET_FARRAY(obj, positions, vcount * 3, mesh->m_Positions);
    SET_FARRAY(obj, normals, vcount * 3, mesh->m_Normals);
    SET_FARRAY(obj, tangents, vcount * 4, mesh->m_Tangents);
    SET_FARRAY(obj, colors, vcount * 4, mesh->m_Color);
    SET_FARRAY(obj, weights, vcount * 4, mesh->m_Weights);
    SET_FARRAY(obj, texCoords0, vcount * mesh->m_TexCoord0NumComponents, mesh->m_TexCoord0);
    SET_FARRAY(obj, texCoords1, vcount * mesh->m_TexCoord1NumComponents, mesh->m_TexCoord1);

    SET_IARRAY(obj, bones, vcount * 4, mesh->m_Bones);

    SET_IARRAY(obj, indices, icount, mesh->m_Indices);

    SetFieldObject(env, obj, types->m_MeshJNI.aabb, CreateAabb(env, types, mesh->m_Aabb));

#undef SET_FARRAY
#undef SET_UARRAY

    return obj;
}

static jobjectArray CreateMeshesArray(JNIEnv* env, const TypeInfos* types, const dmArray<jobject>& materials, uint32_t count, const dmModelImporter::Mesh* meshes)
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

static jobject CreateModel(JNIEnv* env, const TypeInfos* types, const dmArray<jobject>& materials, const dmModelImporter::Model* model)
{
    jobject obj = env->AllocObject(types->m_ModelJNI.cls);
    SetFieldInt(env, obj, types->m_ModelJNI.index, model->m_Index);
    SetFieldString(env, obj, types->m_ModelJNI.name, model->m_Name);
    SetFieldString(env, obj, types->m_ModelJNI.boneParentName, model->m_ParentBone ? model->m_ParentBone->m_Name: "");

    jobjectArray arr = CreateMeshesArray(env, types, materials, model->m_MeshesCount, model->m_Meshes);
    env->SetObjectField(obj, types->m_ModelJNI.meshes, arr);
    env->DeleteLocalRef(arr);
    return obj;
}

static void CreateModels(JNIEnv* env, const TypeInfos* types, const dmModelImporter::Scene* scene, const dmArray<jobject>& materials, dmArray<jobject>& models)
{
    uint32_t count = scene->m_ModelsCount;
    models.SetCapacity(count);
    models.SetSize(count);

    for (uint32_t i = 0; i < count; ++i)
    {
        Model* model = &scene->m_Models[i];
        models[model->m_Index] = CreateModel(env, types, materials, model);
    }
}

// **************************************************
// Animations

static jobject CreateKeyFrame(JNIEnv* env, const TypeInfos* types, const dmModelImporter::KeyFrame* key_frame)
{
    jobject obj = env->AllocObject(types->m_KeyFrameJNI.cls);
    jfloatArray arr = env->NewFloatArray(4);
    env->SetFloatArrayRegion(arr, 0, 4, key_frame->m_Value);
    env->SetObjectField(obj, types->m_KeyFrameJNI.value, arr);
    env->SetFloatField(obj, types->m_KeyFrameJNI.time, key_frame->m_Time);
    env->DeleteLocalRef(arr);
    return obj;
}

static jobjectArray CreateKeyFramesArray(JNIEnv* env, const TypeInfos* types, uint32_t count, const dmModelImporter::KeyFrame* key_frames)
{
    jobjectArray arr = env->NewObjectArray(count, types->m_KeyFrameJNI.cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        jobject o = CreateKeyFrame(env, types, &key_frames[i]);
        env->SetObjectArrayElement(arr, i, o);
        env->DeleteLocalRef(o);
    }
    return arr;
}

static jobject CreateNodeAnimation(JNIEnv* env, const TypeInfos* types, const dmModelImporter::NodeAnimation* node_anim, const dmArray<jobject>& nodes)
{
    jobject obj = env->AllocObject(types->m_NodeAnimationJNI.cls);
    SetFieldObject(env, obj, types->m_NodeAnimationJNI.node, nodes[node_anim->m_Node->m_Index]);

    jobjectArray arr = CreateKeyFramesArray(env, types, node_anim->m_TranslationKeysCount, node_anim->m_TranslationKeys);
    env->SetObjectField(obj, types->m_NodeAnimationJNI.translationKeys, arr);
    env->DeleteLocalRef(arr);

    arr = CreateKeyFramesArray(env, types, node_anim->m_RotationKeysCount, node_anim->m_RotationKeys);
    env->SetObjectField(obj, types->m_NodeAnimationJNI.rotationKeys, arr);
    env->DeleteLocalRef(arr);

    arr = CreateKeyFramesArray(env, types, node_anim->m_ScaleKeysCount, node_anim->m_ScaleKeys);
    env->SetObjectField(obj, types->m_NodeAnimationJNI.scaleKeys, arr);
    env->DeleteLocalRef(arr);

    env->SetFloatField(obj, types->m_NodeAnimationJNI.startTime, node_anim->m_StartTime);
    env->SetFloatField(obj, types->m_NodeAnimationJNI.endTime,  node_anim->m_EndTime);
    return obj;
}

static jobjectArray CreateNodeAnimationsArray(JNIEnv* env, const TypeInfos* types, uint32_t count, const dmModelImporter::NodeAnimation* node_anim, const dmArray<jobject>& nodes)
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

static jobject CreateAnimation(JNIEnv* env, const TypeInfos* types, const dmModelImporter::Animation* animation, const dmArray<jobject>& nodes)
{
    jobject obj = env->AllocObject(types->m_AnimationJNI.cls);
    SetFieldString(env, obj, types->m_AnimationJNI.name, animation->m_Name);

    jobjectArray arr = CreateNodeAnimationsArray(env, types, animation->m_NodeAnimationsCount, animation->m_NodeAnimations, nodes);
    env->SetObjectField(obj, types->m_AnimationJNI.nodeAnimations, arr);
    env->DeleteLocalRef(arr);

    env->SetFloatField(obj, types->m_AnimationJNI.duration, animation->m_Duration);

    return obj;
}

static jobjectArray CreateAnimationsArray(JNIEnv* env, const TypeInfos* types, uint32_t count, const dmModelImporter::Animation* animations, const dmArray<jobject>& nodes)
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

static jobject CreateBone(JNIEnv* env, const TypeInfos* types, dmModelImporter::Bone* bone, const dmArray<jobject>& nodes)
{
    jobject obj = env->AllocObject(types->m_BoneJNI.cls);
    SetFieldInt(env, obj, types->m_BoneJNI.index, bone->m_Index);
    SetFieldString(env, obj, types->m_BoneJNI.name, bone->m_Name);
    SetFieldObject(env, obj, types->m_BoneJNI.invBindPose, CreateTransform(env, types, &bone->m_InvBindPose));
    if (bone->m_Node != 0) // A generated root bone doesn't have a corresponding Node
        SetFieldObject(env, obj, types->m_BoneJNI.node, nodes[bone->m_Node->m_Index]);
    else
        SetFieldObject(env, obj, types->m_BoneJNI.node, 0);
    return obj;
}

static jobjectArray CreateBonesArray(JNIEnv* env, const TypeInfos* types, uint32_t count, dmModelImporter::Bone* bones, const dmArray<jobject>& nodes)
{
    dmArray<jobject> tmp;
    tmp.SetCapacity(count);
    tmp.SetSize(count);

    jobjectArray arr = env->NewObjectArray(count, types->m_BoneJNI.cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        dmModelImporter::Bone* bone = &bones[i];
        tmp[bone->m_Index] = CreateBone(env, types, bone, nodes);
        env->SetObjectArrayElement(arr, i, tmp[bone->m_Index]);
    }

    for (uint32_t i = 0; i < count; ++i)
    {
        dmModelImporter::Bone* bone = &bones[i];
        if (bone->m_ParentIndex != INVALID_INDEX)
            SetFieldObject(env, tmp[bone->m_Index], types->m_BoneJNI.parent, tmp[bone->m_ParentIndex]);
        else
            SetFieldObject(env, tmp[bone->m_Index], types->m_BoneJNI.parent, 0);
    }

    return arr;
}

static void CreateBones(JNIEnv* env, const TypeInfos* types, const dmModelImporter::Scene* scene, const dmArray<jobject>& skins, const dmArray<jobject>& nodes)
{
    uint32_t count = scene->m_SkinsCount;
    for (uint32_t i = 0; i < count; ++i)
    {
        Skin* skin = &scene->m_Skins[i];
        jobject skin_obj = skins[skin->m_Index];

        jobjectArray arr = CreateBonesArray(env, types, skin->m_BonesCount, skin->m_Bones, nodes);
        SetFieldObject(env, skin_obj, types->m_SkinJNI.bones, arr);
        env->DeleteLocalRef(arr);
    }
}

// **************************************************
// Skins

static jobject CreateSkin(JNIEnv* env, const TypeInfos* types, dmModelImporter::Skin* skin)
{
    jobject obj = env->AllocObject(types->m_SkinJNI.cls);
    SetFieldInt(env, obj, types->m_SkinJNI.index, skin->m_Index);
    SetFieldString(env, obj, types->m_SkinJNI.name, skin->m_Name);
    return obj;
}

static void CreateSkins(JNIEnv* env, const TypeInfos* types, const dmModelImporter::Scene* scene, dmArray<jobject>& skins)
{
    uint32_t count = scene->m_SkinsCount;
    skins.SetCapacity(count);
    skins.SetSize(count);

    for (uint32_t i = 0; i < count; ++i)
    {
        Skin* skin = &scene->m_Skins[i];
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

static jobject CreateJavaScene(JNIEnv* env, const dmModelImporter::Scene* scene)
{
    TypeInfos types;
    InitializeJNITypes(env, &types);

    jobject obj = env->AllocObject(types.m_SceneJNI.cls);

    dmArray<jobject> models;
    dmArray<jobject> skins;
    dmArray<jobject> nodes;
    dmArray<jobject> materials;

    {
        jobjectArray arr = CreateBuffersArray(env, &types, scene->m_BuffersCount, scene->m_Buffers);
        env->SetObjectField(obj, types.m_SceneJNI.buffers, arr);
        env->DeleteLocalRef(arr);
    }

    {
        jobjectArray arr = CreateMaterialsArray(env, &types,
                                    scene->m_MaterialsCount, scene->m_Materials,
                                    scene->m_DynamicMaterialsCount, scene->m_DynamicMaterials,
                                    materials);
        env->SetObjectField(obj, types.m_SceneJNI.materials, arr);
        env->DeleteLocalRef(arr);
    }

    // Creates all nodes, and leaves out setting skins/models
    CreateNodes(env, &types, scene, nodes);

    CreateSkins(env, &types, scene, skins);
    CreateModels(env, &types, scene, materials, models);
    CreateBones(env, &types, scene, skins, nodes);

    // Set the skin+model to the nodes
    FixupNodeReferences(env, &types, scene, skins, models, nodes);

    ///
    {
        jobjectArray arr = CreateObjectArray(env, types.m_NodeJNI.cls, nodes);
        env->SetObjectField(obj, types.m_SceneJNI.nodes, arr);
        env->DeleteLocalRef(arr);
    }

    {
        uint32_t count = scene->m_RootNodesCount;
        jobjectArray arr = env->NewObjectArray(count, types.m_NodeJNI.cls, 0);
        for (uint32_t i = 0; i < count; ++i)
        {
            dmModelImporter::Node* root = scene->m_RootNodes[i];
            env->SetObjectArrayElement(arr, i, nodes[root->m_Index]);
        }
        env->SetObjectField(obj, types.m_SceneJNI.rootNodes, arr);
        env->DeleteLocalRef(arr);
    }

    {
        jobjectArray arr = CreateObjectArray(env, types.m_SkinJNI.cls, skins);
        env->SetObjectField(obj, types.m_SceneJNI.skins, arr);
        env->DeleteLocalRef(arr);
    }

    {
        jobjectArray arr = CreateObjectArray(env, types.m_ModelJNI.cls, models);
        env->SetObjectField(obj, types.m_SceneJNI.models, arr);
        env->DeleteLocalRef(arr);
    }

    {
        jobjectArray arr = CreateAnimationsArray(env, &types, scene->m_AnimationsCount, scene->m_Animations, nodes);
        env->SetObjectField(obj, types.m_SceneJNI.animations, arr);
        env->DeleteLocalRef(arr);
    }

    DeleteLocalRefs(env, nodes);
    DeleteLocalRefs(env, skins);
    DeleteLocalRefs(env, models);
    DeleteLocalRefs(env, materials);

    return obj;
}

} // namespace

static jobject LoadFromBufferInternal(JNIEnv* env, jclass cls, jstring _path, jbyteArray array, jobject data_resolver)
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

        for (uint32_t i = 0; i < scene->m_BuffersCount; ++i)
        {
            if (scene->m_Buffers[i].m_Buffer)
                continue;

            const char* uri = scene->m_Buffers[i].m_Uri;
            jstring j_uri = env->NewStringUTF(uri);

            jbyteArray bytes = (jbyteArray)env->CallObjectMethod(data_resolver, get_data, _path, j_uri);
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

    jobject jscene = dmModelImporter::CreateJavaScene(env, scene);

    dmModelImporter::DestroyScene(scene);

    env->ReleaseByteArrayElements(array, file_data, JNI_ABORT);

    return jscene;
}

JNIEXPORT jobject JNICALL Java_ModelImporter_LoadFromBufferInternal(JNIEnv* env, jclass cls, jstring _path, jbyteArray array, jobject data_resolver)
{
    dmLogDebug("Java_ModelImporter_LoadFromBufferInternal: env = %p\n", env);
    dmJNI::SignalContextScope env_scope(env);

    jobject jscene;
    DM_JNI_GUARD_SCOPE_BEGIN();
        jscene = LoadFromBufferInternal(env, cls, _path, array, data_resolver);
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return jscene;
}

JNIEXPORT jint JNICALL Java_ModelImporter_AddressOf(JNIEnv* env, jclass cls, jobject object)
{
    return dmModelImporter::AddressOf(object);
}

JNIEXPORT void JNICALL Java_ModelImporter_TestException(JNIEnv* env, jclass cls, jstring j_message)
{
    dmJNI::SignalContextScope env_scope(env);
    ScopedString s_message(env, j_message);
    const char* message = s_message.m_String;
    printf("Received message: %s\n", message);
    dmJNI::TestSignalFromString(message);
}

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {

    dmLogDebug("JNI_OnLoad ->\n");
    dmJNI::EnableDefaultSignalHandlers(vm);

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
    // Don't forget to add them to the corresponding java file (e.g. ModelImporter.java)
    static const JNINativeMethod methods[] = {
        {"LoadFromBufferInternal", "(Ljava/lang/String;[BLjava/lang/Object;)L" CLASS_SCENE ";", reinterpret_cast<void*>(Java_ModelImporter_LoadFromBufferInternal)},
        {"AddressOf", "(Ljava/lang/Object;)I", reinterpret_cast<void*>(Java_ModelImporter_AddressOf)},
        {"TestException", "(Ljava/lang/String;)V", reinterpret_cast<void*>(Java_ModelImporter_TestException)},
    };
    int rc = env->RegisterNatives(c, methods, sizeof(methods)/sizeof(JNINativeMethod));
    env->DeleteLocalRef(c);

    if (rc != JNI_OK) return rc;

    dmLogDebug("JNI_OnLoad return.\n");
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved)
{
    dmLogDebug("JNI_OnUnload ->\n");
}

namespace dmModelImporter
{

}
