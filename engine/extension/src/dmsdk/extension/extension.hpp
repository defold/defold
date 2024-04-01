// Generated, do not edit!
// Generated with cwd=/Users/mathiaswesterdahl/work/defold/engine/extension and cmd=../../scripts/dmsdk/gen_sdk.py -i ./sdk_gen.json

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

#ifndef DMSDK_EXTENSION_HPP
#define DMSDK_EXTENSION_HPP

#if !defined(__cplusplus)
   #error "This file is supported in C++ only!"
#endif

#include <stdbool.h>
#include <dmsdk/resource/resource.h>
#include <dmsdk/dlib/configfile.h>
#include <dmsdk/lua/lua.h>
#include <dmsdk/lua/lauxlib.h>

#include <dmsdk/extension/extension.h>

namespace dmExtension {
    /*# result enumeration
     *
     * Result enumeration.
     *
     * @enum
     * @name Result
     * @member RESULT_OK
     * @member RESULT_INIT_ERRO
     */
    enum Result {
        RESULT_OK = 0,
        RESULT_INIT_ERROR = -1,
    };

    /*# event id enumeration
     *
     * Event id enumeration.
     *
     * EVENT_ID_ICONIFYAPP and EVENT_ID_DEICONIFYAPP only available on [icon:osx] [icon:windows] [icon:linux]
     *
     * @enum
     * @name EventID
     * @member EVENT_ID_ACTIVATEAPP
     * @member EVENT_ID_DEACTIVATEAPP
     * @member EVENT_ID_ICONIFYAPP
     * @member EVENT_ID_DEICONIFYAPP
     *
     */
    enum EventID {
        EVENT_ID_ACTIVATEAPP,
        EVENT_ID_DEACTIVATEAPP,
        EVENT_ID_ICONIFYAPP,
        EVENT_ID_DEICONIFYAPP,
    };

    /*# extra callback enumeration
     *
     * Extra callback enumeration for RegisterCallback function.
     *
     * @enum
     * @name CallbackType
     * @member CALLBACK_PRE_RENDER
     * @member CALLBACK_POST_RENDER
     *
     */
    enum CallbackType {
        CALLBACK_PRE_RENDER,
        CALLBACK_POST_RENDER,
    };

    // no documentation found
    typedef ExtensionAppParams AppParams;

    // no documentation found
    typedef ExtensionParams Params;

    // no documentation found
    typedef ExtensionEvent Event;

    /*# Extra extension callback typedef
     *
     * Callback typedef for functions passed to RegisterCallback().
     *
     * @typedef
     * @name FCallback
     * @param params [type:Params]
     * @return [type:Result
     */
    typedef FExtensionCallback FCallback;

    // no documentation found
    bool RegisterCallback(CallbackType callback_type,FCallback func);

    /*# Register application delegate
     *
     * Register an iOS application delegate to the engine. Multiple delegates are supported (Max 32)
     *
     * @note Note that the delegate needs to be registered before the UIApplicationMain in order to
     * handle any earlier callbacks.
     *
     * This function is only available on iOS. [icon:ios]
     *
     * @name RegisteriOSUIApplicationDelegate
     * @param delegate [type:id<UIApplicationDelegate>] An UIApplicationDelegate, see: https://developer.apple.com/documentation/uikit/uiapplicationdelegate?language=objc
     *
     * @examples
     * ```objective-c
     *
     * // myextension_ios.mm
     *
     * id<UIApplicationDelegate> g_MyApplicationDelegate;
     *
     * @interface MyApplicationDelegate : NSObject <UIApplicationDelegate>
     *
     * - (void) applicationDidBecomeActive:(UIApplication *) application;
     *
     * @end
     *
     * @implementation MyApplicationDelegate
     *
     * - (void) applicationDidBecomeActive:(UIApplication *) application {
     *     dmLogWarning("applicationDidBecomeActive - MyAppDelegate");
     * }
     *
     * @end
     *
     * struct MyAppDelegateRegister
     * {
     *     MyApplicationDelegate* m_Delegate;
     *     MyAppDelegateRegister() {
     *         m_Delegate = [[FacebookAppDelegate alloc] init];
     *         Extension::RegisteriOSUIApplicationDelegate(m_Delegate);
     *     }
     *     ~MyAppDelegateRegister() {
     *         Extension::UnregisteriOSUIApplicationDelegate(m_Delegate);
     *         [m_Delegate release];
     *     }
     * };
     *
     * MyAppDelegateRegister g_FacebookDelegateRegister;
     * ``
     */
    void RegisteriOSUIApplicationDelegate(void * delegate);

    /*# Unregister an application delegate
     *
     * Deregister a previously registered iOS application delegate
     *
     * This function is only available on iOS. [icon:ios]
     *
     * @name UnregisteriOSUIApplicationDelegate
     * @param delegate an id<UIApplicationDelegate
     */
    void UnregisteriOSUIApplicationDelegate(void * delegate);


} // namespace dmExtension

#endif // #define DMSDK_EXTENSION_HPP

