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

#ifndef DMSDK_GAMEOBJECT_COMPONENT_H
#define DMSDK_GAMEOBJECT_COMPONENT_H

#include <dmsdk/gameobject/gameobject.h>

namespace dmGameObject
{
    /*# SDK Component API documentation
     * [file:<dmsdk/gameobject/component.h>]
     *
     * Api for manipulating game object components (WIP)
     *
     * @document
     * @name Component
     * @namespace dmGameObject
     */

    struct ComponentType;

    /**
     * Register a new component type
     * @param regist Gameobject register
     * @param type Collection of component type registration data
     * @return RESULT_OK on success
     */
    Result RegisterComponentType(HRegister regist, const ComponentType& type);

    enum ComponentFunctionName
    {
        /// Called when a new world is created
        COMPONENT_FUNCTION_NAME_NEW_WORLD,
        ///
        COMPONENT_FUNCTION_NAME_DELETE_WORLD,
        ///
        COMPONENT_FUNCTION_NAME_CREATE,
        ///
        COMPONENT_FUNCTION_NAME_DESTROY,
        ///
        COMPONENT_FUNCTION_NAME_INIT,
        ///
        COMPONENT_FUNCTION_NAME_FINAL,
        ///
        COMPONENT_FUNCTION_NAME_ADDTOUPDATE,
        ///
        COMPONENT_FUNCTION_NAME_GET,
        ///
        COMPONENT_FUNCTION_NAME_UPDATE,
        ///
        COMPONENT_FUNCTION_NAME_RENDER,
        ///
        COMPONENT_FUNCTION_NAME_POSTUPDATE,
        ///
        COMPONENT_FUNCTION_NAME_ONMESSAGE,
        ///
        COMPONENT_FUNCTION_NAME_ONINPUT,
        ///
        COMPONENT_FUNCTION_NAME_ONRELOAD,
        ///
        COMPONENT_FUNCTION_NAME_SETPROPERTIES,
        ///
        COMPONENT_FUNCTION_NAME_GETPROPERTY,
        ///
        COMPONENT_FUNCTION_NAME_SETPROPERTY,
    };
}

#endif // #ifndef DMSDK_GAMEOBJECT_COMPONENT_H
