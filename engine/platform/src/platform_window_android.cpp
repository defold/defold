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

#include "native/native.h"

#include "native/native_handles.h"

#include <dmsdk/dlib/android.h>

#include <dlib/array.h>
#include <dlib/math.h>

#include "platform_window_android.h"

extern "C" {
    extern void dmNativeAndroidSetInputMethod(int);
    extern void dmNativeAndroidSetFullscreenParameters(int, int);
    extern void dmNativeAndroidSetJoystickLayoutLegacy(int, int);
    extern void dmNativeAndroidInitializeJoystickAxes(int, const float*, int);
}

namespace dmPlatform
{
    void SetAndroidJoystickLayoutLegacy(uint32_t joystick_index, bool legacy)
    {
        dmNativeAndroidSetJoystickLayoutLegacy(joystick_index, legacy);
    }

    void InitializeAndroidJoystickAxes(uint32_t joystick_index, const float* axes, uint32_t num_axes)
    {
        dmNativeAndroidInitializeJoystickAxes(joystick_index, axes, num_axes);
    }

    extern "C" int dmNativeAndroidGetSafeAreaInsets(int* left, int* top, int* right, int* bottom);

    int32_t AndroidVerifySurface(HWindow window)
    {
        return dmNativeAndroidVerifySurface();
    }

    void AndroidBeginFrame(HWindow window)
    {
        dmNativeAndroidBeginFrame();
    }

    EGLContext GetAndroidEGLContext()
    {
        return dmNativeGetAndroidEGLContext();
    }

    EGLSurface GetAndroidEGLSurface()
    {
        return dmNativeGetAndroidEGLSurface();
    }

    JavaVM* GetAndroidJavaVM()
    {
        return dmNativeGetAndroidJavaVM();
    }

    jobject GetAndroidActivity()
    {
        return dmNativeGetAndroidActivity();
    }

    android_app* GetAndroidApp()
    {
        return dmAndroid::GetAndroidApp();
    }

    ANativeWindow* AcquireAndroidWindow()
    {
        return dmNativeAcquireAndroidWindow();
    }

    ANativeWindow* WaitForAndroidWindow()
    {
        return dmNativeWaitForAndroidWindow();
    }

    bool IsAndroidWindowCurrent(ANativeWindow* window)
    {
        return dmNativeAndroidIsWindowCurrent(window) != 0;
    }

    void ReleaseAndroidWindow(ANativeWindow* window)
    {
        dmNativeReleaseAndroidWindow(window);
    }

    void SetAndroidInputMethod(bool use_hidden_inputfield)
    {
        dmNativeAndroidSetInputMethod((int)use_hidden_inputfield);
    }

    void SetAndroidFullscreenParameters(bool immersive_mode, bool display_cutout)
    {
        dmNativeAndroidSetFullscreenParameters((int)immersive_mode, (int)display_cutout);
    }

    bool GetSafeAreaAndroid(HWindow window, WindowSafeArea* out)
    {
        const int32_t window_width = (int32_t) GetWindowWidth(window);
        const int32_t window_height = (int32_t) GetWindowHeight(window);
        if (window_width <= 0 || window_height <= 0)
        {
            return false;
        }

        int32_t inset_left = 0;
        int32_t inset_top = 0;
        int32_t inset_right = 0;
        int32_t inset_bottom = 0;

        if (dmNativeAndroidGetSafeAreaInsets(&inset_left, &inset_top, &inset_right, &inset_bottom))
        {
            out->m_X = inset_left;
            out->m_Y = inset_bottom;
            out->m_Width = (uint32_t) dmMath::Max(0, window_width - inset_left - inset_right);
            out->m_Height = (uint32_t) dmMath::Max(0, window_height - inset_top - inset_bottom);
            out->m_InsetLeft = inset_left;
            out->m_InsetTop = inset_top;
            out->m_InsetRight = inset_right;
            out->m_InsetBottom = inset_bottom;
            return true;
        }

        android_app* app = dmAndroid::GetAndroidApp();
        // ARect content depends on window settings we swt in DefoldActivity.java
        const ARect rect = app->contentRect;
        const int32_t rect_width = rect.right - rect.left;
        const int32_t rect_height = rect.bottom - rect.top;
        if (rect_width <= 0 || rect_height <= 0)
        {
            return false;
        }

        inset_left = rect.left;
        inset_right = window_width - rect.right;
        inset_top = rect.top;
        inset_bottom = window_height - rect.bottom;

        out->m_X = rect.left;
        out->m_Y = window_height - rect.bottom;
        out->m_Width = (uint32_t) rect_width;
        out->m_Height = (uint32_t) rect_height;
        out->m_InsetLeft = inset_left;
        out->m_InsetTop = inset_top;
        out->m_InsetRight = inset_right;
        out->m_InsetBottom = inset_bottom;

        return true;
    }

    void GetReservedAreas(HWindow window, dmArray<WindowReservedArea>& areas)
    {
        areas.SetSize(0);
        dmAndroid::ThreadAttacher thread;
        JNIEnv* env = thread.GetEnv();
        if (!env)
            return;

        jobject activity = thread.GetActivity()->clazz;
        jclass activity_class = env->GetObjectClass(activity);
        jmethodID method = env->GetMethodID(activity_class, "getReservedAreas", "()[I");
        env->DeleteLocalRef(activity_class);
        if (!method)
            return;

        // One Java call retains one immutable snapshot, including its coordinate
        // space, even if the UI thread publishes new insets while we copy it.
        jintArray snapshot = (jintArray)env->CallObjectMethod(activity, method);
        if (!snapshot)
            return;

        const jsize length = env->GetArrayLength(snapshot);
        if (length >= 2)
        {
            jint* values = env->GetIntArrayElements(snapshot, 0);
            if (values)
            {
                const uint32_t count = (length - 2) / 4;
                if (areas.Capacity() < count)
                    areas.SetCapacity(count);
                for (uint32_t i = 0; i < count; ++i)
                {
                    const jint* rect = values + 2 + i * 4;
                    WindowReservedArea area;
                    if (ConvertReservedArea(rect[0], rect[1], rect[2], rect[3], values[0], values[1],
                                            GetWindowWidth(window), GetWindowHeight(window), &area))
                        areas.Push(area);
                }
                env->ReleaseIntArrayElements(snapshot, values, JNI_ABORT);
            }
        }
        env->DeleteLocalRef(snapshot);
    }
}
