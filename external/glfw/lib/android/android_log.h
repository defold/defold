// Copyright 2020-2023 The Defold Foundation
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

#ifndef _android_log_h_
#define _android_log_h_

#include <android/log.h>
#include <assert.h>

#define LOGV(...) ((void)__android_log_print(ANDROID_LOG_VERBOSE, "glfw-android", __VA_ARGS__))
#define LOGD(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, "glfw-android", __VA_ARGS__))
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "glfw-android", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "glfw-android", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "glfw-android", __VA_ARGS__))
#define LOGF(...) ((void)__android_log_print(ANDROID_LOG_FATAL, "glfw-android", __VA_ARGS__))

#define CHECK_EGL_ERROR \
{\
    EGLint error;\
    error = eglGetError();\
    switch (error)\
    {\
    case EGL_SUCCESS:\
        break;\
    case EGL_NOT_INITIALIZED:\
        LOGE("EGL_NOT_INITIALIZED");\
        break;\
    case EGL_BAD_ACCESS:\
        LOGE("EGL_BAD_ACCESS");\
        break;\
    case EGL_BAD_ALLOC:\
        LOGE("EGL_BAD_ALLOC");\
        break;\
    case EGL_BAD_ATTRIBUTE:\
        LOGE("EGL_BAD_ATTRIBUTE");\
        break;\
    case EGL_BAD_CONTEXT:\
        LOGE("EGL_BAD_CONTEXT");\
        break;\
    case EGL_BAD_CONFIG:\
        LOGE("EGL_BAD_CONFIG");\
        break;\
    case EGL_BAD_CURRENT_SURFACE:\
        LOGE("EGL_BAD_CURRENT_SURFACE");\
        break;\
    case EGL_BAD_DISPLAY:\
        LOGE("EGL_BAD_DISPLAY");\
        break;\
    case EGL_BAD_SURFACE:\
        LOGE("EGL_BAD_SURFACE");\
        break;\
    case EGL_BAD_MATCH:\
        LOGE("EGL_BAD_MATCH");\
        break;\
    case EGL_BAD_PARAMETER:\
        LOGE("EGL_BAD_PARAMETER");\
        break;\
    case EGL_BAD_NATIVE_PIXMAP:\
        LOGE("EGL_BAD_NATIVE_PIXMAP");\
        break;\
    case EGL_BAD_NATIVE_WINDOW:\
        LOGE("EGL_BAD_NATIVE_WINDOW");\
        break;\
    case EGL_CONTEXT_LOST:\
        LOGE("EGL_CONTEXT_LOST");\
        break;\
    default:\
        LOGE("unknown egl error: %d", error);\
    }\
    assert(error == EGL_SUCCESS);\
}

#endif // _android_log_h_
