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
        GET_FLD_TYPESTR(min, "[F");
        GET_FLD_TYPESTR(max, "[F");
    }
    {
        SETUP_CLASS(MaterialJNI, "Material");
        GET_FLD_TYPESTR(name, "Ljava/lang/String;");
        GET_FLD_TYPESTR(index, "I");
        GET_FLD_TYPESTR(isSkinned, "B");
    }
    {
        SETUP_CLASS(MeshJNI, "Mesh");
        GET_FLD_TYPESTR(name, "Ljava/lang/String;");
        GET_FLD(material, "Material");
        GET_FLD_TYPESTR(positions, "[F");
        GET_FLD_TYPESTR(normals, "[F");
        GET_FLD_TYPESTR(tangents, "[F");
        GET_FLD_TYPESTR(color, "[F");
        GET_FLD_TYPESTR(weights, "[F");
        GET_FLD_TYPESTR(bones, "[I");
        GET_FLD_TYPESTR(texCoord0NumComponents, "I");
        GET_FLD_TYPESTR(texCoord0, "[F");
        GET_FLD_TYPESTR(texCoord1NumComponents, "I");
        GET_FLD_TYPESTR(texCoord1, "[F");
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
        GET_FLD_TYPESTR(buffer, "J");
        GET_FLD_TYPESTR(bufferSize, "I");
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
    dmJNI::SetObjectDeref(env, obj, types->m_AabbJNI.min, dmJNI::C2J_CreateFloatArray(env, src->m_Min, 3));
    dmJNI::SetObjectDeref(env, obj, types->m_AabbJNI.max, dmJNI::C2J_CreateFloatArray(env, src->m_Max, 3));
    return obj;
}

jobject C2J_CreateMaterial(JNIEnv* env, TypeInfos* types, const Material* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_MaterialJNI.cls);
    dmJNI::SetString(env, obj, types->m_MaterialJNI.name, src->m_Name);
    dmJNI::SetUInt(env, obj, types->m_MaterialJNI.index, src->m_Index);
    dmJNI::SetUByte(env, obj, types->m_MaterialJNI.isSkinned, src->m_IsSkinned);
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
    dmJNI::SetObjectDeref(env, obj, types->m_MeshJNI.color, dmJNI::C2J_CreateFloatArray(env, src->m_Color.Begin(), src->m_Color.Size()));
    dmJNI::SetObjectDeref(env, obj, types->m_MeshJNI.weights, dmJNI::C2J_CreateFloatArray(env, src->m_Weights.Begin(), src->m_Weights.Size()));
    dmJNI::SetObjectDeref(env, obj, types->m_MeshJNI.bones, dmJNI::C2J_CreateUIntArray(env, src->m_Bones.Begin(), src->m_Bones.Size()));
    dmJNI::SetUInt(env, obj, types->m_MeshJNI.texCoord0NumComponents, src->m_TexCoord0NumComponents);
    dmJNI::SetObjectDeref(env, obj, types->m_MeshJNI.texCoord0, dmJNI::C2J_CreateFloatArray(env, src->m_TexCoord0.Begin(), src->m_TexCoord0.Size()));
    dmJNI::SetUInt(env, obj, types->m_MeshJNI.texCoord1NumComponents, src->m_TexCoord1NumComponents);
    dmJNI::SetObjectDeref(env, obj, types->m_MeshJNI.texCoord1, dmJNI::C2J_CreateFloatArray(env, src->m_TexCoord1.Begin(), src->m_TexCoord1.Size()));
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
    dmJNI::SetLong(env, obj, types->m_BufferJNI.buffer, (uintptr_t)src->m_Buffer);
    dmJNI::SetUInt(env, obj, types->m_BufferJNI.bufferSize, src->m_BufferSize);
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
            dmJNI::J2C_CopyFloatArray(env, (jfloatArray)field_object, out->m_Min, 3);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_AabbJNI.max);
        if (field_object) {
            dmJNI::J2C_CopyFloatArray(env, (jfloatArray)field_object, out->m_Max, 3);
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
        jobject field_object = env->GetObjectField(obj, types->m_MeshJNI.color);
        if (field_object) {
            uint32_t tmp_count;
            float* tmp = dmJNI::J2C_CreateFloatArray(env, (jfloatArray)field_object, &tmp_count);
            out->m_Color.Set(tmp, tmp_count, tmp_count, false);
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
    out->m_TexCoord0NumComponents = dmJNI::GetUInt(env, obj, types->m_MeshJNI.texCoord0NumComponents);
    {
        jobject field_object = env->GetObjectField(obj, types->m_MeshJNI.texCoord0);
        if (field_object) {
            uint32_t tmp_count;
            float* tmp = dmJNI::J2C_CreateFloatArray(env, (jfloatArray)field_object, &tmp_count);
            out->m_TexCoord0.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_TexCoord1NumComponents = dmJNI::GetUInt(env, obj, types->m_MeshJNI.texCoord1NumComponents);
    {
        jobject field_object = env->GetObjectField(obj, types->m_MeshJNI.texCoord1);
        if (field_object) {
            uint32_t tmp_count;
            float* tmp = dmJNI::J2C_CreateFloatArray(env, (jfloatArray)field_object, &tmp_count);
            out->m_TexCoord1.Set(tmp, tmp_count, tmp_count, false);
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
    out->m_Buffer = (void*)(uintptr_t)dmJNI::GetLong(env, obj, types->m_BufferJNI.buffer);
    out->m_BufferSize = dmJNI::GetUInt(env, obj, types->m_BufferJNI.bufferSize);
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
