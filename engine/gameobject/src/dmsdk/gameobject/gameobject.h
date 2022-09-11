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

#ifndef DMSDK_GAMEOBJECT_H
#define DMSDK_GAMEOBJECT_H

#include <stdint.h>
#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/message.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/hid/hid.h>

/*# Game object functions
 *
 * API for manipulating game objects
 *
 * @document
 * @name Gameobject
 * @namespace dmGameObject
 * @path engine/gameobject/src/dmsdk/gameobject/gameobject.h
 */

namespace dmMessage
{
    struct URL;
}

namespace dmTransform
{
    class Transform;
}

namespace dmGameObject
{
    /*#
     * Value for an invalid instance index, this must be the same as defined in gamesys_ddf.proto for Create#index.
     * @constant
     * @name INVALID_INSTANCE_POOL_INDEX
     */
    const uint32_t INVALID_INSTANCE_POOL_INDEX = 0xffffffff;

    /*#
     * Gameobject instance handle
     * @typedef
     * @name HInstance
     */
    typedef struct Instance* HInstance;

    /*#
     * Script handle
     * @typedef
     * @name HScript
     */
    typedef struct Script* HScript;

    /*#
     * Script instance handle
     * @typedef
     * @name HScriptInstance
     */
    typedef struct ScriptInstance* HScriptInstance;

    /*#
     * Collection register.
     * @typedef
     * @name HRegister
     */
    typedef struct Register* HRegister;

    /*#
     * Gameobject collection handle
     * @typedef
     * @name HCollection
     */
    typedef struct CollectionHandle* HCollection;

    typedef void* HCollectionDesc;

    /*#
     * Gameobject prototype handle
     * @typedef
     * @name HPrototype
     */
    typedef struct Prototype* HPrototype;

    /*#
     * Gameobject properties handle
     * @typedef
     * @name HProperties
     */
    typedef struct Properties* HProperties;

    struct ComponentType;

    /*# result enumeration
     * Result enumeration.
     *
     * @enum
     * @name Result
     * @member dmGameObject::RESULT_OK
     * @member dmGameObject::RESULT_OUT_OF_RESOURCES
     * @member dmGameObject::RESULT_ALREADY_REGISTERED
     * @member dmGameObject::RESULT_IDENTIFIER_IN_USE
     * @member dmGameObject::RESULT_IDENTIFIER_ALREADY_SET
     * @member dmGameObject::RESULT_COMPONENT_NOT_FOUND
     * @member dmGameObject::RESULT_MAXIMUM_HIEARCHICAL_DEPTH
     * @member dmGameObject::RESULT_INVALID_OPERATION
     * @member dmGameObject::RESULT_RESOURCE_TYPE_NOT_FOUND
     * @member dmGameObject::RESULT_BUFFER_OVERFLOW
     * @member dmGameObject::RESULT_UNKNOWN_ERROR
     */
    enum Result
    {
        RESULT_OK = 0,                      //!< RESULT_OK
        RESULT_OUT_OF_RESOURCES = -1,       //!< RESULT_OUT_OF_RESOURCES
        RESULT_ALREADY_REGISTERED = -2,     //!< RESULT_ALREADY_REGISTERED
        RESULT_IDENTIFIER_IN_USE = -3,      //!< RESULT_IDENTIFIER_IN_USE
        RESULT_IDENTIFIER_ALREADY_SET = -4, //!< RESULT_IDENTIFIER_ALREADY_SET
        RESULT_COMPONENT_NOT_FOUND = -5,    //!< RESULT_COMPONENT_NOT_FOUND
        RESULT_MAXIMUM_HIEARCHICAL_DEPTH = -6, //!< RESULT_MAXIMUM_HIEARCHICAL_DEPTH
        RESULT_INVALID_OPERATION = -7,      //!< RESULT_INVALID_OPERATION
        RESULT_RESOURCE_TYPE_NOT_FOUND = -8,    //!< RESULT_COMPONENT_TYPE_NOT_FOUND
        RESULT_BUFFER_OVERFLOW = -9,        //!< RESULT_BUFFER_OVERFLOW
        RESULT_UNKNOWN_ERROR = -1000,       //!< RESULT_UNKNOWN_ERROR
    };

