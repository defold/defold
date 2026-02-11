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

#include "Shaderc_jni.h"
#include <jni/jni_util.h>
#include <dlib/array.h>

#define CLASS_NAME_FORMAT "com/dynamo/bob/pipeline/Shaderc$%s"

namespace dmShaderc {
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
        SETUP_CLASS(ShaderCompilerOptionsJNI, "ShaderCompilerOptions");
        GET_FLD_TYPESTR(version, "I");
        GET_FLD_TYPESTR(entryPoint, "Ljava/lang/String;");
        GET_FLD_TYPESTR(removeUnusedVariables, "B");
        GET_FLD_TYPESTR(no420PackExtension, "B");
        GET_FLD_TYPESTR(glslEmitUboAsPlainUniforms, "B");
        GET_FLD_TYPESTR(glslEs, "B");
    }
    {
        SETUP_CLASS(ResourceTypeJNI, "ResourceType");
        GET_FLD(baseType, "BaseType");
        GET_FLD(dimensionType, "DimensionType");
        GET_FLD(imageStorageType, "ImageStorageType");
        GET_FLD(imageAccessQualifier, "ImageAccessQualifier");
        GET_FLD(imageBaseType, "BaseType");
        GET_FLD_TYPESTR(typeIndex, "I");
        GET_FLD_TYPESTR(vectorSize, "I");
        GET_FLD_TYPESTR(columnCount, "I");
        GET_FLD_TYPESTR(arraySize, "I");
        GET_FLD_TYPESTR(useTypeIndex, "Z");
        GET_FLD_TYPESTR(imageIsArrayed, "Z");
        GET_FLD_TYPESTR(imageIsStorage, "Z");
    }
    {
        SETUP_CLASS(ResourceMemberJNI, "ResourceMember");
        GET_FLD_TYPESTR(name, "Ljava/lang/String;");
        GET_FLD_TYPESTR(nameHash, "J");
        GET_FLD(type, "ResourceType");
        GET_FLD_TYPESTR(offset, "I");
    }
    {
        SETUP_CLASS(ResourceTypeInfoJNI, "ResourceTypeInfo");
        GET_FLD_TYPESTR(name, "Ljava/lang/String;");
        GET_FLD_TYPESTR(nameHash, "J");
        GET_FLD_ARRAY(members, "ResourceMember");
    }
    {
        SETUP_CLASS(ShaderResourceJNI, "ShaderResource");
        GET_FLD_TYPESTR(name, "Ljava/lang/String;");
        GET_FLD_TYPESTR(nameHash, "J");
        GET_FLD_TYPESTR(instanceName, "Ljava/lang/String;");
        GET_FLD_TYPESTR(instanceNameHash, "J");
        GET_FLD(type, "ResourceType");
        GET_FLD_TYPESTR(id, "I");
        GET_FLD_TYPESTR(blockSize, "I");
        GET_FLD_TYPESTR(location, "B");
        GET_FLD_TYPESTR(binding, "B");
        GET_FLD_TYPESTR(set, "B");
        GET_FLD_TYPESTR(stageFlags, "B");
    }
    {
        SETUP_CLASS(ShaderReflectionJNI, "ShaderReflection");
        GET_FLD_ARRAY(inputs, "ShaderResource");
        GET_FLD_ARRAY(outputs, "ShaderResource");
        GET_FLD_ARRAY(uniformBuffers, "ShaderResource");
        GET_FLD_ARRAY(storageBuffers, "ShaderResource");
        GET_FLD_ARRAY(textures, "ShaderResource");
        GET_FLD_ARRAY(types, "ResourceTypeInfo");
    }
    {
        SETUP_CLASS(HLSLResourceMappingJNI, "HLSLResourceMapping");
        GET_FLD_TYPESTR(name, "Ljava/lang/String;");
        GET_FLD_TYPESTR(nameHash, "J");
        GET_FLD_TYPESTR(shaderResourceSet, "B");
        GET_FLD_TYPESTR(shaderResourceBinding, "B");
    }
    {
        SETUP_CLASS(ShaderCompileResultJNI, "ShaderCompileResult");
        GET_FLD_TYPESTR(data, "[B");
        GET_FLD_TYPESTR(lastError, "Ljava/lang/String;");
        GET_FLD_ARRAY(hLSLResourceMappings, "HLSLResourceMapping");
        GET_FLD_TYPESTR(hLSLRootSignature, "[B");
        GET_FLD_TYPESTR(hLSLNumWorkGroupsId, "B");
    }
    {
        SETUP_CLASS(HLSLRootSignatureJNI, "HLSLRootSignature");
        GET_FLD_TYPESTR(lastError, "Ljava/lang/String;");
        GET_FLD_TYPESTR(hLSLRootSignature, "[B");
    }
    #undef GET_FLD
    #undef GET_FLD_ARRAY
    #undef GET_FLD_TYPESTR
}

