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

#include <assert.h>
#include <jni.h>
#include <android_native_app_glue.h>

#include <dlib/log.h>
#include <dlib/dns.h>
#include <c-ares/ares.h>

extern struct android_app* __attribute__((weak)) g_AndroidApp;

namespace dmDNS
{
    bool InitializeAndroid()
    {
        bool result         = true;
        JNIEnv* environment = NULL;
        g_AndroidApp->activity->vm->AttachCurrentThread(&environment, NULL);

        jclass jni_class_NativeActivity     = environment->FindClass("android/app/NativeActivity");
        jclass jni_class_Context            = environment->FindClass("android/content/Context");
        jmethodID jni_method_getClassLoader = environment->GetMethodID(jni_class_NativeActivity, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
        jfieldID jni_service_field_id       = environment->GetStaticFieldID(jni_class_Context, "CONNECTIVITY_SERVICE", "Ljava/lang/String;");
        jobject jni_service_field_str       = environment->GetStaticObjectField(jni_class_Context, jni_service_field_id);
        jobject jni_connectivity_manager    = environment->CallObjectMethod(g_AndroidApp->activity->clazz, jni_method_getClassLoader, (jstring)jni_service_field_str);

        if (jni_connectivity_manager)
        {
            result = ares_library_init_android(jni_connectivity_manager) == RESULT_OK;
        }
        else
        {
            result = false;
        }

        assert(environment != NULL);
        if ((bool)environment->ExceptionCheck())
        {
            dmLogError("An exception occurred within the JNI environment (%p)", environment);
            environment->ExceptionDescribe();
            environment->ExceptionClear();
            result = false;
        }

        g_AndroidApp->activity->vm->DetachCurrentThread();

        return result;
    }
} // namespace dmDNS