    /*# property types
     * Property types.
     *
     * @enum
     * @name PropertyType
     * @member dmGameObject::PROPERTY_TYPE_NUMBER
     * @member dmGameObject::PROPERTY_TYPE_HASH
     * @member dmGameObject::PROPERTY_TYPE_URL
     * @member dmGameObject::PROPERTY_TYPE_VECTOR3
     * @member dmGameObject::PROPERTY_TYPE_VECTOR4
     * @member dmGameObject::PROPERTY_TYPE_QUAT
     * @member dmGameObject::PROPERTY_TYPE_BOOLEAN
     * @member dmGameObject::PROPERTY_TYPE_COUNT
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
        PROPERTY_TYPE_MATRIX4 = 7,
        PROPERTY_TYPE_COUNT
    };

    /*# property result
     * Property result.
     *
     * @enum
     * @name PropertyResult
     * @member dmGameObject::PROPERTY_RESULT_OK
     * @member dmGameObject::PROPERTY_RESULT_NOT_FOUND
     * @member dmGameObject::PROPERTY_RESULT_INVALID_FORMAT
     * @member dmGameObject::PROPERTY_RESULT_UNSUPPORTED_TYPE
     * @member dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH
     * @member dmGameObject::PROPERTY_RESULT_COMP_NOT_FOUND
     * @member dmGameObject::PROPERTY_RESULT_INVALID_INSTANCE
     * @member dmGameObject::PROPERTY_RESULT_BUFFER_OVERFLOW
     * @member dmGameObject::PROPERTY_RESULT_UNSUPPORTED_VALUE
     * @member dmGameObject::PROPERTY_RESULT_UNSUPPORTED_OPERATION
     * @member dmGameObject::PROPERTY_RESULT_RESOURCE_NOT_FOUND
     * @member dmGameObject::PROPERTY_RESULT_INVALID_INDEX
     * @member dmGameObject::PROPERTY_RESULT_INVALID_KEY
     */
    enum PropertyResult
    {
        PROPERTY_RESULT_OK = 0,
        PROPERTY_RESULT_NOT_FOUND = -1,
        PROPERTY_RESULT_INVALID_FORMAT = -2,
        PROPERTY_RESULT_UNSUPPORTED_TYPE = -3,
        PROPERTY_RESULT_TYPE_MISMATCH = -4,
        PROPERTY_RESULT_COMP_NOT_FOUND = -5,
        PROPERTY_RESULT_INVALID_INSTANCE = -6,
        PROPERTY_RESULT_BUFFER_OVERFLOW = -7,
        PROPERTY_RESULT_UNSUPPORTED_VALUE = -8,
        PROPERTY_RESULT_UNSUPPORTED_OPERATION = -9,
        PROPERTY_RESULT_RESOURCE_NOT_FOUND = -10,
        PROPERTY_RESULT_INVALID_INDEX = -11,
        PROPERTY_RESULT_INVALID_KEY = -12,
    };

    /*#
     * Playback type enum
     * @enum
     * @name Playback
     */
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

    /*# Create result enum
     *
     * Create result enum.
     *
     * @enum
     * @name CreateResult
     * @member dmGameObject::CREATE_RESULT_OK
     * @member dmGameObject::CREATE_RESULT_UNKNOWN_ERROR
     */
    enum CreateResult
    {
        CREATE_RESULT_OK = 0,
        CREATE_RESULT_UNKNOWN_ERROR = -1000,
    };


    /*# Update result enum
     *
     * Update result enum.
     *
     * @enum
     * @name UpdateResult
     * @member dmGameObject::UPDATE_RESULT_OK
     * @member dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR
     */
    enum UpdateResult
    {
        UPDATE_RESULT_OK = 0,
        UPDATE_RESULT_UNKNOWN_ERROR = -1000,
    };

    /*# Type of property value
     *
     * Type of property value
     *
     * @enum
     * @name PropertyValueType
     * @member dmGameObject::PROP_VALUE_ARRAY
     * @member dmGameObject::PROP_VALUE_HASHTABLE
     */
    enum PropertyValueType
    {
        PROP_VALUE_ARRAY = 0,
        PROP_VALUE_HASHTABLE = 1,
    };

