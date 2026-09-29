// Copyright 2026 The Defold Foundation
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

#ifndef DMSDK_DATA_FIELD_H
#define DMSDK_DATA_FIELD_H

#include <dmsdk/data/data_types.h>

#ifdef __cplusplus
extern "C"
{
#endif

    DataResult DataGetFieldNumber(HDataStore store, DataId id, uint64_t field, double* out_value);
    void       DataGetFieldNumberBatch(HDataStore store, const DataId* ids, uint64_t field, uint32_t count, double* out_values, DataResult* out_results);
    DataResult DataSetFieldNumber(HDataStore store, DataId id, uint64_t field, double value);

    DataResult DataGetFieldBoolean(HDataStore store, DataId id, uint64_t field, uint8_t* out_value);
    void       DataGetFieldBooleanBatch(HDataStore store, const DataId* ids, uint64_t field, uint32_t count, uint8_t* out_values, DataResult* out_results);
    DataResult DataSetFieldBoolean(HDataStore store, DataId id, uint64_t field, uint8_t value);

    DataResult DataGetFieldString(HDataStore store, DataId id, uint64_t field, const char** out_value);
    void       DataGetFieldStringBatch(HDataStore store, const DataId* ids, uint64_t field, uint32_t count, const char** out_values, DataResult* out_results);
    DataResult DataSetFieldString(HDataStore store, DataId id, uint64_t field, const char* value);

    DataResult DataGetFieldVector3(HDataStore store, DataId id, uint64_t field, DataVector3* out_value);
    void       DataGetFieldVector3Batch(HDataStore store, const DataId* ids, uint64_t field, uint32_t count, DataVector3* out_values, DataResult* out_results);
    DataResult DataSetFieldVector3(HDataStore store, DataId id, uint64_t field, const DataVector3* value);

    DataResult DataGetFieldVector4(HDataStore store, DataId id, uint64_t field, DataVector4* out_value);
    void       DataGetFieldVector4Batch(HDataStore store, const DataId* ids, uint64_t field, uint32_t count, DataVector4* out_values, DataResult* out_results);
    DataResult DataSetFieldVector4(HDataStore store, DataId id, uint64_t field, const DataVector4* value);

    DataResult DataGetFieldMatrix4(HDataStore store, DataId id, uint64_t field, DataMatrix4* out_value);
    void       DataGetFieldMatrix4Batch(HDataStore store, const DataId* ids, uint64_t field, uint32_t count, DataMatrix4* out_values, DataResult* out_results);
    DataResult DataSetFieldMatrix4(HDataStore store, DataId id, uint64_t field, const DataMatrix4* value);

#ifdef __cplusplus
}
#endif

// API documentation

/*# Data field API
 *
 * Typed access by row ID and field name hash. Types must match exactly.
 * Setters preserve metadata, iterators and reset defaults; failed writes change nothing.
 * Reads and fixed-size writes do not allocate. Synchronize access externally;
 * do not overlap query reservations.
 *
 * @document
 * @name DataField
 * @language C
 */

/*# Read Number fields by row ID
 *
 * Reads IDs in input order without allocation. Writes one status per ID; failed
 * values remain unchanged. Synchronize as for DataGetFieldNumber. Input/output
 * arrays must not overlap and contain count entries. Zero count accesses nothing.
 *
 * @name DataGetFieldNumberBatch
 * @param store [type:HDataStore] Store handle.
 * @param ids [type:const DataId*] Row IDs in requested order; duplicates are allowed.
 * @param field [type:uint64_t] Shared field name hash.
 * @param count [type:uint32_t] Number of IDs, values and statuses.
 * @param out_values [type:double*] Receives each value whose status is OK.
 * @param out_results [type:DataResult*] Per-ID OK, NOT_FOUND for an absent row/field, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a Number field
 *
 * @name DataGetFieldNumber
 * @param store [type:HDataStore] Store handle.
 * @param id [type:DataId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param out_value [type:double*] Receives the value on success.
 * @return result [type:DataResult] OK, NOT_FOUND for an absent row/field, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Write a Number field
 *
 * @name DataSetFieldNumber
 * @param store [type:HDataStore] Store handle.
 * @param id [type:DataId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param value [type:double] Replacement value.
 * @return result [type:DataResult] OK, NOT_FOUND for an absent row/field, INVALID_ARGUMENT for an invalid value/kind, or LOCKED during query reservations.
 */

