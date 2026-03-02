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

#ifndef DM_GAMEOBJECT_H
#define DM_GAMEOBJECT_H

#include <stdint.h>

#include <dmsdk/gameobject/gameobject.h>

#include <dlib/easing.h>
#include <dlib/hashtable.h>
#include <dlib/message.h>
#include <dlib/transform.h>

#include <ddf/ddf.h>

#include <script/script.h>

#include <resource/resource.h>

namespace dmGameObject
{
    using namespace dmVMath;

    /// Default max instances in collection
    const uint32_t DEFAULT_MAX_COLLECTION_CAPACITY = 1024;

    /// Default max instances in input stack
    const uint32_t DEFAULT_MAX_INPUT_STACK_CAPACITY = 16;

    /// Config key to use for tweaking maximum number of instances in a collection
    extern const char* COLLECTION_MAX_INSTANCES_KEY;

    /// Config key to use for tweaking the maximum capacity of the input stack
    extern const char* COLLECTION_MAX_INPUT_STACK_ENTRIES_KEY;

    extern const dmhash_t UNNAMED_IDENTIFIER;

    typedef struct PropertyContainer* HPropertyContainer;

    /**
     * Create a new component type register
     * @param regist Register
     * @return Register handle
     */
    HRegister NewRegister();

    /**
     * Delete a component type register
     * @param regist Register to delete
     */
    void DeleteRegister(HRegister regist);

    /**
     * Delete a the loaded collections
     * @param regist the register
     */
    void DeleteCollections(HRegister regist);

    /**
     * Initialize system
     * @param context Script context
     */
    void Initialize(HRegister regist, dmScript::HContext context);

    /**
     * Set default capacity of collections in this register. This does not affect existing collections.
     * @param regist Register
     * @param capacity Default capacity of collections in this register (0-65534).
     * @return RESULT_OK on success or RESULT_INVALID_OPERATION if max_count is not within range
     */
    Result SetCollectionDefaultCapacity(HRegister regist, uint32_t capacity);

    /**
     * Get default capacity of collections in this register.
     * @param regist Register
     * @return Default capacity
     */
    uint32_t GetCollectionDefaultCapacity(HRegister regist);

    /**
     * Set default input stack capacity of collections in this register. This does not affect existing collections.
     * @param regist Register
     * @param capacity Default capacity of collections in this register.
     */
    void SetInputStackDefaultCapacity(HRegister regist, uint32_t capacity);

    /**
     * Creates a new gameobject collection
     * @param name Collection name, which must be unique and follow the same naming as for sockets
     * @param factory Resource factory. Must be valid during the life-time of the collection
     * @param regist Register
     * @param max_instances Max instances in this collection
     * @param collection_desc description data of collections
     * @return HCollection
     */
    HCollection NewCollection(const char* name, dmResource::HFactory factory, HRegister regist, uint32_t max_instances, HCollectionDesc collection_desc);

    /**
     * Deletes a gameobject collection
     * @param collection
     */
    void DeleteCollection(HCollection collection);

    /**
     * Return an instance index to the index pool for the collection.
     * @param index The index to return.
     * @param collection Collection that the index should be returned to.
     */
    void ReleaseInstanceIndex(uint32_t index, HCollection collection);

    /**
     * Spawns a new game object instance.
     * @param collection Collection to spawn into
     * @param proto Prototype
     * @param prototype_name Name of the prototype
     * @param id Identifier for the new instance, must be unique within the collection
     * @param property_container Properties that should be set on the new instance
     * @param position Position
     * @param rotation Rotation
     * @param scale Scale
     * @param out_instance Output parameter for the new instance
     * @return RESULT_OK on success
     */
    Result Spawn(HCollection collection, HPrototype proto, const char *prototype_name, dmhash_t id, 
                        HPropertyContainer property_container, const Point3& position, const Quat& rotation, const Vector3& scale, HInstance* out_instance);

