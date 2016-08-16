#include "sound.h"
#include "sound_private.h"

#include <android_native_app_glue.h>
#include <dlib/log.h>
#include <jni.h>

extern struct android_app* g_AndroidApp;

struct SoundManager
{

    SoundManager()
    {
        memset(this, 0x0, sizeof(struct SoundManager));
    }

    jobject     m_SoundManager;
    jmethodID   m_AcquireAudioFocus;
    jmethodID   m_IsMusicPlaying;
    jmethodID   m_ReleaseAudioFocus;
    bool        m_IsPhoneCallActive;
};

struct SoundManager g_SoundManager;

namespace
{

    bool CheckException(JNIEnv* environment)
    {
        assert(environment != NULL);
        if ((bool) environment->ExceptionCheck())
        {
            dmLogError("An exception occurred within the JNI environment (%p)", environment);
            environment->ExceptionDescribe();
            environment->ExceptionClear();
            return false;
        }

        return true;
    }

    JNIEnv* Attach()
    {
        JNIEnv* environment = NULL;
        g_AndroidApp->activity->vm->AttachCurrentThread(&environment, NULL);
        return environment;
    }

    bool Detach(JNIEnv* environment)
    {
        assert(environment != NULL);
        bool result = CheckException(environment);
        g_AndroidApp->activity->vm->DetachCurrentThread();
        return result;
    }

    bool CallZ(jmethodID method, bool _default)
    {
        assert(method != 0);
        JNIEnv* environment = ::Attach();
        bool result = environment->CallBooleanMethod(g_SoundManager.m_SoundManager, method);
        return (::CheckException(environment) && ::Detach(environment)) ? result : _default;
    }

}

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void JNICALL Java_com_defold_sound_SoundManager_setPhoneCallState(JNIEnv* env, jobject, jint active)
{
    g_SoundManager.m_IsPhoneCallActive = active ? true : false;
}


#ifdef __cplusplus
}
#endif

namespace dmSound
{
    Result PlatformInitialize(dmConfigFile::HConfig config,
            const InitializeParams* params)
    {
        JNIEnv* environment = ::Attach();

        g_SoundManager.m_IsPhoneCallActive       = false; // It will get updated by the call to the construct
        
        jclass      jni_class_NativeActivity     = environment->FindClass("android/app/NativeActivity");
        jmethodID   jni_method_getClassLoader    = environment->GetMethodID(jni_class_NativeActivity, "getClassLoader", "()Ljava/lang/ClassLoader;");
        jobject     jni_object_getClassLoader    = environment->CallObjectMethod(g_AndroidApp->activity->clazz, jni_method_getClassLoader);
        jclass      jni_class_ClassLoader        = environment->FindClass("java/lang/ClassLoader");
        jmethodID   jni_method_loadClass         = environment->GetMethodID(jni_class_ClassLoader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");

        jstring     jni_string_SoundManager      = environment->NewStringUTF("com.defold.sound.SoundManager");
        jclass      jni_class_SoundManager       = (jclass) environment->CallObjectMethod(jni_object_getClassLoader, jni_method_loadClass, jni_string_SoundManager);
        jmethodID   jni_constructor_SoundManager = environment->GetMethodID(jni_class_SoundManager, "<init>", "(Landroid/app/Activity;)V");

        g_SoundManager.m_SoundManager            = environment->NewGlobalRef(environment->NewObject(jni_class_SoundManager, jni_constructor_SoundManager, g_AndroidApp->activity->clazz));
        g_SoundManager.m_AcquireAudioFocus       = environment->GetMethodID(jni_class_SoundManager, "acquireAudioFocus", "()Z");
        g_SoundManager.m_ReleaseAudioFocus       = environment->GetMethodID(jni_class_SoundManager, "releaseAudioFocus", "()Z");
        g_SoundManager.m_IsMusicPlaying          = environment->GetMethodID(jni_class_SoundManager, "isMusicPlaying", "()Z");

        environment->DeleteLocalRef(jni_string_SoundManager);
        return (::CheckException(environment) && ::Detach(environment)) ? RESULT_OK : RESULT_INIT_ERROR;
    }

    Result PlatformFinalize()
    {
        JNIEnv* environment = ::Attach();
        environment->DeleteGlobalRef(g_SoundManager.m_SoundManager);
        return (::CheckException(environment) && ::Detach(environment)) ? RESULT_OK : RESULT_FINI_ERROR;
    }

    bool PlatformAcquireAudioFocus()
    {
        return ::CallZ(g_SoundManager.m_AcquireAudioFocus, false);
    }

    bool PlatformReleaseAudioFocus()
    {
        return ::CallZ(g_SoundManager.m_ReleaseAudioFocus, false);
    }

    bool PlatformIsMusicPlaying()
    {
        return ::CallZ(g_SoundManager.m_IsMusicPlaying, false);
    }

    bool PlatformIsPhoneCallActive()
    {
        return g_SoundManager.m_IsPhoneCallActive;
    }
}