/*# Read Boolean fields by row ID
 *
 * Reads IDs in input order without allocation. Writes one status per ID; failed
 * values remain unchanged. Synchronize as for DataGetFieldBoolean. Input/output
 * arrays must not overlap and contain count entries. Zero count accesses nothing.
 *
 * @name DataGetFieldBooleanBatch
 * @param store [type:HDataStore] Store handle.
 * @param ids [type:const DataId*] Row IDs in requested order; duplicates are allowed.
 * @param field [type:uint64_t] Shared field name hash.
 * @param count [type:uint32_t] Number of IDs, values and statuses.
 * @param out_values [type:uint8_t*] Receives each value whose status is OK.
 * @param out_results [type:DataResult*] Per-ID OK, NOT_FOUND for an absent row/field, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a Boolean field
 *
 * @name DataGetFieldBoolean
 * @param store [type:HDataStore] Store handle.
 * @param id [type:DataId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param out_value [type:uint8_t*] Receives the value on success.
 * @return result [type:DataResult] OK, NOT_FOUND for an absent row/field, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Write a Boolean field
 *
 * @name DataSetFieldBoolean
 * @param store [type:HDataStore] Store handle.
 * @param id [type:DataId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param value [type:uint8_t] Zero or one.
 * @return result [type:DataResult] OK, NOT_FOUND for an absent row/field, INVALID_ARGUMENT for an invalid value/kind, or LOCKED during query reservations.
 */

/*# Read String fields by row ID
 *
 * Reads IDs in input order without allocation. Writes one status per ID; failed
 * values remain unchanged. Synchronize as for DataGetFieldString. Input/output
 * arrays must not overlap and contain count entries. Zero count accesses nothing.
 * Returned strings are borrowed; finish using them before a store write or reset.
 *
 * @name DataGetFieldStringBatch
 * @param store [type:HDataStore] Store handle.
 * @param ids [type:const DataId*] Row IDs in requested order; duplicates are allowed.
 * @param field [type:uint64_t] Shared field name hash.
 * @param count [type:uint32_t] Number of IDs, values and statuses.
 * @param out_values [type:const char**] Receives each value whose status is OK.
 * @param out_results [type:DataResult*] Per-ID OK, NOT_FOUND for an absent row/field, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a String field
 *
 * @name DataGetFieldString
 * @param store [type:HDataStore] Store handle.
 * @param id [type:DataId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param out_value [type:const char**] Borrowed string; copy before a store write, reset or iterator step.
 * @return result [type:DataResult] OK, NOT_FOUND for an absent row/field, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Write a String field
 *
 * Copies the NUL-terminated string into owned storage; self-assignment is supported.
 *
 * @name DataSetFieldString
 * @param store [type:HDataStore] Store handle.
 * @param id [type:DataId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param value [type:const char*] Value to copy; pointer not retained.
 * @return result [type:DataResult] OK, NOT_FOUND for an absent row/field, INVALID_ARGUMENT for an invalid value/kind, or LOCKED during query reservations.
 */

