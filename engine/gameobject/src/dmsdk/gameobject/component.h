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

#ifndef DMSDK_GAMEOBJECT_COMPONENT_H
#define DMSDK_GAMEOBJECT_COMPONENT_H

#include <dmsdk/gameobject/gameobject.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/configfile.h>
#include <dmsdk/dlib/hashtable.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/script/script.h>
#include <dmsdk/resource/resource.h>

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

    /*#
     * Component type handle. It holds the life time functions for a type.
     * @typedef
     * @name HComponentType
     */
    typedef ComponentType* HComponentType;

    /*#
     * Parameters to ComponentNewWorld callback.
     * @struct
     * @name ComponentNewWorldParams
     * @member m_Context [type: void*] Context for the component type
     * @member m_ComponentIndex [type: uint8_t] Component type index that can be used later with GetWorld()
     * @member m_MaxInstances [type: uint32_t] Max component game object instance count (if applicable)
     * @member m_World [type: void**] Out-parameter of the pointer in which to store the created world
     * @member m_MaxComponentInstances [type: uint32_t] Max components count of this type in current collection counted at the build stage.
     *                                         If component in factory then value is 0xFFFFFFFF
     */
    struct ComponentNewWorldParams
    {
        void* m_Context;
        uint8_t m_ComponentIndex;
        uint32_t m_MaxInstances;
        void** m_World;
        uint32_t m_MaxComponentInstances;
    };

    /*#
     * Component world create function
     * @typedef
     * @name ComponentNewWorld
     * @param params [type: const dmGameObject::ComponentNewWorldParams&]
     * @return result [type: CreateResult] CREATE_RESULT_OK on success
     */
    typedef CreateResult (*ComponentNewWorld)(const ComponentNewWorldParams& params);

    /*#
     * Parameters to ComponentDeleteWorld callback.
     * @struct
     * @name ComponentDeleteWorldParams
     * @member m_Context [type void*] Context for the component type
     * @member m_World [type void*] The pointer to the world to destroy
     */
    struct ComponentDeleteWorldParams
    {
        void* m_Context;
        void* m_World;
    };

    /*#
     * Component world destroy function
     * @typedef
     * @name ComponentDeleteWorld
     * @param params [type: const dmGameObject::ComponentDeleteWorldParams&]
     * @return result [type: CreateResult] CREATE_RESULT_OK on success
     */
    typedef CreateResult (*ComponentDeleteWorld)(const ComponentDeleteWorldParams& params);

    /*#
     * Parameters to ComponentCreate callback.
     * @struct
     * @name ComponentCreateParams
     * @member m_Instance [type: HInstance] Game object instance
     * @member m_Position [type: dmVMath::Point3] Local component position
     * @member m_Rotation [type: dmVMath::Quat] Local component rotation
     * @member m_Scale [type: dmVMath::Vector3] Local component scale
     * @member m_PropertySet [type: PropertySet] Set of properties
     * @member m_Resource [type: void*] Component resource
     * @member m_World [type: void*] Component world, as created in the ComponentNewWorld callback
     * @member m_Context [type: void*] User context
     * @member m_UserData [type: uintptr_t*] User data storage pointer
     * @member m_ComponentIndex [type: uint16_t] Index of the component type being created (among all component types)
     */
    struct ComponentCreateParams
    {
        HInstance           m_Instance;
        dmVMath::Point3     m_Position;
        dmVMath::Quat       m_Rotation;
        dmVMath::Vector3    m_Scale;
        PropertySet         m_PropertySet;
        void*               m_Resource;
        void*               m_World;
        void*               m_Context;
        uintptr_t*          m_UserData;
        uint16_t            m_ComponentIndex;
    };

    /*#
     * Component create function. Should allocate all necessary resources for the component.
     * The game object instance is guaranteed to have its id, scene hierarchy and transform data updated when this is called.
     * @typedef
     * @name ComponentCreate
     * @param params [type: const dmGameObject::ComponentCreateParams&]
     * @return result [type: CreateResult] CREATE_RESULT_OK on success
     */
    typedef CreateResult (*ComponentCreate)(const ComponentCreateParams& params);

    /*#
     * Parameters to ComponentDestroy callback.
     * @struct
     * @name ComponentDestroyParams
     * @member m_Collection [type: HCollection] Collection handle
     * @member m_Instance [type: HInstance] Game object instance
     * @member m_World [type: void*] Component world
     * @member m_Context [type: void*] User context
     * @member m_UserData [type: uintptr_t*] User data storage pointer
     */
    struct ComponentDestroyParams
    {
        HCollection m_Collection;
        HInstance m_Instance;
        void* m_World;
        void* m_Context;
        uintptr_t* m_UserData;
    };

    /*#
     * Component destroy function. Should deallocate all necessary resources.
     * @typedef
     * @name ComponentDestroy
     * @param params [type: const dmGameObject::ComponentDestroyParams&]
     * @return result [type: CreateResult] CREATE_RESULT_OK on success
     */
    typedef CreateResult (*ComponentDestroy)(const ComponentDestroyParams& params);

    /*#
     * Parameters to ComponentInit callback.
     * @struct
     * @name ComponentInitParams
     * @member m_Collection [type: HCollection] Collection handle
     * @member m_Instance [type: HInstance] Game object instance
     * @member m_World [type: void*] Component world
     * @member m_Context [type: void*] User context
     * @member m_UserData [type: uintptr_t*] User data storage pointer
     */
    struct ComponentInitParams
    {
        HCollection m_Collection;
        HInstance m_Instance;
        void* m_World;
        void* m_Context;
        uintptr_t* m_UserData;
    };

    /*#
     * Component init function. Should set the components initial state as it is called when the component is enabled.
     * @typedef
     * @name ComponentInit
     * @param params [type: const dmGameObject::ComponentInitParams&]
     * @return result [type: CreateResult] CREATE_RESULT_OK on success
     */
    typedef CreateResult (*ComponentInit)(const ComponentInitParams& params);

    /*#
     * Parameters to ComponentFinal callback.
     * @struct
     * @name ComponentFinalParams
     * @member m_Collection [type: HCollection] Collection handle
     * @member m_Instance [type: HInstance] Game object instance
     * @member m_World [type: void*] Component world
     * @member m_Context [type: void*] User context
     * @member m_UserData [type: uintptr_t*] User data storage pointer
     */
    struct ComponentFinalParams
    {
        HCollection m_Collection;
        HInstance m_Instance;
        void* m_World;
        void* m_Context;
        uintptr_t* m_UserData;
    };

    /*#
     * Component finalize function. Should clean up as it is called when the component is disabled.
     * @typedef
     * @name ComponentFinal
     * @param params [type: const dmGameObject::ComponentFinalParams&]
     * @return result [type: CreateResult] CREATE_RESULT_OK on success
     */
    typedef CreateResult (*ComponentFinal)(const ComponentFinalParams& params);

    /*#
     * Parameters to ComponentAddToUpdate callback.
     * @struct
     * @name ComponentAddToUpdateParams
     * @member m_Collection [type: HCollection] Collection handle
     * @member m_Instance [type: HInstance] Game object instance
     * @member m_World [type: void*] Component world
     * @member m_Context [type: void*] User context
     * @member m_UserData [type: uintptr_t*] User data storage pointer
     */
    struct ComponentAddToUpdateParams
    {
        HCollection m_Collection;
        HInstance m_Instance;
        void* m_World;
        void* m_Context;
        uintptr_t* m_UserData;
    };

    /*#
     * Component add to update function. Only components called with this function should be included in the update passes.
     * @typedef
     * @name ComponentAddToUpdate
     * @param params [type: const dmGameObject::ComponentAddToUpdateParams&]
     * @return result [type: CreateResult] CREATE_RESULT_OK on success
     */
    typedef CreateResult (*ComponentAddToUpdate)(const ComponentAddToUpdateParams& params);

    /*#
     * Parameters to ComponentGet callback.
     * @struct
     * @name ComponentGetParams
     * @member m_World [type: HComponentWorld] Component world
     * @member m_UserData [type: HComponentInternal] Component internal representation
     */
    struct ComponentGetParams
    {
        dmGameObject::HComponentWorld       m_World;
        dmGameObject::HComponentInternal    m_UserData;
    };

    /*#
     * A simple way to get the component instance from the user_data (which was set during creation)
     * @typedef
     * @name ComponentGet
     * @param params [type: const dmGameObject::ComponentGetParams&] Update parameters
     * @return component [type: void*] The internal component pointer
     */
    typedef void* (*ComponentGet)(const ComponentGetParams& params);


    /*#
     * Parameters to ComponentsUpdate callback.
     * @struct
     * @name ComponentsUpdateParams
     * @member m_Collection [type: HCollection] Collection handle
     * @member m_UpdateContext [type: const UpdateContext*] Update context
     * @member m_World [type: void*] Component world
     * @member m_Context [type: void*] User context
     */
    struct ComponentsUpdateParams
    {
        HCollection m_Collection;
        const UpdateContext* m_UpdateContext;
        void* m_World;
        void* m_Context;
    };

    /*#
     * Parameters to ComponentsUpdate callback.
     * @struct
     * @name ComponentsUpdateResult
     * @member m_TransformsUpdated [type: bool] True if a component type updated any game object transforms
     */
    struct ComponentsUpdateResult
    {
        bool m_TransformsUpdated;
    };

    /*#
     * Component update function. Updates all component of this type for all game objects
     * @typedef
     * @name ComponentsUpdate
     * @param params [type: const dmGameObject::ComponentsUpdateParams&] Update parameters
     * @param params [type: dmGameObject::ComponentsUpdateResult&] (out) Update result
     * @return result [type: UpdateResult] UPDATE_RESULT_OK on success
     */
    typedef UpdateResult (*ComponentsUpdate)(const ComponentsUpdateParams& params, ComponentsUpdateResult& result);

    /*#
     * Component fixed update function. Updates all component of this type for all game objects
     * @typedef
     * @name ComponentsFixedUpdate
     * @param params [type: const dmGameObject::ComponentsUpdateParams&] Update parameters
     * @param params [type: dmGameObject::ComponentsUpdateResult&] (out) Update result
     * @return result [type: UpdateResult] UPDATE_RESULT_OK on success
     */
    typedef UpdateResult (*ComponentsFixedUpdate)(const ComponentsUpdateParams& params, ComponentsUpdateResult& result);

    /*#
     * Parameters to ComponentsRender callback.
     * @struct
     * @name ComponentsRenderParams
     * @member m_Collection [type: HCollection] Collection handle
     * @member m_World [type: void*] Component world
     * @member m_Context [type: void*] User context
     */
    struct ComponentsRenderParams
    {
        HCollection m_Collection;
        void* m_World;
        void* m_Context;
    };

    /*#
     * Component render function.
     * @typedef
     * @name ComponentsRender
     * @param params [type: const dmGameObject::ComponentsRenderParams&] Update parameters
     * @return result [type: UpdateResult] UPDATE_RESULT_OK on success
     */
    typedef UpdateResult (*ComponentsRender)(const ComponentsRenderParams& params);

    /*#
     * Parameters for ComponentsPostUpdate callback.
     * @struct
     * @name ComponentsPostUpdateParams
     * @member m_Collection [type: HCollection] Collection handle
     * @member m_World [type: void*] Component world
     * @member m_Context [type: void*] User context
     */
    struct ComponentsPostUpdateParams
    {
        HCollection m_Collection;
        void* m_World;
        void* m_Context;
    };

    /*#
     * Component post update function. The component state should never be modified in this function.
     * @typedef
     * @name ComponentsPostUpdate
     * @param params [type: const dmGameObject::ComponentsPostUpdateParams&] Update parameters
     * @return result [type: UpdateResult] UPDATE_RESULT_OK on success
     */
    typedef UpdateResult (*ComponentsPostUpdate)(const ComponentsPostUpdateParams& params);

    /*#
     * Parameters to ComponentOnMessage callback.
     * @struct
     * @name ComponentOnMessageParams
     * @member m_Instance [type: HInstance] Instance handle
     * @member m_World [type: void*] World
     * @member m_Context [type: void*] User context
     * @member m_UserData [type: uintptr_t*] User data storage pointer
     * @member m_Message [type: dmMessage::Message*] Message
     */
    struct ComponentOnMessageParams
    {
        HInstance m_Instance;
        void* m_World;
        void* m_Context;
        uintptr_t* m_UserData;
        dmMessage::Message* m_Message;
    };

    /*#
     * Component on-message function. Called when message is sent to this component
     * @typedef
     * @name ComponentOnMessage
     * @param params [type: const dmGameObject::ComponentOnMessageParams&] Update parameters
     * @return result [type: UpdateResult] UPDATE_RESULT_OK on success
     */
    typedef UpdateResult (*ComponentOnMessage)(const ComponentOnMessageParams& params);

    /*#
     * Parameters to ComponentOnInput callback.
     * @struct
     * @name ComponentOnInputParams
     * @member m_Instance [type: HInstance] Instance handle
     * @member m_InputAction [type: const InputAction*] Information about the input that occurred (note that input being released is also treated as input)
     * @member m_Context [type: void*] User context
     * @member m_UserData [type: uintptr_t*] User data storage pointer
     */
    struct ComponentOnInputParams
    {
        HInstance m_Instance;
        const InputAction* m_InputAction;
        void* m_Context;
        uintptr_t* m_UserData;
    };

    /*#
     * Component on-input function. Called when input is sent to this component
     * @typedef
     * @name ComponentOnInput
     * @param params [type: const dmGameObject::ComponentOnInputParams&] Input parameters
     * @return result [type: InputResult] How the component handled the input
     */
    typedef InputResult (*ComponentOnInput)(const ComponentOnInputParams& params);

    /*#
     * Parameters to ComponentOnReload callback.
     * @struct
     * @name ComponentOnReloadParams
     * @member m_Instance [type: HInstance] Instance handle
     * @member m_Resource [type: void*] Resource that was reloaded
     * @member m_World [type: void*] Component world
     * @member m_Context [type: void*] User context
     * @member m_UserData [type: uintptr_t*] User data storage pointer
     */
    struct ComponentOnReloadParams
    {
        HInstance m_Instance;
        void* m_Resource;
        void* m_World;
        void* m_Context;
        uintptr_t* m_UserData;
    };

    /*#
     * Called when the resource the component is based on has been reloaded.
     * @name ComponentOnReload
     * @param params [type: const dmGameObject::ComponentOnReloadParams&] the parameters
     */
    typedef void (*ComponentOnReload)(const ComponentOnReloadParams& params);

    /*#
     * Parameters to ComponentSetProperties callback.
     * @struct
     * @name ComponentSetPropertiesParams
     * @member m_Instance [type: HInstance] Instance handle
     * @member m_PropertySet [type: PropertySet] Property set to use
     * @member m_UserData [type: uintptr_t*] User data storage pointer
     */
    struct ComponentSetPropertiesParams
    {
        HInstance m_Instance;
        PropertySet m_PropertySet;
        uintptr_t* m_UserData;
    };

    /*#
     * Set a property set for the component.
     * @typedef
     * @name ComponentSetProperties
     * @param params [type: const dmGameObject::ComponentSetPropertiesParams&] the parameters
     * @return result [type: dmGameObject::PropertyResult] PROPERTY_RESULT_OK if property was set
     */
    typedef PropertyResult (*ComponentSetProperties)(const ComponentSetPropertiesParams& params);

    /*#
     * Parameters to ComponentGetProperty callback.
     * @struct
     * @name ComponentGetPropertyParams
     * @member m_Context [type: void*] Context for the component type
     * @member m_World [type: void*] Component world
     * @member m_Instance [type: HInstance] Game object instance
     * @member m_PropertyId [type: dmhash_t] Id of the property
     * @member m_UserData [type: uintptr_t*] User data storage pointer
     * @member m_Options [type: PropertyOptions] Options for getting the property
     */
    struct ComponentGetPropertyParams
    {
        void* m_Context;
        void* m_World;
        HInstance m_Instance;
        dmhash_t m_PropertyId;
        uintptr_t* m_UserData;
        PropertyOptions m_Options;
    };

    /*#
     * Callback for retrieving a property value of the component.
     * @typedef
     * @name ComponentGetProperty
     * @param params [type: const dmGameObject::ComponentGetPropertyParams&] the parameters
     * @param out_value [type: dmGameObject::PropertyDesc&] (out) the property
     * @return result [type: dmGameObject::PropertyResult] PROPERTY_RESULT_OK if retrieving the property was ok
     */
    typedef PropertyResult (*ComponentGetProperty)(const ComponentGetPropertyParams& params, PropertyDesc& out_value);

    /*#
     * Parameters to ComponentSetProperty callback.
     * @struct
     * @name ComponentSetPropertyParams
     * @member m_Context [type: void*] Context for the component type
     * @member m_World [type: void*] Component world
     * @member m_Instance [type: HInstance] Game object instance
     * @member m_PropertyId [type: dmhash_t] Id of the property
     * @member m_UserData [type: uintptr_t*] User data storage pointer
     * @member m_Value [type: PropertyVar] New value of the property
     * @member m_Options [type: PropertyOptions] Options for setting the property
     */
    struct ComponentSetPropertyParams
    {
        void* m_Context;
        void* m_World;
        HInstance m_Instance;
        dmhash_t m_PropertyId;
        uintptr_t* m_UserData;
        PropertyVar m_Value;
        PropertyOptions m_Options;
    };

    /*#
     * Callback for setting a property value of the component.
     * @typedef
     * @name ComponentSetProperty
     * @param params [type: const dmGameObject::ComponentSetPropertyParams&] the parameters
     * @return result [type: dmGameObject::PropertyResult] PROPERTY_RESULT_OK if property was set
     */
    typedef PropertyResult (*ComponentSetProperty)(const ComponentSetPropertyParams& params);

    /*#
     * Callback when iterating over the properties for a component.
     * @note This function is only available/used in debug builds, when traversing the scene graph in order to export
     * this data for external tools (e.g. external testing libraries like Poco)
     * @typedef
     * @name ComponentIterProperties
     * @param pit [type: dmGameObject::SceneNodePropertyIterator] the property iterator
     * @param node [type: dmGameObject::SceneNode*] the scene node
     */
    typedef void (*ComponentIterProperties)(dmGameObject::SceneNodePropertyIterator* pit, dmGameObject::SceneNode* node);


    /*#
     * Get the component type index. Used for with e.g. dmGameObject::GetWorld()/GetContext()
     * @name ComponentTypeGetTypeIndex
     * @param type [type: HComponentType] the type
     * @return type_index [type: uint32_t] The type index.
     */
    uint32_t ComponentTypeGetTypeIndex(const HComponentType type);

    /*# set the new world callback
     * Set the new world callback. Called when a collection (i.e. a "world") is created.
     * @name ComponentTypeSetNewWorldFn
     * @param type [type: HComponentType] the type
     * @param fn [type: ComponentNewWorld] callback
     */
    void ComponentTypeSetNewWorldFn(HComponentType type, ComponentNewWorld fn);

    /*# set the world destroy callback
     * Set the world destroy callback. Called when a collection (i.e. a "world") is destroyed.
     * @name ComponentTypeSetDeleteWorldFn
     * @param type [type: HComponentType] the type
     * @param fn [type: ComponentDeleteWorld] callback
     */
    void ComponentTypeSetDeleteWorldFn(HComponentType type, ComponentDeleteWorld fn);

    /*# set the component create callback
     * Set the component create callback. Called when a component instance is created.
     * @name ComponentTypeSetCreateFn
     * @param type [type: HComponentType] the type
     * @param fn [type: ComponentCreate] callback
     */
    void ComponentTypeSetCreateFn(HComponentType type, ComponentCreate fn);

    /*# set the component destroy callback
     * Set the component destroy callback. Called when a component instance is destroyed.
     * @name ComponentTypeSetDestroyFn
     * @param type [type: HComponentType] the type
     * @param fn [type: ComponentDestroy] callback
     */
    void ComponentTypeSetDestroyFn(HComponentType type, ComponentDestroy fn);

    /*# set the component init callback
     * Set the component init callback. Called on each gameobject's components, during a gameobject's initialization.
     * @name ComponentTypeSetInitFn
     * @param type [type: HComponentType] the type
     * @param fn [type: ComponentInit] callback
     */
    void ComponentTypeSetInitFn(HComponentType type, ComponentInit fn);

    /*# set the component finalize callback
     * Set the component finalize callback. Called on each gameobject's components, during a gameobject's finalization.
     * @name ComponentTypeSetFinalFn
     * @param type [type: HComponentType] the type
     * @param fn [type: ComponentFinal] callback
     */
    void ComponentTypeSetFinalFn(HComponentType type, ComponentFinal fn);

    /*# set the component add-to-update callback
     * Set the component add-to-update callback. Called for each component instal, when the game object is spawned.
     * @name ComponentTypeSetAddToUpdateFn
     * @param type [type: HComponentType] the type
     * @param fn [type: ComponentAddToUpdate] callback
     */
    void ComponentTypeSetAddToUpdateFn(HComponentType type, ComponentAddToUpdate fn);

    /*# set the component get callback
     * Set the component get callback. Called when the scripts want to retrieve the individual component user data given an url.
     * @name ComponentTypeSetGetFn
     * @param type [type: HComponentType] the type
     * @param fn [type: ComponentGet] callback
     */
    void ComponentTypeSetGetFn(HComponentType type, ComponentGet fn);

    /*# set the component render callback
     * Set the component render callback. Called when it's time to render all component instances.
     * @name ComponentTypeSetRenderFn
     * @param type [type: HComponentType] the type
     * @param fn [type: ComponentsRender] callback
     */
    void ComponentTypeSetRenderFn(HComponentType type, ComponentsRender fn);

    /*# set the component update callback
     * Set the component update callback. Called when it's time to update all component instances.
     * @name ComponentTypeSetUpdateFn
     * @param type [type: HComponentType] the type
     * @param fn [type: ComponentsUpdate] callback
     */
    void ComponentTypeSetUpdateFn(HComponentType type, ComponentsUpdate fn);

    /*# set the component update callback
     * Set the component update callback. Called when it's time to update all component instances.
     * @name ComponentTypeSetFixedUpdateFn
     * @param type [type: HComponentType] the type
     * @param fn [type: ComponentsFixedUpdate] callback
     */
    void ComponentTypeSetFixedUpdateFn(HComponentType type, ComponentsFixedUpdate fn);

    /*# set the component post update callback
     * Set the component post update callback. Called for each collection after the update, before the render.
     * @name ComponentTypeSetPostUpdateFn
     * @param type [type: HComponentType] the type
     * @param fn [type: ComponentsPostUpdate] callback
     */
    void ComponentTypeSetPostUpdateFn(HComponentType type, ComponentsPostUpdate fn);

    /*# set the component on-message callback
     * Set the component on-message callback. Called multiple times per frame, to flush messages.
     * @name ComponentTypeSetOnMessageFn
     * @param type [type: HComponentType] the type
     * @param fn [type: ComponentOnMessage] callback
     */
    void ComponentTypeSetOnMessageFn(HComponentType type, ComponentOnMessage fn);

    /*# set the component on-input callback
     * Set the component on-input callback. Called once per frame, before the Update function.
     * @name ComponentTypeSetOnInputFn
     * @param type [type: HComponentType] the type
     * @param fn [type: ComponentOnInput] callback
     */
    void ComponentTypeSetOnInputFn(HComponentType type, ComponentOnInput fn);

    /*# set the component on-reload callback
     * Set the component on-reload callback. Called when the resource of a component instance is reloaded.
     * @name ComponentTypeSetOnReloadFn
     * @param type [type: HComponentType] the type
     * @param fn [type: ComponentOnReload] callback
     */
    void ComponentTypeSetOnReloadFn(HComponentType type, ComponentOnReload fn);

    /*# set the component set properties callback
     * Set the component set properties callback. Called when the component instance is being spwned.
     * @name ComponentTypeSetSetPropertiesFn
     * @param type [type: HComponentType] the type
     * @param fn [type: ComponentSetProperties] callback
     */
    void ComponentTypeSetSetPropertiesFn(HComponentType type, ComponentSetProperties fn);

    /*# set the component get property callback
     * Set the component get property callback. Called when accessing a property via `go.get()`
     * @name ComponentTypeSetGetPropertyFn
     * @param type [type: HComponentType] the type
     * @param fn [type: ComponentGetProperty] callback
     */
    void ComponentTypeSetGetPropertyFn(HComponentType type, ComponentGetProperty fn);

    /*# set the component set property callback
     * Set the component set property callback. Called when accessing a property via `go.set()`
     * @name ComponentTypeSetSetPropertyFn
     * @param type [type: HComponentType] the type
     * @param fn [type: ComponentSetProperty] callback
     */
    void ComponentTypeSetSetPropertyFn(HComponentType type, ComponentSetProperty fn);

    /*# set the component type global context
     * Set the component type global context. Usually set when registering the component type.
     * @name ComponentTypeSetContext
     * @param type [type: HComponentType] the type
     * @param context [type: void*] component type global context
     */
    void ComponentTypeSetContext(HComponentType type, void* context);

    /*# get the component type global context
     * get the component type global context
     * @name ComponentTypeGetContext
     * @param type [type: HComponentType] the type
     * @return context [type: void*] component type global context
     */
    void* ComponentTypeGetContext(const HComponentType type);

    /*# set the component type transform dependency flag
     * Set the component type transform dependency flag.
     * If this flag is set, it might trigger an dmGameObject::UpdateTransforms() (if there are dirty transforms)
     * @name ComponentTypeSetReadsTransforms
     * @param type [type: HComponentType] the type
     * @param reads_transforms [type: bool] transform dependency flag
     */
    void ComponentTypeSetReadsTransforms(HComponentType type, bool reads_transforms);

    /*# set the component type prio order
     * Set the component type prio order. Defines the update order of the component types.
     * @name ComponentTypeSetPrio
     * @param type [type: HComponentType] the type
     * @param prio [type: uint16_t] prio order
     */
    void ComponentTypeSetPrio(HComponentType type, uint16_t prio);

    /*# set the component type need for a per component instance user data
     * Set the component type need for a per component instance user data. Defaults to true.
     * @name ComponentTypeSetHasUserData
     * @param type [type: HComponentType] the type
     * @param has_user_data [type: bool] does each component instance need user data
     */
    void ComponentTypeSetHasUserData(HComponentType type, bool has_user_data);

    /*# set the component child iterator function
     * set the component child iterator function. Called during inspection
     * @name ComponentTypeSetChildIteratorFn
     * @param type [type: HComponentType] the type
     * @param fn [type: FIteratorChildren] child iterator function
     */
    void ComponentTypeSetChildIteratorFn(HComponentType type, FIteratorChildren fn);

    /*# set the component property iterator function
     * set the component property iterator function. Called during inspection
     * @name ComponentTypeSetPropertyIteratorFn
     * @param type [type: HComponentType] the type
     * @param fn [type: FIteratorProperties] property iterator function
     */
    void ComponentTypeSetPropertyIteratorFn(HComponentType type, FIteratorProperties fn);

    /*#
     * Context used when registering a new component type
     * @struct
     * @name ComponentTypeCreateCtx
     * @member m_Config [type: dmConfigFile::HConfig] The config file
     * @member m_Factory [type: dmResource::HFactory] The resource factory
     * @member m_Register [type: dmGameObject::HRegister] The game object registry
     * @member m_Script [type: dmScript::HContext] The shared script context
     * @member m_Contexts [type: dmHashTable64<void*>] Mappings between names and contextx
     */
    struct ComponentTypeCreateCtx {
        dmConfigFile::HConfig    m_Config;
        dmResource::HFactory     m_Factory;
        dmGameObject::HRegister  m_Register;
        dmScript::HContext       m_Script;
        dmHashTable64<void*>     m_Contexts;
    };

    typedef Result (*ComponentTypeCreateFunction)(const ComponentTypeCreateCtx* ctx, HComponentType type);
    typedef Result (*ComponentTypeDestroyFunction)(const ComponentTypeCreateCtx* ctx, HComponentType type);

    struct ComponentTypeDescriptor;

    /**
     * Register a new component type (Internal)
     * @param regist Gameobject register
     * @param type Collection of component type registration data
     * @return RESULT_OK on success
     */
    Result RegisterComponentTypeDescriptor(ComponentTypeDescriptor* desc, const char* name, ComponentTypeCreateFunction create_fn, ComponentTypeDestroyFunction destroy_fn);

    /**
    * Component type desc bytesize declaration. Internal
    */
    const size_t s_ComponentTypeDescBufferSize = 128;

    #define DM_COMPONENT_PASTE_SYMREG(x, y) x ## y
    #define DM_COMPONENT_PASTE_SYMREG2(x, y) DM_COMPONENT_PASTE_SYMREG(x,y)

    #define DM_REGISTER_COMPONENT_TYPE(symbol, desc, name, type_create_fn, type_destroy_fn) extern "C" void symbol () { \
        dmGameObject::RegisterComponentTypeDescriptor((struct dmGameObject::ComponentTypeDescriptor*) &desc, name, type_create_fn, type_destroy_fn); \
    }

    /*# register a new component type
     * @param symbol [type: cpp_symbol_name] The unique C++ symbol name
     * @param name [type: const char*] name of the component type (i.e. the resource suffix)
     * @param create_fn [type: dmGameObject::Result (*fn)(const ComponentTypeCreateCtx* ctx, HComponentType type)] The type configuration function. May not be 0.
     * @param destroy_fn [type: dmGameObject::Result (*fn)(const ComponentTypeCreateCtx* ctx, HComponentType type)] The type destruction function. May be 0.
     */
    #define DM_DECLARE_COMPONENT_TYPE(symbol, name, type_create_fn, type_destroy_fn) \
        uint8_t DM_ALIGNED(16) DM_COMPONENT_PASTE_SYMREG2(symbol, __LINE__)[dmGameObject::s_ComponentTypeDescBufferSize]; \
        DM_REGISTER_COMPONENT_TYPE(symbol, DM_COMPONENT_PASTE_SYMREG2(symbol, __LINE__), name, type_create_fn, type_destroy_fn);

}

#endif // #ifndef DMSDK_GAMEOBJECT_COMPONENT_H
