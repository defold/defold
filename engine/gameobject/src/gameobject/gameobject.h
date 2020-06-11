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

#ifndef GAMEOBJECT_H
#define GAMEOBJECT_H

#include <stdint.h>
#include <dmsdk/vectormath/cpp/vectormath_aos.h>
#include <dmsdk/gameobject/gameobject.h>

#include <dlib/easing.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include <dlib/message.h>
#include <dlib/transform.h>

#include <ddf/ddf.h>

#include <hid/hid.h>

#include <script/script.h>

#include <resource/resource.h>

namespace dmGameObject
{
    using namespace Vectormath::Aos;

    /// Default max instances in collection
    const uint32_t DEFAULT_MAX_COLLECTION_CAPACITY = 1024;

    // Value for an invalid instance index, this must be the same as defined in
    // gamesys_ddf.proto for Create#index.
    const uint32_t INVALID_INSTANCE_POOL_INDEX = 0xffffffff;

    /// Config key to use for tweaking maximum number of instances in a collection
    extern const char* COLLECTION_MAX_INSTANCES_KEY;

    /// Script handle
    typedef struct Script* HScript;

    /// Instance handle
    typedef struct ScriptInstance* HScriptInstance;

    /// Properties handle
    typedef struct Properties* HProperties;

    /// Prototype handle
    typedef struct Prototype* HPrototype;

    typedef void* HCollectionDesc;


    /**
     * Input result enum
     */
    enum InputResult
    {
        INPUT_RESULT_IGNORED = 0,           //!< INPUT_RESULT_IGNORED
        INPUT_RESULT_CONSUMED = 1,          //!< INPUT_RESULT_CONSUMED
        INPUT_RESULT_UNKNOWN_ERROR = -1000  //!< INPUT_RESULT_UNKNOWN_ERROR
    };

    /**
     * Update context
     */
    struct UpdateContext
    {
        /// Time step
        float m_DT;
    };

    extern const dmhash_t UNNAMED_IDENTIFIER;

    /**
     * Container of input related information.
     */
    struct InputAction
    {
        InputAction();

        /// Action id, hashed action name
        dmhash_t m_ActionId;
        /// Value of the input [0,1]
        float m_Value;
        /// Cursor X coordinate, in virtual screen space
        float m_X;
        /// Cursor Y coordinate, in virtual screen space
        float m_Y;
        /// Cursor dx since last frame, in virtual screen space
        float m_DX;
        /// Cursor dy since last frame, in virtual screen space
        float m_DY;
        /// Cursor X coordinate, in screen space
        float m_ScreenX;
        /// Cursor Y coordinate, in screen space
        float m_ScreenY;
        /// Cursor dx since last frame, in screen space
        float m_ScreenDX;
        /// Cursor dy since last frame, in screen space
        float m_ScreenDY;
        float m_AccX, m_AccY, m_AccZ;
        /// Touch data
        dmHID::Touch m_Touch[dmHID::MAX_TOUCH_COUNT];
        /// Number of m_Touch
        int32_t  m_TouchCount;
        char     m_Text[dmHID::MAX_CHAR_COUNT];
        uint32_t m_TextCount;
        uint32_t m_GamepadIndex;
        uint8_t  m_IsGamepad : 1;
        uint8_t  m_GamepadDisconnected : 1;
        uint8_t  m_GamepadConnected : 1;
        /// If input has a text payload (can be true even if text count is 0)
        uint8_t  m_HasText : 1;
        /// If the input was 0 last update
        uint8_t  m_Pressed : 1;
        /// If the input turned from above 0 to 0 this update
        uint8_t  m_Released : 1;
        /// If the input was held enough for the value to be repeated this update
        uint8_t  m_Repeated : 1;
        /// If the position fields (m_X, m_Y, m_DX, m_DY) were set and valid to read
        uint8_t  m_PositionSet : 1;
        /// If the accelerometer fields (m_AccX, m_AccY, m_AccZ) were set and valid to read
        uint8_t  m_AccelerationSet : 1;
        /// If the input action was consumed in an event dispatch
        uint8_t  m_Consumed : 1;
    };

    /**
     * Supported property types.
     */
    enum PropertyType
    {
        // NOTE These must match the values in gameobject_ddf.proto
        PROPERTY_TYPE_NUMBER = 0,
        PROPERTY_TYPE_HASH = 1,
        PROPERTY_TYPE_URL = 2,
        PROPERTY_TYPE_VECTOR3 = 3,
        PROPERTY_TYPE_VECTOR4 = 4,
        PROPERTY_TYPE_QUAT = 5,
        PROPERTY_TYPE_BOOLEAN = 6,
        PROPERTY_TYPE_COUNT
    };

