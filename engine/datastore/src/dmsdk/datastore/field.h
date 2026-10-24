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

#ifndef DMSDK_DATASTORE_FIELD_H
#define DMSDK_DATASTORE_FIELD_H

#include <dmsdk/datastore/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

    DSResult DSFieldGetNumber(HDSContext store, DSId id, uint64_t field, double* out_value);
    DSResult DSFieldGetNumberBatch(HDSContext store, uint32_t count, const DSId* ids, uint64_t field, double* out_values);
    DSResult DSSetFieldNumber(HDSContext store, DSId id, uint64_t field, double value);

    DSResult DSFieldGetFloat(HDSContext store, DSId id, uint64_t field, float* out_value);
    DSResult DSFieldGetFloatBatch(HDSContext store, uint32_t count, const DSId* ids, uint64_t field, float* out_values);
    DSResult DSSetFieldFloat(HDSContext store, DSId id, uint64_t field, float value);

    DSResult DSFieldGetUInt32(HDSContext store, DSId id, uint64_t field, uint32_t* out_value);
    DSResult DSFieldGetUInt32Batch(HDSContext store, uint32_t count, const DSId* ids, uint64_t field, uint32_t* out_values);
    DSResult DSSetFieldUInt32(HDSContext store, DSId id, uint64_t field, uint32_t value);

    DSResult DSFieldGetBoolean(HDSContext store, DSId id, uint64_t field, uint8_t* out_value);
    DSResult DSFieldGetBooleanBatch(HDSContext store, uint32_t count, const DSId* ids, uint64_t field, uint8_t* out_values);
    DSResult DSSetFieldBoolean(HDSContext store, DSId id, uint64_t field, uint8_t value);

    DSResult DSFieldGetString(HDSContext store, DSId id, uint64_t field, const char** out_value);
    DSResult DSFieldGetStringBatch(HDSContext store, uint32_t count, const DSId* ids, uint64_t field, const char** out_values);
    DSResult DSSetFieldString(HDSContext store, DSId id, uint64_t field, const char* value);

    DSResult DSFieldGetVector3(HDSContext store, DSId id, uint64_t field, DSVector3* out_value);
    DSResult DSFieldGetVector3Batch(HDSContext store, uint32_t count, const DSId* ids, uint64_t field, DSVector3* out_values);
    DSResult DSSetFieldVector3(HDSContext store, DSId id, uint64_t field, const DSVector3* value);

    DSResult DSFieldGetVector4(HDSContext store, DSId id, uint64_t field, DSVector4* out_value);
    DSResult DSFieldGetVector4Batch(HDSContext store, uint32_t count, const DSId* ids, uint64_t field, DSVector4* out_values);
    DSResult DSSetFieldVector4(HDSContext store, DSId id, uint64_t field, const DSVector4* value);

    DSResult DSFieldGetMatrix4(HDSContext store, DSId id, uint64_t field, DSMatrix4* out_value);
    DSResult DSFieldGetMatrix4Batch(HDSContext store, uint32_t count, const DSId* ids, uint64_t field, DSMatrix4* out_values);
    DSResult DSSetFieldMatrix4(HDSContext store, DSId id, uint64_t field, const DSMatrix4* value);

#ifdef __cplusplus
}
#endif

// API documentation

/*# Data field API
 *
 * Typed access by row ID and full field-name hash, including inline names such as
 * dmHashString64("light.color"). Types must match exactly.
 * Setters preserve metadata and reset defaults; failed writes change nothing.
 * Reads and fixed-size writes do not allocate. Synchronize access externally;
 * do not overlap query reservations.
 *
 * @document
 * @name DSField
 * @language C
 */

/*# Read Number fields by row ID
 *
 * Reads IDs in input order without allocation. Stops at the first error; outputs
 * may be partially written on failure; use them only after OK. Synchronize as
 * for DSFieldGetNumber. Input/output arrays must not overlap and must contain
 * count entries. Zero count accesses nothing.
 *
 * @name DSFieldGetNumberBatch
 * @param store [type:HDSContext] Store handle.
 * @param count [type:uint32_t] Number of IDs and output values.
 * @param ids [type:const DSId*] Row IDs in requested order; duplicates are allowed.
 * @param field [type:uint64_t] Shared field name hash.
 * @param out_values [type:double*] Receives the values on success.
 * @return result [type:DSResult] OK (including zero count), or the first NOT_FOUND for an absent row/field or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a Number field
 *
 * @name DSFieldGetNumber
 * @param store [type:HDSContext] Store handle.
 * @param id [type:DSId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param out_value [type:double*] Receives the value on success.
 * @return result [type:DSResult] OK, NOT_FOUND for an absent row/field, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Write a Number field
 *
 * @name DSSetFieldNumber
 * @param store [type:HDSContext] Store handle.
 * @param id [type:DSId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param value [type:double] Replacement value.
 * @return result [type:DSResult] OK, NOT_FOUND for an absent row/field, INVALID_ARGUMENT for an invalid value/kind, or LOCKED during query reservations.
 */

