#include "sound.h"
#include "sound_private.h"

#include <jni.h>
#include <cstdio>

#include <android_native_app_glue.h>
#include <dlib/log.h>

extern struct android_app* g_AndroidApp;

struct AudioManager
{

    AudioManager()
    {
        memset(this, 0x0, sizeof(struct AudioManager));
    }

    jobject m_AudioManager;
    jmethodID m_AcquireAudioFocus;
    jmethodID m_IsMusicPlaying;
    jmethodID m_ReleaseAudioFocus;

};

struct AudioManager g_AudioManager;

namespace
{

    bool CheckException(JNIEnv* environment)
    {
        assert(environment);
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
        assert(environment);
        bool result = CheckException(environment);
        g_AndroidApp->activity->vm->DetachCurrentThread();
        return result;
    }

    bool CallZ(jmethodID method, bool _default)
    {
        JNIEnv* environment = ::Attach();

        bool result = environment->CallObjectMethod(g_AudioManager.m_AudioManager, method);
        if (::CheckException(environment) && ::Detach(environment))
        {
            return result;
        }
        return _default;
    }

}

namespace dmSound
{
    Result PlatformInitialize(dmConfigFile::HConfig config,
            const InitializeParams* params)
    {
        JNIEnv* environment = ::Attach();

        jclass      jni_class_NativeActivity      = environment->FindClass("android/app/NativeActivity");
        jmethodID   jni_method_getClassLoader     = environment->GetMethodID(jni_class_NativeActivity, "getClassLoader", "()Ljava/lang/ClassLoader;");
        jobject     jni_object_getClassLoader     = environment->CallObjectMethod(g_AndroidApp->activity->clazz, jni_method_getClassLoader);
        jclass      jni_class_ClassLoader         = environment->FindClass("java/lang/ClassLoader");
        jmethodID   jni_method_loadClass          = environment->GetMethodID(jni_class_ClassLoader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");

        jstring     jni_string_AudioManager       = environment->NewStringUTF("com.defold.sound.IsMusicPlaying");
        jclass      jni_class_AudioManager        = (jclass) environment->CallObjectMethod(jni_object_getClassLoader, jni_method_loadClass, jni_string_AudioManager);
        jmethodID   jni_constructor_AudioManager  = environment->GetMethodID(jni_class_AudioManager, "<init>", "(Landroid/content/Context;)V");

        g_AudioManager.m_AudioManager = environment->NewGlobalRef(environment->NewObject(jni_class_AudioManager, jni_constructor_AudioManager, g_AndroidApp->activity->clazz));
        g_AudioManager.m_AcquireAudioFocus = environment->GetMethodID(jni_class_AudioManager, "acquireAudioFocus", "()Z");
        g_AudioManager.m_ReleaseAudioFocus = environment->GetMethodID(jni_class_AudioManager, "releaseAudioFocus", "()Z");
        g_AudioManager.m_IsMusicPlaying = environment->GetMethodID(jni_class_AudioManager, "isMusicPlaying", "()Z");

        environment->DeleteLocalRef(jni_string_AudioManager);
        bool result = ::CheckException(environment) && ::Detach(environment);
        return result ? RESULT_OK : RESULT_INIT_ERROR;
    }

    Result PlatformFinalize()
    {
        JNIEnv* environment = ::Attach();
        environment->DeleteGlobalRef(g_AudioManager.m_AudioManager);
        bool result = ::CheckException(environment) && ::Detach(environment);
        return result ? RESULT_OK : RESULT_FINI_ERROR;
    }

    bool PlatformAcquireAudioFocus()
    {
        bool result = ::CallZ(g_AudioManager.m_AcquireAudioFocus, false);
        return result;
    }

    bool PlatformReleaseAudioFocus()
    {
        bool result = ::CallZ(g_AudioManager.m_ReleaseAudioFocus, false);
        return result;
    }

    bool PlatformIsMusicPlaying()
    {
        bool result = ::CallZ(g_AudioManager.m_IsMusicPlaying, false);
        return result;
    }
}