    /*# Property Options
     *
     * Parameters variant that holds key or index for a propertys data structure.
     *
     * @struct
     * @name PropertyOptions
     * @member m_Index [type:int32_t] The index of the property to set, only applicable if property is array.
     * @member m_Key [type:dmhash_t] The key of the property to set, only applicable if property is hashtable.
     * @member m_HasKey [type:uint8_t] A flag if structure contain m_Key value (it can't contain both)
     */
    struct PropertyOptions
    {
        union
        {
            dmhash_t m_Key;
            int32_t m_Index;
        };

        uint8_t m_HasKey : 1;
    };

    /*# property variant
     * Property variant that holds the data for a variable
     *
     * @struct
     * @name PropertyVar
     * @member m_Type [type:dmGameObject::PropertyType] property type
     * @member m_Number [type:double] A floating point value (union)
     * @member m_Hash [type:dmhash_t] A hash value (union)
     * @member m_Url [type:const uin8_t*] An URL value (union)
     * @member m_V4 [type:float] A vector4 value (union)
     * @member m_Bool [type:bool] A boolean value (union)
     */
    struct PropertyVar
    {
        PropertyVar();
        PropertyVar(float v);
        PropertyVar(double v);
        PropertyVar(dmhash_t v);
        PropertyVar(const dmMessage::URL& v);
        PropertyVar(dmVMath::Vector3 v);
        PropertyVar(dmVMath::Vector4 v);
        PropertyVar(dmVMath::Quat v);
        PropertyVar(dmVMath::Matrix4 v);
        PropertyVar(bool v);

        PropertyType m_Type;
        union
        {
            double m_Number;
            dmhash_t m_Hash;
            // NOTE: We can't store an URL as is due to constructor
            char  m_URL[32]; // the same size as the dmMessage::URL
            float m_V4[4];
            float m_M4[16];
            bool m_Bool;
        };
    };

    typedef PropertyResult (*GetPropertyCallback)(const HProperties properties, uintptr_t user_data, dmhash_t id, PropertyVar& out_var);
    typedef void (*FreeUserDataCallback)(uintptr_t user_data);

    struct PropertySet
    {
        PropertySet();

        GetPropertyCallback m_GetPropertyCallback;
        FreeUserDataCallback m_FreeUserDataCallback;
        uintptr_t m_UserData;
    };

    /*#
     * Description of a property.
     *
     * If the property is externally mutable, m_ValuePtr points to the value and its length is m_ElementCount.
     * m_Variant always reflects the value.
     * @struct
     * @name PropertyDesc
     * @member m_ElementIds [type: dmhash_t] For composite properties (float arrays), these ids name each element
     * @member m_Variant [type: PropertyVar] Variant holding the value
     * @member m_ValuePtr [type: float*] Pointer to the value, only set for mutable values. The actual data type is described by the variant.
     * @member m_ReadOnly [type: bool] Determines whether we are permitted to write to this property.
     * @member m_ValueType [type: uint8_t] Indicates type of the property.
     */
    struct PropertyDesc
    {
        PropertyDesc();
        dmhash_t m_ElementIds[4];
        PropertyVar m_Variant;
        float* m_ValuePtr;
        bool m_ReadOnly;
        uint8_t m_ValueType : 1;
    };

    /*#
     * Update context
     * @struct
     * @name UpdateContext
     * @member m_TimeScale [type: float] the scaling factor what was applied on the dt (i.e. the collection update time scale)
     * @member m_DT [type: float] the delta time elapsed since last frame (seconds)
     * @member m_FixedUpdateFrequency [type: uint32_t] Number of of calls per second to the FixedUpdate of each component
     */
    struct UpdateContext
    {
        float    m_TimeScale;
        float    m_DT;
        float    m_AccumFrameTime;          // Unscaled time. Amount of time left after last fixed update tick
        uint32_t m_FixedUpdateFrequency;    // Hz

        UpdateContext()
        : m_TimeScale(1.0f)
        , m_DT(0.0f)
        , m_AccumFrameTime(0.0f)
        , m_FixedUpdateFrequency(0) {}
    };

