// Copyright 2020-2022 The Defold Foundation
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
     * Parameters to ComponentNewWorld callback.
     */
    struct ComponentNewWorldParams
    {
        /// Context for the component type
        void* m_Context;
        /// Component type index that can be used later with GetWorld()
        uint8_t m_ComponentIndex;
        /// Max component game object instance count (if applicable)
        uint32_t m_MaxInstances;
        /// Out-parameter of the pointer in which to store the created world
        void** m_World;
        /// Max components count of this type in current collection counted at the build stage.
        /// If component in factory then value is 0xFFFFFFFF
        uint32_t m_MaxComponentInstances;
    };

    /*#
     * Component world create function
     * @param params Input parameters
     * @return CREATE_RESULT_OK on success
     */
    typedef CreateResult (*ComponentNewWorld)(const ComponentNewWorldParams& params);

    /*#
     * Parameters to ComponentDeleteWorld callback.
     */
    struct ComponentDeleteWorldParams
    {
        /// Context for the component type
        void* m_Context;
        /// The pointer to the world to destroy
        void* m_World;
    };

    /*#
     * Component world destroy function
     * @param params Input parameters
     * @param world The pointer to the world to destroy
     * @return CREATE_RESULT_OK on success
     */
    typedef CreateResult (*ComponentDeleteWorld)(const ComponentDeleteWorldParams& params);

    /*#
     * Parameters to ComponentCreate callback.
     */
    struct ComponentCreateParams
    {
        /// Game object instance
        HInstance m_Instance;
        /// Local component position
        dmVMath::Point3    m_Position;
        /// Local component rotation
        dmVMath::Quat      m_Rotation;
        /// Local component scale
        dmVMath::Vector3   m_Scale;
        PropertySet        m_PropertySet;
        /// Component resource
        void* m_Resource;
        /// Component world, as created in the ComponentNewWorld callback
        void* m_World;
        /// User context
        void* m_Context;
        /// User data storage pointer
        uintptr_t* m_UserData;
        /// Index of the component type being created (among all component types)
        uint16_t m_ComponentIndex;
    };

    /*#
     * Component create function. Should allocate all necessary resources for the component.
     * The game object instance is guaranteed to have its id, scene hierarchy and transform data updated when this is called.
     * @param params Input parameters
     * @return CREATE_RESULT_OK on success
     */
    typedef CreateResult (*ComponentCreate)(const ComponentCreateParams& params);

    /*#
     * Parameters to ComponentDestroy callback.
     */
    struct ComponentDestroyParams
    {
        /// Collection handle
        HCollection m_Collection;
        /// Game object instance
        HInstance m_Instance;
        /// Component world
        void* m_World;
        /// User context
        void* m_Context;
        /// User data storage pointer
        uintptr_t* m_UserData;
    };

    /*#
     * Component destroy function. Should deallocate all necessary resources.
     * @param params Input parameters
     * @return CREATE_RESULT_OK on success
     */
    typedef CreateResult (*ComponentDestroy)(const ComponentDestroyParams& params);

    /*#
     * Parameters to ComponentInit callback.
     */
    struct ComponentInitParams
    {
        /// Collection handle
        HCollection m_Collection;
        /// Game object instance
        HInstance m_Instance;
        /// Component world
        void* m_World;
        /// User context
        void* m_Context;
        /// User data storage pointer
        uintptr_t* m_UserData;
    };

    /*#
     * Component init function. Should set the components initial state as it is called when the component is enabled.
     * @param params Input parameters
     * @return CREATE_RESULT_OK on success
     */
    typedef CreateResult (*ComponentInit)(const ComponentInitParams& params);

    /*#
     * Parameters to ComponentFinal callback.
     */
    struct ComponentFinalParams
    {
        /// Collection handle
        HCollection m_Collection;
        /// Game object instance
        HInstance m_Instance;
        /// Component world
        void* m_World;
        /// User context
        void* m_Context;
        /// User data storage pointer
        uintptr_t* m_UserData;
    };

    /*#
     * Component finalize function. Should clean up as it is called when the component is disabled.
     * @param params Input parameters
     * @return CREATE_RESULT_OK on success
     */
    typedef CreateResult (*ComponentFinal)(const ComponentFinalParams& params);

    /*#
     * Parameters to ComponentAddToUpdate callback.
     */
    struct ComponentAddToUpdateParams
    {
        /// Collection handle
        HCollection m_Collection;
        /// Game object instance
        HInstance m_Instance;
        /// Component world
        void* m_World;
        /// User context
        void* m_Context;
        /// User data storage pointer
        uintptr_t* m_UserData;
    };

    /*#
     * Component add to update function. Only components called with this function should be included in the update passes.
     * @param params Input parameters
     * @return CREATE_RESULT_OK on success
     */
    typedef CreateResult (*ComponentAddToUpdate)(const ComponentAddToUpdateParams& params);

    /*#
     * Parameters to ComponentGet callback.
     */
    struct ComponentGetParams
    {
        /// Component world
        void* m_World;
        /// User data storage pointer
        uintptr_t* m_UserData;
    };

    /*#
     * A simple way to get the component instance from the user_data (which was set during creation)
     */
    typedef void* (*ComponentGet)(const ComponentGetParams& params);


    /*#
     * Parameters to ComponentsUpdate callback.
     */
    struct ComponentsUpdateParams
    {
        /// Collection handle
        HCollection m_Collection;
        /// Update context
        const UpdateContext* m_UpdateContext;
        /// Component world
        void* m_World;
        /// User context
        void* m_Context;
    };

    /*#
     * Parameters to ComponentsUpdate callback.
     */
    struct ComponentsUpdateResult
    {
        /// True if a component type updated any game object transforms
        bool m_TransformsUpdated;
    };

    /*#
     * Component update function. Updates all component of this type for all game objects
     * @param params Input parameters
     * @return UPDATE_RESULT_OK on success
     */
    typedef UpdateResult (*ComponentsUpdate)(const ComponentsUpdateParams& params, ComponentsUpdateResult& result);

    /*#
     * Component fixed update function. Updates all component of this type for all game objects
     * @param params Input parameters
     * @return UPDATE_RESULT_OK on success
     */
    typedef UpdateResult (*ComponentsFixedUpdate)(const ComponentsUpdateParams& params, ComponentsUpdateResult& result);

    /*#
     * Parameters to ComponentsRender callback.
     */
    struct ComponentsRenderParams
    {
        /// Collection handle
        HCollection m_Collection;
        /// Component world
        void* m_World;
        /// User context
        void* m_Context;
    };

    /*#
     * Component render function.
     * @param params Input parameters
     * @return UPDATE_RESULT_OK on success
     */
    typedef UpdateResult (*ComponentsRender)(const ComponentsRenderParams& params);

    /*#
     * Parameters for ComponentsPostUpdate callback.
     */
    struct ComponentsPostUpdateParams
    {
        /// Collection handle
        HCollection m_Collection;
        /// Component world
        void* m_World;
        /// User context
        void* m_Context;
    };

    /*#
     * Component post update function. The component state should never be modified in this function.
     * @param params Input parameters
     * @return UPDATE_RESULT_OK on success
     */
    typedef UpdateResult (*ComponentsPostUpdate)(const ComponentsPostUpdateParams& params);

    /*#
     * Parameters to ComponentOnMessage callback.
     */
    struct ComponentOnMessageParams
    {
        /// Instance handle
        HInstance m_Instance;
        /// World
        void* m_World;
        /// User context
        void* m_Context;
        /// User data storage pointer
        uintptr_t* m_UserData;
        /// Message
        dmMessage::Message* m_Message;
    };

    /*#
     * Component on-message function. Called when message is sent to this component
     * @param params Input parameters
     * @return UPDATE_RESULT_OK on success
     */
    typedef UpdateResult (*ComponentOnMessage)(const ComponentOnMessageParams& params);

    /*#
     * Parameters to ComponentOnInput callback.
     */
    struct ComponentOnInputParams
    {
        /// Instance handle
        HInstance m_Instance;
        /// Information about the input that occurred (note that input being released is also treated as input)
        const InputAction* m_InputAction;
        /// User context
        void* m_Context;
        /// User data storage pointer
        uintptr_t* m_UserData;
    };

    /*#
     * Component on-input function. Called when input is sent to this component
     * @param params Input parameters
     * @return How the component handled the input
     */
    typedef InputResult (*ComponentOnInput)(const ComponentOnInputParams& params);

    /*#
     * Parameters to ComponentOnReload callback.
     */
    struct ComponentOnReloadParams
    {
        /// Instance handle
        HInstance m_Instance;
        /// Resource that was reloaded
        void* m_Resource;
        /// Component world
        void* m_World;
        /// User context
        void* m_Context;
        /// User data storage pointer
        uintptr_t* m_UserData;
    };

    /*#
     * Called when the resource the component is based on has been reloaded.
     * @param params Input parameters
     */
    typedef void (*ComponentOnReload)(const ComponentOnReloadParams& params);

    /*#
     * Parameters to ComponentSetProperties callback.
     */
    struct ComponentSetPropertiesParams
    {
        /// Instance handle
        HInstance m_Instance;
        /// Property set to use
        PropertySet m_PropertySet;
        /// User data storage pointer
        uintptr_t* m_UserData;
    };

    /*#
     * Set a property set for the component.
     */
    typedef PropertyResult (*ComponentSetProperties)(const ComponentSetPropertiesParams& params);

    /*#
     * Parameters to ComponentGetProperty callback.
     */
    struct ComponentGetPropertyParams
    {
        /// Context for the component type
        void* m_Context;
        /// Component world
        void* m_World;
        /// Game object instance
        HInstance m_Instance;
        /// Id of the property
        dmhash_t m_PropertyId;
        /// User data storage pointer
        uintptr_t* m_UserData;
        /// Options for getting the property
        PropertyOptions m_Options;
    };

    /*#
     * Callback for retrieving a property value of the component.
     */
    typedef PropertyResult (*ComponentGetProperty)(const ComponentGetPropertyParams& params, PropertyDesc& out_value);

    /*#
     * Parameters to ComponentSetProperty callback.
     */
    struct ComponentSetPropertyParams
    {
        /// Context for the component type
        void* m_Context;
        /// Component world
        void* m_World;
        /// Game object instance
        HInstance m_Instance;
        /// Id of the property
        dmhash_t m_PropertyId;
        /// User data storage pointer
        uintptr_t* m_UserData;
        /// New value of the property
        PropertyVar m_Value;
        /// Options for setting the property
        PropertyOptions m_Options;
    };

    /*#
     * Callback for setting a property value of the component.
     */
    typedef PropertyResult (*ComponentSetProperty)(const ComponentSetPropertyParams& params);

    /*#
     * Callback when iterating over the properties for a component.
     * @note This function is only available/used in debug builds, when traversing the scene graph in order to export
     * this data for external tools (e.g. external testing libraries like Poco)
     * @name ComponentIterProperties
     * @param pit [type:dmGameObject::SceneNodePropertyIterator] the property iterator
     * @param node [type:dmGameObject::SceneNode*] the scene node
     */
    typedef void (*ComponentIterProperties)(dmGameObject::SceneNodePropertyIterator* pit, dmGameObject::SceneNode* node);


    /*#
     * Get the component type index. Used for with e.g. dmGameObject::GetWorld()/GetContext()
     * @name ComponentTypeGetTypeIndex
     * @param type [type: ComponentType*] the type
     * @return type_index [type: uint32_t] The type index.
     */
    uint32_t ComponentTypeGetTypeIndex(const ComponentType* type);

    /*# set the new world callback
     * Set the new world callback. Called when a collection (i.e. a "world") is created.
     * @name ComponentTypeSetNewWorldFn
     * @param type [type: ComponentType*] the type
     * @param fn [type: ComponentNewWorld] callback
     */
    void ComponentTypeSetNewWorldFn(ComponentType* type, ComponentNewWorld fn);

    /*# set the world destroy callback
     * Set the world destroy callback. Called when a collection (i.e. a "world") is destroyed.
     * @name ComponentTypeSetDeleteWorldFn
     * @param type [type: ComponentType*] the type
     * @param fn [type: ComponentDeleteWorld] callback
     */
    void ComponentTypeSetDeleteWorldFn(ComponentType* type, ComponentDeleteWorld fn);

    /*# set the component create callback
     * Set the component create callback. Called when a component instance is created.
     * @name ComponentTypeSetCreateFn
     * @param type [type: ComponentType*] the type
     * @param fn [type: ComponentCreate] callback
     */
    void ComponentTypeSetCreateFn(ComponentType* type, ComponentCreate fn);

    /*# set the component destroy callback
     * Set the component destroy callback. Called when a component instance is destroyed.
     * @name ComponentTypeSetDestroyFn
     * @param type [type: ComponentType*] the type
     * @param fn [type: ComponentDestroy] callback
     */
    void ComponentTypeSetDestroyFn(ComponentType* type, ComponentDestroy fn);

    /*# set the component init callback
     * Set the component init callback. Called on each gameobject's components, during a gameobject's initialization.
     * @name ComponentTypeSetInitFn
     * @param type [type: ComponentType*] the type
     * @param fn [type: ComponentInit] callback
     */
    void ComponentTypeSetInitFn(ComponentType* type, ComponentInit fn);

    /*# set the component finalize callback
     * Set the component finalize callback. Called on each gameobject's components, during a gameobject's finalization.
     * @name ComponentTypeSetFinalFn
     * @param type [type: ComponentType*] the type
     * @param fn [type: ComponentFinal] callback
     */
    void ComponentTypeSetFinalFn(ComponentType* type, ComponentFinal fn);

    /*# set the component add-to-update callback
     * Set the component add-to-update callback. Called for each component instal, when the game object is spawned.
     * @name ComponentTypeSetAddToUpdateFn
     * @param type [type: ComponentType*] the type
     * @param fn [type: ComponentAddToUpdate] callback
     */
    void ComponentTypeSetAddToUpdateFn(ComponentType* type, ComponentAddToUpdate fn);

    /*# set the component get callback
     * Set the component get callback. Called when the scripts want to retrieve the individual component user data given an url.
     * @name ComponentTypeSetGetFn
     * @param type [type: ComponentType*] the type
     * @param fn [type: ComponentGet] callback
     */
    void ComponentTypeSetGetFn(ComponentType* type, ComponentGet fn);

    /*# set the component render callback
     * Set the component render callback. Called when it's time to render all component instances.
     * @name ComponentTypeSetRenderFn
     * @param type [type: ComponentType*] the type
     * @param fn [type: ComponentsRender] callback
     */
    void ComponentTypeSetRenderFn(ComponentType* type, ComponentsRender fn);

    /*# set the component update callback
     * Set the component update callback. Called when it's time to update all component instances.
     * @name ComponentTypeSetUpdateFn
     * @param type [type: ComponentType*] the type
     * @param fn [type: ComponentsUpdate] callback
     */
    void ComponentTypeSetUpdateFn(ComponentType* type, ComponentsUpdate fn);

    /*# set the component update callback
     * Set the component update callback. Called when it's time to update all component instances.
     * @name ComponentTypeSetFixedUpdateFn
     * @param type [type: ComponentType*] the type
     * @param fn [type: ComponentsFixedUpdate] callback
     */
    void ComponentTypeSetFixedUpdateFn(ComponentType* type, ComponentsFixedUpdate fn);

    /*# set the component post update callback
     * Set the component post update callback. Called for each collection after the update, before the render.
     * @name ComponentTypeSetPostUpdateFn
     * @param type [type: ComponentType*] the type
     * @param fn [type: ComponentsPostUpdate] callback
     */
    void ComponentTypeSetPostUpdateFn(ComponentType* type, ComponentsPostUpdate fn);

    /*# set the component on-message callback
     * Set the component on-message callback. Called multiple times per frame, to flush messages.
     * @name ComponentTypeSetOnMessageFn
     * @param type [type: ComponentType*] the type
     * @param fn [type: ComponentOnMessage] callback
     */
    void ComponentTypeSetOnMessageFn(ComponentType* type, ComponentOnMessage fn);

    /*# set the component on-input callback
     * Set the component on-input callback. Called once per frame, before the Update function.
     * @name ComponentTypeSetOnInputFn
     * @param type [type: ComponentType*] the type
     * @param fn [type: ComponentOnInput] callback
     */
    void ComponentTypeSetOnInputFn(ComponentType* type, ComponentOnInput fn);

    /*# set the component on-reload callback
     * Set the component on-reload callback. Called when the resource of a component instance is reloaded.
     * @name ComponentTypeSetOnReloadFn
     * @param type [type: ComponentType*] the type
     * @param fn [type: ComponentOnReload] callback
     */
    void ComponentTypeSetOnReloadFn(ComponentType* type, ComponentOnReload fn);

    /*# set the component set properties callback
     * Set the component set properties callback. Called when the component instance is being spwned.
     * @name ComponentTypeSetSetPropertiesFn
     * @param type [type: ComponentType*] the type
     * @param fn [type: ComponentSetProperties] callback
     */
    void ComponentTypeSetSetPropertiesFn(ComponentType* type, ComponentSetProperties fn);

    /*# set the component get property callback
     * Set the component get property callback. Called when accessing a property via `go.get()`
     * @name ComponentTypeSetGetPropertyFn
     * @param type [type: ComponentType*] the type
     * @param fn [type: ComponentGetProperty] callback
     */
    void ComponentTypeSetGetPropertyFn(ComponentType* type, ComponentGetProperty fn);

    /*# set the component set property callback
     * Set the component set property callback. Called when accessing a property via `go.set()`
     * @name ComponentTypeSetSetPropertyFn
     * @param type [type: ComponentType*] the type
     * @param fn [type: ComponentSetProperty] callback
     */
    void ComponentTypeSetSetPropertyFn(ComponentType* type, ComponentSetProperty fn);

    /*# set the component type global context
     * Set the component type global context. Usually set when registering the component type.
     * @name ComponentTypeSetContext
     * @param type [type: ComponentType*] the type
     * @param context [type: void*] component type global context
     */
    void ComponentTypeSetContext(ComponentType* type, void* context);

    /*# get the component type global context
     * get the component type global context
     * @name ComponentTypeGetContext
     * @param type [type: ComponentType*] the type
     * @return context [type: void*] component type global context
     */
    void* ComponentTypeGetContext(const ComponentType* type);

    /*# set the component type transform dependency flag
     * Set the component type transform dependency flag.
     * If this flag is set, it might trigger an dmGameObject::UpdateTransforms() (if there are dirty transforms)
     * @name ComponentTypeSetReadsTransforms
     * @param type [type: ComponentType*] the type
     * @param reads_transforms [type: bool] transform dependency flag
     */
    void ComponentTypeSetReadsTransforms(ComponentType* type, bool reads_transforms);

    /*# set the component type prio order
     * Set the component type prio order. Defines the update order of the component types.
     * @name ComponentTypeSetPrio
     * @param type [type: ComponentType*] the type
     * @param prio [type: uint16_t] prio order
     */
    void ComponentTypeSetPrio(ComponentType* type, uint16_t prio);

    /*# set the component type need for a per component instance user data
     * Set the component type need for a per component instance user data. Defaults to true.
     * @name ComponentTypeSetHasUserData
     * @param type [type: ComponentType*] the type
     * @param has_user_data [type: bool] does each component instance need user data
     */
    void ComponentTypeSetHasUserData(ComponentType* type, bool has_user_data);

    /*# set the component child iterator function
     * set the component child iterator function. Called during inspection
     * @name ComponentTypeSetChildIteratorFn
     * @param type [type: ComponentType*] the type
     * @param fn [type: FIteratorChildren] child iterator function
     */
    void ComponentTypeSetChildIteratorFn(ComponentType* type, FIteratorChildren fn);

    /*# set the component property iterator function
     * set the component property iterator function. Called during inspection
     * @name ComponentTypeSetPropertyIteratorFn
     * @param type [type: ComponentType*] the type
     * @param fn [type: FIteratorProperties] property iterator function
     */
    void ComponentTypeSetPropertyIteratorFn(ComponentType* type, FIteratorProperties fn);

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

    typedef Result (*ComponentTypeCreateFunction)(const ComponentTypeCreateCtx* ctx, ComponentType* type);
    typedef Result (*ComponentTypeDestroyFunction)(const ComponentTypeCreateCtx* ctx, ComponentType* type);

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

    #ifdef __GNUC__
        // Workaround for dead-stripping on OSX/iOS. The symbol "name" is explicitly exported. See wscript "exported_symbols"
        // Otherwise it's dead-stripped even though -no_dead_strip_inits_and_terms is passed to the linker
        // The bug only happens when the symbol is in a static library though
        #define DM_REGISTER_COMPONENT_TYPE(symbol, desc, name, type_create_fn, type_destroy_fn) extern "C" void __attribute__((constructor)) symbol () { \
            dmGameObject::RegisterComponentTypeDescriptor((struct dmGameObject::ComponentTypeDescriptor*) &desc, name, type_create_fn, type_destroy_fn); \
        }
    #else
        #define DM_REGISTER_COMPONENT_TYPE(symbol, desc, name, type_create_fn, type_destroy_fn) extern "C" void symbol () { \
            dmGameObject::RegisterComponentTypeDescriptor((struct dmGameObject::ComponentTypeDescriptor*) &desc, name, type_create_fn, type_destroy_fn); \
            }\
            int symbol ## Wrapper(void) { symbol(); return 0; } \
            __pragma(section(".CRT$XCU",read)) \
            __declspec(allocate(".CRT$XCU")) int (* _Fp ## symbol)(void) = symbol ## Wrapper;
    #endif

    /*# register a new component type
     * @param symbol [type: cpp_symbol_name] The unique C++ symbol name
     * @param name [type: const char*] name of the component type (i.e. the resource suffix)
     * @param create_fn [type: dmGameObject::Result (*fn)(const ComponentTypeCreateCtx* ctx, ComponentType* type)] The type configuration function. May not be 0.
     * @param destroy_fn [type: dmGameObject::Result (*fn)(const ComponentTypeCreateCtx* ctx, ComponentType* type)] The type destruction function. May be 0.
     */
    #define DM_DECLARE_COMPONENT_TYPE(symbol, name, type_create_fn, type_destroy_fn) \
        uint8_t DM_ALIGNED(16) DM_COMPONENT_PASTE_SYMREG2(symbol, __LINE__)[dmGameObject::s_ComponentTypeDescBufferSize]; \
        DM_REGISTER_COMPONENT_TYPE(symbol, DM_COMPONENT_PASTE_SYMREG2(symbol, __LINE__), name, type_create_fn, type_destroy_fn);

}

#endif // #ifndef DMSDK_GAMEOBJECT_COMPONENT_H
