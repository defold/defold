#include <assert.h>
#include <jni.h>
#include <android_native_app_glue.h>

#include <dlib/log.h>
#include <dlib/dns.h>
#include "ares.h"

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