    /*#
     * Container of input related information.
     * @struct
     * @name InputAction
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
        /// Accelerometer x value (if present)
        float m_AccX;
        /// Accelerometer y value (if present)
        float m_AccY;
        /// Accelerometer z value (if present)
        float m_AccZ;
        /// Touch data
        dmHID::Touch m_Touch[dmHID::MAX_TOUCH_COUNT];
        /// Number of m_Touch
        int32_t  m_TouchCount;
        /// Contains text input if m_HasText, and gamepad name if m_GamepadConnected
        char     m_Text[dmHID::MAX_CHAR_COUNT];
        uint32_t m_TextCount;
        uint32_t m_GamepadIndex;
        dmHID::GamepadPacket m_GamepadPacket;

        uint8_t  m_IsGamepad : 1;
        uint8_t  m_GamepadDisconnected : 1;
        uint8_t  m_GamepadConnected : 1;
        uint8_t  m_HasGamepadPacket : 1;
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

    /*#
     * Input result enum
     * @enum
     * @name InputResult
     * @member INPUT_RESULT_IGNORED = 0
     * @member INPUT_RESULT_CONSUMED = 1
     * @member INPUT_RESULT_UNKNOWN_ERROR = -1000
     */
    enum InputResult
    {
        INPUT_RESULT_IGNORED = 0,
        INPUT_RESULT_CONSUMED = 1,
        INPUT_RESULT_UNKNOWN_ERROR = -1000
    };

    /*#
     * Retrieve the message socket for the specified collection.
     * @name GetMessageSocket
     * @param collection [type: dmGameObject::HCollection] Collection handle
     * @return socket [type: dmMessage::HSocket] The message socket of the specified collection
     */
    dmMessage::HSocket GetMessageSocket(HCollection collection);

    /*#
     * Retrieve a collection from the specified instance
     * @name GetCollection
     * @param instance [type: dmGameObject::HInstance] Game object instance
     * @return collection [type: dmGameObject::HInstance] The collection the specified instance belongs to
     */
    HCollection GetCollection(HInstance instance);

    /*#
     * Create a new gameobject instance
     * @note Calling this function during update is not permitted. Use #Spawn instead for deferred creation
     * @name New
     * @param collection [type: dmGameObject::HCollection] Gameobject collection
     * @param prototype_name |type: const char*] Prototype file name. May be 0.
     * @return instance [type: dmGameObject::HInstance] New gameobject instance. NULL if any error occured
     */
    HInstance New(HCollection collection, const char* name);

    /*#
     * Delete gameobject instance
     * @name Delete
     * @param collection [type: dmGameObject::HCollection] Gameobject collection
     * @param instance [type: dmGameObject::HInstance] Gameobject instance
     * @param recursive [type: bool] If true, delete child hierarchy recursively in child to parent order (leaf first)
     */
    void Delete(HCollection collection, HInstance instance, bool recursive);

    /*#
     * Construct a hash of an instance id based on the index provided.
     * @name ConstructInstanceId
     * @param index [type: uint32_t] The index to base the id off of.
     * @return id [type: dmhash_t] hash of the instance id constructed.
     */
    dmhash_t ConstructInstanceId(uint32_t index);

    /*#
     * Retrieve an instance index from the index pool for the collection.
     * @name AcquireInstanceIndex
     * @param collection [type: dmGameObject::HColleciton] Collection from which to retrieve the instance index.
     * @return instance [type: uint32_t] index from the index pool of collection.
     */
    uint32_t AcquireInstanceIndex(HCollection collection);

    /*#
     * Assign an index to the instance, only if the instance is not null.
     * @name AssignInstanceIndex
     * @param index [type: uint32_t] The index to assign.
     * @param instance [type: dmGameObject::HInstance] The instance that should be assigned the index.
     */
    void AssignInstanceIndex(uint32_t index, HInstance instance);

    /*# Get instance identifier
     * Get instance identifier
     * @name GetIdentifier
     * @param instance [type:dmGameObject::HInstance] Gameobject instance
     * @return [type:dmhash_t] Identifier. dmGameObject::UNNAMED_IDENTIFIER if not set.
     */
    dmhash_t GetIdentifier(HInstance instance);

