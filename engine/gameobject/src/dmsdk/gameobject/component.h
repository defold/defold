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
#include <dmsdk/dlib/configfile.h>
#include <dmsdk/script/script.h>

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
        Point3    m_Position;
        /// Local component rotation
        Quat      m_Rotation;
        PropertySet m_PropertySet;
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
    };

    /*#
     * Callback for setting a property value of the component.
     */
    typedef PropertyResult (*ComponentSetProperty)(const ComponentSetPropertyParams& params);


    /*#
     */
    void ComponentTypeSetNewWorldFn(ComponentType* type, ComponentNewWorld fn);
    void ComponentTypeSetDeleteWorldFn(ComponentType* type, ComponentDeleteWorld fn);
    void ComponentTypeSetCreateFn(ComponentType* type, ComponentCreate fn);
    void ComponentTypeSetDestroyFn(ComponentType* type, ComponentDestroy fn);
    void ComponentTypeSetInitFn(ComponentType* type, ComponentInit fn);
    void ComponentTypeSetFinalFn(ComponentType* type, ComponentFinal fn);
    void ComponentTypeSetAddToUpdateFn(ComponentType* type, ComponentAddToUpdate fn);
    void ComponentTypeSetGetFn(ComponentType* type, ComponentGet fn);
    void ComponentTypeSetRenderFn(ComponentType* type, ComponentsRender fn);
    void ComponentTypeSetUpdateFn(ComponentType* type, ComponentsUpdate fn);
    void ComponentTypeSetPostUpdateFn(ComponentType* type, ComponentsPostUpdate fn);
    void ComponentTypeSetOnMessageFn(ComponentType* type, ComponentOnMessage fn);
    void ComponentTypeSetOnInputFn(ComponentType* type, ComponentOnInput fn);
    void ComponentTypeSetOnReloadFn(ComponentType* type, ComponentOnReload fn);
    void ComponentTypeSetSetPropertiesFn(ComponentType* type, ComponentSetProperties fn);
    void ComponentTypeSetGetPropertyFn(ComponentType* type, ComponentGetProperty fn);
    void ComponentTypeSetSetPropertyFn(ComponentType* type, ComponentSetProperty fn);
    void ComponentTypeSetContext(ComponentType* type, void* context);
    void ComponentTypeSetReadsTransforms(ComponentType* type, bool reads_transforms);
    void ComponentTypeSetPrio(ComponentType* type, uint16_t prio);
    /// defaults to true
    void ComponentTypeSetHasUserData(ComponentType* type, bool has_user_data);



    /*#
     * Register a new component type (Internal)
     * @param regist Gameobject register
     * @param type Collection of component type registration data
     * @return RESULT_OK on success
     */
    struct ComponentTypeCreateCtx {
        dmConfigFile::HConfig    m_Config;
        dmResource::HFactory     m_Factory;
        dmGameObject::HRegister  m_Register;
        dmScript::HContext       m_Script;
    };

    typedef Result (*ComponentTypeCreateFunction)(const ComponentTypeCreateCtx* ctx, ComponentType* type);

    struct ComponentTypeDescriptor;

    /**
     * Register a new component type (Internal)
     * @param regist Gameobject register
     * @param type Collection of component type registration data
     * @return RESULT_OK on success
     */
    Result RegisterComponentTypeDescriptor(ComponentTypeDescriptor* desc, const char* name, ComponentTypeCreateFunction create_fn);

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
        #define DM_REGISTER_COMPONENT_TYPE(symbol, desc, name, type_create_fn) extern "C" void __attribute__((constructor)) symbol () { \
            dmGameObject::RegisterComponentTypeDescriptor((struct dmGameObject::ComponentTypeDescriptor*) &desc, name, type_create_fn); \
        }
    #else
        #define DM_REGISTER_COMPONENT_TYPE(symbol, desc, name, type_create_fn) extern "C" void symbol () { \
            dmGameObject::RegisterComponentTypeDescriptor((struct dmGameObject::ComponentTypeDescriptor*) &desc, name, type_create_fn); \
            }\
            int symbol ## Wrapper(void) { symbol(); return 0; } \
            __pragma(section(".CRT$XCU",read)) \
            __declspec(allocate(".CRT$XCU")) int (* _Fp ## symbol)(void) = symbol ## Wrapper;
    #endif


    /*#
     */
    #define DM_DECLARE_COMPONENT_TYPE(symbol, name, type_create_fn) \
        uint8_t DM_ALIGNED(16) DM_COMPONENT_PASTE_SYMREG2(symbol, __LINE__)[dmGameObject::s_ComponentTypeDescBufferSize]; \
        DM_REGISTER_COMPONENT_TYPE(symbol, DM_COMPONENT_PASTE_SYMREG2(symbol, __LINE__), name, type_create_fn);


}

#endif // #ifndef DMSDK_GAMEOBJECT_COMPONENT_H
