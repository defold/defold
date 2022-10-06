// Copyright 2020-2022 The Defold Foundation
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

extern struct android_app* __attribute__((weak)) g_AndroidApp;

namespace dmAndroid
{

ThreadAttacher::ThreadAttacher()
: m_Activity(g_AndroidApp->activity)
, m_Env(NULL)
, m_IsAttached(false)
{
    if (m_Activity->vm->GetEnv((void **)&m_Env, JNI_VERSION_1_6) != JNI_OK)
    {
        m_Activity->vm->AttachCurrentThread(&m_Env, 0);
        m_IsAttached = true;
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
    jclass activity_class = env->FindClass("android/app/NativeActivity");
    jmethodID get_class_loader = env->GetMethodID(activity_class,"getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject cls = env->CallObjectMethod(activity, get_class_loader);
    jclass class_loader = env->FindClass("java/lang/ClassLoader");
    jmethodID find_class = env->GetMethodID(class_loader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jstring str_class_name = env->NewStringUTF(class_name);
    jclass klass = (jclass)env->CallObjectMethod(cls, find_class, str_class_name);
    assert(klass);
    env->DeleteLocalRef(str_class_name);
    return klass;
}

jclass LoadClass(JNIEnv* env, const char* class_name)
{
    return LoadClass(env, g_AndroidApp->activity->clazz, class_name);
}

} // namespace

#endif // ANDROID