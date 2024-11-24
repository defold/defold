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
        SETUP_CLASS(HeaderJNI, "Header");
        GET_FLD_TYPESTR(version, "I");
        GET_FLD_TYPESTR(flags, "I");
        GET_FLD_TYPESTR(pixelFormat, "J");
        GET_FLD_TYPESTR(colourSpace, "I");
        GET_FLD_TYPESTR(channelType, "I");
        GET_FLD_TYPESTR(height, "I");
        GET_FLD_TYPESTR(width, "I");
        GET_FLD_TYPESTR(depth, "I");
        GET_FLD_TYPESTR(numSurfaces, "I");
        GET_FLD_TYPESTR(numFaces, "I");
        GET_FLD_TYPESTR(mipMapCount, "I");
        GET_FLD_TYPESTR(metaDataSize, "I");
    }
    {
        SETUP_CLASS(TextureJNI, "Texture");
        GET_FLD_TYPESTR(impl, "J");
    }
    {
        SETUP_CLASS(BufferJNI, "Buffer");
        GET_FLD_TYPESTR(data, "[B");
        GET_FLD_TYPESTR(width, "I");
        GET_FLD_TYPESTR(height, "I");
        GET_FLD_TYPESTR(isCompressed, "Z");
    }
    {
        SETUP_CLASS(BasisUSettingsJNI, "BasisUSettings");
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
    #undef GET_FLD
    #undef GET_FLD_ARRAY
    #undef GET_FLD_TYPESTR
}

void FinalizeJNITypes(JNIEnv* env, TypeInfos* infos) {
    env->DeleteLocalRef(infos->m_HeaderJNI.cls);
    env->DeleteLocalRef(infos->m_TextureJNI.cls);
    env->DeleteLocalRef(infos->m_BufferJNI.cls);
    env->DeleteLocalRef(infos->m_BasisUSettingsJNI.cls);
}


//----------------------------------------
// From C to Jni
//----------------------------------------
jobject C2J_CreateHeader(JNIEnv* env, TypeInfos* types, const Header* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_HeaderJNI.cls);
    dmJNI::SetUInt(env, obj, types->m_HeaderJNI.version, src->m_Version);
    dmJNI::SetUInt(env, obj, types->m_HeaderJNI.flags, src->m_Flags);
    dmJNI::SetULong(env, obj, types->m_HeaderJNI.pixelFormat, src->m_PixelFormat);
    dmJNI::SetUInt(env, obj, types->m_HeaderJNI.colourSpace, src->m_ColourSpace);
    dmJNI::SetUInt(env, obj, types->m_HeaderJNI.channelType, src->m_ChannelType);
    dmJNI::SetUInt(env, obj, types->m_HeaderJNI.height, src->m_Height);
    dmJNI::SetUInt(env, obj, types->m_HeaderJNI.width, src->m_Width);
    dmJNI::SetUInt(env, obj, types->m_HeaderJNI.depth, src->m_Depth);
    dmJNI::SetUInt(env, obj, types->m_HeaderJNI.numSurfaces, src->m_NumSurfaces);
    dmJNI::SetUInt(env, obj, types->m_HeaderJNI.numFaces, src->m_NumFaces);
    dmJNI::SetUInt(env, obj, types->m_HeaderJNI.mipMapCount, src->m_MipMapCount);
    dmJNI::SetUInt(env, obj, types->m_HeaderJNI.metaDataSize, src->m_MetaDataSize);
    return obj;
}

jobject C2J_CreateTexture(JNIEnv* env, TypeInfos* types, const Texture* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_TextureJNI.cls);
    dmJNI::SetLong(env, obj, types->m_TextureJNI.impl, (uintptr_t)src->m_Impl);
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

