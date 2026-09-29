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

#ifndef DMSDK_DATA_ITER_H
#define DMSDK_DATA_ITER_H

#include <stddef.h>
#include <dmsdk/data/data_types.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct DataIterator
    {
        // private
        HDataQuery      m_Query;
        uint32_t        m_TableIndex;
        uint32_t        m_NextRow;
        uint32_t        m_StartRow;
        uint32_t        m_Count;
        void*           m_Table;
        void*           m_Rows;
        const void*     m_Fields;
        const uint32_t* m_FieldOffsets;
        uint8_t*        m_Values;
        uint32_t        m_RowStride;
        uint32_t        m_FieldCount;
        uint32_t        m_RangeIndex;
        uint32_t        m_Remaining;
        uint8_t         m_IsRange;
    } DataIterator;

    typedef struct DataRowIterator
    {
        // private
        const DataIterator* m_Parent;
        uint32_t            m_Index;
        uint32_t            m_NextRow;
    } DataRowIterator;

    typedef struct DataFieldIterator
    {
        uint32_t m_Index;

        // private
        uint32_t            m_NextField;
        const DataIterator* m_Batch;
        uint32_t            m_RowIndex;
    } DataFieldIterator;

    // Query batches
    DataIterator DataQueryIterRange(HDataQuery query, uint32_t first, uint32_t count);
    DataIterator DataQueryIter(HDataQuery query);
    DataResult   DataIterNext(DataIterator* iterator);
    uint64_t     DataIterGetType(const DataIterator* iterator);

    // Rows
    static inline DataRowIterator DataIterRows(const DataIterator* iterator);
    static inline DataResult      DataRowIterNext(DataRowIterator* iterator);
    DataId                        DataRowIterGetId(const DataRowIterator* iterator);
    DataOwnerId                   DataRowIterGetOwnerId(const DataRowIterator* iterator);

    // Field pointers
    static inline const double*      DataFieldGetNumber(const DataRowIterator* iterator, uint32_t field);
    static inline double*            DataFieldGetNumberMut(const DataRowIterator* iterator, uint32_t field);

    static inline const uint8_t*     DataFieldGetBoolean(const DataRowIterator* iterator, uint32_t field);
    static inline uint8_t*           DataFieldGetBooleanMut(const DataRowIterator* iterator, uint32_t field);

    static inline const DataVector3* DataFieldGetVector3(const DataRowIterator* iterator, uint32_t field);
    static inline DataVector3*       DataFieldGetVector3Mut(const DataRowIterator* iterator, uint32_t field);

    static inline const DataVector4* DataFieldGetVector4(const DataRowIterator* iterator, uint32_t field);
    static inline DataVector4*       DataFieldGetVector4Mut(const DataRowIterator* iterator, uint32_t field);

    static inline const DataMatrix4* DataFieldGetMatrix4(const DataRowIterator* iterator, uint32_t field);
    static inline DataMatrix4*       DataFieldGetMatrix4Mut(const DataRowIterator* iterator, uint32_t field);

    // Field iteration
    static inline DataFieldIterator DataRowIterFields(const DataRowIterator* iterator);
    static inline DataResult        DataFieldIterNext(DataFieldIterator* iterator);
    DataValueType                   DataFieldIterGetType(const DataFieldIterator* iterator);
    uint64_t                        DataFieldIterGetNameHash(const DataFieldIterator* iterator);

    // Field values
    static inline DataResult DataFieldIterGetNumber(const DataFieldIterator* iterator, double* out_value);
    DataResult               DataFieldIterSetNumber(const DataFieldIterator* iterator, double value);

    static inline DataResult DataFieldIterGetBoolean(const DataFieldIterator* iterator, uint8_t* out_value);
    DataResult               DataFieldIterSetBoolean(const DataFieldIterator* iterator, uint8_t value);

    static inline DataResult DataFieldIterGetString(const DataFieldIterator* iterator, const char** out_value);
    DataResult               DataFieldIterSetString(const DataFieldIterator* iterator, const char* value);

    static inline DataResult DataFieldIterGetVector3(const DataFieldIterator* iterator, DataVector3* out_value);
    DataResult               DataFieldIterSetVector3(const DataFieldIterator* iterator, const DataVector3* value);

    static inline DataResult DataFieldIterGetVector4(const DataFieldIterator* iterator, DataVector4* out_value);
    DataResult               DataFieldIterSetVector4(const DataFieldIterator* iterator, const DataVector4* value);

    static inline DataResult DataFieldIterGetMatrix4(const DataFieldIterator* iterator, DataMatrix4* out_value);
    DataResult               DataFieldIterSetMatrix4(const DataFieldIterator* iterator, const DataMatrix4* value);

    // Inline implementations

    // private
    // Address calculation for the public pointer wrappers. Validated field handles and current
    // row indices are caller obligations; no repeated checks occur here.
    static inline const void* DataGetFieldPointerInternal(const DataIterator* batch, uint32_t row, uint32_t field);
    static inline void*       DataGetFieldPointerMutInternal(const DataIterator* batch, uint32_t row, uint32_t field);

    // private
    // Linkage for the inline typed getters below, not standalone SDK entry points.
    // Arguments borrow the locked batch and use its row/requested-field indices.
    // UINT32_MAX denotes no current field; outputs remain unchanged on errors.
    DataResult                    DataGetFieldNumberInternal(const DataIterator* batch, uint32_t row, uint32_t field, double* out_value);
    DataResult                    DataGetFieldBooleanInternal(const DataIterator* batch, uint32_t row, uint32_t field, uint8_t* out_value);
    DataResult                    DataGetFieldStringInternal(const DataIterator* batch, uint32_t row, uint32_t field, const char** out_value);
    DataResult                    DataGetFieldVector3Internal(const DataIterator* batch, uint32_t row, uint32_t field, DataVector3* out_value);
    DataResult                    DataGetFieldVector4Internal(const DataIterator* batch, uint32_t row, uint32_t field, DataVector4* out_value);
    DataResult                    DataGetFieldMatrix4Internal(const DataIterator* batch, uint32_t row, uint32_t field, DataMatrix4* out_value);

    static inline DataRowIterator DataIterRows(const DataIterator* iterator)
    {
        DataRowIterator rows = { .m_Parent = iterator, .m_Index = UINT32_MAX };
        return rows;
    }

    static inline DataResult DataRowIterNext(DataRowIterator* iterator)
    {
        if (iterator->m_NextRow >= iterator->m_Parent->m_Count)
        {
            iterator->m_Index = UINT32_MAX;
            return DATA_RESULT_END;
        }
        iterator->m_Index = iterator->m_NextRow++;
        return DATA_RESULT_OK;
    }

    static inline const void* DataGetFieldPointerInternal(const DataIterator* batch, uint32_t row, uint32_t field)
    {
        return batch->m_Values + (size_t)row * batch->m_RowStride + batch->m_FieldOffsets[field];
    }

    static inline void* DataGetFieldPointerMutInternal(const DataIterator* batch, uint32_t row, uint32_t field)
    {
        return batch->m_Values + (size_t)row * batch->m_RowStride + batch->m_FieldOffsets[field];
    }

    static inline const double* DataFieldGetNumber(const DataRowIterator* iterator, uint32_t field)
    {
        return (const double*)DataGetFieldPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline double* DataFieldGetNumberMut(const DataRowIterator* iterator, uint32_t field)
    {
        return (double*)DataGetFieldPointerMutInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline const uint8_t* DataFieldGetBoolean(const DataRowIterator* iterator, uint32_t field)
    {
        return (const uint8_t*)DataGetFieldPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline uint8_t* DataFieldGetBooleanMut(const DataRowIterator* iterator, uint32_t field)
    {
        return (uint8_t*)DataGetFieldPointerMutInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline const DataVector3* DataFieldGetVector3(const DataRowIterator* iterator, uint32_t field)
    {
        return (const DataVector3*)DataGetFieldPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline DataVector3* DataFieldGetVector3Mut(const DataRowIterator* iterator, uint32_t field)
    {
        return (DataVector3*)DataGetFieldPointerMutInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline const DataVector4* DataFieldGetVector4(const DataRowIterator* iterator, uint32_t field)
    {
        return (const DataVector4*)DataGetFieldPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline DataVector4* DataFieldGetVector4Mut(const DataRowIterator* iterator, uint32_t field)
    {
        return (DataVector4*)DataGetFieldPointerMutInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline const DataMatrix4* DataFieldGetMatrix4(const DataRowIterator* iterator, uint32_t field)
    {
        return (const DataMatrix4*)DataGetFieldPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline DataMatrix4* DataFieldGetMatrix4Mut(const DataRowIterator* iterator, uint32_t field)
    {
        return (DataMatrix4*)DataGetFieldPointerMutInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline DataFieldIterator DataRowIterFields(const DataRowIterator* iterator)
    {
        DataFieldIterator fields = {
            .m_Index = UINT32_MAX,
            .m_NextField = iterator->m_Index == UINT32_MAX ? iterator->m_Parent->m_FieldCount : 0,
            .m_Batch = iterator->m_Parent,
            .m_RowIndex = iterator->m_Index
        };
        return fields;
    }

    static inline DataResult DataFieldIterNext(DataFieldIterator* iterator)
    {
        if (iterator->m_NextField == iterator->m_Batch->m_FieldCount)
        {
            iterator->m_Index = UINT32_MAX;
            return DATA_RESULT_END;
        }
        iterator->m_Index = iterator->m_NextField++;
        return DATA_RESULT_OK;
    }

    static inline DataResult DataFieldIterGetNumber(const DataFieldIterator* iterator, double* out_value)
    {
        return DataGetFieldNumberInternal(iterator->m_Batch, iterator->m_RowIndex, iterator->m_Index, out_value);
    }

    static inline DataResult DataFieldIterGetBoolean(const DataFieldIterator* iterator, uint8_t* out_value)
    {
        return DataGetFieldBooleanInternal(iterator->m_Batch, iterator->m_RowIndex, iterator->m_Index, out_value);
    }

    static inline DataResult DataFieldIterGetString(const DataFieldIterator* iterator, const char** out_value)
    {
        return DataGetFieldStringInternal(iterator->m_Batch, iterator->m_RowIndex, iterator->m_Index, out_value);
    }

    static inline DataResult DataFieldIterGetVector3(const DataFieldIterator* iterator, DataVector3* out_value)
    {
        return DataGetFieldVector3Internal(iterator->m_Batch, iterator->m_RowIndex, iterator->m_Index, out_value);
    }

    static inline DataResult DataFieldIterGetVector4(const DataFieldIterator* iterator, DataVector4* out_value)
    {
        return DataGetFieldVector4Internal(iterator->m_Batch, iterator->m_RowIndex, iterator->m_Index, out_value);
    }

    static inline DataResult DataFieldIterGetMatrix4(const DataFieldIterator* iterator, DataMatrix4* out_value)
    {
        return DataGetFieldMatrix4Internal(iterator->m_Batch, iterator->m_RowIndex, iterator->m_Index, out_value);
    }

#ifdef __cplusplus
}
#endif

// API documentation

/*# Data iterator API
 *
 * Batch, row and field iteration with typed value access.
 *
 * @document
 * @name DataIter
 * @language C
 */

/*# Query iterator
 *
 * Stack cursor; iterators and fixed-size field access allocate nothing.
 * Keep the query/store alive and the store locked or query reservation active.
 * Do not copy active iterators. Parents must stay at the same address; stepping
 * one invalidates its children, including on END. Lifetimes are not checked.
 * Batch, row and field order are unspecified.
 *
 * @struct
 * @name DataIterator
 */

/*# Row iterator
 *
 * Traverses the current batch. [ref:DataIterator] lifetime rules apply.
 * Writes and resets preserve iteration; fixed-size writes preserve sibling fields and defaults.
 *
 * @struct
 * @name DataRowIterator
 */

/*# Field iterator
 *
 * Visits requested fields, or all top-level fields when none are requested;
 * containers are not traversed recursively. [ref:DataIterator] lifetime rules apply.
 * Typed access requires an exact kind. Writes preserve iterators and reset defaults;
 * failed writes change nothing. Reads observe current values.
 *
 * @struct
 * @name DataFieldIterator
 * @member m_Index [type:uint32_t] Read-only field index; UINT32_MAX without a current field.
 */

/*# Iterate a reserved row range
 *
 * Selects [first, first + count) within reserved rows, possibly across tables.
 * Each job owns its cursor; ranges must be in bounds and disjoint when writing.
 * Keep the reservation active until all iterators and borrowed pointers finish.
 *
 * @name DataQueryIterRange
 * @param query [type:HDataQuery] Query reserved for these jobs.
 * @param first [type:uint32_t] Index in the reserved matching rows.
 * @param count [type:uint32_t] Row count; zero is allowed.
 * @return iterator [type:DataIterator] Cursor to advance with DataIterNext.
 */

/*# Start query iteration
 *
 * Requires DataStoreLock throughout traversal, including early exits.
 *
 * @name DataQueryIter
 * @param query [type:HDataQuery] Query to traverse.
 * @return iterator [type:DataIterator] Iterator with no current batch; advance using DataIterNext.
 */

/*# Advance to the next batch
 *
 * Selects a nonempty contiguous batch. Invalidates child iterators, including on END.
 *
 * @name DataIterNext
 * @param iterator [type:DataIterator*] Iterator to advance.
 * @return result [type:DataResult] OK for a new batch, or END when exhausted.
 */

/*# Get the current table type
 *
 * @name DataIterGetType
 * @param iterator [type:const DataIterator*] Batch iterator.
 * @return type [type:uint64_t] Table type hash, or zero without a valid current batch. Zero may also be a valid type hash.
 */

/*# Start row iteration
 *
 * Borrows the current batch; empty when no batch is selected.
 *
 * @name DataIterRows
 * @param iterator [type:const DataIterator*] Parent query iterator to borrow.
 * @return rows [type:DataRowIterator] Iterator with no current row; advance using DataRowIterNext.
 */

/*# Advance to the next row
 *
 * Invalidates child field iterators; clears the current row on END.
 *
 * @name DataRowIterNext
 * @param iterator [type:DataRowIterator*] Row iterator.
 * @return result [type:DataResult] OK for a row, or END when exhausted.
 */

/*# Get the current row ID
 *
 * @name DataRowIterGetId
 * @param iterator [type:const DataRowIterator*] Row iterator.
 * @return id [type:DataId] Store-local row ID, or zero without a valid current row.
 */

/*# Get the current row owner
 *
 * @name DataRowIterGetOwnerId
 * @param iterator [type:const DataRowIterator*] Row iterator.
 * @return owner [type:DataOwnerId] Current owner, or zero without a valid row. Zero may also be a valid owner.
 */

/*# Borrow a read-only Number field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataQueryFindField].
 *
 * @name DataFieldGetNumber
 * @param iterator [type:const DataRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Number handle from DataQueryFindField.
 * @return value [type:const double*] Borrowed read-only value.
 */

/*# Borrow a writable Number field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataQueryFindField].
 *
 * @name DataFieldGetNumberMut
 * @param iterator [type:const DataRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Number handle from DataQueryFindField.
 * @return value [type:double*] Borrowed writable value.
 */

/*# Borrow a read-only Boolean field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataQueryFindField].
 *
 * @name DataFieldGetBoolean
 * @param iterator [type:const DataRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Boolean handle from DataQueryFindField.
 * @return value [type:const uint8_t*] Borrowed read-only value.
 */

/*# Borrow a writable Boolean field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataQueryFindField]. Write only zero or one.
 *
 * @name DataFieldGetBooleanMut
 * @param iterator [type:const DataRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Boolean handle from DataQueryFindField.
 * @return value [type:uint8_t*] Borrowed writable value.
 */

/*# Borrow a read-only Vector3 field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataQueryFindField].
 *
 * @name DataFieldGetVector3
 * @param iterator [type:const DataRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Vector3 handle from DataQueryFindField.
 * @return value [type:const DataVector3*] Borrowed read-only value.
 */

/*# Borrow a writable Vector3 field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataQueryFindField].
 *
 * @name DataFieldGetVector3Mut
 * @param iterator [type:const DataRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Vector3 handle from DataQueryFindField.
 * @return value [type:DataVector3*] Borrowed writable value.
 */

/*# Borrow a read-only Vector4 field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataQueryFindField].
 *
 * @name DataFieldGetVector4
 * @param iterator [type:const DataRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Vector4 handle from DataQueryFindField.
 * @return value [type:const DataVector4*] Borrowed read-only value.
 */

/*# Borrow a writable Vector4 field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataQueryFindField].
 *
 * @name DataFieldGetVector4Mut
 * @param iterator [type:const DataRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Vector4 handle from DataQueryFindField.
 * @return value [type:DataVector4*] Borrowed writable value.
 */

/*# Borrow a read-only Matrix4 field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataQueryFindField].
 *
 * @name DataFieldGetMatrix4
 * @param iterator [type:const DataRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Matrix4 handle from DataQueryFindField.
 * @return value [type:const DataMatrix4*] Borrowed read-only value.
 */

/*# Borrow a writable Matrix4 field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataQueryFindField].
 *
 * @name DataFieldGetMatrix4Mut
 * @param iterator [type:const DataRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Matrix4 handle from DataQueryFindField.
 * @return value [type:DataMatrix4*] Borrowed writable value.
 */

/*# Start field iteration
 *
 * Borrows the current row; empty when no row is selected.
 *
 * @name DataRowIterFields
 * @param iterator [type:const DataRowIterator*] Row iterator.
 * @return fields [type:DataFieldIterator] Iterator with no current field; advance using DataFieldIterNext.
 */

/*# Advance to the next field
 *
 * Sets m_Index to UINT32_MAX on END.
 *
 * @name DataFieldIterNext
 * @param iterator [type:DataFieldIterator*] Field iterator.
 * @return result [type:DataResult] OK for a field, or END when exhausted.
 */

/*# Get the current field kind
 *
 * @name DataFieldIterGetType
 * @param iterator [type:const DataFieldIterator*] Field iterator.
 * @return type [type:DataValueType] Declared kind, or NULL without a current field. NULL may also be a declared kind.
 */

/*# Get the current field name hash
 *
 * For a bound member path, returns the final member name.
 *
 * @name DataFieldIterGetNameHash
 * @param iterator [type:const DataFieldIterator*] Field iterator.
 * @return name [type:uint64_t] Name hash, or zero without a current field. Zero may also be a valid name hash.
 */

/*# Read a Number field
 *
 * @name DataFieldIterGetNumber
 * @param iterator [type:const DataFieldIterator*] Iterator on the field.
 * @param out_value [type:double*] Receives the value on success.
 * @return result [type:DataResult] OK, or INVALID_ARGUMENT for no current field or a kind mismatch.
 */

/*# Write a Number field
 *
 * @name DataFieldIterSetNumber
 * @param iterator [type:const DataFieldIterator*] Iterator on the field.
 * @param value [type:double] Replacement value.
 * @return result [type:DataResult] OK, or INVALID_ARGUMENT for no current field or invalid value/kind/access.
 */

/*# Read a Boolean field
 *
 * @name DataFieldIterGetBoolean
 * @param iterator [type:const DataFieldIterator*] Iterator on the field.
 * @param out_value [type:uint8_t*] Receives the value on success.
 * @return result [type:DataResult] OK, or INVALID_ARGUMENT for no current field or a kind mismatch.
 */

/*# Write a Boolean field
 *
 * @name DataFieldIterSetBoolean
 * @param iterator [type:const DataFieldIterator*] Iterator on the field.
 * @param value [type:uint8_t] Zero or one.
 * @return result [type:DataResult] OK, or INVALID_ARGUMENT for no current field or invalid value/kind/access.
 */

/*# Read a String field
 *
 * @name DataFieldIterGetString
 * @param iterator [type:const DataFieldIterator*] Iterator on the field.
 * @param out_value [type:const char**] Borrowed string; copy before a store write, reset or iterator step.
 * @return result [type:DataResult] OK, or INVALID_ARGUMENT for no current field or a kind mismatch.
 */

/*# Write a String field
 *
 * Copies the NUL-terminated string into owned storage; self-assignment is supported.
 *
 * @name DataFieldIterSetString
 * @param iterator [type:const DataFieldIterator*] Iterator on the field.
 * @param value [type:const char*] Value to copy; pointer not retained.
 * @return result [type:DataResult] OK, or INVALID_ARGUMENT for no current field or invalid value/kind/access.
 */

/*# Read a Vector3 field
 *
 * @name DataFieldIterGetVector3
 * @param iterator [type:const DataFieldIterator*] Iterator on the field.
 * @param out_value [type:DataVector3*] Receives the value on success.
 * @return result [type:DataResult] OK, or INVALID_ARGUMENT for no current field or a kind mismatch.
 */

/*# Write a Vector3 field
 *
 * @name DataFieldIterSetVector3
 * @param iterator [type:const DataFieldIterator*] Iterator on the field.
 * @param value [type:const DataVector3*] Value to copy; pointer not retained.
 * @return result [type:DataResult] OK, or INVALID_ARGUMENT for no current field or invalid value/kind/access.
 */

/*# Read a Vector4 field
 *
 * @name DataFieldIterGetVector4
 * @param iterator [type:const DataFieldIterator*] Iterator on the field.
 * @param out_value [type:DataVector4*] Receives the value on success.
 * @return result [type:DataResult] OK, or INVALID_ARGUMENT for no current field or a kind mismatch.
 */

/*# Write a Vector4 field
 *
 * @name DataFieldIterSetVector4
 * @param iterator [type:const DataFieldIterator*] Iterator on the field.
 * @param value [type:const DataVector4*] Value to copy; pointer not retained.
 * @return result [type:DataResult] OK, or INVALID_ARGUMENT for no current field or invalid value/kind/access.
 */

/*# Read a Matrix4 field
 *
 * @name DataFieldIterGetMatrix4
 * @param iterator [type:const DataFieldIterator*] Iterator on the field.
 * @param out_value [type:DataMatrix4*] Receives the value on success.
 * @return result [type:DataResult] OK, or INVALID_ARGUMENT for no current field or a kind mismatch.
 */

/*# Write a Matrix4 field
 *
 * @name DataFieldIterSetMatrix4
 * @param iterator [type:const DataFieldIterator*] Iterator on the field.
 * @param value [type:const DataMatrix4*] Value to copy; pointer not retained.
 * @return result [type:DataResult] OK, or INVALID_ARGUMENT for no current field or invalid value/kind/access.
 */

#endif // DMSDK_DATA_ITER_H