void FinalizeJNITypes(JNIEnv* env, TypeInfos* infos) {
    env->DeleteLocalRef(infos->m_ShaderCompilerOptionsJNI.cls);
    env->DeleteLocalRef(infos->m_ResourceTypeJNI.cls);
    env->DeleteLocalRef(infos->m_ResourceMemberJNI.cls);
    env->DeleteLocalRef(infos->m_ResourceTypeInfoJNI.cls);
    env->DeleteLocalRef(infos->m_ShaderResourceJNI.cls);
    env->DeleteLocalRef(infos->m_ShaderReflectionJNI.cls);
    env->DeleteLocalRef(infos->m_HLSLResourceMappingJNI.cls);
    env->DeleteLocalRef(infos->m_ShaderCompileResultJNI.cls);
    env->DeleteLocalRef(infos->m_HLSLRootSignatureJNI.cls);
}


//----------------------------------------
// From C to Jni
//----------------------------------------
jobject C2J_CreateShaderCompilerOptions(JNIEnv* env, TypeInfos* types, const ShaderCompilerOptions* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_ShaderCompilerOptionsJNI.cls);
    dmJNI::SetUInt(env, obj, types->m_ShaderCompilerOptionsJNI.version, src->m_Version);
    dmJNI::SetString(env, obj, types->m_ShaderCompilerOptionsJNI.entryPoint, src->m_EntryPoint);
    dmJNI::SetUByte(env, obj, types->m_ShaderCompilerOptionsJNI.removeUnusedVariables, src->m_RemoveUnusedVariables);
    dmJNI::SetUByte(env, obj, types->m_ShaderCompilerOptionsJNI.no420PackExtension, src->m_No420PackExtension);
    dmJNI::SetUByte(env, obj, types->m_ShaderCompilerOptionsJNI.glslEmitUboAsPlainUniforms, src->m_GlslEmitUboAsPlainUniforms);
    dmJNI::SetUByte(env, obj, types->m_ShaderCompilerOptionsJNI.glslEs, src->m_GlslEs);
    return obj;
}

jobject C2J_CreateResourceType(JNIEnv* env, TypeInfos* types, const ResourceType* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_ResourceTypeJNI.cls);
    dmJNI::SetEnum(env, obj, types->m_ResourceTypeJNI.baseType, src->m_BaseType);
    dmJNI::SetEnum(env, obj, types->m_ResourceTypeJNI.dimensionType, src->m_DimensionType);
    dmJNI::SetEnum(env, obj, types->m_ResourceTypeJNI.imageStorageType, src->m_ImageStorageType);
    dmJNI::SetEnum(env, obj, types->m_ResourceTypeJNI.imageAccessQualifier, src->m_ImageAccessQualifier);
    dmJNI::SetEnum(env, obj, types->m_ResourceTypeJNI.imageBaseType, src->m_ImageBaseType);
    dmJNI::SetUInt(env, obj, types->m_ResourceTypeJNI.typeIndex, src->m_TypeIndex);
    dmJNI::SetUInt(env, obj, types->m_ResourceTypeJNI.vectorSize, src->m_VectorSize);
    dmJNI::SetUInt(env, obj, types->m_ResourceTypeJNI.columnCount, src->m_ColumnCount);
    dmJNI::SetUInt(env, obj, types->m_ResourceTypeJNI.arraySize, src->m_ArraySize);
    dmJNI::SetBoolean(env, obj, types->m_ResourceTypeJNI.useTypeIndex, src->m_UseTypeIndex);
    dmJNI::SetBoolean(env, obj, types->m_ResourceTypeJNI.imageIsArrayed, src->m_ImageIsArrayed);
    dmJNI::SetBoolean(env, obj, types->m_ResourceTypeJNI.imageIsStorage, src->m_ImageIsStorage);
    return obj;
}