/*# Read Vector3 fields by row ID
 *
 * Reads IDs in input order without allocation. Writes one status per ID; failed
 * values remain unchanged. Synchronize as for DataGetFieldVector3. Input/output
 * arrays must not overlap and contain count entries. Zero count accesses nothing.
 *
 * @name DataGetFieldVector3Batch
 * @param store [type:HDataStore] Store handle.
 * @param ids [type:const DataId*] Row IDs in requested order; duplicates are allowed.
 * @param field [type:uint64_t] Shared field name hash.
 * @param count [type:uint32_t] Number of IDs, values and statuses.
 * @param out_values [type:DataVector3*] Receives each value whose status is OK.
 * @param out_results [type:DataResult*] Per-ID OK, NOT_FOUND for an absent row/field, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a Vector3 field
 *
 * @name DataGetFieldVector3
 * @param store [type:HDataStore] Store handle.
 * @param id [type:DataId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param out_value [type:DataVector3*] Receives the value on success.
 * @return result [type:DataResult] OK, NOT_FOUND for an absent row/field, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Write a Vector3 field
 *
 * @name DataSetFieldVector3
 * @param store [type:HDataStore] Store handle.
 * @param id [type:DataId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param value [type:const DataVector3*] Value to copy; pointer not retained.
 * @return result [type:DataResult] OK, NOT_FOUND for an absent row/field, INVALID_ARGUMENT for an invalid value/kind, or LOCKED during query reservations.
 */

/*# Read Vector4 fields by row ID
 *
 * Reads IDs in input order without allocation. Writes one status per ID; failed
 * values remain unchanged. Synchronize as for DataGetFieldVector4. Input/output
 * arrays must not overlap and contain count entries. Zero count accesses nothing.
 *
 * @name DataGetFieldVector4Batch
 * @param store [type:HDataStore] Store handle.
 * @param ids [type:const DataId*] Row IDs in requested order; duplicates are allowed.
 * @param field [type:uint64_t] Shared field name hash.
 * @param count [type:uint32_t] Number of IDs, values and statuses.
 * @param out_values [type:DataVector4*] Receives each value whose status is OK.
 * @param out_results [type:DataResult*] Per-ID OK, NOT_FOUND for an absent row/field, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a Vector4 field
 *
 * @name DataGetFieldVector4
 * @param store [type:HDataStore] Store handle.
 * @param id [type:DataId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param out_value [type:DataVector4*] Receives the value on success.
 * @return result [type:DataResult] OK, NOT_FOUND for an absent row/field, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Write a Vector4 field
 *
 * @name DataSetFieldVector4
 * @param store [type:HDataStore] Store handle.
 * @param id [type:DataId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param value [type:const DataVector4*] Value to copy; pointer not retained.
 * @return result [type:DataResult] OK, NOT_FOUND for an absent row/field, INVALID_ARGUMENT for an invalid value/kind, or LOCKED during query reservations.
 */

/*# Read Matrix4 fields by row ID
 *
 * Reads IDs in input order without allocation. Writes one status per ID; failed
 * values remain unchanged. Synchronize as for DataGetFieldMatrix4. Input/output
 * arrays must not overlap and contain count entries. Zero count accesses nothing.
 *
 * @name DataGetFieldMatrix4Batch
 * @param store [type:HDataStore] Store handle.
 * @param ids [type:const DataId*] Row IDs in requested order; duplicates are allowed.
 * @param field [type:uint64_t] Shared field name hash.
 * @param count [type:uint32_t] Number of IDs, values and statuses.
 * @param out_values [type:DataMatrix4*] Receives each value whose status is OK.
 * @param out_results [type:DataResult*] Per-ID OK, NOT_FOUND for an absent row/field, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a Matrix4 field
 *
 * @name DataGetFieldMatrix4
 * @param store [type:HDataStore] Store handle.
 * @param id [type:DataId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param out_value [type:DataMatrix4*] Receives the value on success.
 * @return result [type:DataResult] OK, NOT_FOUND for an absent row/field, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Write a Matrix4 field
 *
 * @name DataSetFieldMatrix4
 * @param store [type:HDataStore] Store handle.
 * @param id [type:DataId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param value [type:const DataMatrix4*] Value to copy; pointer not retained.
 * @return result [type:DataResult] OK, NOT_FOUND for an absent row/field, INVALID_ARGUMENT for an invalid value/kind, or LOCKED during query reservations.
 */

#endif // DMSDK_DATA_FIELD_H
