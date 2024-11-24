// generated, do not edit

#include <jni.h>
#include "texc.h"

#define JAVA_PACKAGE_NAME "com/dynamo/bob/pipeline"
#define CLASS_NAME "com/dynamo/bob/pipeline/Texc"

namespace dmTexc {
namespace jni {
struct HeaderJNI {
    jclass cls;
    jfieldID version;
    jfieldID flags;
    jfieldID pixelFormat;
    jfieldID colourSpace;
    jfieldID channelType;
    jfieldID height;
    jfieldID width;
    jfieldID depth;
    jfieldID numSurfaces;
    jfieldID numFaces;
    jfieldID mipMapCount;
    jfieldID metaDataSize;
};
struct TextureJNI {
    jclass cls;
    jfieldID impl;
};
struct BufferJNI {
    jclass cls;
    jfieldID data;
    jfieldID width;
    jfieldID height;
    jfieldID isCompressed;
};
struct BasisUSettingsJNI {
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
struct TypeInfos {
    HeaderJNI m_HeaderJNI;
    TextureJNI m_TextureJNI;
    BufferJNI m_BufferJNI;
    BasisUSettingsJNI m_BasisUSettingsJNI;
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
jobject C2J_CreateHeader(JNIEnv* env, TypeInfos* types, const Header* src);
jobject C2J_CreateTexture(JNIEnv* env, TypeInfos* types, const Texture* src);
jobject C2J_CreateBuffer(JNIEnv* env, TypeInfos* types, const Buffer* src);
jobject C2J_CreateBasisUSettings(JNIEnv* env, TypeInfos* types, const BasisUSettings* src);
jobjectArray C2J_CreateHeaderArray(JNIEnv* env, TypeInfos* types, const Header* src, uint32_t src_count);
jobjectArray C2J_CreateHeaderPtrArray(JNIEnv* env, TypeInfos* types, const Header* const* src, uint32_t src_count);
jobjectArray C2J_CreateTextureArray(JNIEnv* env, TypeInfos* types, const Texture* src, uint32_t src_count);
jobjectArray C2J_CreateTexturePtrArray(JNIEnv* env, TypeInfos* types, const Texture* const* src, uint32_t src_count);
jobjectArray C2J_CreateBufferArray(JNIEnv* env, TypeInfos* types, const Buffer* src, uint32_t src_count);
jobjectArray C2J_CreateBufferPtrArray(JNIEnv* env, TypeInfos* types, const Buffer* const* src, uint32_t src_count);
jobjectArray C2J_CreateBasisUSettingsArray(JNIEnv* env, TypeInfos* types, const BasisUSettings* src, uint32_t src_count);
jobjectArray C2J_CreateBasisUSettingsPtrArray(JNIEnv* env, TypeInfos* types, const BasisUSettings* const* src, uint32_t src_count);
//----------------------------------------
// From Jni to C
//----------------------------------------
bool J2C_CreateHeader(JNIEnv* env, TypeInfos* types, jobject obj, Header* out);
bool J2C_CreateTexture(JNIEnv* env, TypeInfos* types, jobject obj, Texture* out);
bool J2C_CreateBuffer(JNIEnv* env, TypeInfos* types, jobject obj, Buffer* out);
bool J2C_CreateBasisUSettings(JNIEnv* env, TypeInfos* types, jobject obj, BasisUSettings* out);
Header* J2C_CreateHeaderArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateHeaderArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Header* dst, uint32_t dst_count);
Header** J2C_CreateHeaderPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateHeaderPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Header** dst, uint32_t dst_count);
Texture* J2C_CreateTextureArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateTextureArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Texture* dst, uint32_t dst_count);
Texture** J2C_CreateTexturePtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateTexturePtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Texture** dst, uint32_t dst_count);
Buffer* J2C_CreateBufferArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateBufferArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Buffer* dst, uint32_t dst_count);
Buffer** J2C_CreateBufferPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateBufferPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, Buffer** dst, uint32_t dst_count);
BasisUSettings* J2C_CreateBasisUSettingsArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateBasisUSettingsArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, BasisUSettings* dst, uint32_t dst_count);
BasisUSettings** J2C_CreateBasisUSettingsPtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count);
void J2C_CreateBasisUSettingsPtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, BasisUSettings** dst, uint32_t dst_count);
} // jni
} // dmTexc
