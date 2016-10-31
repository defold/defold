#include <scripts/script_window.h>

#include <android_native_app_glue.h>
#include <dlib/log.h>
#include <assert.h>
#include <jni.h>

extern struct android_app* g_AndroidApp;

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

    void Initialize()
    {
        JNIEnv* environment = ::Attach();
        if (environment != NULL)
        {
            jclass      jni_class_NativeActivity  = environment->FindClass("android/app/NativeActivity");
            jmethodID   jni_method_getClassLoader = environment->GetMethodID(jni_class_NativeActivity, "getClassLoader", "()Ljava/lang/ClassLoader;");
            jobject     jni_object_getClassLoader = environment->CallObjectMethod(g_AndroidApp->activity->clazz, jni_method_getClassLoader);
            jclass      jni_class_ClassLoader     = environment->FindClass("java/lang/ClassLoader");
            jmethodID   jni_method_loadClass      = environment->GetMethodID(jni_class_ClassLoader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");

            jstring     jni_string_Window         = environment->NewStringUTF("com.defold.window.WindowJNI");
            jclass      jni_class_Window          = (jclass) environment->CallObjectMethod(jni_object_getClassLoader, jni_method_loadClass, jni_string_Window);
            jmethodID   jni_constructor_Window    = environment->GetMethodID(jni_class_Window, "<init>", "(Landroid/app/Activity;)V");

            g_Window.m_Window                     = environment->NewGlobalRef(environment->NewObject(jni_class_Window, jni_constructor_Window, g_AndroidApp->activity->clazz));
            g_Window.m_EnableScreenDimming        = environment->GetMethodID(jni_class_Window, "enableScreenDimming", "()V");
            g_Window.m_DisableScreenDimming       = environment->GetMethodID(jni_class_Window, "disableScreenDimming", "()V");
            g_Window.m_IsScreenDimmingEnabled     = environment->GetMethodID(jni_class_Window, "isScreenDimmingEnabled", "()Z");

            g_Window.m_Initialized                = true;

            environment->DeleteLocalRef(jni_string_Window);
            (void) ::Detach(environment);
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
            JNIEnv* environment = ::Attach();
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

                ::Detach(environment);
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
            JNIEnv* environment = ::Attach();
            if (environment != NULL)
            {
                bool dimming = (bool) environment->CallBooleanMethod(g_Window.m_Window, g_Window.m_IsScreenDimmingEnabled);
                (void) ::Detach(environment);
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