jobject C2J_CreateResourceMember(JNIEnv* env, TypeInfos* types, const ResourceMember* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_ResourceMemberJNI.cls);
    dmJNI::SetString(env, obj, types->m_ResourceMemberJNI.name, src->m_Name);
    dmJNI::SetULong(env, obj, types->m_ResourceMemberJNI.nameHash, src->m_NameHash);
    dmJNI::SetObjectDeref(env, obj, types->m_ResourceMemberJNI.type, C2J_CreateResourceType(env, types, &src->m_Type));
    dmJNI::SetUInt(env, obj, types->m_ResourceMemberJNI.offset, src->m_Offset);
    return obj;
}

jobject C2J_CreateResourceTypeInfo(JNIEnv* env, TypeInfos* types, const ResourceTypeInfo* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_ResourceTypeInfoJNI.cls);
    dmJNI::SetString(env, obj, types->m_ResourceTypeInfoJNI.name, src->m_Name);
    dmJNI::SetULong(env, obj, types->m_ResourceTypeInfoJNI.nameHash, src->m_NameHash);
    dmJNI::SetObjectDeref(env, obj, types->m_ResourceTypeInfoJNI.members, C2J_CreateResourceMemberArray(env, types, src->m_Members.Begin(), src->m_Members.Size()));
    return obj;
}

jobject C2J_CreateShaderResource(JNIEnv* env, TypeInfos* types, const ShaderResource* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_ShaderResourceJNI.cls);
    dmJNI::SetString(env, obj, types->m_ShaderResourceJNI.name, src->m_Name);
    dmJNI::SetULong(env, obj, types->m_ShaderResourceJNI.nameHash, src->m_NameHash);
    dmJNI::SetString(env, obj, types->m_ShaderResourceJNI.instanceName, src->m_InstanceName);
    dmJNI::SetULong(env, obj, types->m_ShaderResourceJNI.instanceNameHash, src->m_InstanceNameHash);
    dmJNI::SetObjectDeref(env, obj, types->m_ShaderResourceJNI.type, C2J_CreateResourceType(env, types, &src->m_Type));
    dmJNI::SetUInt(env, obj, types->m_ShaderResourceJNI.id, src->m_Id);
    dmJNI::SetUInt(env, obj, types->m_ShaderResourceJNI.blockSize, src->m_BlockSize);
    dmJNI::SetUByte(env, obj, types->m_ShaderResourceJNI.location, src->m_Location);
    dmJNI::SetUByte(env, obj, types->m_ShaderResourceJNI.binding, src->m_Binding);
    dmJNI::SetUByte(env, obj, types->m_ShaderResourceJNI.set, src->m_Set);
    dmJNI::SetUByte(env, obj, types->m_ShaderResourceJNI.stageFlags, src->m_StageFlags);
    return obj;
}

jobject C2J_CreateShaderReflection(JNIEnv* env, TypeInfos* types, const ShaderReflection* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_ShaderReflectionJNI.cls);
    dmJNI::SetObjectDeref(env, obj, types->m_ShaderReflectionJNI.inputs, C2J_CreateShaderResourceArray(env, types, src->m_Inputs.Begin(), src->m_Inputs.Size()));
    dmJNI::SetObjectDeref(env, obj, types->m_ShaderReflectionJNI.outputs, C2J_CreateShaderResourceArray(env, types, src->m_Outputs.Begin(), src->m_Outputs.Size()));
    dmJNI::SetObjectDeref(env, obj, types->m_ShaderReflectionJNI.uniformBuffers, C2J_CreateShaderResourceArray(env, types, src->m_UniformBuffers.Begin(), src->m_UniformBuffers.Size()));
    dmJNI::SetObjectDeref(env, obj, types->m_ShaderReflectionJNI.storageBuffers, C2J_CreateShaderResourceArray(env, types, src->m_StorageBuffers.Begin(), src->m_StorageBuffers.Size()));
    dmJNI::SetObjectDeref(env, obj, types->m_ShaderReflectionJNI.textures, C2J_CreateShaderResourceArray(env, types, src->m_Textures.Begin(), src->m_Textures.Size()));
    dmJNI::SetObjectDeref(env, obj, types->m_ShaderReflectionJNI.types, C2J_CreateResourceTypeInfoArray(env, types, src->m_Types.Begin(), src->m_Types.Size()));
    return obj;
}

