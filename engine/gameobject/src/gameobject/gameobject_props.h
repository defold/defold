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

#ifndef DM_GAMEOBJECT_PROPS_H
#define DM_GAMEOBJECT_PROPS_H

#include <stdint.h>

#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/message.h>
#include <resource/resource.h>
#include <script/script.h>

#include "gameobject.h"

#include <dmsdk/gameobject/gameobject_props.h>

namespace dmGameObject
{
    enum PropertyLayer
    {
        PROPERTY_LAYER_INSTANCE = 0,
        PROPERTY_LAYER_PROTOTYPE = 1,
        PROPERTY_LAYER_DEFAULT = 2,
        MAX_PROPERTY_LAYER_COUNT
    };

    struct Properties
    {
        Properties();

        PropertySet m_Set[MAX_PROPERTY_LAYER_COUNT];
        dmScript::ResolvePathCallback m_ResolvePathCallback;
        uintptr_t m_ResolvePathUserData;
        dmScript::GetURLCallback m_GetURLCallback;
    };

    struct NewPropertiesParams
    {
        NewPropertiesParams();

        dmScript::ResolvePathCallback m_ResolvePathCallback;
        uintptr_t m_ResolvePathUserData;
        dmScript::GetURLCallback m_GetURLCallback;
    };

    HProperties NewProperties(const NewPropertiesParams& params);
    void DeleteProperties(HProperties properties);

    void SetPropertySet(HProperties properties, PropertyLayer layer, const PropertySet& set);

    PropertyResult GetProperty(const HProperties properties, dmhash_t id, PropertyVar& var);

    // dmResource::Get supplied paths and stores into out_resources which will be re-allocated through SetCapacity(resource_path_count)
    // Unwinds at failure
    dmResource::Result LoadPropertyResources(dmResource::HFactory factory, const char** resource_paths, uint32_t resource_path_count, dmArray<void*>& out_resources);
    // dmResource::Release supplied paths and de-allocates through SetCapacity(0)
    void UnloadPropertyResources(dmResource::HFactory factory, dmArray<void*>& resources);

    PropertyResult PropertyContainerGetPropertyCallback(const HProperties properties, uintptr_t user_data, dmhash_t id, PropertyVar& out_var);
    void PropertyContainerDestroyCallback(uintptr_t user_data);

    HPropertyContainer PropertyContainerAllocateWithSize(uint32_t size);
    uint32_t PropertyContainerGetMemorySize(HPropertyContainer container);
    void PropertyContainerSerialize(HPropertyContainer container, uint8_t* buffer, uint32_t buffer_size);
    void PropertyContainerDeserialize(const uint8_t* buffer, uint32_t buffer_size, HPropertyContainer out);

    void PropertyContainerPrint(HPropertyContainer container);
}

#endif // DM_GAMEOBJECT_PROPS_H
