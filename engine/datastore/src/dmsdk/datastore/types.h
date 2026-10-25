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

#ifndef DMSDK_DATASTORE_TYPES_H
#define DMSDK_DATASTORE_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct DSStore* HDSContext;
    typedef struct DSQuery* HDSQuery;
    typedef uint64_t        DSId;
    typedef uint64_t        DSGroupId;

    typedef enum DSResult
    {
        DS_RESULT_OK = 0,
        DS_RESULT_NOT_FOUND = -1,
        DS_RESULT_LOCKED = -3,
        DS_RESULT_INVALID_ARGUMENT = -4,
        DS_RESULT_ALREADY_EXISTS = -5,
        DS_RESULT_BUFFER_TOO_SMALL = -6,
        DS_RESULT_INVALID_FORMAT = -7,
        DS_RESULT_BUSY = -8,
    } DSResult;

    typedef enum DSValueType
    {
        DS_TYPE_NUMBER = 0,
        DS_TYPE_BOOLEAN = 1,
        DS_TYPE_STRING = 2,
        DS_TYPE_NULL = 3,
        DS_TYPE_STRUCT = 4,
        DS_TYPE_LIST = 5,
        DS_TYPE_VECTOR3 = 6,
        DS_TYPE_VECTOR4 = 7,
        DS_TYPE_MATRIX4 = 8,
        DS_TYPE_FLOAT = 9,
        DS_TYPE_UINT32 = 10,
    } DSValueType;

    typedef struct DSVector3
    {
        float m_Values[3];
    } DSVector3;

    typedef struct DSVector4
    {
        float m_Values[4];
    } DSVector4;

    typedef struct DSMatrix4
    {
        float m_Values[16];
    } DSMatrix4;

#ifdef __cplusplus
}
#endif

// API documentation

/*# Shared data types
 *
 * Handles, identifiers, results and value types.
 *
 * @document
 * @name DSTypes
 * @language C
 */

/*# Store handle
 *
 * Owned when created with [ref:DSCreateStore]; engine-supplied handles are borrowed.
 *
 * @typedef
 * @name HDSContext
 */

/*# Query handle
 *
 * Owns a live query. Destroy after its iterators finish, before its store shuts down.
 *
 * @typedef
 * @name HDSQuery
 */

/*# Row identifier
 *
 * Store-local row ID, valid until removal; zero is invalid.
 * IDs survive row moves but are not serialized.
 *
 * @typedef
 * @name DSId
 */

/*# Owning group identifier
 *
 * Caller-provided 64-bit owner ID, for example a game-object handle. Nonzero IDs
 * acquire group bookkeeping when rows or blob instances are first added. Group
 * removal removes its rows; the caller controls ID uniqueness and reuse. Query
 * filters continue matching the same ID if it is reused. Zero selects ungrouped
 * rows. The datastore has no knowledge of the external owner.
 *
 * @typedef
 * @name DSGroupId
 */

/*# Operation results
 *
 * Output arguments remain unchanged on error unless stated otherwise.
 *
 * @enum
 * @name DSResult
 * @member DS_RESULT_OK Operation succeeded (0).
 * @member DS_RESULT_NOT_FOUND Table, row, group, field, system or event was not found (-1).
 * @member DS_RESULT_LOCKED An operation requires query reservations or a schedule to finish (-3).
 * @member DS_RESULT_INVALID_ARGUMENT Invalid argument, value or count (-4).
 * @member DS_RESULT_ALREADY_EXISTS Table type, field name or function/event name is duplicated (-5).
 * @member DS_RESULT_BUFFER_TOO_SMALL The output buffer is too small (-6).
 * @member DS_RESULT_INVALID_FORMAT The serialized blob is malformed or has an unsupported version (-7).
 * @member DS_RESULT_BUSY Unfinished schedule work, query already active, conflicting access, or store mutation in progress (-8).
 */

/*# Value kinds
 *
 * Declared field kinds. Structs and lists may nest.
 *
 * @enum
 * @name DSValueType
 * @member DS_TYPE_NUMBER Double-precision number (0).
 * @member DS_TYPE_BOOLEAN Boolean encoded as zero or one (1).
 * @member DS_TYPE_STRING NUL-terminated string (2).
 * @member DS_TYPE_NULL Null value with no payload (3).
 * @member DS_TYPE_STRUCT Named fields (4).
 * @member DS_TYPE_LIST Ordered values (5).
 * @member DS_TYPE_VECTOR3 Three float32 components (6).
 * @member DS_TYPE_VECTOR4 Four float32 components (7).
 * @member DS_TYPE_MATRIX4 Sixteen float32 components in column-major order (8).
 * @member DS_TYPE_FLOAT Single-precision number, stored as float.
 * @member DS_TYPE_UINT32 Unsigned 32-bit integer.
 */

/*# Vector3 value
 *
 * @struct
 * @name DSVector3
 * @member m_Values [type:float[3]] X, Y and Z components.
 */

/*# Vector4 value
 *
 * @struct
 * @name DSVector4
 * @member m_Values [type:float[4]] X, Y, Z and W components.
 */

/*# Matrix4 value
 *
 * @struct
 * @name DSMatrix4
 * @member m_Values [type:float[16]] Column-major components; element at row r, column c is m_Values[c * 4 + r].
 */

#endif // DMSDK_DATASTORE_TYPES_H
