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

#include <jni.h> // JDK
#include <jni/jni_util.h> // defold

#include <jni/Texc_jni.h>

#include <dlib/log.h>

#include <texc.h>


    // Get header (info) of a texture
    // bool GetHeader(Texture* texture, Header* out_header);

    // Get the compressed data size in bytes of a mip map. Returns 0 if not compressed
    // uint32_t GetDataSizeCompressed(Texture* texture, uint32_t mip_map);

    // Get the uncompressed data size in bytes of a mip map in a texture
    // uint32_t GetDataSizeUncompressed(Texture* texture, uint32_t mip_map);

    // Get the total data size in bytes including all mip maps in a texture (compressed or not)
    // uint32_t GetTotalDataSize(Texture* texture);

    // Get the data pointer to texture (mip maps linear layout in memory)
    // uint32_t GetData(Texture* texture, void* out_data, uint32_t out_data_size);

    // Get compression flags
    // uint64_t GetCompressionFlags(Texture* texture);

    // Resize a texture. The texture must have format PF_R8G8B8A8 to be resized.
    // bool Resize(Texture* texture, uint32_t width, uint32_t height);

    // Pre-multiply the color with alpha in a texture. The texture must have format PF_R8G8B8A8 for the alpha to be pre-multiplied.
    // bool PreMultiplyAlpha(Texture* texture);

    // Generate mip maps. The texture must have format PF_R8G8B8A8 for mip maps to be generated.
    // bool GenMipMaps(Texture* texture);

    // Flips a texture vertically
    // bool Flip(Texture* texture, FlipAxis flip_axis);

    // Encode a texture into basis format.
    // bool Encode(Texture* texture, PixelFormat pixelFormat, ColorSpace color_space, CompressionLevel compressionLevel, CompressionType compression_type, bool mipmaps, int max_threads);



    // Now only used for font glyphs
    // Compresses an image buffer
    // BufferData* CompressBuffer(uint8_t* byte, uint32_t byte_count);

    // Get the total data size in bytes including all mip maps in a texture (compressed or not)
    // // uint32_t GetTotalBufferDataSize(HBuffer buffer);

    // Gets the data from a buffer
    // // uint32_t GetBufferData(HBuffer buffer, void* buffer, uint32_t buffer_size);

    // // Destroys a buffer created by CompressBuffer
    // void DestroyBuffer(BufferData* buffer);


JNIEXPORT jlong JNICALL Java_TexcLibraryJni_CreateTexture(JNIEnv* env, jclass cls, jstring _path, jint width, jint height, jint pixelFormat, jint colorSpace, jint compressionType, jbyteArray array)
{
    dmLogDebug("%s: env = %p\n", __FUNCTION__, env);
    //DM_SCOPED_SIGNAL_CONTEXT(env, return 0;);

    jlong obj = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        dmTexc::jni::ScopedContext jni_scope(env);
        //dmTexc::jni::TypeInfos* types = &jni_scope.m_TypeInfos;

        dmJNI::ScopedString j_path(env, _path);
        const char* path = strdup(j_path.m_String ? j_path.m_String : "null");

        //jsize array_size = env->GetArrayLength(array);
        jbyte* array_data = env->GetByteArrayElements(array, 0);

        dmTexc::Texture* texture = dmTexc::CreateTexture(path, width, height, (dmTexc::PixelFormat)pixelFormat, (dmTexc::ColorSpace)colorSpace, (dmTexc::CompressionType)compressionType, array_data);
        obj = (jlong)texture;

        env->ReleaseByteArrayElements(array, array_data, JNI_ABORT);
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return obj;
}

JNIEXPORT void JNICALL Java_TexcLibraryJni_DestroyTexture(JNIEnv* env, jclass cls, jlong texture)
{
    dmLogDebug("%s: env = %p\n", __FUNCTION__, env);
    //DM_SCOPED_SIGNAL_CONTEXT(env, return 0;);

    DM_JNI_GUARD_SCOPE_BEGIN();
        dmTexc::jni::ScopedContext jni_scope(env);
        dmTexc::DestroyTexture((dmTexc::Texture*)texture);
    DM_JNI_GUARD_SCOPE_END(return;);
}