jobject C2J_CreateBasisUSettings(JNIEnv* env, TypeInfos* types, const BasisUSettings* src) {
    if (src == 0) return 0;
    jobject obj = env->AllocObject(types->m_BasisUSettingsJNI.cls);
    dmJNI::SetString(env, obj, types->m_BasisUSettingsJNI.path, src->m_Path);
    dmJNI::SetInt(env, obj, types->m_BasisUSettingsJNI.width, src->m_Width);
    dmJNI::SetInt(env, obj, types->m_BasisUSettingsJNI.height, src->m_Height);
    dmJNI::SetEnum(env, obj, types->m_BasisUSettingsJNI.pixelFormat, src->m_PixelFormat);
    dmJNI::SetEnum(env, obj, types->m_BasisUSettingsJNI.colorSpace, src->m_ColorSpace);
    dmJNI::SetObjectDeref(env, obj, types->m_BasisUSettingsJNI.data, dmJNI::C2J_CreateUByteArray(env, src->m_Data, src->m_DataCount));
    dmJNI::SetInt(env, obj, types->m_BasisUSettingsJNI.numThreads, src->m_NumThreads);
    dmJNI::SetBoolean(env, obj, types->m_BasisUSettingsJNI.debug, src->m_Debug);
    dmJNI::SetEnum(env, obj, types->m_BasisUSettingsJNI.outPixelFormat, src->m_OutPixelFormat);
    dmJNI::SetBoolean(env, obj, types->m_BasisUSettingsJNI.rdo_uastc, src->m_rdo_uastc);
    dmJNI::SetUInt(env, obj, types->m_BasisUSettingsJNI.pack_uastc_flags, src->m_pack_uastc_flags);
    dmJNI::SetInt(env, obj, types->m_BasisUSettingsJNI.rdo_uastc_dict_size, src->m_rdo_uastc_dict_size);
    dmJNI::SetFloat(env, obj, types->m_BasisUSettingsJNI.rdo_uastc_quality_scalar, src->m_rdo_uastc_quality_scalar);
    return obj;
}

