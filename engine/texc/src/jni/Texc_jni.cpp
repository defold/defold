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

#include "Texc_jni.h"
#include <jni/jni_util.h>
#include <dlib/array.h>

#define CLASS_NAME_FORMAT "com/dynamo/bob/pipeline/Texc$%s"

namespace dmTexc {
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
        SETUP_CLASS(ImageJNI, "Image");
        GET_FLD_TYPESTR(path, "Ljava/lang/String;");
        GET_FLD_TYPESTR(data, "[B");
        GET_FLD_TYPESTR(width, "I");
        GET_FLD_TYPESTR(height, "I");
        GET_FLD(pixelFormat, "PixelFormat");
        GET_FLD(colorSpace, "ColorSpace");
    }
    {
        SETUP_CLASS(BufferJNI, "Buffer");
        GET_FLD_TYPESTR(data, "[B");
        GET_FLD_TYPESTR(width, "I");
        GET_FLD_TYPESTR(height, "I");
        GET_FLD_TYPESTR(isCompressed, "Z");
    }
    {
        SETUP_CLASS(BasisUEncodeSettingsJNI, "BasisUEncodeSettings");
        GET_FLD_TYPESTR(path, "Ljava/lang/String;");
        GET_FLD_TYPESTR(width, "I");
        GET_FLD_TYPESTR(height, "I");
        GET_FLD(pixelFormat, "PixelFormat");
        GET_FLD(colorSpace, "ColorSpace");
        GET_FLD_TYPESTR(data, "[B");
        GET_FLD_TYPESTR(numThreads, "I");
        GET_FLD_TYPESTR(debug, "Z");
        GET_FLD(outPixelFormat, "PixelFormat");
        GET_FLD_TYPESTR(rdo_uastc, "Z");
        GET_FLD_TYPESTR(pack_uastc_flags, "I");
        GET_FLD_TYPESTR(rdo_uastc_dict_size, "I");
        GET_FLD_TYPESTR(rdo_uastc_quality_scalar, "F");
    }
    {
        SETUP_CLASS(DefaultEncodeSettingsJNI, "DefaultEncodeSettings");
        GET_FLD_TYPESTR(path, "Ljava/lang/String;");
        GET_FLD_TYPESTR(width, "I");
        GET_FLD_TYPESTR(height, "I");
        GET_FLD(pixelFormat, "PixelFormat");
        GET_FLD(colorSpace, "ColorSpace");
        GET_FLD_TYPESTR(data, "[B");
        GET_FLD_TYPESTR(numThreads, "I");
        GET_FLD_TYPESTR(debug, "Z");
        GET_FLD(outPixelFormat, "PixelFormat");
    }
    {
        SETUP_CLASS(ASTCEncodeSettingsJNI, "ASTCEncodeSettings");
        GET_FLD_TYPESTR(path, "Ljava/lang/String;");
        GET_FLD_TYPESTR(width, "I");
        GET_FLD_TYPESTR(height, "I");
        GET_FLD(pixelFormat, "PixelFormat");
        GET_FLD(colorSpace, "ColorSpace");
        GET_FLD_TYPESTR(data, "[B");
        GET_FLD_TYPESTR(numThreads, "I");
        GET_FLD_TYPESTR(qualityLevel, "F");
        GET_FLD(outPixelFormat, "PixelFormat");
    }
    #undef GET_FLD
    #undef GET_FLD_ARRAY
    #undef GET_FLD_TYPESTR
}

void FinalizeJNITypes(JNIEnv* env, TypeInfos* infos) {
    env->DeleteLocalRef(infos->m_ImageJNI.cls);
    env->DeleteLocalRef(infos->m_BufferJNI.cls);
    env->DeleteLocalRef(infos->m_BasisUEncodeSettingsJNI.cls);
    env->DeleteLocalRef(infos->m_DefaultEncodeSettingsJNI.cls);
    env->DeleteLocalRef(infos->m_ASTCEncodeSettingsJNI.cls);
}


