// Generated, do not edit!
// Generated with cwd=/Users/mawe/work/defold/engine/extension and cmd=/Users/mawe/work/defold/scripts/dmsdk/gen_sdk.py -i /Users/mawe/work/defold/engine/extension/sdk_gen.json

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

#ifndef DMSDK_EXTENSION_GEN_HPP
#define DMSDK_EXTENSION_GEN_HPP

#if !defined(__cplusplus)
   #error "This file is supported in C++ only!"
#endif


/*# Extension_gen
 *
 * description
 *
 * @document
 * @name Extension_gen
 * @namespace dmExtension
 * @path dmsdk/extension/extension_gen.hpp
 */

#include <dmsdk/resource/resource.h>
#include <dmsdk/dlib/align.h>
#include <dmsdk/dlib/configfile.h>

#include <dmsdk/extension/extension.h>

namespace dmExtension
{
    /*#
    * Generated from [ref:ExtensionResult]
    */
    enum Result {
        RESULT_OK = 0,
        RESULT_INIT_ERROR = -1,
    };

    /*#
    * Generated from [ref:ExtensionEventID]
    */
    enum EventID {
        EVENT_ID_ACTIVATEAPP,
        EVENT_ID_DEACTIVATEAPP,
        EVENT_ID_ICONIFYAPP,
        EVENT_ID_DEICONIFYAPP,
        EVENT_ID_ENGINE_INITIALIZED,
        EVENT_ID_ENGINE_DELETE,
    };

    /*#
    * Generated from [ref:ExtensionCallbackType]
    */
    enum CallbackType {
        CALLBACK_PRE_RENDER,
        CALLBACK_POST_RENDER,
    };

    /*#
    * Generated from [ref:ExtensionAppParams]
    */
    typedef ExtensionAppParams AppParams;

    /*#
    * Generated from [ref:ExtensionParams]
    */
    typedef ExtensionParams Params;

    /*#
    * Generated from [ref:ExtensionEvent]
    */
    typedef ExtensionEvent Event;

    /*#
    * Generated from [ref:FExtensionCallback]
    */
    typedef FExtensionCallback FCallback;

    /*#
    * Generated from [ref:ExtensionRegisterCallback]
    */
    bool RegisterCallback(CallbackType callback_type,FCallback func);

    /*#
    * Generated from [ref:ExtensionRegisteriOSUIApplicationDelegate]
    */
    void RegisteriOSUIApplicationDelegate(void * delegate);

    /*#
    * Generated from [ref:ExtensionUnregisteriOSUIApplicationDelegate]
    */
    void UnregisteriOSUIApplicationDelegate(void * delegate);


} // namespace dmExtension

#endif // #define DMSDK_EXTENSION_GEN_HPP

