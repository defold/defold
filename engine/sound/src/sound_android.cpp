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
        printf("[BEGIN] CheckException\n");

        assert(environment);
        if ((bool) environment->ExceptionCheck())
        {
            dmLogError("An exception occurred within the JNI environment (%p)", environment);
            environment->ExceptionDescribe();
            environment->ExceptionClear();

            printf("[END]   CheckException\n");
            return false;
        }

        printf("[END]   CheckException\n");
        return true;
    }

    JNIEnv* Attach()
    {
        printf("[BEGIN] Attach\n");

        JNIEnv* environment = NULL;
        g_AndroidApp->activity->vm->AttachCurrentThread(&environment, NULL);

        printf("[END]   Attach\n");
        return environment;
    }

    bool Detach(JNIEnv* environment)
    {
        printf("[BEGIN] Detach\n");

        assert(environment);
        bool result = CheckException(environment);
        g_AndroidApp->activity->vm->DetachCurrentThread();

        printf("[END]   Detach\n");
        return result;
    }

    bool CallZ(jmethodID method, bool _default)
    {
        printf("[BEGIN] CallZ\n");

        JNIEnv* environment = ::Attach();
        bool result = environment->CallObjectMethod(g_AudioManager.m_AudioManager, method);
        if (::CheckException(environment) && ::Detach(environment))
        {
            printf("[END]   CallZ\n");
            return result;
        }

        printf("[END]   CallZ\n");
        return _default;
    }

}

namespace dmSound
{
    Result PlatformInitialize(dmConfigFile::HConfig config,
            const InitializeParams* params)
    {
        printf("[BEGIN] PlatformInitialize\n");
        JNIEnv* environment = ::Attach();

        printf("  Fetching class loader ...\n");
        jclass      jni_class_NativeActivity      = environment->FindClass("android/app/NativeActivity");
        jmethodID   jni_method_getClassLoader     = environment->GetMethodID(jni_class_NativeActivity, "getClassLoader", "()Ljava/lang/ClassLoader;");
        jobject     jni_object_getClassLoader     = environment->CallObjectMethod(g_AndroidApp->activity->clazz, jni_method_getClassLoader);
        jclass      jni_class_ClassLoader         = environment->FindClass("java/lang/ClassLoader");
        jmethodID   jni_method_loadClass          = environment->GetMethodID(jni_class_ClassLoader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");

        printf("  Loading IsMusicPlaying ...\n");
        jstring     jni_string_AudioManager       = environment->NewStringUTF("com.defold.sound.IsMusicPlaying");
        jclass      jni_class_AudioManager        = (jclass) environment->CallObjectMethod(jni_object_getClassLoader, jni_method_loadClass, jni_string_AudioManager);
        jmethodID   jni_constructor_AudioManager  = environment->GetMethodID(jni_class_AudioManager, "<init>", "(Landroid/content/Context;)V");

        printf("  Constructing IsMusicPlaying ...\n");
        g_AudioManager.m_AudioManager = environment->NewGlobalRef(environment->NewObject(jni_class_AudioManager, jni_constructor_AudioManager, g_AndroidApp->activity->clazz));
        g_AudioManager.m_AcquireAudioFocus = environment->GetMethodID(jni_class_AudioManager, "acquireAudioFocus", "()Z");
        g_AudioManager.m_AcquireAudioFocus = environment->GetMethodID(jni_class_AudioManager, "releaseAudioFocus", "()Z");
        g_AudioManager.m_AcquireAudioFocus = environment->GetMethodID(jni_class_AudioManager, "isMusicPlaying", "()Z");

        environment->DeleteLocalRef(jni_string_AudioManager);
        bool result = ::CheckException(environment) && ::Detach(environment);

        printf("[END]   CallZ\n");
        return result ? RESULT_OK : RESULT_INIT_ERROR;
    }

    Result PlatformFinalize()
    {
        printf("[BEGIN] PlatformFinalize\n");

        JNIEnv* environment = ::Attach();
        environment->DeleteGlobalRef(g_AudioManager.m_AudioManager);
        bool result = ::CheckException(environment) && ::Detach(environment);

        printf("[END]   PlatformFinalize\n");
        return result ? RESULT_OK : RESULT_FINI_ERROR;
    }

    bool PlatformAcquireAudioFocus()
    {
        printf("[BEGIN] PlatformAcquireAudioFocus\n");

        bool result = ::CallZ(g_AudioManager.m_AcquireAudioFocus, false);

        printf("[END]   PlatformAcquireAudioFocus\n");
        return result;
    }

    bool PlatformReleaseAudioFocus()
    {
        printf("[BEGIN] PlatformReleaseAudioFocus\n");

        bool result = ::CallZ(g_AudioManager.m_ReleaseAudioFocus, false);

        printf("[END]   PlatformReleaseAudioFocus\n");
        return result;
    }

    bool PlatformIsMusicPlaying()
    {
        printf("[BEGIN] PlatformIsMusicPlaying\n");

        bool result = ::CallZ(g_AudioManager.m_IsMusicPlaying, false);

        printf("[END]   PlatformIsMusicPlaying\n");
        return result;
    }
}