    /*#
     * Set instance identifier. Must be unique within the collection.
     * @name SetIdentifier
     * @param collection [type: dmGameObject::HCollection] Collection
     * @param instance [type: dmGameObject::HInstance] Instance
     * @param identifier [type: dmhash_t] Identifier
     * @return result [type: dmGameObject::Result]  RESULT_OK on success
     */
    Result SetIdentifier(HCollection collection, HInstance instance, dmhash_t identifier);

    /*#
     * Get instance from identifier
     * @param collection [type: dmGameObject::HCollection] Collection
     * @param identifier [type: dmhash_t] Identifier
     * @return instance [type: dmGameObject::HInstance] Instance. NULL if instance isn't found.
     */
    HInstance GetInstanceFromIdentifier(HCollection collection, dmhash_t identifier);

    /*#
     * Get component id from component index.
     * @name GetComponentId
     * @param instance [type: dmGameObject::HInstance] Instance
     * @param component_index [type: uint16_t] Component index
     * @param component_id [type: dmhash_t* Component id as out-argument
     * @return result [type: dmGameObject::Result] RESULT_OK if the comopnent was found
     */
    Result GetComponentId(HInstance instance, uint16_t component_index, dmhash_t* component_id);

    /*# set position
     * Set gameobject instance position
     * @name SetPosition
     * @param instance [type:dmGameObject::HInstance] Gameobject instance
     * @param position [type:dmGameObject::Point3] New Position
     */
    void SetPosition(HInstance instance, dmVMath::Point3 position);

    /*# get position
     * Get gameobject instance position
     * @name GetPosition
     * @param instance [type:dmGameObject::HInstance] Gameobject instance
     * @return [type:dmGameObject::Point3] Position
     */
    dmVMath::Point3 GetPosition(HInstance instance);

    /*# set rotation
     * Set gameobject instance rotation
     * @name SetRotation
     * @param instance [type:dmGameObject::HInstance] Gameobject instance
     * @param position New Position
     */
    void SetRotation(HInstance instance, dmVMath::Quat rotation);

    /*# get rotation
     * Get gameobject instance rotation
     * @name GetRotation
     * @param instance [type:dmGameObject::HInstance] Gameobject instance
     * @return [type:dmGameObject::Quat] rotation
     */
    dmVMath::Quat GetRotation(HInstance instance);

    /*# set uniform scale
     * Set gameobject instance uniform scale
     * @name SetScale
     * @param instance [type:dmGameObject::HInstance] Gameobject instance
     * @param scale New uniform scale
     */
    void SetScale(HInstance instance, float scale);

    /*# set scale
     * Set gameobject instance non-uniform scale
     * @name SetScale
     * @param instance [type:dmGameObject::HInstance] Gameobject instance
     * @param scale New uniform scale
     */
    void SetScale(HInstance instance, dmVMath::Vector3 scale);

    /*# get uniform scale
     * Get gameobject instance uniform scale
     * @name GetUniformScale
     * @param instance [type:dmGameObject::HInstance] Gameobject instance
     * @return [type:float] Uniform scale
     */
    float GetUniformScale(HInstance instance);

    /*# get scale
     * Get gameobject instance scale
     * @name GetScale
     * @param instance [type:dmGameObject::HInstance] Gameobject instance
     * @return [type:dmGameObject::Vector3] Non-uniform scale
     */
    dmVMath::Vector3 GetScale(HInstance instance);

    /*# get world position
     * Get gameobject instance world position
     * @name GetWorldPosition
     * @param instance [type:dmGameObject::HInstance] Gameobject instance
     * @return [type:dmGameObject::Point3] World position
     */
    dmVMath::Point3 GetWorldPosition(HInstance instance);

    /*# get world rotation
     * Get gameobject instance world rotation
     * @name GetWorldRotation
     * @param instance [type:dmGameObject::HInstance] Gameobject instance
     * @return [type:dmGameObject::Quat] World rotation
     */
    dmVMath::Quat GetWorldRotation(HInstance instance);

    /*# get world scale
     * Get game object instance world transform
     * @name GetWorldScale
     * @param instance [type:dmGameObject::HInstance] Gameobject instance
     * @return [type:dmGameObject::Vector3] World scale
     */
    dmVMath::Vector3 GetWorldScale(HInstance instance);

