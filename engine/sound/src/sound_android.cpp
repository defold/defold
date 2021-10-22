// Copyright 2020 The Defold Foundation
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

#include "sound.h"
#include "sound_private.h"
#include <dlib/log.h>
#include <dmsdk/dlib/android.h>

#include <android_native_app_glue.h>
#include <jni.h>
#include <assert.h>

extern struct android_app* g_AndroidApp;

struct SoundManager
{

    SoundManager()
    {
        memset(this, 0x0, sizeof(struct SoundManager));
    }

    jobject     m_SoundManager;
    jmethodID   m_IsMusicPlaying;
    bool        m_IsPhoneCallActive;
};

struct SoundManager g_SoundManager;

namespace
{
    bool CallZ(jmethodID method, bool _default)
    {
        assert(method != 0);
        bool result = _default;
        dmAndroid::ThreadAttacher thread;
        JNIEnv* environment = thread.GetEnv();
        if (environment != NULL)
        {
            result = environment->CallBooleanMethod(g_SoundManager.m_SoundManager, method);
        }
        return thread.Detach() ? result : _default;
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
    Result PlatformInitialize(dmConfigFile::HConfig config, const InitializeParams* params)
    {
        g_SoundManager.m_IsPhoneCallActive = false; // It will get updated by the call to the construct

        dmAndroid::ThreadAttacher thread;
        JNIEnv* environment = thread.GetEnv();
        if (environment != NULL)
        {
            jclass      jni_class_NativeActivity     = environment->FindClass("android/app/NativeActivity");
            jmethodID   jni_method_getClassLoader    = environment->GetMethodID(jni_class_NativeActivity, "getClassLoader", "()Ljava/lang/ClassLoader;");
            jobject     jni_object_getClassLoader    = environment->CallObjectMethod(g_AndroidApp->activity->clazz, jni_method_getClassLoader);
            jclass      jni_class_ClassLoader        = environment->FindClass("java/lang/ClassLoader");
            jmethodID   jni_method_loadClass         = environment->GetMethodID(jni_class_ClassLoader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");

            jstring     jni_string_SoundManager      = environment->NewStringUTF("com.defold.sound.SoundManager");
            jclass      jni_class_SoundManager       = (jclass) environment->CallObjectMethod(jni_object_getClassLoader, jni_method_loadClass, jni_string_SoundManager);
            jmethodID   jni_constructor_SoundManager = environment->GetMethodID(jni_class_SoundManager, "<init>", "(Landroid/app/Activity;)V");

            g_SoundManager.m_SoundManager            = environment->NewGlobalRef(environment->NewObject(jni_class_SoundManager, jni_constructor_SoundManager, g_AndroidApp->activity->clazz));
            g_SoundManager.m_IsMusicPlaying          = environment->GetMethodID(jni_class_SoundManager, "isMusicPlaying", "()Z");

            environment->DeleteLocalRef(jni_string_SoundManager);
        }
        return thread.Detach() ? RESULT_OK : RESULT_INIT_ERROR;
    }

    Result PlatformFinalize()
    {
        dmAndroid::ThreadAttacher thread;
        JNIEnv* environment = thread.GetEnv();
        if (environment != NULL)
        {
            environment->DeleteGlobalRef(g_SoundManager.m_SoundManager);
        }
        return thread.Detach() ? RESULT_OK : RESULT_FINI_ERROR;
    }

    bool PlatformIsMusicPlaying(bool is_device_started, bool has_window_focus)
    {
        // DEF-3138 If you queue silent audio to the device it will still be registered by Android
        // as music is playing.
        // We therefore only ask the platform if music is playing if we have either not recevied our
        // window focus or if we have not started the device playback
        if (has_window_focus && is_device_started)
        {
            return false;
        }
        return ::CallZ(g_SoundManager.m_IsMusicPlaying, false);
    }

    bool PlatformIsAudioInterrupted()
    {
        return g_SoundManager.m_IsPhoneCallActive;
    }
}