    /**
     * Spawns a collection into an existing one, from a collection definition resource. Script properties
     * can be overridden by passing property buffers through the property_buffers hashtable. An empty game
     * object is created under which all spawned children are placed.
     *
     * @param collection Gameobject collection to spawn into
     * @param collection_desc Description data of collections
     * @param id_prefix Identifier prefix, must start with a forward slash (/) and must be unique within the collection. Pass nullptr to use the default identifier (e.g. /collection1, /collection2 etc.).
     * @param property_containers Serialized property buffers hashtable (key: game object identifier, value: property buffer)
     * @param position Position for the root object
     * @param rotation Rotation for the root object
     * @param scale Scale of the root object
     * @param instances Hash table to be filled with instance identifier mapping.
     * @return RESULT_OK on success
     */
    Result SpawnFromCollection(HCollection collection, HCollectionDesc collection_desc, const char* id_prefix, 
                             InstancePropertyContainers *property_containers,
                             const Point3& position, const Quat& rotation, const Vector3& scale,
                             InstanceIdMap *out_instances);

    /**
     * Delete all gameobject instances in the collection
     * @param collection Gameobject collection
     */
    void DeleteAll(HCollection collection);

    /**
     * Set instance identifier. Must be unique within the collection.
     * @param collection Collection
     * @param instance Instance
     * @param identifier Identifier
     * @return RESULT_OK on success
     */
    Result SetIdentifier(HCollection collection, HInstance instance, const char* identifier);

    /**
     * Get component index from component identifier. This function has complexity O(n), where n is the number of components of the instance.
     * @param instance Instance
     * @param component_id Component id
     * @param component_index Component index as out-argument
     * @return RESULT_OK if the comopnent was found
     */
    Result GetComponentIndex(HInstance instance, dmhash_t component_id, uint16_t* component_index);

    /**
     * Initializes all game object instances in the supplied collection.
     * @param collection Game object collection
     */
    bool Init(HCollection collection);

    /**
     * Finalizes all game object instances in the supplied collection.
     * @param collection Game object collection
     */
    bool Final(HCollection collection);

    /**
     * Update all gameobjects and its components and dispatches all message to script.
     * The order is to update each component type, one at a time, of the collection each iteration.
     * @param collection Game object collection to be updated
     * @param update_context Update contexts
     * @return True on success
     */
    bool Update(HCollection collection, const UpdateContext* update_context);

    /**
     * Render all components in all game objects.
     * @param collection Collection to be rendered
     * @return True on success
     */
    bool Render(HCollection collection);

    /**
     * Performs clean up of the collection after update, such as deleting all instances scheduled for delete.
     * @param collection Game object collection
     * @return True on success
     */
    bool PostUpdate(HCollection collection);

    /**
     * Performs clean up of the register after update, such as deleting all collections scheduled for delete.
     * @param reg Game object register
     * @return True on success
     */
    bool PostUpdate(HRegister reg);

    /**
     * Dispatches input actions to the input focus stacks in the supplied game object collection.
     * @param collection Game object collection
     * @param input_actions Array of input actions to dispatch
     * @param input_action_count Number of input actions
     */
    UpdateResult DispatchInput(HCollection collection, InputAction* input_actions, uint32_t input_action_count);

    void AcquireInputFocus(HCollection collection, HInstance instance);
    void ReleaseInputFocus(HCollection collection, HInstance instance);

    /**
     * Retrieve a factory from the specified collection
     * @param collection Game object collection
     * @return The resource factory bound to the specified collection
     */
    dmResource::HFactory GetFactory(HCollection collection);

    /**
     * Retrieve a factory from the specified instance
     * Convenience for GetFactory(GetCollection(instance)).
     * @param instance Game object instance
     * @return The resource factory bound to the specified instance, via its collection
     */
    dmResource::HFactory GetFactory(HInstance instance);

