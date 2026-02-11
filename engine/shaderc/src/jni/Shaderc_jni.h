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
#include "shaderc.h"

#define JAVA_PACKAGE_NAME "com/dynamo/bob/pipeline"
#define CLASS_NAME "com/dynamo/bob/pipeline/Shaderc"

namespace dmShaderc {
namespace jni {
struct ShaderCompilerOptionsJNI {
    jclass cls;
    jfieldID version;
    jfieldID entryPoint;
    jfieldID removeUnusedVariables;
    jfieldID no420PackExtension;
    jfieldID glslEmitUboAsPlainUniforms;
    jfieldID glslEs;
};
struct ResourceTypeJNI {
    jclass cls;
    jfieldID baseType;
    jfieldID dimensionType;
    jfieldID imageStorageType;
    jfieldID imageAccessQualifier;
    jfieldID imageBaseType;
    jfieldID typeIndex;
    jfieldID vectorSize;
    jfieldID columnCount;
    jfieldID arraySize;
    jfieldID useTypeIndex;
    jfieldID imageIsArrayed;
    jfieldID imageIsStorage;
};
struct ResourceMemberJNI {
    jclass cls;
    jfieldID name;
    jfieldID nameHash;
    jfieldID type;
    jfieldID offset;
};
struct ResourceTypeInfoJNI {
    jclass cls;
    jfieldID name;
    jfieldID nameHash;
    jfieldID members;
};
struct ShaderResourceJNI {
    jclass cls;
    jfieldID name;
    jfieldID nameHash;
    jfieldID instanceName;
    jfieldID instanceNameHash;
    jfieldID type;
    jfieldID id;
    jfieldID blockSize;
    jfieldID location;
    jfieldID binding;
    jfieldID set;
    jfieldID stageFlags;
};
struct ShaderReflectionJNI {
    jclass cls;
    jfieldID inputs;
    jfieldID outputs;
    jfieldID uniformBuffers;
    jfieldID storageBuffers;
    jfieldID textures;
    jfieldID types;
};
struct HLSLResourceMappingJNI {
    jclass cls;
    jfieldID name;
    jfieldID nameHash;
    jfieldID shaderResourceSet;
    jfieldID shaderResourceBinding;
};
struct ShaderCompileResultJNI {
    jclass cls;
    jfieldID data;
    jfieldID lastError;
    jfieldID hLSLResourceMappings;
    jfieldID hLSLRootSignature;
    jfieldID hLSLNumWorkGroupsId;
};
struct HLSLRootSignatureJNI {
    jclass cls;
    jfieldID lastError;
    jfieldID hLSLRootSignature;
};
struct TypeInfos {
    ShaderCompilerOptionsJNI m_ShaderCompilerOptionsJNI;
    ResourceTypeJNI m_ResourceTypeJNI;
    ResourceMemberJNI m_ResourceMemberJNI;
    ResourceTypeInfoJNI m_ResourceTypeInfoJNI;
    ShaderResourceJNI m_ShaderResourceJNI;
    ShaderReflectionJNI m_ShaderReflectionJNI;
    HLSLResourceMappingJNI m_HLSLResourceMappingJNI;
    ShaderCompileResultJNI m_ShaderCompileResultJNI;
    HLSLRootSignatureJNI m_HLSLRootSignatureJNI;
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
jobject C2J_CreateShaderCompilerOptions(JNIEnv* env, TypeInfos* types, const ShaderCompilerOptions* src);
jobject C2J_CreateResourceType(JNIEnv* env, TypeInfos* types, const ResourceType* src);
jobject C2J_CreateResourceMember(JNIEnv* env, TypeInfos* types, const ResourceMember* src);
jobject C2J_CreateResourceTypeInfo(JNIEnv* env, TypeInfos* types, const ResourceTypeInfo* src);
jobject C2J_CreateShaderResource(JNIEnv* env, TypeInfos* types, const ShaderResource* src);
jobject C2J_CreateShaderReflection(JNIEnv* env, TypeInfos* types, const ShaderReflection* src);
jobject C2J_CreateHLSLResourceMapping(JNIEnv* env, TypeInfos* types, const HLSLResourceMapping* src);
jobject C2J_CreateShaderCompileResult(JNIEnv* env, TypeInfos* types, const ShaderCompileResult* src);
jobject C2J_CreateHLSLRootSignature(JNIEnv* env, TypeInfos* types, const HLSLRootSignature* src);
jobjectArray C2J_CreateShaderCompilerOptionsArray(JNIEnv* env, TypeInfos* types, const ShaderCompilerOptions* src, uint32_t src_count);
jobjectArray C2J_CreateShaderCompilerOptionsPtrArray(JNIEnv* env, TypeInfos* types, const ShaderCompilerOptions* const* src, uint32_t src_count);
jobjectArray C2J_CreateResourceTypeArray(JNIEnv* env, TypeInfos* types, const ResourceType* src, uint32_t src_count);
jobjectArray C2J_CreateResourceTypePtrArray(JNIEnv* env, TypeInfos* types, const ResourceType* const* src, uint32_t src_count);
jobjectArray C2J_CreateResourceMemberArray(JNIEnv* env, TypeInfos* types, const ResourceMember* src, uint32_t src_count);
jobjectArray C2J_CreateResourceMemberPtrArray(JNIEnv* env, TypeInfos* types, const ResourceMember* const* src, uint32_t src_count);
jobjectArray C2J_CreateResourceTypeInfoArray(JNIEnv* env, TypeInfos* types, const ResourceTypeInfo* src, uint32_t src_count);
jobjectArray C2J_CreateResourceTypeInfoPtrArray(JNIEnv* env, TypeInfos* types, const ResourceTypeInfo* const* src, uint32_t src_count);
jobjectArray C2J_CreateShaderResourceArray(JNIEnv* env, TypeInfos* types, const ShaderResource* src, uint32_t src_count);
jobjectArray C2J_CreateShaderResourcePtrArray(JNIEnv* env, TypeInfos* types, const ShaderResource* const* src, uint32_t src_count);
jobjectArray C2J_CreateShaderReflectionArray(JNIEnv* env, TypeInfos* types, const ShaderReflection* src, uint32_t src_count);
jobjectArray C2J_CreateShaderReflectionPtrArray(JNIEnv* env, TypeInfos* types, const ShaderReflection* const* src, uint32_t src_count);
jobjectArray C2J_CreateHLSLResourceMappingArray(JNIEnv* env, TypeInfos* types, const HLSLResourceMapping* src, uint32_t src_count);
jobjectArray C2J_CreateHLSLResourceMappingPtrArray(JNIEnv* env, TypeInfos* types, const HLSLResourceMapping* const* src, uint32_t src_count);
jobjectArray C2J_CreateShaderCompileResultArray(JNIEnv* env, TypeInfos* types, const ShaderCompileResult* src, uint32_t src_count);
jobjectArray C2J_CreateShaderCompileResultPtrArray(JNIEnv* env, TypeInfos* types, const ShaderCompileResult* const* src, uint32_t src_count);
jobjectArray C2J_CreateHLSLRootSignatureArray(JNIEnv* env, TypeInfos* types, const HLSLRootSignature* src, uint32_t src_count);
jobjectArray C2J_CreateHLSLRootSignaturePtrArray(JNIEnv* env, TypeInfos* types, const HLSLRootSignature* const* src, uint32_t src_count);
//----------------------------------------
// From Jni to C
//----------------------------------------
bool J2C_CreateShaderCompilerOptions(JNIEnv* env, TypeInfos* types, jobject obj, ShaderCompilerOptions* out);
bool J2C_CreateResourceType(JNIEnv* env, TypeInfos* types, jobject obj, ResourceType* out);
bool J2C_CreateResourceMember(JNIEnv* env, TypeInfos* types, jobject obj, ResourceMember* out);
bool J2C_CreateResourceTypeInfo(JNIEnv* env, TypeInfos* types, jobject obj, ResourceTypeInfo* out);
bool J2C_CreateShaderResource(JNIEnv* env, TypeInfos* types, jobject obj, ShaderResource* out);
bool J2C_CreateShaderReflection(JNIEnv* env, TypeInfos* types, jobject obj, ShaderReflection* out);
bool J2C_CreateHLSLResourceMapping(JNIEnv* env, TypeInfos* types, jobject obj, HLSLResourceMapping* out);
bool J2C_CreateShaderCompileResult(JNIEnv* env, TypeInfos* types, jobject obj, ShaderCompileResult* out);
bool J2C_CreateHLSLRootSignature(JNIEnv* env, TypeInfos* types, jobject obj, HLSLRootSignature* out);
ShaderCompilerOptions* J2C_CreateShaderCompilerOptionsArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateShaderCompilerOptionsArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ShaderCompilerOptions* dst, uint32_t dst_count);
ShaderCompilerOptions** J2C_CreateShaderCompilerOptionsPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateShaderCompilerOptionsPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ShaderCompilerOptions** dst, uint32_t dst_count);
ResourceType* J2C_CreateResourceTypeArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateResourceTypeArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ResourceType* dst, uint32_t dst_count);
ResourceType** J2C_CreateResourceTypePtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateResourceTypePtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ResourceType** dst, uint32_t dst_count);
ResourceMember* J2C_CreateResourceMemberArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateResourceMemberArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ResourceMember* dst, uint32_t dst_count);
ResourceMember** J2C_CreateResourceMemberPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateResourceMemberPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ResourceMember** dst, uint32_t dst_count);
ResourceTypeInfo* J2C_CreateResourceTypeInfoArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateResourceTypeInfoArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ResourceTypeInfo* dst, uint32_t dst_count);
ResourceTypeInfo** J2C_CreateResourceTypeInfoPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateResourceTypeInfoPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ResourceTypeInfo** dst, uint32_t dst_count);
ShaderResource* J2C_CreateShaderResourceArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateShaderResourceArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ShaderResource* dst, uint32_t dst_count);
ShaderResource** J2C_CreateShaderResourcePtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateShaderResourcePtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ShaderResource** dst, uint32_t dst_count);
ShaderReflection* J2C_CreateShaderReflectionArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateShaderReflectionArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ShaderReflection* dst, uint32_t dst_count);
ShaderReflection** J2C_CreateShaderReflectionPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateShaderReflectionPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ShaderReflection** dst, uint32_t dst_count);
HLSLResourceMapping* J2C_CreateHLSLResourceMappingArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateHLSLResourceMappingArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, HLSLResourceMapping* dst, uint32_t dst_count);
HLSLResourceMapping** J2C_CreateHLSLResourceMappingPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateHLSLResourceMappingPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, HLSLResourceMapping** dst, uint32_t dst_count);
ShaderCompileResult* J2C_CreateShaderCompileResultArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateShaderCompileResultArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ShaderCompileResult* dst, uint32_t dst_count);
ShaderCompileResult** J2C_CreateShaderCompileResultPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateShaderCompileResultPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ShaderCompileResult** dst, uint32_t dst_count);
HLSLRootSignature* J2C_CreateHLSLRootSignatureArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateHLSLRootSignatureArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, HLSLRootSignature* dst, uint32_t dst_count);
HLSLRootSignature** J2C_CreateHLSLRootSignaturePtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateHLSLRootSignaturePtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, HLSLRootSignature** dst, uint32_t dst_count);
} // jni
} // dmShaderc