    /*# get world uniform scale
     * Get game object instance uniform scale
     * @name GetWorldUniformScale
     * @param instance [type:dmGameObject::HInstance] Gameobject instance
     * @return [type:float] World uniform scale
     */
    float GetWorldUniformScale(HInstance instance);

    /*# get world matrix
     * Get game object instance world transform as Matrix4.
     * @name GetWorldMatrix
     * @param instance [type:dmGameObject::HInstance] Gameobject instance
     * @return [type:dmGameObject::MAtrix4] World transform matrix.
     */
    const dmVMath::Matrix4& GetWorldMatrix(HInstance instance);

    /*# get world transform
     * Get game object instance world transform
     * @name GetWorldTransform
     * @param instance [type:dmGameObject::HInstance] Gameobject instance
     * @return [type:dmTransform::Transform] World transform
     */
    dmTransform::Transform GetWorldTransform(HInstance instance);

    /*#
     * Set whether the instance should be flagged as a bone.
     * Instances flagged as bones can have their transforms updated in a batch through SetBoneTransforms.
     * Used for animated skeletons.
     * @name SetBone
     * @param instance [type: HImstance] Instance
     * @param bone [type: bool] true if the instance is a bone
     */
    void SetBone(HInstance instance, bool bone);

    /*#
     * Check whether the instance is flagged as a bone.
     * @name IsBone
     * @param instance [type: HImstance] Instance
     * @return result [type: bool] True if flagged as a bone
     */
    bool IsBone(HInstance instance);

    /*#
     * Set the local transforms recursively of all instances flagged as bones, starting with component with id.
     * The order of the transforms is depth-first.
     * @name SetBoneTransforms
     * @param instance [type: HImstance] First Instance of the hierarchy to set
     * @param component_transform [type: dmTransform::Transform] the transform for component root
     * @param transforms Array of transforms to set depth-first for the bone instances
     * @param transform_count Size of the transforms array
     * @return Number of instances found
     */
    uint32_t SetBoneTransforms(HInstance instance, dmTransform::Transform& component_transform, dmTransform::Transform* transforms, uint32_t transform_count);

    /*#
     * Recursively delete all instances flagged as bones under the given parent instance.
     * The order of deletion is depth-first, so that the children are deleted before the parents.
     * @name DeleteBones
     * @param parent [type: HInstance] Parent instance of the hierarchy
     */
    void DeleteBones(HInstance parent);

    /*
     * Set parent instance to child
     * @note Instances must belong to the same collection
     * @name SetParent
     * @param child [type: dmGameObject::HInstance] Child instance
     * @param parent [type: dmGameObject::HInstance] Parent instance. If 0, the child will be detached from its current parent, if any.
     * @return result [type: dmGameObject::Result] RESULT_OK on success. RESULT_MAXIMUM_HIEARCHICAL_DEPTH if parent at maximal level
     */
    Result SetParent(HInstance child, HInstance parent);

    /*
     * Get parent instance if it exists
     * @name GetParent
     * @param instance [type: dmGameObject::HInstance] Gameobject instance
     * @return parent [type: dmGameObject::HInstance] Parent instance. NULL if passed instance is root
     */
    HInstance GetParent(HInstance instance);


    // These functions are used for profiling functionality

    // Currently used internally by component types to implement iteration
    typedef void (*FIteratorChildren)(struct SceneNodeIterator* it, struct SceneNode* node);
    typedef bool (*FIteratorNext)(struct SceneNodeIterator* it);

    // Scene traversal functions

    /** scene node types
     * @enum
     * @name SceneNodeType
     * @member dmGameObject::SCENE_NODE_TYPE_COLLECTION
     * @member dmGameObject::SCENE_NODE_TYPE_GAMEOBJECT
     * @member dmGameObject::SCENE_NODE_TYPE_COMPONENT
     * @member dmGameObject::SCENE_NODE_TYPE_SUBCOMPONENT
     */
    enum SceneNodeType
    {
        SCENE_NODE_TYPE_COLLECTION,
        SCENE_NODE_TYPE_GAMEOBJECT,
        SCENE_NODE_TYPE_COMPONENT,
        SCENE_NODE_TYPE_SUBCOMPONENT,
    };

