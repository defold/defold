// Copyright 2020-2025 The Defold Foundation
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

#ifndef DM_COMPONENT_H
#define DM_COMPONENT_H

#include <dlib/hash.h>
#include "gameobject.h"
#include <dmsdk/gameobject/component.h>
#include <resource/resource.h>

namespace dmGameObject
{
    /*#
     * Collection of component registration data.
     */
    struct ComponentType
    {
        ComponentType();

        HResourceType           m_ResourceType;
        const char*             m_Name;
        dmhash_t                m_NameHash;
        void*                   m_Context;
        ComponentNewWorld       m_NewWorldFunction;
        ComponentDeleteWorld    m_DeleteWorldFunction;
        ComponentCreate         m_CreateFunction;
        ComponentDestroy        m_DestroyFunction;
        ComponentInit           m_InitFunction;
        ComponentFinal          m_FinalFunction;
        ComponentAddToUpdate    m_AddToUpdateFunction;
        ComponentGet            m_GetFunction;
        ComponentsPreUpdate     m_PreUpdateFunction;
        ComponentsUpdate        m_UpdateFunction;
        ComponentsFixedUpdate   m_FixedUpdateFunction;
        ComponentsRender        m_RenderFunction;
        ComponentsPostUpdate    m_PostUpdateFunction;
        ComponentOnMessage      m_OnMessageFunction;
        ComponentOnInput        m_OnInputFunction;
        ComponentOnReload       m_OnReloadFunction;
        ComponentSetProperties  m_SetPropertiesFunction;
        ComponentGetProperty    m_GetPropertyFunction;
        ComponentSetProperty    m_SetPropertyFunction;
        FIteratorChildren       m_IterChildren; // for debug/testing
        FIteratorProperties     m_IterProperties; // for debug/testing
        uint32_t                m_TypeIndex : 16;
        uint32_t                m_InstanceHasUserData : 1;
        uint32_t                m_ReadsTransforms : 1;
        uint32_t                m_Reserved : 14;
        uint16_t                m_UpdateOrderPrio;
    };

    /*#
     * Register a new component type
     * @param regist Gameobject register
     * @param type Collection of component type registration data
     * @return RESULT_OK on success
     */
    Result RegisterComponentType(HRegister regist, const ComponentType& type);

    /*#
     * Retrieves a registered component type given its resource type.
     * @param regist Game object register
     * @param resource_type The resource type of the component type
     * @param out_component_index Optional component index out argument, 0x0 is accepted
     * @return the registered component type or 0x0 if not found
     */
    ComponentType* FindComponentType(HRegister regist, HResourceType resource_type, uint32_t* out_component_index);

    /*#
     * Gets the number of registered component types
     * @name GetNumComponentTypes
     * @param regist [type: dmGameObject::HRegister] the game object register
     * @return count [type: uint32_t] the number of registered component types
     */
    uint32_t GetNumComponentTypes(HRegister regist);

    /*#
     * Gets the number of registered component types
     * @name GetComponentType
     * @param regist [type: dmGameObject::HRegister] the game object register
     * @param index [type: uint32_t] the index
     * @return count [type: uint32_t] the number of registered component types
     */
    ComponentType* GetComponentType(HRegister regist, uint32_t index);

    /*#
     * Set update order priority. Zero is highest priority.
     * @param regist Register
     * @param resource_type Resource type
     * @param prio Priority
     * @return RESULT_OK on success
     */
    Result SetUpdateOrderPrio(HRegister regist, HResourceType resource_type, uint16_t prio);

    /*#
     * Sort component types according to update order priority.
     * @param regist Register
     */
    void SortComponentTypes(HRegister regist);


    struct ComponentTypeDescriptor
    {
        ComponentTypeDescriptor*        m_Next;
        ComponentTypeCreateFunction     m_CreateFn;
        ComponentTypeDestroyFunction    m_DestroyFn;
        const char*                     m_Name;
        uint16_t                        m_TypeIndex;
        uint16_t                        m_Prio;
        uint16_t                        m_ReadsTransforms:1;
        uint16_t                        :15;
    };


    /*# Calls the create function for all registered component types
     */
    Result CreateRegisteredComponentTypes(const ComponentTypeCreateCtx* ctx);

    /*# Calls the destroy function for all registered component types
     */
    Result DestroyRegisteredComponentTypes(const ComponentTypeCreateCtx* ctx);
}

#endif // #ifndef DM_COMPONENT_H
