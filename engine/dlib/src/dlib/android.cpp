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

#if defined(ANDROID)

#include <assert.h>
#include <dmsdk/dlib/android.h>
#include <dmsdk/dlib/log.h>

namespace dmAndroid
{

static struct android_app* g_AndroidApp = 0;

void SetAndroidApp(struct android_app* app)
{
    g_AndroidApp = app;
}

struct android_app* GetAndroidApp()
{
    return g_AndroidApp;
}

ThreadAttacher::ThreadAttacher()
: m_Activity(NULL)
, m_Env(NULL)
, m_IsAttached(false)
{
    struct android_app* app = dmAndroid::GetAndroidApp();
    if (app)
    {
        m_Activity = app->activity;

        // Only Attach (and later Detach) when this native thread is not already
        // owned by ART — e.g. Compose/UI calling into the engine. Detaching an
        // ART-attached thread aborts: "attempting to detach while still running code".
        jint status = m_Activity->vm->GetEnv((void **)&m_Env, JNI_VERSION_1_6);
        if (status == JNI_EDETACHED)
        {
            if (m_Activity->vm->AttachCurrentThread(&m_Env, 0) == JNI_OK)
                m_IsAttached = true;
            else
                m_Env = NULL;
        }
        else if (status != JNI_OK)
        {
            m_Env = NULL;
        }
    }
}

bool ThreadAttacher::Detach()
{
    bool ok = true;
    if (m_IsAttached)
    {
        if (m_Env->ExceptionCheck())
        {
            m_Env->ExceptionDescribe();
            ok = false;
        }
        m_Env->ExceptionClear();
        m_Activity->vm->DetachCurrentThread();
        m_IsAttached = false;
    }
    return ok;
}

jclass LoadClass(JNIEnv* env, jobject activity, const char* class_name)
{
    // Use the runtime Activity/Context class — embed hosts are not NativeActivity.
    jclass activity_class = env->GetObjectClass(activity);
    jmethodID get_class_loader = env->GetMethodID(activity_class, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject cls = get_class_loader ? env->CallObjectMethod(activity, get_class_loader) : 0;
    env->DeleteLocalRef(activity_class);

    jclass klass = 0;
    if (cls)
    {
        jclass class_loader = env->FindClass("java/lang/ClassLoader");
        jmethodID find_class = env->GetMethodID(class_loader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
        jstring str_class_name = env->NewStringUTF(class_name);
        klass = (jclass)env->CallObjectMethod(cls, find_class, str_class_name);
        if (env->ExceptionCheck())
        {
            // Missing optional helpers (Sound, VkQuality, …) are expected on
            // embed hosts — clear without dumping a stack to logcat.
            env->ExceptionClear();
            klass = 0;
        }
        env->DeleteLocalRef(str_class_name);
        env->DeleteLocalRef(class_loader);
        env->DeleteLocalRef(cls);
    }

    if (!klass)
    {
        // Callers must handle null (optional classes). Do not abort.
        dmLogWarning("dmAndroid::LoadClass: '%s' not found", class_name ? class_name : "(null)");
        return 0;
    }
    return klass;
}

jclass LoadClass(JNIEnv* env, const char* class_name)
{
    struct android_app* app = dmAndroid::GetAndroidApp();
    assert(app != 0);

    return LoadClass(env, app->activity->clazz, class_name);
}

} // namespace

#endif // ANDROID