    struct PropertyVar
    {
        PropertyVar();
        PropertyVar(float v);
        PropertyVar(double v);
        PropertyVar(dmhash_t v);
        PropertyVar(const dmMessage::URL& v);
        PropertyVar(Vectormath::Aos::Vector3 v);
        PropertyVar(Vectormath::Aos::Vector4 v);
        PropertyVar(Vectormath::Aos::Quat v);
        PropertyVar(bool v);

        PropertyType m_Type;
        union
        {
            double m_Number;
            dmhash_t m_Hash;
            // NOTE: We can't store an URL as is due to constructor
            char  m_URL[sizeof(dmMessage::URL)];
            float m_V4[4];
            bool m_Bool;
        };
    };

    /**
     * Description of a property.
     *
     * If the property is externally mutable, m_ValuePtr points to the value and its length is m_ElementCount.
     * m_Variant always reflects the value.
     */
    struct PropertyDesc
    {
        PropertyDesc();

        /// For composite properties (float arrays), these ids name each element
        dmhash_t m_ElementIds[4];
        /// Variant holding the value
        PropertyVar m_Variant;
        /// Pointer to the value, only set for mutable values. The actual data type is described by the variant.
        float* m_ValuePtr;
        /// Determines whether we are permitted to write to this property.
        bool m_ReadOnly;
    };

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
     * Initialize system
     * @param context Script context
     */
    void Initialize(HRegister regist, dmScript::HContext context);

    /**
     * Set default capacity of collections in this register. This does not affect existing collections.
     * @param regist Register
     * @param capacity Default capacity of collections in this register (0-32766).
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
     * Creates a new gameobject collection
     * @param name Collection name, which must be unique and follow the same naming as for sockets
     * @param factory Resource factory. Must be valid during the life-time of the collection
     * @param regist Register
     * @param max_instances Max instances in this collection
     * @return HCollection
     */
    HCollection NewCollection(const char* name, dmResource::HFactory factory, HRegister regist, uint32_t max_instances);

    /**
     * Deletes a gameobject collection
     * @param collection
     */
    void DeleteCollection(HCollection collection);

    /**
     * Retrieve the world in the collection connected to the supplied component
     * @param collection Collection handle
     * @param component_index index of the component for which the world is to be retrieved
     * @return a pointer to the world, 0x0 if not found
     */
    void* GetWorld(HCollection collection, uint32_t component_index);

    /**
     * Create a new gameobject instance
     * @note Calling this function during update is not permitted. Use #Spawn instead for deferred creation
     * @param collection Gameobject collection
     * @param prototype_name Prototype file name
     * @return New gameobject instance. NULL if any error occured
     */
    HInstance New(HCollection collection, const char* prototype_name);

    /**
     * Construct a hash of an instance id based on the index provided.
     * @param index The index to base the id off of.
     * @return hash of the instance id constructed.
     */
    dmhash_t ConstructInstanceId(uint32_t index);

    /**
     * Retrieve an instance index from the index pool for the collection.
     * @param collection Collection from which to retrieve the instance index.
     * @return instance index from the index pool of collection.
     */
    uint32_t AcquireInstanceIndex(HCollection collection);

    /**
     * Return an instance index to the index pool for the collection.
     * @param index The index to return.
     * @param collection Collection that the index should be returned to.
     */
    void ReleaseInstanceIndex(uint32_t index, HCollection collection);

    /**
     * Assign an index to the instance, only if the instance is not null.
     * @param index The index to assign.
     * @param instance The instance that should be assigned the index.
     */
    void AssignInstanceIndex(uint32_t index, HInstance instance);

    /**
     * Spawns a new gameobject instance. The actual creation is performed after the update is completed.
     * @param collection Gameobject collection
     * @param prototype_name Prototype file name
     * @param id Id of the spawned instance
     * @param property_buffer Buffer with serialized properties
     * @param property_buffer_size Size of property buffer
     * @param position Position of the spawed object
     * @param rotation Rotation of the spawned object
     * @param scale Scale of the spawned object
     * return the spawned instance, 0 at failure
     */
    HInstance Spawn(HCollection collection, HPrototype prototype, const char* prototype_name, dmhash_t id, uint8_t* property_buffer, uint32_t property_buffer_size, const Point3& position, const Quat& rotation, const Vector3& scale);

    struct InstancePropertyBuffer
    {
        uint8_t *property_buffer;
        uint32_t property_buffer_size;
    };