//----------------------------------------
// From C to Jni
//----------------------------------------
jobject C2J_CreateImage(JNIEnv* env, TypeInfos* types, const Image* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_ImageJNI.cls);
    dmJNI::SetString(env, obj, types->m_ImageJNI.path, src->m_Path);
    dmJNI::SetObjectDeref(env, obj, types->m_ImageJNI.data, dmJNI::C2J_CreateUByteArray(env, src->m_Data, src->m_DataCount));
    dmJNI::SetUInt(env, obj, types->m_ImageJNI.width, src->m_Width);
    dmJNI::SetUInt(env, obj, types->m_ImageJNI.height, src->m_Height);
    dmJNI::SetEnum(env, obj, types->m_ImageJNI.pixelFormat, src->m_PixelFormat);
    dmJNI::SetEnum(env, obj, types->m_ImageJNI.colorSpace, src->m_ColorSpace);
    return obj;
}

jobject C2J_CreateBuffer(JNIEnv* env, TypeInfos* types, const Buffer* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_BufferJNI.cls);
    dmJNI::SetObjectDeref(env, obj, types->m_BufferJNI.data, dmJNI::C2J_CreateUByteArray(env, src->m_Data, src->m_DataCount));
    dmJNI::SetUInt(env, obj, types->m_BufferJNI.width, src->m_Width);
    dmJNI::SetUInt(env, obj, types->m_BufferJNI.height, src->m_Height);
    dmJNI::SetBoolean(env, obj, types->m_BufferJNI.isCompressed, src->m_IsCompressed);
    return obj;
}

jobject C2J_CreateBasisUEncodeSettings(JNIEnv* env, TypeInfos* types, const BasisUEncodeSettings* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_BasisUEncodeSettingsJNI.cls);
    dmJNI::SetString(env, obj, types->m_BasisUEncodeSettingsJNI.path, src->m_Path);
    dmJNI::SetInt(env, obj, types->m_BasisUEncodeSettingsJNI.width, src->m_Width);
    dmJNI::SetInt(env, obj, types->m_BasisUEncodeSettingsJNI.height, src->m_Height);
    dmJNI::SetEnum(env, obj, types->m_BasisUEncodeSettingsJNI.pixelFormat, src->m_PixelFormat);
    dmJNI::SetEnum(env, obj, types->m_BasisUEncodeSettingsJNI.colorSpace, src->m_ColorSpace);
    dmJNI::SetObjectDeref(env, obj, types->m_BasisUEncodeSettingsJNI.data, dmJNI::C2J_CreateUByteArray(env, src->m_Data, src->m_DataCount));
    dmJNI::SetInt(env, obj, types->m_BasisUEncodeSettingsJNI.numThreads, src->m_NumThreads);
    dmJNI::SetBoolean(env, obj, types->m_BasisUEncodeSettingsJNI.debug, src->m_Debug);
    dmJNI::SetEnum(env, obj, types->m_BasisUEncodeSettingsJNI.outPixelFormat, src->m_OutPixelFormat);
    dmJNI::SetBoolean(env, obj, types->m_BasisUEncodeSettingsJNI.rdo_uastc, src->m_rdo_uastc);
    dmJNI::SetUInt(env, obj, types->m_BasisUEncodeSettingsJNI.pack_uastc_flags, src->m_pack_uastc_flags);
    dmJNI::SetInt(env, obj, types->m_BasisUEncodeSettingsJNI.rdo_uastc_dict_size, src->m_rdo_uastc_dict_size);
    dmJNI::SetFloat(env, obj, types->m_BasisUEncodeSettingsJNI.rdo_uastc_quality_scalar, src->m_rdo_uastc_quality_scalar);
    return obj;
}

jobject C2J_CreateDefaultEncodeSettings(JNIEnv* env, TypeInfos* types, const DefaultEncodeSettings* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_DefaultEncodeSettingsJNI.cls);
    dmJNI::SetString(env, obj, types->m_DefaultEncodeSettingsJNI.path, src->m_Path);
    dmJNI::SetInt(env, obj, types->m_DefaultEncodeSettingsJNI.width, src->m_Width);
    dmJNI::SetInt(env, obj, types->m_DefaultEncodeSettingsJNI.height, src->m_Height);
    dmJNI::SetEnum(env, obj, types->m_DefaultEncodeSettingsJNI.pixelFormat, src->m_PixelFormat);
    dmJNI::SetEnum(env, obj, types->m_DefaultEncodeSettingsJNI.colorSpace, src->m_ColorSpace);
    dmJNI::SetObjectDeref(env, obj, types->m_DefaultEncodeSettingsJNI.data, dmJNI::C2J_CreateUByteArray(env, src->m_Data, src->m_DataCount));
    dmJNI::SetInt(env, obj, types->m_DefaultEncodeSettingsJNI.numThreads, src->m_NumThreads);
    dmJNI::SetBoolean(env, obj, types->m_DefaultEncodeSettingsJNI.debug, src->m_Debug);
    dmJNI::SetEnum(env, obj, types->m_DefaultEncodeSettingsJNI.outPixelFormat, src->m_OutPixelFormat);
    return obj;
}

