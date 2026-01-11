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

#include <scripts/script_window.h>
#include <dlib/log.h>
#include <dmsdk/dlib/android.h>

#include <android_native_app_glue.h>
#include <jni.h>

struct Window
{

    Window()
    {
        memset(this, 0x0, sizeof(struct Window));
        m_Initialized = false;
    }

    jobject     m_Window;
    jmethodID   m_EnableScreenDimming;
    jmethodID   m_DisableScreenDimming;
    jmethodID   m_IsScreenDimmingEnabled;
    bool        m_Initialized;

};

struct Window g_Window;

namespace
{
    void Initialize()
    {
        dmAndroid::ThreadAttacher thread;
        JNIEnv* environment = thread.GetEnv();
        if (environment != NULL)
        {
            jclass      jni_class_NativeActivity  = environment->FindClass("android/app/NativeActivity");
            jmethodID   jni_method_getClassLoader = environment->GetMethodID(jni_class_NativeActivity, "getClassLoader", "()Ljava/lang/ClassLoader;");
            jobject     jni_object_getClassLoader = environment->CallObjectMethod(thread.GetActivity()->clazz, jni_method_getClassLoader);
            jclass      jni_class_ClassLoader     = environment->FindClass("java/lang/ClassLoader");
            jmethodID   jni_method_loadClass      = environment->GetMethodID(jni_class_ClassLoader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");

            jstring     jni_string_Window         = environment->NewStringUTF("com.defold.window.WindowJNI");
            jclass      jni_class_Window          = (jclass) environment->CallObjectMethod(jni_object_getClassLoader, jni_method_loadClass, jni_string_Window);
            jmethodID   jni_constructor_Window    = environment->GetMethodID(jni_class_Window, "<init>", "(Landroid/app/Activity;)V");

            g_Window.m_Window                     = environment->NewGlobalRef(environment->NewObject(jni_class_Window, jni_constructor_Window, thread.GetActivity()->clazz));
            g_Window.m_EnableScreenDimming        = environment->GetMethodID(jni_class_Window, "enableScreenDimming", "()V");
            g_Window.m_DisableScreenDimming       = environment->GetMethodID(jni_class_Window, "disableScreenDimming", "()V");
            g_Window.m_IsScreenDimmingEnabled     = environment->GetMethodID(jni_class_Window, "isScreenDimmingEnabled", "()Z");

            g_Window.m_Initialized                = true;

            environment->DeleteLocalRef(jni_class_Window);
            environment->DeleteLocalRef(jni_string_Window);
            environment->DeleteLocalRef(jni_class_ClassLoader);
            environment->DeleteLocalRef(jni_object_getClassLoader);
            environment->DeleteLocalRef(jni_class_NativeActivity);
        }
        else
        {
            dmLogError("Unable to attach JNI environment");
        }
    }

};

namespace dmGameSystem
{

    void PlatformSetDimMode(DimMode mode)
    {
        if (!g_Window.m_Initialized)
        {
            ::Initialize();
        }

        if (g_Window.m_Initialized)
        {
            dmAndroid::ThreadAttacher thread;
            JNIEnv* environment = thread.GetEnv();
            if (environment != NULL)
            {
                if (mode == DIMMING_ON)
                {
                    environment->CallVoidMethod(g_Window.m_Window, g_Window.m_EnableScreenDimming);
                }
                else if (mode == DIMMING_OFF)
                {
                    environment->CallVoidMethod(g_Window.m_Window, g_Window.m_DisableScreenDimming);
                }
            }
            else
            {
                dmLogError("Unable to attach JNI environment");
            }
        }
        else
        {
            dmLogError("Unable to set dimming, JNI was not initialized");
        }
    }

    DimMode PlatformGetDimMode()
    {
        if (!g_Window.m_Initialized)
        {
            ::Initialize();
        }

        if (g_Window.m_Initialized)
        {
            dmAndroid::ThreadAttacher thread;
            JNIEnv* environment = thread.GetEnv();
            if (environment != NULL)
            {
                bool dimming = (bool) environment->CallBooleanMethod(g_Window.m_Window, g_Window.m_IsScreenDimmingEnabled);
                return dimming ? DIMMING_ON : DIMMING_OFF;
            }
            else
            {
                dmLogError("Unable to attach JNI environment");
            }
        }
        else
        {
            dmLogError("Unable to set dimming, JNI was not initialized");
        }

        return DIMMING_UNKNOWN;
    }

}
