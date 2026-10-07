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

#ifndef DMSDK_DATA_TYPES_H
#define DMSDK_DATA_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct DataStore* HDataStore;
    typedef struct DataQuery* HDataQuery;
    typedef uint64_t          DataId;
    typedef uint64_t          DataGroupId;

    typedef enum DataResult
    {
        DATA_RESULT_OK = 0,
        DATA_RESULT_END = 1,
        DATA_RESULT_NOT_FOUND = -1,
        DATA_RESULT_LOCKED = -3,
        DATA_RESULT_INVALID_ARGUMENT = -4,
        DATA_RESULT_ALREADY_EXISTS = -5,
        DATA_RESULT_BUFFER_TOO_SMALL = -6,
        DATA_RESULT_INVALID_FORMAT = -7,
        DATA_RESULT_BUSY = -8,
    } DataResult;

    typedef enum DataValueType
    {
        DATA_TYPE_NUMBER = 0,
        DATA_TYPE_BOOLEAN = 1,
        DATA_TYPE_STRING = 2,
        DATA_TYPE_NULL = 3,
        DATA_TYPE_STRUCT = 4,
        DATA_TYPE_LIST = 5,
        DATA_TYPE_VECTOR3 = 6,
        DATA_TYPE_VECTOR4 = 7,
        DATA_TYPE_MATRIX4 = 8,
    } DataValueType;

    typedef struct DataVector3
    {
        float m_Values[3];
    } DataVector3;

    typedef struct DataVector4
    {
        float m_Values[4];
    } DataVector4;

    typedef struct DataMatrix4
    {
        float m_Values[16];
    } DataMatrix4;

#ifdef __cplusplus
}
#endif

// API documentation

/*# Shared data types
 *
 * Handles, identifiers, results and value types.
 *
 * @document
 * @name DataTypes
 * @language C
 */

/*# Store handle
 *
 * Owned when created with [ref:DataCreateStore]; engine-supplied handles are borrowed.
 *
 * @typedef
 * @name HDataStore
 */

/*# Query handle
 *
 * Owns a live query. Destroy after its iterators finish, before its store shuts down.
 *
 * @typedef
 * @name HDataQuery
 */

/*# Row identifier
 *
 * Store-local row ID, valid until removal; zero is invalid.
 * IDs survive row moves but are not serialized.
 *
 * @typedef
 * @name DataId
 */

/*# Logical group identifier
 *
 * Caller-defined group shared by any number of rows; zero is valid.
 *
 * @typedef
 * @name DataGroupId
 */

/*# Operation results
 *
 * Output arguments remain unchanged on error unless stated otherwise.
 *
 * @enum
 * @name DataResult
 * @member DATA_RESULT_OK Operation succeeded (0).
 * @member DATA_RESULT_END No more batches, rows or fields (1).
 * @member DATA_RESULT_NOT_FOUND Table, row or field was not found (-1).
 * @member DATA_RESULT_LOCKED An operation requires an unlocked store or no active query reservations (-3).
 * @member DATA_RESULT_INVALID_ARGUMENT Invalid argument, value or count (-4).
 * @member DATA_RESULT_ALREADY_EXISTS Table type or field name is duplicated (-5).
 * @member DATA_RESULT_BUFFER_TOO_SMALL The output buffer is too small (-6).
 * @member DATA_RESULT_INVALID_FORMAT The serialized blob is malformed or has an unsupported version (-7).
 * @member DATA_RESULT_BUSY Query already active, conflicting access, or store mutation in progress (-8).
 */

/*# Value kinds
 *
 * Declared field kinds. Structs and lists may nest.
 *
 * @enum
 * @name DataValueType
 * @member DATA_TYPE_NUMBER Double-precision number (0).
 * @member DATA_TYPE_BOOLEAN Boolean encoded as zero or one (1).
 * @member DATA_TYPE_STRING NUL-terminated string (2).
 * @member DATA_TYPE_NULL Null value with no payload (3).
 * @member DATA_TYPE_STRUCT Named fields (4).
 * @member DATA_TYPE_LIST Ordered values (5).
 * @member DATA_TYPE_VECTOR3 Three float32 components (6).
 * @member DATA_TYPE_VECTOR4 Four float32 components (7).
 * @member DATA_TYPE_MATRIX4 Sixteen float32 components in column-major order (8).
 */

/*# Vector3 value
 *
 * @struct
 * @name DataVector3
 * @member m_Values [type:float[3]] X, Y and Z components.
 */

/*# Vector4 value
 *
 * @struct
 * @name DataVector4
 * @member m_Values [type:float[4]] X, Y, Z and W components.
 */

/*# Matrix4 value
 *
 * @struct
 * @name DataMatrix4
 * @member m_Values [type:float[16]] Column-major components; element at row r, column c is m_Values[c * 4 + r].
 */

#endif // DMSDK_DATA_TYPES_H
