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
 * @struct
 * @name ThreadAttacher
 * @member m_Func [type: dmGraphics::CompareFunc] the compare function
 * @member m_OpSFail [type: dmGraphics::StencilOp] the stencil fail operation
 * @member m_OpDPFail [type: dmGraphics::StencilOp] the depth pass fail operation
 * @member m_OpDPPass [type: dmGraphics::StencilOp] the depth pass pass operation
 * @member m_Ref [type: uint8_t]
 * @member m_RefMask [type: uint8_t]
 * @member m_BufferMask [type: uint8_t]
 * @member m_ColorBufferMask [type: uint8_t:4]
 * @member m_ClearBuffer [type: uint8_t:1]
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
     *   ThreadAttacher jni;
     *   SomeFunction( jni.GetEnv() );
     *   // Automatically detaches
     * }
     * ```
     *
     * ```cpp
     * {
     *   ThreadAttacher jni;
     *   JNIEnv* env = jni.GetEnv();
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

    /*# Is the environment attached and valid?
     *
     * @name Detach
     * @return ok [type:bool] true if there was no java exceptions. False if there was an exception.
     */
    bool Detach();

    /*# Is the environment attached and valid?
     *
     * @name IsAttached
     * @return isattached [type:bool] true if the environment is valid
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
 * @return class [type:jclass] the activity class loader
 */
jclass LoadClass(JNIEnv* env, const char* class_name);


} // namespace dmAndroid

#endif // ANDROID

#endif // DMSDK_ANDROID_H