    /*# scene graph traversal node
     * Opaque struct that holds info about the current node
     *
     * @note The concept of a `scene node` only exists here, for the purposes of inspecting the scene graph for inspection and testing purposes only.
     * @struct
     * @name SceneNode
     */
    struct SceneNode
    {
        // The iterator type specific data
        uint64_t        m_Node; // Opaque data that translates into an object in the internal iterator function
        SceneNodeType   m_Type;

        // set depending on the node type
        HCollection     m_Collection;
        HInstance       m_Instance;
        ComponentType*  m_ComponentType;
        void*           m_ComponentPrototype;
        void*           m_ComponentWorld;
        uintptr_t       m_Component;
    };

    /*# scene graph traversal iterator
     * Opaque struct that holds info about the current position when traversing the scene
     *
     * @struct
     * @name SceneNodeIterator
     */
    struct SceneNodeIterator
    {
        // public data
        SceneNode           m_Node;

        // ****************************************************************
        // private data for the iterator functions
        SceneNode           m_Parent;
        SceneNode           m_NextChild;
        FIteratorNext       m_FnIterateNext;    // Specified by each node type we're iterating over
    };

    /*#
     * Gets the top node of the whole game (the main collection)
     * @name TraverseGetRoot
     * @param regist [type:dmGameObject::HRegister] the full gameobject register
     * @param node [type:dmGameObject::HRegister] the node to inspect
     * @return result [type:bool] True if successful
     *
     * @note The dmGameObject::HRegister is obtained from the `dmEngine::GetGameObjectRegister(dmExtension::AppParams)`
     * @note Traversing the scene like this is not efficient. These functions are here for inspection and testing purposes only.
     *
     * @examples
     *
     * The following examples show how to iterate over currently loaded scene graph
     *
     *```cpp
     * void OutputNode(dmGameObject::SceneNode* node) {
     *     dmGameObject::SceneNodeIterator it = dmGameObject::TraverseIterateChildren(node);
     *     while(dmGameObject::TraverseIterateNext(&it))
     *     {
     *         OutputProperties(&it.m_Node); // see dmGameObject::TraverseIterateProperties()
     *         OutputNode(&it.m_Node);
     *     }
     * }
     *
     * bool OutputScene(HRegister regist) {
     *     dmGameObject::SceneNode root;
     *     if (!dmGameObject::TraverseGetRoot(regist, &root))
     *         return false;
     *     OutputNode(&node);
     * }
     *```
     */
    bool TraverseGetRoot(HRegister regist, SceneNode* node);

    /*#
     * Get a scene node iterator for the nodes' children
     * @name TraverseIterateChildren
     * @param node [type:dmGameObject::SceneNode*] the parent node
     * @return iterator [type:dmGameObject::SceneNodeIterator] the iterator
    */
    SceneNodeIterator TraverseIterateChildren(SceneNode* node);

    /*#
     * Step a scene node iterator to the next sibling
     * @name TraverseIterateNext
     * @param it [type:dmGameObject::SceneNodeIterator*] the iterator
     * @return result [type:bool] true if successful. false if the iterator is finished
    */
    bool TraverseIterateNext(SceneNodeIterator* it);

    /*# scene node property types
     * @note Since we don't support text properties, we'll keep a separate enum here for now
     * @enum
     * @name SceneNodePropertyType
     * @member dmGameObject::SCENE_NODE_PROPERTY_TYPE_NUMBER
     * @member dmGameObject::SCENE_NODE_PROPERTY_TYPE_HASH
     * @member dmGameObject::SCENE_NODE_PROPERTY_TYPE_URL
     * @member dmGameObject::SCENE_NODE_PROPERTY_TYPE_VECTOR3
     * @member dmGameObject::SCENE_NODE_PROPERTY_TYPE_VECTOR4
     * @member dmGameObject::SCENE_NODE_PROPERTY_TYPE_QUAT
     * @member dmGameObject::SCENE_NODE_PROPERTY_TYPE_BOOLEAN
     * @member dmGameObject::SCENE_NODE_PROPERTY_TYPE_TEXT
     * @member dmGameObject::SCENE_NODE_PROPERTY_TYPE_COUNT
     */
    enum SceneNodePropertyType
    {
        SCENE_NODE_PROPERTY_TYPE_NUMBER = 0,
        SCENE_NODE_PROPERTY_TYPE_HASH = 1,
        SCENE_NODE_PROPERTY_TYPE_URL = 2,
        SCENE_NODE_PROPERTY_TYPE_VECTOR3 = 3,
        SCENE_NODE_PROPERTY_TYPE_VECTOR4 = 4,
        SCENE_NODE_PROPERTY_TYPE_QUAT = 5,
        SCENE_NODE_PROPERTY_TYPE_BOOLEAN = 6,
        SCENE_NODE_PROPERTY_TYPE_TEXT = 7,
        SCENE_NODE_PROPERTY_TYPE_COUNT
    };

