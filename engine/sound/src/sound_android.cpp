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

#include "sound.h"
#include "sound_private.h"
#include <dlib/log.h>
#include <dmsdk/dlib/android.h>

#include <android_native_app_glue.h>
#include <jni.h>
#include <string.h>

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
        if (method == 0 || g_SoundManager.m_SoundManager == 0)
            return _default;
        bool result = _default;
        dmAndroid::ThreadAttacher thread;
        JNIEnv* environment = thread.GetEnv();
        if (environment != NULL)
        {
            result = environment->CallBooleanMethod(g_SoundManager.m_SoundManager, method);
            if (environment->ExceptionCheck())
            {
                environment->ExceptionClear();
                result = _default;
            }
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
        (void)config;
        (void)params;
        g_SoundManager.m_IsPhoneCallActive = false;

        dmAndroid::ThreadAttacher thread;
        JNIEnv* environment = thread.GetEnv();
        if (environment == NULL)
            return thread.Detach() ? RESULT_OK : RESULT_INIT_ERROR;

        // Optional for embed hosts — missing SoundManager is not fatal.
        jclass jni_class_SoundManager = dmAndroid::LoadClass(environment, "com.defold.sound.SoundManager");
        if (!jni_class_SoundManager)
        {
            dmLogWarning("com.defold.sound.SoundManager not found; phone-call / music-playing hooks disabled.");
            return thread.Detach() ? RESULT_OK : RESULT_INIT_ERROR;
        }

        jmethodID jni_constructor_SoundManager = environment->GetMethodID(jni_class_SoundManager, "<init>", "(Landroid/app/Activity;)V");
        if (environment->ExceptionCheck() || !jni_constructor_SoundManager)
        {
            environment->ExceptionClear();
            environment->DeleteLocalRef(jni_class_SoundManager);
            return thread.Detach() ? RESULT_OK : RESULT_INIT_ERROR;
        }

        struct android_app* app = dmAndroid::GetAndroidApp();
        if (!app || !app->activity)
        {
            environment->DeleteLocalRef(jni_class_SoundManager);
            return thread.Detach() ? RESULT_OK : RESULT_INIT_ERROR;
        }

        jobject instance = environment->NewObject(jni_class_SoundManager, jni_constructor_SoundManager, app->activity->clazz);
        if (environment->ExceptionCheck() || !instance)
        {
            environment->ExceptionClear();
            environment->DeleteLocalRef(jni_class_SoundManager);
            return thread.Detach() ? RESULT_OK : RESULT_INIT_ERROR;
        }

        g_SoundManager.m_SoundManager   = environment->NewGlobalRef(instance);
        g_SoundManager.m_IsMusicPlaying = environment->GetMethodID(jni_class_SoundManager, "isMusicPlaying", "()Z");
        if (environment->ExceptionCheck())
        {
            environment->ExceptionClear();
            g_SoundManager.m_IsMusicPlaying = 0;
        }

        environment->DeleteLocalRef(instance);
        environment->DeleteLocalRef(jni_class_SoundManager);
        return thread.Detach() ? RESULT_OK : RESULT_INIT_ERROR;
    }

    Result PlatformFinalize()
    {
        dmAndroid::ThreadAttacher thread;
        JNIEnv* environment = thread.GetEnv();
        if (environment != NULL && g_SoundManager.m_SoundManager != 0)
        {
            environment->DeleteGlobalRef(g_SoundManager.m_SoundManager);
            g_SoundManager.m_SoundManager = 0;
            g_SoundManager.m_IsMusicPlaying = 0;
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
