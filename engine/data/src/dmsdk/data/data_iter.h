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

    // Query batches
    DataIterator DataQueryIterRange(HDataQuery query, uint32_t first, uint32_t count);
    DataIterator DataQueryIter(HDataQuery query);
    DataResult   DataIterNext(DataIterator* iterator);
    uint64_t     DataIterGetType(const DataIterator* iterator);

    // Rows
    static inline DataRowIterator DataIterRows(const DataIterator* iterator);
    static inline DataResult      DataRowIterNext(DataRowIterator* iterator);
    DataId                        DataRowIterGetId(const DataRowIterator* iterator);
    DataGroupId                   DataRowIterGetGroupId(const DataRowIterator* iterator);

    // Field pointers
    static inline const double*      DataRowIterGetNumber(const DataRowIterator* iterator, uint32_t field);
    static inline double*            DataRowIterGetNumberMut(const DataRowIterator* iterator, uint32_t field);

    static inline const uint8_t*     DataRowIterGetBoolean(const DataRowIterator* iterator, uint32_t field);
    static inline uint8_t*           DataRowIterGetBooleanMut(const DataRowIterator* iterator, uint32_t field);

    static inline const DataVector3* DataRowIterGetVector3(const DataRowIterator* iterator, uint32_t field);
    static inline DataVector3*       DataRowIterGetVector3Mut(const DataRowIterator* iterator, uint32_t field);

    static inline const DataVector4* DataRowIterGetVector4(const DataRowIterator* iterator, uint32_t field);
    static inline DataVector4*       DataRowIterGetVector4Mut(const DataRowIterator* iterator, uint32_t field);

    static inline const DataMatrix4* DataRowIterGetMatrix4(const DataRowIterator* iterator, uint32_t field);
    static inline DataMatrix4*       DataRowIterGetMatrix4Mut(const DataRowIterator* iterator, uint32_t field);

    // Inline implementations

    // private
    // Address calculation for the public pointer wrappers. Validated field handles and current
    // row indices are caller obligations; no repeated checks occur here.
    static inline const void*     DataFieldGetPointerInternal(const DataIterator* batch, uint32_t row, uint32_t field);
    static inline void*           DataFieldGetPointerMutInternal(const DataIterator* batch, uint32_t row, uint32_t field);

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

    static inline const void* DataFieldGetPointerInternal(const DataIterator* batch, uint32_t row, uint32_t field)
    {
        return batch->m_Values + (size_t)row * batch->m_RowStride + batch->m_FieldOffsets[field];
    }

    static inline void* DataFieldGetPointerMutInternal(const DataIterator* batch, uint32_t row, uint32_t field)
    {
        return batch->m_Values + (size_t)row * batch->m_RowStride + batch->m_FieldOffsets[field];
    }

    static inline const double* DataRowIterGetNumber(const DataRowIterator* iterator, uint32_t field)
    {
        return (const double*)DataFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline double* DataRowIterGetNumberMut(const DataRowIterator* iterator, uint32_t field)
    {
        return (double*)DataFieldGetPointerMutInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline const uint8_t* DataRowIterGetBoolean(const DataRowIterator* iterator, uint32_t field)
    {
        return (const uint8_t*)DataFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline uint8_t* DataRowIterGetBooleanMut(const DataRowIterator* iterator, uint32_t field)
    {
        return (uint8_t*)DataFieldGetPointerMutInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline const DataVector3* DataRowIterGetVector3(const DataRowIterator* iterator, uint32_t field)
    {
        return (const DataVector3*)DataFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline DataVector3* DataRowIterGetVector3Mut(const DataRowIterator* iterator, uint32_t field)
    {
        return (DataVector3*)DataFieldGetPointerMutInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline const DataVector4* DataRowIterGetVector4(const DataRowIterator* iterator, uint32_t field)
    {
        return (const DataVector4*)DataFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline DataVector4* DataRowIterGetVector4Mut(const DataRowIterator* iterator, uint32_t field)
    {
        return (DataVector4*)DataFieldGetPointerMutInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline const DataMatrix4* DataRowIterGetMatrix4(const DataRowIterator* iterator, uint32_t field)
    {
        return (const DataMatrix4*)DataFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline DataMatrix4* DataRowIterGetMatrix4Mut(const DataRowIterator* iterator, uint32_t field)
    {
        return (DataMatrix4*)DataFieldGetPointerMutInternal(iterator->m_Parent, iterator->m_Index, field);
    }

#ifdef __cplusplus
}
#endif

// API documentation

/*# Data iterator API
 *
 * Batch and row iteration with typed field pointers.
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
 * Batch and row order are unspecified.
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
 * Invalidates borrowed field pointers; clears the current row on END.
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

/*# Get the current row group
 *
 * @name DataRowIterGetGroupId
 * @param iterator [type:const DataRowIterator*] Row iterator.
 * @return group [type:DataGroupId] Current group, or zero without a valid row. Zero may also be a valid group.
 */

/*# Borrow a read-only Number field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataQueryFindField].
 *
 * @name DataRowIterGetNumber
 * @param iterator [type:const DataRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Number handle from DataQueryFindField.
 * @return value [type:const double*] Borrowed read-only value.
 */

/*# Borrow a writable Number field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataQueryFindField].
 *
 * @name DataRowIterGetNumberMut
 * @param iterator [type:const DataRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Number handle from DataQueryFindField.
 * @return value [type:double*] Borrowed writable value.
 */

/*# Borrow a read-only Boolean field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataQueryFindField].
 *
 * @name DataRowIterGetBoolean
 * @param iterator [type:const DataRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Boolean handle from DataQueryFindField.
 * @return value [type:const uint8_t*] Borrowed read-only value.
 */

/*# Borrow a writable Boolean field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataQueryFindField]. Write only zero or one.
 *
 * @name DataRowIterGetBooleanMut
 * @param iterator [type:const DataRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Boolean handle from DataQueryFindField.
 * @return value [type:uint8_t*] Borrowed writable value.
 */

/*# Borrow a read-only Vector3 field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataQueryFindField].
 *
 * @name DataRowIterGetVector3
 * @param iterator [type:const DataRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Vector3 handle from DataQueryFindField.
 * @return value [type:const DataVector3*] Borrowed read-only value.
 */

/*# Borrow a writable Vector3 field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataQueryFindField].
 *
 * @name DataRowIterGetVector3Mut
 * @param iterator [type:const DataRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Vector3 handle from DataQueryFindField.
 * @return value [type:DataVector3*] Borrowed writable value.
 */

/*# Borrow a read-only Vector4 field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataQueryFindField].
 *
 * @name DataRowIterGetVector4
 * @param iterator [type:const DataRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Vector4 handle from DataQueryFindField.
 * @return value [type:const DataVector4*] Borrowed read-only value.
 */

/*# Borrow a writable Vector4 field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataQueryFindField].
 *
 * @name DataRowIterGetVector4Mut
 * @param iterator [type:const DataRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Vector4 handle from DataQueryFindField.
 * @return value [type:DataVector4*] Borrowed writable value.
 */

/*# Borrow a read-only Matrix4 field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataQueryFindField].
 *
 * @name DataRowIterGetMatrix4
 * @param iterator [type:const DataRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Matrix4 handle from DataQueryFindField.
 * @return value [type:const DataMatrix4*] Borrowed read-only value.
 */

/*# Borrow a writable Matrix4 field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataQueryFindField].
 *
 * @name DataRowIterGetMatrix4Mut
 * @param iterator [type:const DataRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Matrix4 handle from DataQueryFindField.
 * @return value [type:DataMatrix4*] Borrowed writable value.
 */

#endif // DMSDK_DATA_ITER_H