/*# Read Boolean fields by row ID
 *
 * Reads IDs in input order without allocation. Stops at the first error; outputs
 * may be partially written on failure; use them only after OK. Synchronize as
 * for DSFieldGetBoolean. Input/output arrays must not overlap and must contain
 * count entries. Zero count accesses nothing.
 *
 * @name DSFieldGetBooleanBatch
 * @param store [type:HDSContext] Store handle.
 * @param count [type:uint32_t] Number of IDs and output values.
 * @param ids [type:const DSId*] Row IDs in requested order; duplicates are allowed.
 * @param field [type:uint64_t] Shared field name hash.
 * @param out_values [type:uint8_t*] Receives the values on success.
 * @return result [type:DSResult] OK (including zero count), or the first NOT_FOUND for an absent row/field or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a Boolean field
 *
 * @name DSFieldGetBoolean
 * @param store [type:HDSContext] Store handle.
 * @param id [type:DSId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param out_value [type:uint8_t*] Receives the value on success.
 * @return result [type:DSResult] OK, NOT_FOUND for an absent row/field, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Write a Boolean field
 *
 * @name DSSetFieldBoolean
 * @param store [type:HDSContext] Store handle.
 * @param id [type:DSId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param value [type:uint8_t] Zero or one.
 * @return result [type:DSResult] OK, NOT_FOUND for an absent row/field, INVALID_ARGUMENT for an invalid value/kind, or LOCKED during query reservations.
 */

/*# Read String fields by row ID
 *
 * Reads IDs in input order without allocation. Stops at the first error; outputs
 * may be partially written on failure; use them only after OK. Synchronize as
 * for DSFieldGetString. Input/output arrays must not overlap and must contain
 * count entries. Zero count accesses nothing.
 * Returned strings are borrowed; finish using them before a store write or reset.
 *
 * @name DSFieldGetStringBatch
 * @param store [type:HDSContext] Store handle.
 * @param count [type:uint32_t] Number of IDs and output values.
 * @param ids [type:const DSId*] Row IDs in requested order; duplicates are allowed.
 * @param field [type:uint64_t] Shared field name hash.
 * @param out_values [type:const char**] Receives the values on success.
 * @return result [type:DSResult] OK (including zero count), or the first NOT_FOUND for an absent row/field or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a String field
 *
 * @name DSFieldGetString
 * @param store [type:HDSContext] Store handle.
 * @param id [type:DSId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param out_value [type:const char**] Borrowed string; copy before a store write, reset or iterator step.
 * @return result [type:DSResult] OK, NOT_FOUND for an absent row/field, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Write a String field
 *
 * Copies the NUL-terminated string into owned storage; self-assignment is supported.
 *
 * @name DSSetFieldString
 * @param store [type:HDSContext] Store handle.
 * @param id [type:DSId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param value [type:const char*] Value to copy; pointer not retained.
 * @return result [type:DSResult] OK, NOT_FOUND for an absent row/field, INVALID_ARGUMENT for an invalid value/kind, or LOCKED during query reservations.
 */

/*# Read Vector3 fields by row ID
 *
 * Reads IDs in input order without allocation. Stops at the first error; outputs
 * may be partially written on failure; use them only after OK. Synchronize as
 * for DSFieldGetVector3. Input/output arrays must not overlap and must contain
 * count entries. Zero count accesses nothing.
 *
 * @name DSFieldGetVector3Batch
 * @param store [type:HDSContext] Store handle.
 * @param count [type:uint32_t] Number of IDs and output values.
 * @param ids [type:const DSId*] Row IDs in requested order; duplicates are allowed.
 * @param field [type:uint64_t] Shared field name hash.
 * @param out_values [type:DSVector3*] Receives the values on success.
 * @return result [type:DSResult] OK (including zero count), or the first NOT_FOUND for an absent row/field or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a Vector3 field
 *
 * @name DSFieldGetVector3
 * @param store [type:HDSContext] Store handle.
 * @param id [type:DSId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param out_value [type:DSVector3*] Receives the value on success.
 * @return result [type:DSResult] OK, NOT_FOUND for an absent row/field, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Write a Vector3 field
 *
 * @name DSSetFieldVector3
 * @param store [type:HDSContext] Store handle.
 * @param id [type:DSId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param value [type:const DSVector3*] Value to copy; pointer not retained.
 * @return result [type:DSResult] OK, NOT_FOUND for an absent row/field, INVALID_ARGUMENT for an invalid value/kind, or LOCKED during query reservations.
 */

/*# Read Vector4 fields by row ID
 *
 * Reads IDs in input order without allocation. Stops at the first error; outputs
 * may be partially written on failure; use them only after OK. Synchronize as
 * for DSFieldGetVector4. Input/output arrays must not overlap and must contain
 * count entries. Zero count accesses nothing.
 *
 * @name DSFieldGetVector4Batch
 * @param store [type:HDSContext] Store handle.
 * @param count [type:uint32_t] Number of IDs and output values.
 * @param ids [type:const DSId*] Row IDs in requested order; duplicates are allowed.
 * @param field [type:uint64_t] Shared field name hash.
 * @param out_values [type:DSVector4*] Receives the values on success.
 * @return result [type:DSResult] OK (including zero count), or the first NOT_FOUND for an absent row/field or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a Vector4 field
 *
 * @name DSFieldGetVector4
 * @param store [type:HDSContext] Store handle.
 * @param id [type:DSId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param out_value [type:DSVector4*] Receives the value on success.
 * @return result [type:DSResult] OK, NOT_FOUND for an absent row/field, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Write a Vector4 field
 *
 * @name DSSetFieldVector4
 * @param store [type:HDSContext] Store handle.
 * @param id [type:DSId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param value [type:const DSVector4*] Value to copy; pointer not retained.
 * @return result [type:DSResult] OK, NOT_FOUND for an absent row/field, INVALID_ARGUMENT for an invalid value/kind, or LOCKED during query reservations.
 */

