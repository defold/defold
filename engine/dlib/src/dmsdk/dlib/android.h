// Copyright 2020-2024 The Defold Foundation
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

#ifndef DMSDK_ANDROID_H
#define DMSDK_ANDROID_H

#if defined(ANDROID)

#include <jni.h>
#include <android_native_app_glue.h> // For ANativeActivity

/*# SDK Android API documentation
 * Android utility functions
 *
 * @document
 * @name Android
 * @namespace dmAndroid
 * @path engine/dlib/src/dmsdk/dlib/android.h
 */

namespace dmAndroid {

/*#
 * Struct attaching the JNI environment. Detaches the
 * @class
 * @name ThreadAttacher
 */
class ThreadAttacher
{
    ANativeActivity*    m_Activity;
    JNIEnv*             m_Env;
    bool                m_IsAttached;
public:

    /*# constructor
     *
     * @name ThreadAttacher
     * @examples
     *
     * ```cpp
     * {
     *   ThreadAttacher thread;
     *   SomeFunction( thread.GetEnv() );
     *   // Automatically detaches
     * }
     * ```
     *
     * ```cpp
     * {
     *   ThreadAttacher thread;
     *   JNIEnv* env = thread.GetEnv();
     *   if (!env)
     *     return;
     *   ...
     * }
     * ```
     */
    ThreadAttacher();

    ~ThreadAttacher()
    {
        Detach();
    }

    /*# Detaches the jni environment
     *
     * @name Detach
     * @return ok [type:bool] true if there was no java exceptions. False if there was an exception.
     */
    bool Detach();

    /*# Is the environment attached and valid?
     *
     * @name IsAttached
     * @return isattached [type:bool] true if the environment is valid
     *
     * @examples
     *
     * ```cpp
     * Result SomeFunc() {
     *   ThreadAttacher thread;
     *   JNIEnv* env = thread.GetEnv();
     *   if (!env)
     *     return RESULT_ATTACH_FAILED;
     *   ... calls using jni
     *   return thread.Detach() ? RESULT_OK : RESULT_JNI_CALLS_FAILED;
     * }
     * ```
     */
    bool IsAttached()
    {
        return m_IsAttached;
    }

    /*# Gets the JNI environment
     *
     * @name GetEnv
     * @return env [type:JNIENV*] the attached environment
     */
    JNIEnv* GetEnv()
    {
        return m_Env;
    }

    /*# Gets the app native activity
     *
     * @name GetActivity
     * @return activity [type:ANativeActivity*] the app native activity
     */
    ANativeActivity* GetActivity()
    {
        return m_Activity;
    }
};

/*# Load a class
 *
 * @name LoadClass
 * @param env [type:JNIEnv*]
 * @param class_name [type:const char*]
 * @return class [type:jclass] the activity class loader
 */
jclass LoadClass(JNIEnv* env, const char* class_name);

/*# Load a class
 *
 * @name LoadClass
 * @param env [type:JNIEnv*]
 * @param activity [type:jobject]
 * @param class_name [type:const char*]
 * @return class [type:jclass] the activity class loader
 */
jclass LoadClass(JNIEnv* env, jobject activity, const char* class_name);

/*# OnActivityResult callback typedef
 *
 * Activity result callback function type. Monitors events from the main activity.
 * Used with RegisterOnActivityResultListener() and UnregisterOnActivityResultListener()
 *
 * @typedef
 * @name OnActivityResult
 * @param env [type:JNIEnv*]
 * @param activity [type:jobject]
 * @param request_code [type:int32_t]
 * @param result_code [type:int32_t]
 * @param result [type:void*]
 */
typedef void (*OnActivityResult)(JNIEnv* env, jobject activity, int32_t request_code, int32_t result_code, void* result);

/*# register Android activity result callback
 *
 * Registers an activity result callback. Multiple listeners are allowed.
 *
 * @note [icon:android] Only available on Android
 * @name RegisterOnActivityResultListener
 * @param [type:dmAndroid::OnActivityResult] listener
 */
void RegisterOnActivityResultListener(OnActivityResult listener);

/*# unregister Android activity result callback
 *
 * Unregisters an activity result callback
 *
 * @note [icon:android] Only available on Android
 * @name UnregisterOnActivityResultListener
 * @param [type:dmAndroid::OnActivityResult] listener
 */
void UnregisterOnActivityResultListener(OnActivityResult listener);

/*# OnActivityCreate callback typedef
 *
 * onCreate callback function type.
 * Used with RegisterOnActivityCreateListener() and UnregisterOnActivityCreateListener()
 *
 * @typedef
 * @name OnActivityCreate
 * @param env [type:JNIEnv*]
 * @param activity [type:jobject]
 */
typedef void (*OnActivityCreate)(JNIEnv* env, jobject activity);

/*# register Android onCreate callback
 *
 * Registers an onCreate callback. Multiple listeners are allowed.
 *
 * @note [icon:android] Only available on Android
 * @name RegisterOnActivityCreateListener
 * @param [type:dmAndroid::OnActivityCreate] listener
 */
void RegisterOnActivityCreateListener(OnActivityCreate listener);

/*# unregister Android onCreate callback
 *
 * Unregisters an onCreate callback
 *
 * @note [icon:android] Only available on Android
 * @name UnregisterOnActivityCreateListener
 * @param [type:dmAndroid::OnActivityCreate] listener
 */
void UnregisterOnActivityCreateListener(OnActivityCreate listener);

} // namespace dmAndroid

#endif // ANDROID

#endif // DMSDK_ANDROID_H
