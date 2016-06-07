#include "sound.h"
#include "sound_private.h"

#include <jni.h>
#include <cstdio>

#include <android_native_app_glue.h>
#include <dlib/log.h>

extern struct android_app* g_AndroidApp;

namespace
{

    bool Detach(JNIEnv* environment)
    {
        assert(environment);

        bool exception = (bool) environment->ExceptionCheck();
        environment->ExceptionClear();
        g_AndroidApp->activity->vm->DetachCurrentThread();

        return !exception;
    }

    jclass LoadClass(JNIEnv* environment, const char* class_name)
    {
        assert(environment);
        jclass      jni_class_NativeActivity    = environment->FindClass("android/app/NativeActivity");

        jmethodID   jni_method_getClassLoader   = environment->GetMethodID(jni_class_NativeActivity, "getClassLoader", "()Ljava/lang/ClassLoader;");
        jobject     jni_object_getClassLoader   = environment->CallObjectMethod(g_AndroidApp->activity->clazz, jni_method_getClassLoader);
        jclass      jni_class_ClassLoader       = environment->FindClass("java/lang/ClassLoader");

        jmethodID   jni_method_loadClass        = environment->GetMethodID(jni_class_ClassLoader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
        jstring     jni_string_class_name       = environment->NewStringUTF(class_name);

        jclass      jni_class_result            = (jclass) environment->CallObjectMethod(jni_object_getClassLoader, jni_method_loadClass, jni_string_class_name);

        environment->DeleteLocalRef(jni_string_class_name);

        assert(jni_class_result);
        return jni_class_result;
    }

    bool CheckException(JNIEnv* environment)
    {
        bool exception = (bool) environment->ExceptionCheck();
        if (exception)
        {
            dmLogError("An exception occurred within the JNI environment!");
            environment->ExceptionDescribe();
            environment->ExceptionClear();
        }

        return !exception;
    }

}

namespace dmSound
{
    Result PlatformInitialize(dmConfigFile::HConfig config,
            const InitializeParams* params)
    {
        return RESULT_OK;
    }

    Result PlatformFinalize()
    {
        return RESULT_OK;
    }

    bool PlatformIsMusicPlaying()
    {
        // Setup
        JNIEnv* environment;
        g_AndroidApp->activity->vm->AttachCurrentThread(&environment, NULL);
        jclass SoundJNI = ::LoadClass(environment, "com.defold.sound.IsMusicPlaying");
        assert(::CheckException(environment));

        // Execute
        jmethodID isMusicPlaying = environment->GetStaticMethodID(SoundJNI, "isMusicPlaying", "(Landroid/content/Context;)Z");
        assert(::CheckException(environment));

        int result = (int) environment->CallStaticBooleanMethod(SoundJNI, isMusicPlaying, g_AndroidApp->activity->clazz);
        assert(::CheckException(environment));

        // Teardown
        if (!::Detach(environment))
        {
            dmLogError("Unhandled exceptions occurred during JNI call to isMusicPlaying!");
        }

        // Result

        return (bool) result;
    }
}
