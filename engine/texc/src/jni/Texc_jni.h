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
#include "texc.h"

#define JAVA_PACKAGE_NAME "com/dynamo/bob/pipeline"
#define CLASS_NAME "com/dynamo/bob/pipeline/Texc"

namespace dmTexc {
namespace jni {
struct ImageJNI {
    jclass cls;
    jfieldID path;
    jfieldID data;
    jfieldID width;
    jfieldID height;
    jfieldID pixelFormat;
    jfieldID colorSpace;
};
struct BufferJNI {
    jclass cls;
    jfieldID data;
    jfieldID width;
    jfieldID height;
    jfieldID isCompressed;
};
struct BasisUEncodeSettingsJNI {
    jclass cls;
    jfieldID path;
    jfieldID width;
    jfieldID height;
    jfieldID pixelFormat;
    jfieldID colorSpace;
    jfieldID data;
    jfieldID numThreads;
    jfieldID debug;
    jfieldID outPixelFormat;
    jfieldID rdo_uastc;
    jfieldID pack_uastc_flags;
    jfieldID rdo_uastc_dict_size;
    jfieldID rdo_uastc_quality_scalar;
};
struct DefaultEncodeSettingsJNI {
    jclass cls;
    jfieldID path;
    jfieldID width;
    jfieldID height;
    jfieldID pixelFormat;
    jfieldID colorSpace;
    jfieldID data;
    jfieldID numThreads;
    jfieldID debug;
    jfieldID outPixelFormat;
};
struct ASTCEncodeSettingsJNI {
    jclass cls;
    jfieldID path;
    jfieldID width;
    jfieldID height;
    jfieldID pixelFormat;
    jfieldID colorSpace;
    jfieldID data;
    jfieldID numThreads;
    jfieldID qualityLevel;
    jfieldID outPixelFormat;
};
struct TypeInfos {
    ImageJNI m_ImageJNI;
    BufferJNI m_BufferJNI;
    BasisUEncodeSettingsJNI m_BasisUEncodeSettingsJNI;
    DefaultEncodeSettingsJNI m_DefaultEncodeSettingsJNI;
    ASTCEncodeSettingsJNI m_ASTCEncodeSettingsJNI;
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
jobject C2J_CreateImage(JNIEnv* env, TypeInfos* types, const Image* src);
jobject C2J_CreateBuffer(JNIEnv* env, TypeInfos* types, const Buffer* src);
jobject C2J_CreateBasisUEncodeSettings(JNIEnv* env, TypeInfos* types, const BasisUEncodeSettings* src);
jobject C2J_CreateDefaultEncodeSettings(JNIEnv* env, TypeInfos* types, const DefaultEncodeSettings* src);
jobject C2J_CreateASTCEncodeSettings(JNIEnv* env, TypeInfos* types, const ASTCEncodeSettings* src);
jobjectArray C2J_CreateImageArray(JNIEnv* env, TypeInfos* types, const Image* src, uint32_t src_count);
jobjectArray C2J_CreateImagePtrArray(JNIEnv* env, TypeInfos* types, const Image* const* src, uint32_t src_count);
jobjectArray C2J_CreateBufferArray(JNIEnv* env, TypeInfos* types, const Buffer* src, uint32_t src_count);
jobjectArray C2J_CreateBufferPtrArray(JNIEnv* env, TypeInfos* types, const Buffer* const* src, uint32_t src_count);
jobjectArray C2J_CreateBasisUEncodeSettingsArray(JNIEnv* env, TypeInfos* types, const BasisUEncodeSettings* src, uint32_t src_count);
jobjectArray C2J_CreateBasisUEncodeSettingsPtrArray(JNIEnv* env, TypeInfos* types, const BasisUEncodeSettings* const* src, uint32_t src_count);
jobjectArray C2J_CreateDefaultEncodeSettingsArray(JNIEnv* env, TypeInfos* types, const DefaultEncodeSettings* src, uint32_t src_count);
jobjectArray C2J_CreateDefaultEncodeSettingsPtrArray(JNIEnv* env, TypeInfos* types, const DefaultEncodeSettings* const* src, uint32_t src_count);
jobjectArray C2J_CreateASTCEncodeSettingsArray(JNIEnv* env, TypeInfos* types, const ASTCEncodeSettings* src, uint32_t src_count);
jobjectArray C2J_CreateASTCEncodeSettingsPtrArray(JNIEnv* env, TypeInfos* types, const ASTCEncodeSettings* const* src, uint32_t src_count);
//----------------------------------------
// From Jni to C
//----------------------------------------
bool J2C_CreateImage(JNIEnv* env, TypeInfos* types, jobject obj, Image* out);
bool J2C_CreateBuffer(JNIEnv* env, TypeInfos* types, jobject obj, Buffer* out);
bool J2C_CreateBasisUEncodeSettings(JNIEnv* env, TypeInfos* types, jobject obj, BasisUEncodeSettings* out);
bool J2C_CreateDefaultEncodeSettings(JNIEnv* env, TypeInfos* types, jobject obj, DefaultEncodeSettings* out);
bool J2C_CreateASTCEncodeSettings(JNIEnv* env, TypeInfos* types, jobject obj, ASTCEncodeSettings* out);
Image* J2C_CreateImageArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateImageArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Image* dst, uint32_t dst_count);
Image** J2C_CreateImagePtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateImagePtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Image** dst, uint32_t dst_count);
Buffer* J2C_CreateBufferArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateBufferArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Buffer* dst, uint32_t dst_count);
Buffer** J2C_CreateBufferPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateBufferPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Buffer** dst, uint32_t dst_count);
BasisUEncodeSettings* J2C_CreateBasisUEncodeSettingsArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateBasisUEncodeSettingsArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, BasisUEncodeSettings* dst, uint32_t dst_count);
BasisUEncodeSettings** J2C_CreateBasisUEncodeSettingsPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateBasisUEncodeSettingsPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, BasisUEncodeSettings** dst, uint32_t dst_count);
DefaultEncodeSettings* J2C_CreateDefaultEncodeSettingsArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateDefaultEncodeSettingsArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, DefaultEncodeSettings* dst, uint32_t dst_count);
DefaultEncodeSettings** J2C_CreateDefaultEncodeSettingsPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateDefaultEncodeSettingsPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, DefaultEncodeSettings** dst, uint32_t dst_count);
ASTCEncodeSettings* J2C_CreateASTCEncodeSettingsArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateASTCEncodeSettingsArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ASTCEncodeSettings* dst, uint32_t dst_count);
ASTCEncodeSettings** J2C_CreateASTCEncodeSettingsPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateASTCEncodeSettingsPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ASTCEncodeSettings** dst, uint32_t dst_count);
} // jni
} // dmTexc
