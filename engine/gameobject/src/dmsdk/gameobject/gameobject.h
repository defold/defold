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

#ifndef DMSDK_GAMEOBJECT_GAMEOBJECT_H
#define DMSDK_GAMEOBJECT_GAMEOBJECT_H

#include <stdint.h>
#include <dmsdk/dlib/hash.h>

namespace dmGameObject
{
    /*# SDK Gameobject API documentation
     * [file:<dmsdk/gameobject/gameobject.h>]
     *
     * Api for manipulating game objects (WIP)
     *
     * @document
     * @name GameObject
     * @namespace dmGameObject
     */

    /// Component register
    typedef struct Register* HRegister;

    /*# Result enumeration
     *
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
     *
     */
    enum Result
    {
        RESULT_OK = 0,
        RESULT_OUT_OF_RESOURCES = -1,
        RESULT_ALREADY_REGISTERED = -2,
        RESULT_IDENTIFIER_IN_USE = -3,
        RESULT_IDENTIFIER_ALREADY_SET = -4,
        RESULT_COMPONENT_NOT_FOUND = -5,
        RESULT_MAXIMUM_HIEARCHICAL_DEPTH = -6,
        RESULT_INVALID_OPERATION = -7,
        RESULT_RESOURCE_TYPE_NOT_FOUND = -8,
        RESULT_BUFFER_OVERFLOW = -9,
        RESULT_UNKNOWN_ERROR = -1000,
    };

    /*# Create result enum
     *
     * Create result enum.
     *
     * @enum
     * @name Result
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
     * @name Result
     * @member dmGameObject::UPDATE_RESULT_OK
     * @member dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR
     */
    enum UpdateResult
    {
        UPDATE_RESULT_OK = 0,
        UPDATE_RESULT_UNKNOWN_ERROR = -1000,
    };

    /*# Property result enum
     *
     * Property result enum.
     *
     * @enum
     * @name Result
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


    typedef struct Properties* HProperties;
    struct PropertyVar;

    typedef PropertyResult (*GetPropertyCallback)(const HProperties properties, uintptr_t user_data, dmhash_t id, PropertyVar& out_var);
    typedef void (*FreeUserDataCallback)(uintptr_t user_data);

    struct PropertySet
    {
        PropertySet();

        GetPropertyCallback m_GetPropertyCallback;
        FreeUserDataCallback m_FreeUserDataCallback;
        uintptr_t m_UserData;
    };
}

#endif // #ifndef DMSDK_GAMEOBJECT_GAMEOBJECT_H