jobject C2J_CreateHLSLResourceMapping(JNIEnv* env, TypeInfos* types, const HLSLResourceMapping* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_HLSLResourceMappingJNI.cls);
    dmJNI::SetString(env, obj, types->m_HLSLResourceMappingJNI.name, src->m_Name);
    dmJNI::SetULong(env, obj, types->m_HLSLResourceMappingJNI.nameHash, src->m_NameHash);
    dmJNI::SetUByte(env, obj, types->m_HLSLResourceMappingJNI.shaderResourceSet, src->m_ShaderResourceSet);
    dmJNI::SetUByte(env, obj, types->m_HLSLResourceMappingJNI.shaderResourceBinding, src->m_ShaderResourceBinding);
    return obj;
}

jobject C2J_CreateShaderCompileResult(JNIEnv* env, TypeInfos* types, const ShaderCompileResult* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_ShaderCompileResultJNI.cls);
    dmJNI::SetObjectDeref(env, obj, types->m_ShaderCompileResultJNI.data, dmJNI::C2J_CreateUByteArray(env, src->m_Data.Begin(), src->m_Data.Size()));
    dmJNI::SetString(env, obj, types->m_ShaderCompileResultJNI.lastError, src->m_LastError);
    dmJNI::SetObjectDeref(env, obj, types->m_ShaderCompileResultJNI.hLSLResourceMappings, C2J_CreateHLSLResourceMappingArray(env, types, src->m_HLSLResourceMappings.Begin(), src->m_HLSLResourceMappings.Size()));
    dmJNI::SetObjectDeref(env, obj, types->m_ShaderCompileResultJNI.hLSLRootSignature, dmJNI::C2J_CreateUByteArray(env, src->m_HLSLRootSignature.Begin(), src->m_HLSLRootSignature.Size()));
    dmJNI::SetUByte(env, obj, types->m_ShaderCompileResultJNI.hLSLNumWorkGroupsId, src->m_HLSLNumWorkGroupsId);
    return obj;
}

jobject C2J_CreateHLSLRootSignature(JNIEnv* env, TypeInfos* types, const HLSLRootSignature* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_HLSLRootSignatureJNI.cls);
    dmJNI::SetString(env, obj, types->m_HLSLRootSignatureJNI.lastError, src->m_LastError);
    dmJNI::SetObjectDeref(env, obj, types->m_HLSLRootSignatureJNI.hLSLRootSignature, dmJNI::C2J_CreateUByteArray(env, src->m_HLSLRootSignature.Begin(), src->m_HLSLRootSignature.Size()));
    return obj;
}