jobject C2J_CreateASTCEncodeSettings(JNIEnv* env, TypeInfos* types, const ASTCEncodeSettings* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_ASTCEncodeSettingsJNI.cls);
    dmJNI::SetString(env, obj, types->m_ASTCEncodeSettingsJNI.path, src->m_Path);
    dmJNI::SetInt(env, obj, types->m_ASTCEncodeSettingsJNI.width, src->m_Width);
    dmJNI::SetInt(env, obj, types->m_ASTCEncodeSettingsJNI.height, src->m_Height);
    dmJNI::SetEnum(env, obj, types->m_ASTCEncodeSettingsJNI.pixelFormat, src->m_PixelFormat);
    dmJNI::SetEnum(env, obj, types->m_ASTCEncodeSettingsJNI.colorSpace, src->m_ColorSpace);
    dmJNI::SetObjectDeref(env, obj, types->m_ASTCEncodeSettingsJNI.data, dmJNI::C2J_CreateUByteArray(env, src->m_Data, src->m_DataCount));
    dmJNI::SetInt(env, obj, types->m_ASTCEncodeSettingsJNI.numThreads, src->m_NumThreads);
    dmJNI::SetFloat(env, obj, types->m_ASTCEncodeSettingsJNI.qualityLevel, src->m_QualityLevel);
    dmJNI::SetEnum(env, obj, types->m_ASTCEncodeSettingsJNI.outPixelFormat, src->m_OutPixelFormat);
    return obj;
}