JNIEXPORT jobject JNICALL Java_TexcLibraryJni_GetHeader(JNIEnv* env, jclass cls, jlong texture)
{
    //DM_SCOPED_SIGNAL_CONTEXT(env, return 0;);

    jobject obj = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        dmTexc::jni::ScopedContext jni_scope(env);
        dmTexc::jni::TypeInfos* types = &jni_scope.m_TypeInfos;

        dmTexc::Texture* ptexture = (dmTexc::Texture*)texture;
        if (ptexture)
        {
            dmTexc::Header header;
            if (dmTexc::GetHeader(ptexture, &header))
            {
                obj = C2J_CreateHeader(env, types, &header);
            }
        }
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return obj;
}

JNIEXPORT jint JNICALL Java_TexcLibraryJni_GetDataSizeCompressed(JNIEnv* env, jclass cls, jlong texture, jint mip_map)
{
    jint result = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        //dmTexc::jni::ScopedContext jni_scope(env);
        dmTexc::Texture* ptexture = (dmTexc::Texture*)texture;
        if (ptexture)
        {
            result = dmTexc::GetDataSizeCompressed(ptexture, mip_map);
        }
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return result;
}

JNIEXPORT jint JNICALL Java_TexcLibraryJni_GetDataSizeUncompressed(JNIEnv* env, jclass cls, jlong texture, jint mip_map)
{
    jint result = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        //dmTexc::jni::ScopedContext jni_scope(env);
        dmTexc::Texture* ptexture = (dmTexc::Texture*)texture;
        if (ptexture)
        {
            result = dmTexc::GetDataSizeUncompressed(ptexture, mip_map);
        }
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return result;
}

JNIEXPORT jint JNICALL Java_TexcLibraryJni_GetCompressionFlags(JNIEnv* env, jclass cls, jlong texture)
{
    jint result = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        //dmTexc::jni::ScopedContext jni_scope(env);
        dmTexc::Texture* ptexture = (dmTexc::Texture*)texture;
        if (ptexture)
        {
            result = dmTexc::GetCompressionFlags(ptexture);
        }
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return result;
}

JNIEXPORT jobject JNICALL Java_TexcLibraryJni_GetData(JNIEnv* env, jclass cls, jlong texture)
{
    jobject obj = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        dmTexc::jni::ScopedContext jni_scope(env);

        dmTexc::Texture* ptexture = (dmTexc::Texture*)texture;
        if (ptexture)
        {
            uint32_t data_count = dmTexc::GetTotalDataSize(ptexture);

            jbyteArray arr = env->NewByteArray(data_count);
            jbyte* data = env->GetByteArrayElements(arr, 0);
            dmTexc::GetData(ptexture, data, data_count);
            env->ReleaseByteArrayElements(arr, data, 0);

            obj = arr;
        }
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return obj;
}


JNIEXPORT jboolean JNICALL Java_TexcLibraryJni_GetCompressionFlags(JNIEnv* env, jclass cls, jlong texture)
{
    jboolean result = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        dmTexc::Texture* ptexture = (dmTexc::Texture*)texture;
        if (ptexture)
        {
            result = dmTexc::GetCompressionFlags(ptexture);
        }
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return result;
}


    // Resize a texture. The texture must have format PF_R8G8B8A8 to be resized.
    bool Resize(Texture* texture, uint32_t width, uint32_t height);

    // Pre-multiply the color with alpha in a texture. The texture must have format PF_R8G8B8A8 for the alpha to be pre-multiplied.
    bool PreMultiplyAlpha(Texture* texture);

    // Generate mip maps. The texture must have format PF_R8G8B8A8 for mip maps to be generated.
    bool GenMipMaps(Texture* texture);

    // Flips a texture vertically
    bool Flip(Texture* texture, FlipAxis flip_axis);


// JNIEXPORT jobject JNICALL Java_TexcLibraryJni_CreateImage(JNIEnv* env, jclass cls, jstring path, jbyteArray bytes, jint width, jint height, jint depth, jint numChannels)
// {
//     dmLogDebug("%s: env = %p\n", __FUNCTION__, env);
//     //DM_SCOPED_SIGNAL_CONTEXT(env, return 0;);

//     jobject jimage;
//     DM_JNI_GUARD_SCOPE_BEGIN();