    /**
     * Retrieve a register from the specified collection
     * @param collection Game object collection
     * @return The register bound to the specified collection
     */
    HRegister GetRegister(HCollection collection);

    /**
     * Retrieve the frame message socket for the specified collection.
     * @param collection Collection handle
     * @return The frame message socket of the specified collection
     */
    dmMessage::HSocket GetFrameMessageSocket(HCollection collection);

    /**
     * Get instance hierarchical depth
     * @param instance Gameobject instance
     * @return Hierarchical depth
     */
    uint32_t GetDepth(HInstance instance);

    /**
     * Get child count
     * @note O(n) operation. Should only be used for debugging purposes.
     * @param instance Gameobject instance
     * @return Child count
     */
    uint32_t GetChildCount(HInstance instance);

    /**
     * Test if "child" is a direct parent of "parent"
     * @param child Child Gamebject
     * @param parent Parent Gameobject
     * @return True if child of
     */
    bool IsChildOf(HInstance child, HInstance parent);

    /**
     * Retrieve a property from a component.
     * @param instance Instance of the game object
     * @param component_id [type:dmhash_t] Id of the component
     * @param property_id [type:dmhash_t] Id of the property
     * @param options [type:PropertyOptions] Additional options when getting value
     * @param out_value [type:PropertyDesc] Description of the retrieved property value
     * @return PROPERTY_RESULT_OK if the out-parameters were written
     */
    PropertyResult GetProperty(HInstance instance, dmhash_t component_id, dmhash_t property_id, PropertyOptions options, PropertyDesc& out_value);

    /**
     * Sets the value of a property on a component.
     * @param instance Instance of the game object
     * @param component_id [type:dmhash_t] Id of the component
     * @param property_id [type:dmhash_t] Id of the property
     * @param options [type:PropertyOptions] Additional options when setting value
     * @param value [type:PropertyVar] Value and type of the property
     * @return PROPERTY_RESULT_OK if the value could be set
     */
    PropertyResult SetProperty(HInstance instance, dmhash_t component_id, dmhash_t property_id, PropertyOptions options, const PropertyVar& value);

    typedef void (*AnimationStopped)(dmGameObject::HInstance instance, dmhash_t component_id, dmhash_t property_id,
                                        bool finished, void* userdata1, void* userdata2);

    PropertyResult Animate(HCollection collection, HInstance instance, dmhash_t component_id,
                     dmhash_t property_id,
                     Playback playback,
                     PropertyVar& to,
                     dmEasing::Curve easing,
                     float duration,
                     float delay,
                     AnimationStopped animation_stopped,
                     void* userdata1, void* userdata2);

    PropertyResult CancelAnimations(HCollection collection, HInstance instance, dmhash_t component_id,
                     dmhash_t property_id);
    /**
     * Cancel all animations belonging to the specified instance.
     * @param collection Collection the instance belongs to
     * @param instance Instance for which to cancel all animations
     */
    void CancelAnimations(HCollection collection, HInstance instance);

    /**
     * Cancel animation callbacks installed during callbacks to Animate
     * @param userdata1 corresponds to the userdata1 parameter passed to Animate.
     */
    void CancelAnimationCallbacks(HCollection collection, void* userdata1);

    struct ModuleContext
    {
        dmArray<dmScript::HContext> m_ScriptContexts;
    };

    /*
     * Allows for flushing any pending messages
    */
    bool DispatchMessages(HCollection hcollection, dmMessage::HSocket* sockets, uint32_t socket_count);

    /*
     * Allows for updating transforms an extra time
     */
    void UpdateTransforms(HCollection hcollection);

    /**
     * Remove the reference to a dynamically created resource. This implies
     * that the resource has been externally removed and should no longer be
     * tracked by the collection, e.g resource.release(id) has been called
     */
    void RemoveDynamicResourceHash(HCollection collection, dmhash_t resource_hash);
}

#endif // DM_GAMEOBJECT_H