jobjectArray C2J_CreateImageArray(JNIEnv* env, TypeInfos* types, const Image* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_ImageJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateImage(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateImagePtrArray(JNIEnv* env, TypeInfos* types, const Image* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_ImageJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateImage(env, types, src[i]);
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
jobjectArray C2J_CreateBasisUEncodeSettingsArray(JNIEnv* env, TypeInfos* types, const BasisUEncodeSettings* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_BasisUEncodeSettingsJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateBasisUEncodeSettings(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateBasisUEncodeSettingsPtrArray(JNIEnv* env, TypeInfos* types, const BasisUEncodeSettings* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_BasisUEncodeSettingsJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateBasisUEncodeSettings(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateDefaultEncodeSettingsArray(JNIEnv* env, TypeInfos* types, const DefaultEncodeSettings* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_DefaultEncodeSettingsJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateDefaultEncodeSettings(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateDefaultEncodeSettingsPtrArray(JNIEnv* env, TypeInfos* types, const DefaultEncodeSettings* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_DefaultEncodeSettingsJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateDefaultEncodeSettings(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateASTCEncodeSettingsArray(JNIEnv* env, TypeInfos* types, const ASTCEncodeSettings* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_ASTCEncodeSettingsJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateASTCEncodeSettings(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateASTCEncodeSettingsPtrArray(JNIEnv* env, TypeInfos* types, const ASTCEncodeSettings* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_ASTCEncodeSettingsJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateASTCEncodeSettings(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
//----------------------------------------
// From Jni to C
//----------------------------------------
bool J2C_CreateImage(JNIEnv* env, TypeInfos* types, jobject obj, Image* out) {
    if (out == 0) return false;
    out->m_Path = dmJNI::GetString(env, obj, types->m_ImageJNI.path);
    {
        jobject field_object = env->GetObjectField(obj, types->m_ImageJNI.data);
        if (field_object) {
            out->m_Data = dmJNI::J2C_CreateUByteArray(env, (jbyteArray)field_object, &out->m_DataCount);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_Width = dmJNI::GetUInt(env, obj, types->m_ImageJNI.width);
    out->m_Height = dmJNI::GetUInt(env, obj, types->m_ImageJNI.height);
    out->m_PixelFormat = (PixelFormat)dmJNI::GetEnum(env, obj, types->m_ImageJNI.pixelFormat);
    out->m_ColorSpace = (ColorSpace)dmJNI::GetEnum(env, obj, types->m_ImageJNI.colorSpace);
    return true;
}

bool J2C_CreateBuffer(JNIEnv* env, TypeInfos* types, jobject obj, Buffer* out) {
    if (out == 0) return false;
    {
        jobject field_object = env->GetObjectField(obj, types->m_BufferJNI.data);
        if (field_object) {
            out->m_Data = dmJNI::J2C_CreateUByteArray(env, (jbyteArray)field_object, &out->m_DataCount);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_Width = dmJNI::GetUInt(env, obj, types->m_BufferJNI.width);
    out->m_Height = dmJNI::GetUInt(env, obj, types->m_BufferJNI.height);
    out->m_IsCompressed = dmJNI::GetBoolean(env, obj, types->m_BufferJNI.isCompressed);
    return true;
}

bool J2C_CreateBasisUEncodeSettings(JNIEnv* env, TypeInfos* types, jobject obj, BasisUEncodeSettings* out) {
    if (out == 0) return false;
    out->m_Path = dmJNI::GetString(env, obj, types->m_BasisUEncodeSettingsJNI.path);
    out->m_Width = dmJNI::GetInt(env, obj, types->m_BasisUEncodeSettingsJNI.width);
    out->m_Height = dmJNI::GetInt(env, obj, types->m_BasisUEncodeSettingsJNI.height);
    out->m_PixelFormat = (PixelFormat)dmJNI::GetEnum(env, obj, types->m_BasisUEncodeSettingsJNI.pixelFormat);
    out->m_ColorSpace = (ColorSpace)dmJNI::GetEnum(env, obj, types->m_BasisUEncodeSettingsJNI.colorSpace);
    {
        jobject field_object = env->GetObjectField(obj, types->m_BasisUEncodeSettingsJNI.data);
        if (field_object) {
            out->m_Data = dmJNI::J2C_CreateUByteArray(env, (jbyteArray)field_object, &out->m_DataCount);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_NumThreads = dmJNI::GetInt(env, obj, types->m_BasisUEncodeSettingsJNI.numThreads);
    out->m_Debug = dmJNI::GetBoolean(env, obj, types->m_BasisUEncodeSettingsJNI.debug);
    out->m_OutPixelFormat = (PixelFormat)dmJNI::GetEnum(env, obj, types->m_BasisUEncodeSettingsJNI.outPixelFormat);
    out->m_rdo_uastc = dmJNI::GetBoolean(env, obj, types->m_BasisUEncodeSettingsJNI.rdo_uastc);
    out->m_pack_uastc_flags = dmJNI::GetUInt(env, obj, types->m_BasisUEncodeSettingsJNI.pack_uastc_flags);
    out->m_rdo_uastc_dict_size = dmJNI::GetInt(env, obj, types->m_BasisUEncodeSettingsJNI.rdo_uastc_dict_size);
    out->m_rdo_uastc_quality_scalar = dmJNI::GetFloat(env, obj, types->m_BasisUEncodeSettingsJNI.rdo_uastc_quality_scalar);
    return true;
}

bool J2C_CreateDefaultEncodeSettings(JNIEnv* env, TypeInfos* types, jobject obj, DefaultEncodeSettings* out) {
    if (out == 0) return false;
    out->m_Path = dmJNI::GetString(env, obj, types->m_DefaultEncodeSettingsJNI.path);
    out->m_Width = dmJNI::GetInt(env, obj, types->m_DefaultEncodeSettingsJNI.width);
    out->m_Height = dmJNI::GetInt(env, obj, types->m_DefaultEncodeSettingsJNI.height);
    out->m_PixelFormat = (PixelFormat)dmJNI::GetEnum(env, obj, types->m_DefaultEncodeSettingsJNI.pixelFormat);
    out->m_ColorSpace = (ColorSpace)dmJNI::GetEnum(env, obj, types->m_DefaultEncodeSettingsJNI.colorSpace);
    {
        jobject field_object = env->GetObjectField(obj, types->m_DefaultEncodeSettingsJNI.data);
        if (field_object) {
            out->m_Data = dmJNI::J2C_CreateUByteArray(env, (jbyteArray)field_object, &out->m_DataCount);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_NumThreads = dmJNI::GetInt(env, obj, types->m_DefaultEncodeSettingsJNI.numThreads);
    out->m_Debug = dmJNI::GetBoolean(env, obj, types->m_DefaultEncodeSettingsJNI.debug);
    out->m_OutPixelFormat = (PixelFormat)dmJNI::GetEnum(env, obj, types->m_DefaultEncodeSettingsJNI.outPixelFormat);
    return true;
}

bool J2C_CreateASTCEncodeSettings(JNIEnv* env, TypeInfos* types, jobject obj, ASTCEncodeSettings* out) {
    if (out == 0) return false;
    out->m_Path = dmJNI::GetString(env, obj, types->m_ASTCEncodeSettingsJNI.path);
    out->m_Width = dmJNI::GetInt(env, obj, types->m_ASTCEncodeSettingsJNI.width);
    out->m_Height = dmJNI::GetInt(env, obj, types->m_ASTCEncodeSettingsJNI.height);
    out->m_PixelFormat = (PixelFormat)dmJNI::GetEnum(env, obj, types->m_ASTCEncodeSettingsJNI.pixelFormat);
    out->m_ColorSpace = (ColorSpace)dmJNI::GetEnum(env, obj, types->m_ASTCEncodeSettingsJNI.colorSpace);
    {
        jobject field_object = env->GetObjectField(obj, types->m_ASTCEncodeSettingsJNI.data);
        if (field_object) {
            out->m_Data = dmJNI::J2C_CreateUByteArray(env, (jbyteArray)field_object, &out->m_DataCount);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_NumThreads = dmJNI::GetInt(env, obj, types->m_ASTCEncodeSettingsJNI.numThreads);
    out->m_QualityLevel = dmJNI::GetFloat(env, obj, types->m_ASTCEncodeSettingsJNI.qualityLevel);
    out->m_OutPixelFormat = (PixelFormat)dmJNI::GetEnum(env, obj, types->m_ASTCEncodeSettingsJNI.outPixelFormat);
    return true;
}

void J2C_CreateImageArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Image* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateImage(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Image* J2C_CreateImageArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Image* out = new Image[len];
    J2C_CreateImageArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateImagePtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Image** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new Image();
        J2C_CreateImage(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Image** J2C_CreateImagePtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Image** out = new Image*[len];
    J2C_CreateImagePtrArrayInPlace(env, types, arr, out, len);
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
void J2C_CreateBasisUEncodeSettingsArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, BasisUEncodeSettings* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateBasisUEncodeSettings(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
BasisUEncodeSettings* J2C_CreateBasisUEncodeSettingsArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    BasisUEncodeSettings* out = new BasisUEncodeSettings[len];
    J2C_CreateBasisUEncodeSettingsArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateBasisUEncodeSettingsPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, BasisUEncodeSettings** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new BasisUEncodeSettings();
        J2C_CreateBasisUEncodeSettings(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
BasisUEncodeSettings** J2C_CreateBasisUEncodeSettingsPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    BasisUEncodeSettings** out = new BasisUEncodeSettings*[len];
    J2C_CreateBasisUEncodeSettingsPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateDefaultEncodeSettingsArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, DefaultEncodeSettings* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateDefaultEncodeSettings(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
DefaultEncodeSettings* J2C_CreateDefaultEncodeSettingsArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    DefaultEncodeSettings* out = new DefaultEncodeSettings[len];
    J2C_CreateDefaultEncodeSettingsArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateDefaultEncodeSettingsPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, DefaultEncodeSettings** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new DefaultEncodeSettings();
        J2C_CreateDefaultEncodeSettings(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
DefaultEncodeSettings** J2C_CreateDefaultEncodeSettingsPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    DefaultEncodeSettings** out = new DefaultEncodeSettings*[len];
    J2C_CreateDefaultEncodeSettingsPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateASTCEncodeSettingsArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ASTCEncodeSettings* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateASTCEncodeSettings(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
ASTCEncodeSettings* J2C_CreateASTCEncodeSettingsArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    ASTCEncodeSettings* out = new ASTCEncodeSettings[len];
    J2C_CreateASTCEncodeSettingsArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateASTCEncodeSettingsPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, ASTCEncodeSettings** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new ASTCEncodeSettings();
        J2C_CreateASTCEncodeSettings(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
ASTCEncodeSettings** J2C_CreateASTCEncodeSettingsPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    ASTCEncodeSettings** out = new ASTCEncodeSettings*[len];
    J2C_CreateASTCEncodeSettingsPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
} // jni
} // dmTexc