    /**
     * Used for mapping instance ids from a collection definition to newly spawned instances
     */
    typedef dmHashTable<dmhash_t, dmhash_t> InstanceIdMap;

    /**
     * Contains property buffers for game objects to be spawned
     */
    typedef dmHashTable<dmhash_t, InstancePropertyBuffer> InstancePropertyBuffers;

    /**
     * Spawns a collection into an existing one, from a collection definition resource. Script properties
     * can be overridden by passing property buffers through the property_buffers hashtable. An empty game
     * object is created under which all spawned children are placed.
     *
     * @param collection Gameobject collection to spawn into
     * @param prototype_name Collection file name
     * @param property_buffers Serialized property buffers hashtable (key: game object identifier, value: property buffer)
     * @param position Position for the root object
     * @param rotation Rotation for the root object
     * @param scale Scale of the root object
     * @param instances Hash table to be filled with instance identifier mapping.
     * return true on success
     */
    bool SpawnFromCollection(HCollection collection, HCollectionDesc collection_desc, InstancePropertyBuffers *property_buffers,
                             const Point3& position, const Quat& rotation, const Vector3& scale,
                             InstanceIdMap *instances);

    /**
     * Delete gameobject instance
     * @param collection Gameobject collection
     * @param instance Gameobject instance
     * @param recursive Delete child hierarchy recursively, child to parent order (leaf first)
     */
    void Delete(HCollection collection, HInstance instance, bool recursive);

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
     * Set instance identifier. Must be unique within the collection.
     * @param collection Collection
     * @param instance Instance
     * @param identifier Identifier
     * @return RESULT_OK on success
     */
    Result SetIdentifier(HCollection collection, HInstance instance, dmhash_t identifier);

    /**
     * Get instance identifier
     * @param instance Instance
     * @return Identifier. dmGameObject::UNNAMED_IDENTIFIER if not set.
     */
    dmhash_t GetIdentifier(HInstance instance);

    /**
     * Get absolute identifier relative to #instance. The returned identifier is the
     * representation of the qualified name, i.e. the path from root-collection to the sub-collection which the #instance belongs to.
     * Example: if #instance is part of a sub-collection in the root-collection named "sub" and id == "a" the returned identifier represents the path "sub.a"
     * @param instance Instance to absolute identifier to
     * @param id Identifier relative to #instance
     * @param id_size Lenght of the id
     * @return Absolute identifier
     */
    dmhash_t GetAbsoluteIdentifier(HInstance instance, const char* id, uint32_t id_size);

    /**
     * Get instance from identifier
     * @param collection Collection
     * @param identifier Identifier
     * @return Instance. NULL if instance isn't found.
     */
    HInstance GetInstanceFromIdentifier(HCollection collection, dmhash_t identifier);

    /**
     * Get component user data from lua-argument. This function is typically used from lua-bindings
     * and can only be used from protected lua-calls as luaL_error might be invoked
     * @param L lua-state
     * @param index index to argument
     * @param collection in which to search
     * @param component_ext when specified, the call will fail if the found component does not have the specified extension
     * @param user_data will be overwritten component user-data output if available
     * @param url will be overwritten with a URL to the component when specified
     * @param world world associated when specified
     */
    void GetComponentUserDataFromLua(lua_State* L, int index, HCollection collection, const char* component_ext, uintptr_t* out_user_data, dmMessage::URL* out_url, void** world);

    /**
     * Gets a component given an URL
     * @param url the url to the object
     * @return the component matching the url. returns null if no match was found
     */
    void* GetComponentFromURL(const dmMessage::URL& url);

    /**
     * Get current game object instance from the lua state, if any.
     * The lua state has an instance while the script callbacks are being run on the state.
     * @param L lua-state
     * @return current game object instance
     */
    HInstance GetInstanceFromLua(lua_State* L);

    /**
     * Get component index from component identifier. This function has complexity O(n), where n is the number of components of the instance.
     * @param instance Instance
     * @param component_id Component id
     * @param component_index Component index as out-argument
     * @return RESULT_OK if the comopnent was found
     */
    Result GetComponentIndex(HInstance instance, dmhash_t component_id, uint16_t* component_index);

    /**
     * Get component id from component index.
     * @param instance Instance
     * @param component_index Component index
     * @param component_id Component id as out-argument
     * @return RESULT_OK if the comopnent was found
     */
    Result GetComponentId(HInstance instance, uint16_t component_index, dmhash_t* component_id);

    /**
     * Returns whether the scale of the supplied instance should be applied along Z or not.
     * @param instance Instance
     * @return if the scale should be applied along Z
     */
    bool ScaleAlongZ(HInstance instance);

