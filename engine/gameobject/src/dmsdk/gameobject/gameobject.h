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

#ifndef DMSDK_GAMEOBJECT_H
#define DMSDK_GAMEOBJECT_H

#include <stdint.h>
#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/vmath.h>

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

namespace dmGameObject
{
    /// Instance handle
    typedef struct Instance* HInstance;

    /// Script handle
    typedef struct Script* HScript;

    /// Instance handle
    typedef struct ScriptInstance* HScriptInstance;

    /// Component register
    typedef struct Register* HRegister;

    /// Collection handle
    typedef struct CollectionHandle* HCollection;

    /// Properties handle
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
        PROPERTY_RESULT_RESOURCE_NOT_FOUND = -10
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
        PropertyVar(bool v);

        PropertyType m_Type;
        union
        {
            double m_Number;
            dmhash_t m_Hash;
            // NOTE: We can't store an URL as is due to constructor
            char  m_URL[32]; // the same size as the dmMessage::URL
            float m_V4[4];
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


    /*# Get instance identifier
     * Get instance identifier
     * @param instance [type:dmGameObject::HInstance] Gameobject instance
     * @return [type:dmhash_t] Identifier. dmGameObject::UNNAMED_IDENTIFIER if not set.
     */
    dmhash_t GetIdentifier(HInstance instance);

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


    // These functions are used for profiling/testing functionality
    // They're not included in release builds

    // Used by component types to implement iteration
    typedef void (*FIteratorChildren)(struct SceneNodeIterator* it, struct SceneNode* node);
    typedef bool (*FIteratorNext)(struct SceneNodeIterator* it);

    // Scene traversal functions
    enum SceneNodeType
    {
        SCENE_NODE_TYPE_COLLECTION,
        SCENE_NODE_TYPE_GAMEOBJECT,
        SCENE_NODE_TYPE_COMPONENT,
        SCENE_NODE_TYPE_SUBCOMPONENT,
    };

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

    bool                        TraverseGetRoot(HRegister regist, SceneNode* node);
    SceneNodeIterator           TraverseIterateChildren(SceneNode* node);
    bool                        TraverseIterateNext(SceneNodeIterator* it);


    // Since we don't support for text properties, we'll keep a separate enum here for now
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

    typedef void (*FIteratorProperties)(struct SceneNodePropertyIterator* it, struct SceneNode* node);
    typedef bool (*FIteratorPropertiesNext)(struct SceneNodePropertyIterator* it);

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

    SceneNodePropertyIterator   TraverseIterateProperties(SceneNode* node);
    bool                        TraverseIteratePropertiesNext(SceneNodePropertyIterator* it);
}

#endif // DMSDK_GAMEOBJECT_H