jobjectArray C2J_CreateShaderCompilerOptionsArray(JNIEnv* env, TypeInfos* types, const ShaderCompilerOptions* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_ShaderCompilerOptionsJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateShaderCompilerOptions(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateShaderCompilerOptionsPtrArray(JNIEnv* env, TypeInfos* types, const ShaderCompilerOptions* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_ShaderCompilerOptionsJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateShaderCompilerOptions(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateResourceTypeArray(JNIEnv* env, TypeInfos* types, const ResourceType* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_ResourceTypeJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateResourceType(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateResourceTypePtrArray(JNIEnv* env, TypeInfos* types, const ResourceType* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_ResourceTypeJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateResourceType(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateResourceMemberArray(JNIEnv* env, TypeInfos* types, const ResourceMember* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_ResourceMemberJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateResourceMember(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateResourceMemberPtrArray(JNIEnv* env, TypeInfos* types, const ResourceMember* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_ResourceMemberJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateResourceMember(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateResourceTypeInfoArray(JNIEnv* env, TypeInfos* types, const ResourceTypeInfo* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_ResourceTypeInfoJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateResourceTypeInfo(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateResourceTypeInfoPtrArray(JNIEnv* env, TypeInfos* types, const ResourceTypeInfo* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_ResourceTypeInfoJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateResourceTypeInfo(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateShaderResourceArray(JNIEnv* env, TypeInfos* types, const ShaderResource* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_ShaderResourceJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateShaderResource(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateShaderResourcePtrArray(JNIEnv* env, TypeInfos* types, const ShaderResource* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_ShaderResourceJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateShaderResource(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateShaderReflectionArray(JNIEnv* env, TypeInfos* types, const ShaderReflection* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_ShaderReflectionJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateShaderReflection(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateShaderReflectionPtrArray(JNIEnv* env, TypeInfos* types, const ShaderReflection* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_ShaderReflectionJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateShaderReflection(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateHLSLResourceMappingArray(JNIEnv* env, TypeInfos* types, const HLSLResourceMapping* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_HLSLResourceMappingJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateHLSLResourceMapping(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateHLSLResourceMappingPtrArray(JNIEnv* env, TypeInfos* types, const HLSLResourceMapping* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_HLSLResourceMappingJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateHLSLResourceMapping(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateShaderCompileResultArray(JNIEnv* env, TypeInfos* types, const ShaderCompileResult* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_ShaderCompileResultJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateShaderCompileResult(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateShaderCompileResultPtrArray(JNIEnv* env, TypeInfos* types, const ShaderCompileResult* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_ShaderCompileResultJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateShaderCompileResult(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateHLSLRootSignatureArray(JNIEnv* env, TypeInfos* types, const HLSLRootSignature* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_HLSLRootSignatureJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateHLSLRootSignature(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateHLSLRootSignaturePtrArray(JNIEnv* env, TypeInfos* types, const HLSLRootSignature* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_HLSLRootSignatureJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateHLSLRootSignature(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
//----------------------------------------
// From Jni to C
//----------------------------------------
bool J2C_CreateShaderCompilerOptions(JNIEnv* env, TypeInfos* types, jobject obj, ShaderCompilerOptions* out) {
    if (out == 0) return false;
    out->m_Version = dmJNI::GetUInt(env, obj, types->m_ShaderCompilerOptionsJNI.version);
    out->m_EntryPoint = dmJNI::GetString(env, obj, types->m_ShaderCompilerOptionsJNI.entryPoint);
    out->m_RemoveUnusedVariables = dmJNI::GetUByte(env, obj, types->m_ShaderCompilerOptionsJNI.removeUnusedVariables);
    out->m_No420PackExtension = dmJNI::GetUByte(env, obj, types->m_ShaderCompilerOptionsJNI.no420PackExtension);
    out->m_GlslEmitUboAsPlainUniforms = dmJNI::GetUByte(env, obj, types->m_ShaderCompilerOptionsJNI.glslEmitUboAsPlainUniforms);
    out->m_GlslEs = dmJNI::GetUByte(env, obj, types->m_ShaderCompilerOptionsJNI.glslEs);
    return true;
}

bool J2C_CreateResourceType(JNIEnv* env, TypeInfos* types, jobject obj, ResourceType* out) {
    if (out == 0) return false;
    out->m_BaseType = (BaseType)dmJNI::GetEnum(env, obj, types->m_ResourceTypeJNI.baseType);
    out->m_DimensionType = (DimensionType)dmJNI::GetEnum(env, obj, types->m_ResourceTypeJNI.dimensionType);
    out->m_ImageStorageType = (ImageStorageType)dmJNI::GetEnum(env, obj, types->m_ResourceTypeJNI.imageStorageType);
    out->m_ImageAccessQualifier = (ImageAccessQualifier)dmJNI::GetEnum(env, obj, types->m_ResourceTypeJNI.imageAccessQualifier);
    out->m_ImageBaseType = (BaseType)dmJNI::GetEnum(env, obj, types->m_ResourceTypeJNI.imageBaseType);
    out->m_TypeIndex = dmJNI::GetUInt(env, obj, types->m_ResourceTypeJNI.typeIndex);
    out->m_VectorSize = dmJNI::GetUInt(env, obj, types->m_ResourceTypeJNI.vectorSize);
    out->m_ColumnCount = dmJNI::GetUInt(env, obj, types->m_ResourceTypeJNI.columnCount);
    out->m_ArraySize = dmJNI::GetUInt(env, obj, types->m_ResourceTypeJNI.arraySize);
    out->m_UseTypeIndex = dmJNI::GetBoolean(env, obj, types->m_ResourceTypeJNI.useTypeIndex);
    out->m_ImageIsArrayed = dmJNI::GetBoolean(env, obj, types->m_ResourceTypeJNI.imageIsArrayed);
    out->m_ImageIsStorage = dmJNI::GetBoolean(env, obj, types->m_ResourceTypeJNI.imageIsStorage);
    return true;
}

bool J2C_CreateResourceMember(JNIEnv* env, TypeInfos* types, jobject obj, ResourceMember* out) {
    if (out == 0) return false;
    out->m_Name = dmJNI::GetString(env, obj, types->m_ResourceMemberJNI.name);
    out->m_NameHash = dmJNI::GetULong(env, obj, types->m_ResourceMemberJNI.nameHash);
    {
        jobject field_object = env->GetObjectField(obj, types->m_ResourceMemberJNI.type);
        if (field_object) {
            J2C_CreateResourceType(env, types, field_object, &out->m_Type);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_Offset = dmJNI::GetUInt(env, obj, types->m_ResourceMemberJNI.offset);
    return true;
}

bool J2C_CreateResourceTypeInfo(JNIEnv* env, TypeInfos* types, jobject obj, ResourceTypeInfo* out) {
    if (out == 0) return false;
    out->m_Name = dmJNI::GetString(env, obj, types->m_ResourceTypeInfoJNI.name);
    out->m_NameHash = dmJNI::GetULong(env, obj, types->m_ResourceTypeInfoJNI.nameHash);
    {
        jobject field_object = env->GetObjectField(obj, types->m_ResourceTypeInfoJNI.members);
        if (field_object) {
            uint32_t tmp_count;
            ResourceMember* tmp = J2C_CreateResourceMemberArray(env, types, (jobjectArray)field_object, &tmp_count);
            out->m_Members.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    return true;
}

bool J2C_CreateShaderResource(JNIEnv* env, TypeInfos* types, jobject obj, ShaderResource* out) {
    if (out == 0) return false;
    out->m_Name = dmJNI::GetString(env, obj, types->m_ShaderResourceJNI.name);
    out->m_NameHash = dmJNI::GetULong(env, obj, types->m_ShaderResourceJNI.nameHash);
    out->m_InstanceName = dmJNI::GetString(env, obj, types->m_ShaderResourceJNI.instanceName);
    out->m_InstanceNameHash = dmJNI::GetULong(env, obj, types->m_ShaderResourceJNI.instanceNameHash);
    {
        jobject field_object = env->GetObjectField(obj, types->m_ShaderResourceJNI.type);
        if (field_object) {
            J2C_CreateResourceType(env, types, field_object, &out->m_Type);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_Id = dmJNI::GetUInt(env, obj, types->m_ShaderResourceJNI.id);
    out->m_BlockSize = dmJNI::GetUInt(env, obj, types->m_ShaderResourceJNI.blockSize);
    out->m_Location = dmJNI::GetUByte(env, obj, types->m_ShaderResourceJNI.location);
    out->m_Binding = dmJNI::GetUByte(env, obj, types->m_ShaderResourceJNI.binding);
    out->m_Set = dmJNI::GetUByte(env, obj, types->m_ShaderResourceJNI.set);
    out->m_StageFlags = dmJNI::GetUByte(env, obj, types->m_ShaderResourceJNI.stageFlags);
    return true;
}

bool J2C_CreateShaderReflection(JNIEnv* env, TypeInfos* types, jobject obj, ShaderReflection* out) {
    if (out == 0) return false;
    {
        jobject field_object = env->GetObjectField(obj, types->m_ShaderReflectionJNI.inputs);
        if (field_object) {
            uint32_t tmp_count;
            ShaderResource* tmp = J2C_CreateShaderResourceArray(env, types, (jobjectArray)field_object, &tmp_count);
            out->m_Inputs.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_ShaderReflectionJNI.outputs);
        if (field_object) {
            uint32_t tmp_count;
            ShaderResource* tmp = J2C_CreateShaderResourceArray(env, types, (jobjectArray)field_object, &tmp_count);
            out->m_Outputs.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_ShaderReflectionJNI.uniformBuffers);
        if (field_object) {
            uint32_t tmp_count;
            ShaderResource* tmp = J2C_CreateShaderResourceArray(env, types, (jobjectArray)field_object, &tmp_count);
            out->m_UniformBuffers.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_ShaderReflectionJNI.storageBuffers);
        if (field_object) {
            uint32_t tmp_count;
            ShaderResource* tmp = J2C_CreateShaderResourceArray(env, types, (jobjectArray)field_object, &tmp_count);
            out->m_StorageBuffers.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_ShaderReflectionJNI.textures);
        if (field_object) {
            uint32_t tmp_count;
            ShaderResource* tmp = J2C_CreateShaderResourceArray(env, types, (jobjectArray)field_object, &tmp_count);
            out->m_Textures.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_ShaderReflectionJNI.types);
        if (field_object) {
            uint32_t tmp_count;
            ResourceTypeInfo* tmp = J2C_CreateResourceTypeInfoArray(env, types, (jobjectArray)field_object, &tmp_count);
            out->m_Types.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    return true;
}

bool J2C_CreateHLSLResourceMapping(JNIEnv* env, TypeInfos* types, jobject obj, HLSLResourceMapping* out) {
    if (out == 0) return false;
    out->m_Name = dmJNI::GetString(env, obj, types->m_HLSLResourceMappingJNI.name);
    out->m_NameHash = dmJNI::GetULong(env, obj, types->m_HLSLResourceMappingJNI.nameHash);
    out->m_ShaderResourceSet = dmJNI::GetUByte(env, obj, types->m_HLSLResourceMappingJNI.shaderResourceSet);
    out->m_ShaderResourceBinding = dmJNI::GetUByte(env, obj, types->m_HLSLResourceMappingJNI.shaderResourceBinding);
    return true;
}

bool J2C_CreateShaderCompileResult(JNIEnv* env, TypeInfos* types, jobject obj, ShaderCompileResult* out) {
    if (out == 0) return false;
    {
        jobject field_object = env->GetObjectField(obj, types->m_ShaderCompileResultJNI.data);
        if (field_object) {
            uint32_t tmp_count;
            uint8_t* tmp = dmJNI::J2C_CreateUByteArray(env, (jbyteArray)field_object, &tmp_count);
            out->m_Data.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_LastError = dmJNI::GetString(env, obj, types->m_ShaderCompileResultJNI.lastError);
    {
        jobject field_object = env->GetObjectField(obj, types->m_ShaderCompileResultJNI.hLSLResourceMappings);
        if (field_object) {
            uint32_t tmp_count;
            HLSLResourceMapping* tmp = J2C_CreateHLSLResourceMappingArray(env, types, (jobjectArray)field_object, &tmp_count);
            out->m_HLSLResourceMappings.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    {
        jobject field_object = env->GetObjectField(obj, types->m_ShaderCompileResultJNI.hLSLRootSignature);
        if (field_object) {
            uint32_t tmp_count;
            uint8_t* tmp = dmJNI::J2C_CreateUByteArray(env, (jbyteArray)field_object, &tmp_count);
            out->m_HLSLRootSignature.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_HLSLNumWorkGroupsId = dmJNI::GetUByte(env, obj, types->m_ShaderCompileResultJNI.hLSLNumWorkGroupsId);
    return true;
}

bool J2C_CreateHLSLRootSignature(JNIEnv* env, TypeInfos* types, jobject obj, HLSLRootSignature* out) {
    if (out == 0) return false;
    out->m_LastError = dmJNI::GetString(env, obj, types->m_HLSLRootSignatureJNI.lastError);
    {
        jobject field_object = env->GetObjectField(obj, types->m_HLSLRootSignatureJNI.hLSLRootSignature);
        if (field_object) {
            uint32_t tmp_count;
            uint8_t* tmp = dmJNI::J2C_CreateUByteArray(env, (jbyteArray)field_object, &tmp_count);
            out->m_HLSLRootSignature.Set(tmp, tmp_count, tmp_count, false);
            env->DeleteLocalRef(field_object);
        }
    }
    return true;
}

void J2C_CreateShaderCompilerOptionsArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ShaderCompilerOptions* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateShaderCompilerOptions(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
ShaderCompilerOptions* J2C_CreateShaderCompilerOptionsArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    ShaderCompilerOptions* out = new ShaderCompilerOptions[len];
    J2C_CreateShaderCompilerOptionsArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateShaderCompilerOptionsPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ShaderCompilerOptions** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new ShaderCompilerOptions();
        J2C_CreateShaderCompilerOptions(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
ShaderCompilerOptions** J2C_CreateShaderCompilerOptionsPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    ShaderCompilerOptions** out = new ShaderCompilerOptions*[len];
    J2C_CreateShaderCompilerOptionsPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateResourceTypeArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ResourceType* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateResourceType(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
ResourceType* J2C_CreateResourceTypeArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    ResourceType* out = new ResourceType[len];
    J2C_CreateResourceTypeArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateResourceTypePtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ResourceType** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new ResourceType();
        J2C_CreateResourceType(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
ResourceType** J2C_CreateResourceTypePtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    ResourceType** out = new ResourceType*[len];
    J2C_CreateResourceTypePtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateResourceMemberArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ResourceMember* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateResourceMember(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
ResourceMember* J2C_CreateResourceMemberArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    ResourceMember* out = new ResourceMember[len];
    J2C_CreateResourceMemberArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateResourceMemberPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ResourceMember** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new ResourceMember();
        J2C_CreateResourceMember(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
ResourceMember** J2C_CreateResourceMemberPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    ResourceMember** out = new ResourceMember*[len];
    J2C_CreateResourceMemberPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateResourceTypeInfoArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ResourceTypeInfo* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateResourceTypeInfo(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
ResourceTypeInfo* J2C_CreateResourceTypeInfoArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    ResourceTypeInfo* out = new ResourceTypeInfo[len];
    J2C_CreateResourceTypeInfoArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateResourceTypeInfoPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ResourceTypeInfo** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new ResourceTypeInfo();
        J2C_CreateResourceTypeInfo(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
ResourceTypeInfo** J2C_CreateResourceTypeInfoPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    ResourceTypeInfo** out = new ResourceTypeInfo*[len];
    J2C_CreateResourceTypeInfoPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateShaderResourceArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ShaderResource* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateShaderResource(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
ShaderResource* J2C_CreateShaderResourceArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    ShaderResource* out = new ShaderResource[len];
    J2C_CreateShaderResourceArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateShaderResourcePtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ShaderResource** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new ShaderResource();
        J2C_CreateShaderResource(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
ShaderResource** J2C_CreateShaderResourcePtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    ShaderResource** out = new ShaderResource*[len];
    J2C_CreateShaderResourcePtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateShaderReflectionArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ShaderReflection* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateShaderReflection(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
ShaderReflection* J2C_CreateShaderReflectionArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    ShaderReflection* out = new ShaderReflection[len];
    J2C_CreateShaderReflectionArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateShaderReflectionPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ShaderReflection** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new ShaderReflection();
        J2C_CreateShaderReflection(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
ShaderReflection** J2C_CreateShaderReflectionPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    ShaderReflection** out = new ShaderReflection*[len];
    J2C_CreateShaderReflectionPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateHLSLResourceMappingArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, HLSLResourceMapping* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateHLSLResourceMapping(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
HLSLResourceMapping* J2C_CreateHLSLResourceMappingArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    HLSLResourceMapping* out = new HLSLResourceMapping[len];
    J2C_CreateHLSLResourceMappingArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateHLSLResourceMappingPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, HLSLResourceMapping** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new HLSLResourceMapping();
        J2C_CreateHLSLResourceMapping(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
HLSLResourceMapping** J2C_CreateHLSLResourceMappingPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    HLSLResourceMapping** out = new HLSLResourceMapping*[len];
    J2C_CreateHLSLResourceMappingPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateShaderCompileResultArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ShaderCompileResult* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateShaderCompileResult(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
ShaderCompileResult* J2C_CreateShaderCompileResultArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    ShaderCompileResult* out = new ShaderCompileResult[len];
    J2C_CreateShaderCompileResultArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateShaderCompileResultPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ShaderCompileResult** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new ShaderCompileResult();
        J2C_CreateShaderCompileResult(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
ShaderCompileResult** J2C_CreateShaderCompileResultPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    ShaderCompileResult** out = new ShaderCompileResult*[len];
    J2C_CreateShaderCompileResultPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateHLSLRootSignatureArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, HLSLRootSignature* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateHLSLRootSignature(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
HLSLRootSignature* J2C_CreateHLSLRootSignatureArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    HLSLRootSignature* out = new HLSLRootSignature[len];
    J2C_CreateHLSLRootSignatureArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateHLSLRootSignaturePtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, HLSLRootSignature** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new HLSLRootSignature();
        J2C_CreateHLSLRootSignature(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
HLSLRootSignature** J2C_CreateHLSLRootSignaturePtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    HLSLRootSignature** out = new HLSLRootSignature*[len];
    J2C_CreateHLSLRootSignaturePtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
} // jni
} // dmShaderc