    /**
     * Set whether the instance should inherit the scale from its parent or not.
     * @param instance Instance
     * @param inherit_scale true if the instance should inherit scale
     */
    void SetInheritScale(HInstance instance, bool inherit_scale);

    /**
     * Tells the collection that a transform was updated
     */
    void SetDirtyTransforms(HCollection collection);

    /**
     * Set whether the instance should be flagged as a bone.
     * Instances flagged as bones can have their transforms updated in a batch through SetBoneTransforms.
     * Used for animated skeletons.
     * @param instance Instance
     * @param bone true if the instance is a bone
     */
    void SetBone(HInstance instance, bool bone);

    /**
     * Check whether the instance is flagged as a bone.
     * @param instance Instance
     * @return True if flagged as a bone
     */
    bool IsBone(HInstance instance);

    /**
     * Set the local transforms recursively of all instances flagged as bones, starting with component with id.
     * The order of the transforms is depth-first.
     * @param instance First Instance of the hierarchy to set
     * @param component_transform the transform for component root
     * @param transforms Array of transforms to set depth-first for the bone instances
     * @param transform_count Size of the transforms array
     * @return Number of instances found
     */
    uint32_t SetBoneTransforms(HInstance instance, dmTransform::Transform& component_transform, dmTransform::Transform* transforms, uint32_t transform_count);

    /**
     * Recursively delete all instances flagged as bones under the given parent instance.
     * The order of deletion is depth-first, so that the children are deleted before the parents.
     * @param parent Parent instance of the hierarchy to set
     */
    void DeleteBones(HInstance parent);

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
     * Retrieve a collection from the specified instance
     * @param instance Game object instance
     * @return The collection the specified instance belongs to
     */
    HCollection GetCollection(HInstance instance);

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
     * Retrieve the message socket for the specified collection.
     * @param collection Collection handle
     * @return The message socket of the specified collection
     */
    dmMessage::HSocket GetMessageSocket(HCollection collection);

    /**
     * Retrieve the frame message socket for the specified collection.
     * @param collection Collection handle
     * @return The frame message socket of the specified collection
     */
    dmMessage::HSocket GetFrameMessageSocket(HCollection collection);

    /**
     * Returns whether the scale of the instances in a collection should be applied along Z or not.
     * @param collection Collection
     * @return if the scale should be applied along Z
     */
    bool ScaleAlongZ(HCollection collection);

    /**
     * Returns whether the scale of the instances in a collection should be applied along Z or not.
     * @param collection Collection
     * @return if the scale should be applied along Z
     */
    bool ScaleAlongZ(HCollection collection);

    /**
     * Set gameobject instance position
     * @param instance Gameobject instance
     * @param position New Position
     */
    void SetPosition(HInstance instance, Point3 position);

    /**
     * Get gameobject instance position
     * @param instance Gameobject instance
     * @return Position
     */
    Point3 GetPosition(HInstance instance);

    /**
     * Set gameobject instance rotation
     * @param instance Gameobject instance
     * @param position New Position
     */
    void SetRotation(HInstance instance, Quat rotation);

    /**
     * Get gameobject instance rotation
     * @param instance Gameobject instance
     * @return Position
     */
    Quat GetRotation(HInstance instance);

    /**
     * Set gameobject instance uniform scale
     * @param instance Gameobject instance
     * @param scale New uniform scale
     */
    void SetScale(HInstance instance, float scale);

    /**
     * Set gameobject instance non-uniform scale
     * @param instance Gameobject instance
     * @param scale New uniform scale
     */
    void SetScale(HInstance instance, Vector3 scale);

    /**
     * Get gameobject instance uniform scale
     * @param instance Gameobject instance
     * @return Uniform scale
     */
    float GetUniformScale(HInstance instance);

    /**
     * Get gameobject instance scale
     * @param instance Gameobject instance
     * @return Non-uniform scale
     */
    Vector3 GetScale(HInstance instance);

    /**
     * Get gameobject instance world position
     * @param instance Gameobject instance
     * @return World position
     */
    Point3 GetWorldPosition(HInstance instance);

    /**
     * Get gameobject instance world rotation
     * @param instance Gameobject instance
     * @return World rotation
     */
    Quat GetWorldRotation(HInstance instance);

    /**
     * Get game object instance world transform
     * @param instance Game object instance
     * @return World scale
     */
    Vector3 GetWorldScale(HInstance instance);

    /**
     * Get game object instance uniform scale
     * @param instance Game object instance
     * @return World uniform scale
     */
    float GetWorldUniformScale(HInstance instance);