jobjectArray C2J_CreateHeaderArray(JNIEnv* env, TypeInfos* types, const Header* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_HeaderJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateHeader(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateHeaderPtrArray(JNIEnv* env, TypeInfos* types, const Header* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_HeaderJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateHeader(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateTextureArray(JNIEnv* env, TypeInfos* types, const Texture* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_TextureJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateTexture(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateTexturePtrArray(JNIEnv* env, TypeInfos* types, const Texture* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_TextureJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateTexture(env, types, src[i]);
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
jobjectArray C2J_CreateBasisUSettingsArray(JNIEnv* env, TypeInfos* types, const BasisUSettings* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_BasisUSettingsJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateBasisUSettings(env, types, &src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
jobjectArray C2J_CreateBasisUSettingsPtrArray(JNIEnv* env, TypeInfos* types, const BasisUSettings* const* src, uint32_t src_count) {
    if (src == 0 || src_count == 0) return 0;
    jobjectArray arr = env->NewObjectArray(src_count, types->m_BasisUSettingsJNI.cls, 0);
    for (uint32_t i = 0; i < src_count; ++i) {
        jobject obj = C2J_CreateBasisUSettings(env, types, src[i]);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}
//----------------------------------------
// From Jni to C
//----------------------------------------
bool J2C_CreateHeader(JNIEnv* env, TypeInfos* types, jobject obj, Header* out) {
    if (out == 0) return false;
    out->m_Version = dmJNI::GetUInt(env, obj, types->m_HeaderJNI.version);
    out->m_Flags = dmJNI::GetUInt(env, obj, types->m_HeaderJNI.flags);
    out->m_PixelFormat = dmJNI::GetULong(env, obj, types->m_HeaderJNI.pixelFormat);
    out->m_ColourSpace = dmJNI::GetUInt(env, obj, types->m_HeaderJNI.colourSpace);
    out->m_ChannelType = dmJNI::GetUInt(env, obj, types->m_HeaderJNI.channelType);
    out->m_Height = dmJNI::GetUInt(env, obj, types->m_HeaderJNI.height);
    out->m_Width = dmJNI::GetUInt(env, obj, types->m_HeaderJNI.width);
    out->m_Depth = dmJNI::GetUInt(env, obj, types->m_HeaderJNI.depth);
    out->m_NumSurfaces = dmJNI::GetUInt(env, obj, types->m_HeaderJNI.numSurfaces);
    out->m_NumFaces = dmJNI::GetUInt(env, obj, types->m_HeaderJNI.numFaces);
    out->m_MipMapCount = dmJNI::GetUInt(env, obj, types->m_HeaderJNI.mipMapCount);
    out->m_MetaDataSize = dmJNI::GetUInt(env, obj, types->m_HeaderJNI.metaDataSize);
    return true;
}

bool J2C_CreateTexture(JNIEnv* env, TypeInfos* types, jobject obj, Texture* out) {
    if (out == 0) return false;
    out->m_Impl = (void*)(uintptr_t)dmJNI::GetLong(env, obj, types->m_TextureJNI.impl);
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

bool J2C_CreateBasisUSettings(JNIEnv* env, TypeInfos* types, jobject obj, BasisUSettings* out) {
    if (out == 0) return false;
    out->m_Path = dmJNI::GetString(env, obj, types->m_BasisUSettingsJNI.path);
    out->m_Width = dmJNI::GetInt(env, obj, types->m_BasisUSettingsJNI.width);
    out->m_Height = dmJNI::GetInt(env, obj, types->m_BasisUSettingsJNI.height);
    out->m_PixelFormat = (PixelFormat)dmJNI::GetEnum(env, obj, types->m_BasisUSettingsJNI.pixelFormat);
    out->m_ColorSpace = (ColorSpace)dmJNI::GetEnum(env, obj, types->m_BasisUSettingsJNI.colorSpace);
    {
        jobject field_object = env->GetObjectField(obj, types->m_BasisUSettingsJNI.data);
        if (field_object) {
            out->m_Data = dmJNI::J2C_CreateUByteArray(env, (jbyteArray)field_object, &out->m_DataCount);
            env->DeleteLocalRef(field_object);
        }
    }
    out->m_NumThreads = dmJNI::GetInt(env, obj, types->m_BasisUSettingsJNI.numThreads);
    out->m_Debug = dmJNI::GetBoolean(env, obj, types->m_BasisUSettingsJNI.debug);
    out->m_OutPixelFormat = (PixelFormat)dmJNI::GetEnum(env, obj, types->m_BasisUSettingsJNI.outPixelFormat);
    out->m_rdo_uastc = dmJNI::GetBoolean(env, obj, types->m_BasisUSettingsJNI.rdo_uastc);
    out->m_pack_uastc_flags = dmJNI::GetUInt(env, obj, types->m_BasisUSettingsJNI.pack_uastc_flags);
    out->m_rdo_uastc_dict_size = dmJNI::GetInt(env, obj, types->m_BasisUSettingsJNI.rdo_uastc_dict_size);
    out->m_rdo_uastc_quality_scalar = dmJNI::GetFloat(env, obj, types->m_BasisUSettingsJNI.rdo_uastc_quality_scalar);
    return true;
}

void J2C_CreateHeaderArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Header* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateHeader(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Header* J2C_CreateHeaderArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Header* out = new Header[len];
    J2C_CreateHeaderArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateHeaderPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Header** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new Header();
        J2C_CreateHeader(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Header** J2C_CreateHeaderPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Header** out = new Header*[len];
    J2C_CreateHeaderPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateTextureArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Texture* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateTexture(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Texture* J2C_CreateTextureArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Texture* out = new Texture[len];
    J2C_CreateTextureArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateTexturePtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Texture** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new Texture();
        J2C_CreateTexture(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
Texture** J2C_CreateTexturePtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    Texture** out = new Texture*[len];
    J2C_CreateTexturePtrArrayInPlace(env, types, arr, out, len);
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
void J2C_CreateBasisUSettingsArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, BasisUSettings* dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        J2C_CreateBasisUSettings(env, types, obj, &dst[i]);
        env->DeleteLocalRef(obj);
    }
}
BasisUSettings* J2C_CreateBasisUSettingsArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    BasisUSettings* out = new BasisUSettings[len];
    J2C_CreateBasisUSettingsArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
void J2C_CreateBasisUSettingsPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, BasisUSettings** dst, uint32_t dst_count) {
    jsize len = env->GetArrayLength(arr);
    if (len != dst_count) {
        printf("Number of elements mismatch. Expected %u, but got %u\n", dst_count, len);
    }
    if (len > dst_count)
        len = dst_count;
    for (uint32_t i = 0; i < len; ++i) {
        jobject obj = env->GetObjectArrayElement(arr, i);
        dst[i] = new BasisUSettings();
        J2C_CreateBasisUSettings(env, types, obj, dst[i]);
        env->DeleteLocalRef(obj);
    }
}
BasisUSettings** J2C_CreateBasisUSettingsPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count) {
    jsize len = env->GetArrayLength(arr);
    BasisUSettings** out = new BasisUSettings*[len];
    J2C_CreateBasisUSettingsPtrArrayInPlace(env, types, arr, out, len);
    *out_count = (uint32_t)len;
    return out;
}
} // jni
} // dmTexc