/*# Read Matrix4 fields by row ID
 *
 * Reads IDs in input order without allocation. Stops at the first error; outputs
 * may be partially written on failure; use them only after OK. Synchronize as
 * for DSFieldGetMatrix4. Input/output arrays must not overlap and must contain
 * count entries. Zero count accesses nothing.
 *
 * @name DSFieldGetMatrix4Batch
 * @param store [type:HDSContext] Store handle.
 * @param count [type:uint32_t] Number of IDs and output values.
 * @param ids [type:const DSId*] Row IDs in requested order; duplicates are allowed.
 * @param field [type:uint64_t] Shared field name hash.
 * @param out_values [type:DSMatrix4*] Receives the values on success.
 * @return result [type:DSResult] OK (including zero count), or the first NOT_FOUND for an absent row/field or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a Matrix4 field
 *
 * @name DSFieldGetMatrix4
 * @param store [type:HDSContext] Store handle.
 * @param id [type:DSId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param out_value [type:DSMatrix4*] Receives the value on success.
 * @return result [type:DSResult] OK, NOT_FOUND for an absent row/field, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Write a Matrix4 field
 *
 * @name DSSetFieldMatrix4
 * @param store [type:HDSContext] Store handle.
 * @param id [type:DSId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param value [type:const DSMatrix4*] Value to copy; pointer not retained.
 * @return result [type:DSResult] OK, NOT_FOUND for an absent row/field, INVALID_ARGUMENT for an invalid value/kind, or LOCKED during query reservations.
 */

/*# Read Float fields by row ID
 *
 * Reads IDs in input order without allocation. Stops at the first error; outputs
 * may be partially written on failure; use them only after OK. Synchronize as
 * for DSFieldGetFloat. Input/output arrays must not overlap and must contain
 * count entries. Zero count accesses nothing.
 *
 * @name DSFieldGetFloatBatch
 * @param store [type:HDSContext] Store handle.
 * @param count [type:uint32_t] Number of IDs and output values.
 * @param ids [type:const DSId*] Row IDs in requested order; duplicates are allowed.
 * @param field [type:uint64_t] Shared field name hash.
 * @param out_values [type:float*] Receives the values on success.
 * @return result [type:DSResult] OK (including zero count), or the first NOT_FOUND for an absent row/field or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read UInt32 fields by row ID
 *
 * Reads IDs in input order without allocation. Stops at the first error; outputs
 * may be partially written on failure; use them only after OK. Synchronize as
 * for DSFieldGetUInt32. Input/output arrays must not overlap and must contain
 * count entries. Zero count accesses nothing.
 *
 * @name DSFieldGetUInt32Batch
 * @param store [type:HDSContext] Store handle.
 * @param count [type:uint32_t] Number of IDs and output values.
 * @param ids [type:const DSId*] Row IDs in requested order; duplicates are allowed.
 * @param field [type:uint64_t] Shared field name hash.
 * @param out_values [type:uint32_t*] Receives the values on success.
 * @return result [type:DSResult] OK (including zero count), or the first NOT_FOUND for an absent row/field or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a Float field
 *
 * @name DSFieldGetFloat
 * @param store [type:HDSContext] Store handle.
 * @param id [type:DSId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param out_value [type:float*] Receives the value on success.
 * @return result [type:DSResult] OK, NOT_FOUND for an absent row/field, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a UInt32 field
 *
 * @name DSFieldGetUInt32
 * @param store [type:HDSContext] Store handle.
 * @param id [type:DSId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param out_value [type:uint32_t*] Receives the value on success.
 * @return result [type:DSResult] OK, NOT_FOUND for an absent row/field, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Write a Float field
 *
 * @name DSSetFieldFloat
 * @param store [type:HDSContext] Store handle.
 * @param id [type:DSId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param value [type:float] Replacement value.
 * @return result [type:DSResult] OK, NOT_FOUND for an absent row/field, INVALID_ARGUMENT for an invalid value/kind, or LOCKED during query reservations.
 */

/*# Write a UInt32 field
 *
 * @name DSSetFieldUInt32
 * @param store [type:HDSContext] Store handle.
 * @param id [type:DSId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param value [type:uint32_t] Replacement value.
 * @return result [type:DSResult] OK, NOT_FOUND for an absent row/field, INVALID_ARGUMENT for an invalid value/kind, or LOCKED during query reservations.
 */

#endif // DMSDK_DATASTORE_FIELD_H