    /**
     * Get game object instance world transform
     * @param instance Game object instance
     * @return World transform
     */
    const dmTransform::Transform GetWorldTransform(HInstance instance);

    /**
     * Get game object instance world transform as Matrix4.
     * @param instance Game object instance
     * @return World transform matrix.
     */
    const Matrix4 & GetWorldMatrix(HInstance instance);

    /**
     * Set parent instance to child
     * @note Instances must belong to the same collection
     * @param child Child instance
     * @param parent Parent instance. If 0, the child will be detached from its current parent, if any.
     * @return RESULT_OK on success. RESULT_MAXIMUM_HIEARCHICAL_DEPTH if parent at maximal level
     */
    Result SetParent(HInstance child, HInstance parent);

    /**
     * Get parent instance if exists
     * @param instance Gameobject instance
     * @return Parent instance. NULL if passed instance is rooted
     */
    HInstance GetParent(HInstance instance);

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
     * @param component_id Id of the component
     * @param property_id Id of the property
     * @param out_value Description of the retrieved property value
     * @return PROPERTY_RESULT_OK if the out-parameters were written
     */
    PropertyResult GetProperty(HInstance instance, dmhash_t component_id, dmhash_t property_id, PropertyDesc& out_value);

    /**
     * Sets the value of a property.
     * @param instance Instance of the game object
     * @param component_id Id of the component
     * @param property_id Id of the property
     * @param var Value and type of the property
     * @param finished If the animation finished or not
     * @param userdata1 User specified data
     * @param userdata2 User specified data
     * @return PROPERTY_RESULT_OK if the value could be set
     */
    PropertyResult SetProperty(HInstance instance, dmhash_t component_id, dmhash_t property_id, const PropertyVar& value);

    typedef void (*AnimationStopped)(dmGameObject::HInstance instance, dmhash_t component_id, dmhash_t property_id,
                                        bool finished, void* userdata1, void* userdata2);

    enum Playback
    {
        PLAYBACK_NONE          = 0,
        PLAYBACK_ONCE_FORWARD  = 1,
        PLAYBACK_ONCE_BACKWARD = 2,
        PLAYBACK_ONCE_PINGPONG = 3,
        PLAYBACK_LOOP_FORWARD  = 4,
        PLAYBACK_LOOP_BACKWARD = 5,
        PLAYBACK_LOOP_PINGPONG = 6,
        PLAYBACK_COUNT = 7,
    };

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

    /**
     * Register all resource types in resource factory
     * @param factory Resource factory
     * @param regist Register
     * @param script_context Script context
     * @param module_context Module context, must be persistent throughout the application
     * @return dmResource::Result
     */
    dmResource::Result RegisterResourceTypes(dmResource::HFactory factory, HRegister regist, dmScript::HContext script_context, ModuleContext* module_context);

    /** Used in the callback from the collection object iterator
     */
    struct IteratorCollection
    {
        HCollection     m_Collection;   // The collection
        dmhash_t        m_NameHash;     // The user specified name
        dmhash_t        m_Resource;     // The resource path
    };

    /** Used in the callback from the game object iterator
     */
    struct IteratorGameObject
    {
        HCollection     m_Collection;   // The collection
        HInstance       m_Instance;     // The game object
        dmhash_t        m_NameHash;     // The user specified name
        dmhash_t        m_Resource;     // The resource path
    };

    /** Used in the callback from the comonent iterator
     */
    struct IteratorComponent
    {
        HCollection     m_Collection;   // The collection
        HInstance       m_Instance;     // The game object
        dmhash_t        m_NameHash;     // The user specified name
        dmhash_t        m_Resource;     // The resource path
        const char*     m_Type;         // The type of the component
    };

    /** Callback function for iterating over all collection in a register
     */
    typedef bool (*FCollectionIterator)(const IteratorCollection* iterator, void* user_ctx);

    /** Callback function for iterating over all game objects in a collection
     */
    typedef bool (*FGameObjectIterator)(const IteratorGameObject* iterator, void* user_ctx);

    /** Callback function for iterating over all game objects in a collection
     */
    typedef bool (*FGameComponentIterator)(const IteratorComponent* iterator, void* user_ctx);

    /** Iterate over a registry to get all collections
     */
    bool IterateCollections(HRegister regist, FCollectionIterator callback, void* user_ctx);

    /** Iterate over a collection to get all game objects
     */
    bool IterateGameObjects(HCollection collection, FGameObjectIterator callback, void* user_ctx);

    /** Iterate over a gameobject to get all components
     */
    bool IterateComponents(HInstance instance, FGameComponentIterator callback, void* user_ctx);
}

#endif // GAMEOBJECT_H