//         dmTexc::jni::ScopedContext jni_scope(env);
//         dmTexc::jni::TypeInfos* types = &jni_scope.m_TypeInfos;
//         //jimage = LoadFromBufferInternal(env, cls, _path, array, data_resolver);
//         jimage = dmTexc::jni::C2J_CreateImage(env, types, path, bytes, width, height, depth, numChannels);
//     DM_JNI_GUARD_SCOPE_END(return 0;);
//     return jimage;
// }

// public static native Pointer TEXC_Create(String name, int width, int height, int pixelFormat, int colorSpace, int compressionType, Buffer data);
// public static native void TEXC_Destroy(Pointer texture);

// public static native int TEXC_GetDataSizeCompressed(Pointer texture, int minMap);
// public static native int TEXC_GetDataSizeUncompressed(Pointer texture, int minMap);
// public static native int TEXC_GetTotalDataSize(Pointer texture);
// public static native int TEXC_GetData(Pointer texture, Buffer outData, int maxOutDataSize);
// public static native int TEXC_GetCompressionFlags(Pointer texture);

// public static native boolean TEXC_Resize(Pointer texture, int width, int height);
// public static native boolean TEXC_PreMultiplyAlpha(Pointer texture);
// public static native boolean TEXC_GenMipMaps(Pointer texture);
// public static native boolean TEXC_Flip(Pointer texture, int flipAxis);
// public static native boolean TEXC_Encode(Pointer texture, int pixelFormat, int colorSpace, int compressionLevel, int compressionType, boolean mipmaps, int num_threads);

// // For font glyphs
// public static native Pointer TEXC_CompressBuffer(Buffer data, int datasize);
// public static native int TEXC_GetTotalBufferDataSize(Pointer buffer);
// public static native int TEXC_GetBufferData(Pointer buffer, Buffer outData, int maxOutDataSize);
// public static native void TEXC_DestroyBuffer(Pointer buffer);

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    dmLogDebug("JNI_OnLoad ->\n");
    //dmJNI::EnableDefaultSignalHandlers(vm);

    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8) != JNI_OK) {
        printf("JNI_OnLoad GetEnv error\n");
        return JNI_ERR;
    }

    // Find your class. JNI_OnLoad is called from the correct class loader context for this to work.
    jclass c = env->FindClass( JAVA_PACKAGE_NAME "/TexcLibraryJni");
    dmLogDebug("JNI_OnLoad: c = %p\n", c);
    if (c == 0)
        return JNI_ERR;

#define JNIFUNC(NAME, SIGNATURE) {(char*) #NAME, (char*)(SIGNATURE), reinterpret_cast<void*>(Java_TexcLibraryJni_ ## NAME )}

    // Register your class' native methods.
    // Don't forget to add them to the corresponding java file (e.g. ModelImporter.java)
    static const JNINativeMethod methods[] = {
        // OLD API
        JNIFUNC(CreateTexture,          "(Ljava/lang/String;IIIII[B)J"),
        JNIFUNC(DestroyTexture,         "(J)V"),
        JNIFUNC(GetHeader,              "(J)L" CLASS_NAME "$Header;"),
        JNIFUNC(GetDataSizeCompressed,  "(JI)I"),
        JNIFUNC(GetDataSizeUncompressed,"(JI)I"),
        JNIFUNC(GetData,                "(J)[B"),

        JNIFUNC(Resize,                 "(JII)Z"),
        JNIFUNC(PreMultiplyAlpha,       "(J)Z"),
        JNIFUNC(GenMipMaps,             "(J)Z"),
        JNIFUNC(Flip,                   "(JI)Z"),

        // Image api
        //{(char*)"CreateImage", (char*)"(Ljava/lang/String;[BIIII)L" CLASS_NAME "$Scene;", reinterpret_cast<void*>(Java_TexcLibraryJni_CreateImage)},
    };
    int rc = env->RegisterNatives(c, methods, sizeof(methods)/sizeof(JNINativeMethod));
    env->DeleteLocalRef(c);

    if (rc != JNI_OK) return rc;

    dmLogDebug("JNI_OnLoad return.\n");
    return JNI_VERSION_1_8;
}

JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved)
{
    dmLogDebug("JNI_OnUnload ->\n");
}