    // Currently used internally by the component types
    typedef void (*FIteratorProperties)(struct SceneNodePropertyIterator* it, struct SceneNode* node);
    typedef bool (*FIteratorPropertiesNext)(struct SceneNodePropertyIterator* it);

    /*# scene traversal node property
     * Struct that holds info about the current position when traversing the scene
     *
     * @struct
     * @name SceneNodeProperty
     * @member m_NameHash [type:dmhash_t] name
     * @member m_Type [type:dmGameObject::SceneNodePropertyType] type
     * @member m_Value [type:union] value
     *
     * `m_Number`
     * : [type:double] floating point number
     *
     * `m_Hash`
     * : [type:dmhash_t] The hashed value.
     *
     * `m_URL`
     * : [type:char[1024]] The text representation of the url (if reverse hashes are enabled)
     *
     * `m_V4`
     * : [type:float[4]] Used for Vector3, Vector4 and Quat
     *
     * `m_Bool`
     * : [type:bool] A boolean value
     *
     * `m_Text`
     * : [type:const char*] Text from a text property
     *
     */
    struct SceneNodeProperty
    {
        dmhash_t                m_NameHash;
        SceneNodePropertyType   m_Type;

        union
        {
            double              m_Number;
            dmhash_t            m_Hash;
            char  m_URL[1024];  // The text representation of the url (if reverse hashes are enabled)
            float m_V4[4];      // used for Vector3, Vector4 and Quat
            bool m_Bool;
            const char* m_Text; //
        } m_Value;
    };

    /*# scene traversal node property
     * Holds the property
     *
     * @struct
     * @name SceneNodePropertyIterator
     * @member m_Property [type:dmGameObject::SceneNodeProperty] property
     */
    struct SceneNodePropertyIterator
    {
        // public data
        SceneNodeProperty   m_Property;

        // ****************************************************************
        // private data for the iterator functions
        SceneNode*              m_Node;
        uint64_t                m_Next;             // Opaque info interpreted by each node type
        FIteratorPropertiesNext m_FnIterateNext;    // Specified by each node type we're iterating over
    };

    /*#
     * Create a scene node traversal property iterator
     * @name TraverseIterateProperties
     * @param node [type:dmGameObject::SceneNode*] the node to inspect
     * @return iterator [type:dmGameObject::SceneNodePropertyIterator] the property iterator
     *
     * @note Getting the properties like this is not efficient. These functions are here for inspection and testing purposes only.
     * @note Reverse hashes via `dmHashReverseSafe64()` isn't available in release builds.
     *
     * @examples
     *
     * The following examples show how to iterate over the properties of a node
     *
     *```cpp
     * dmGameObject::SceneNodePropertyIterator pit = TraverseIterateProperties(node);
     * while(dmGameObject::TraverseIteratePropertiesNext(&pit))
     * {
     *     const char* name = dmHashReverseSafe64(pit.m_Property.m_NameHash);
     *     switch(pit.m_Property.m_Type)
     *     {
     *     case dmGameObject::SCENE_NODE_PROPERTY_TYPE_NUMBER: ...
     *     ...
     *     }
     * }
     *```
     */
    SceneNodePropertyIterator TraverseIterateProperties(SceneNode* node);

    /*#
     * Steps the scene node traversal property iterator to the next property
     * @name TraverseIteratePropertiesNext
     * @param it [type:dmGameObject::SceneNodePropertyIterator*] the iterator
     * @return finished [type:bool] True if the iterator it valid, false if the iterator is finished.
     *
     */
    bool TraverseIteratePropertiesNext(SceneNodePropertyIterator* it);
}

#endif // DMSDK_GAMEOBJECT_H
