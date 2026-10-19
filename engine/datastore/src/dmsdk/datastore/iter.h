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

#ifndef DMSDK_DATASTORE_ITER_H
#define DMSDK_DATASTORE_ITER_H

#include <stdbool.h>
#include <stddef.h>
#include <dmsdk/datastore/query.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct DSIterator
    {
        // private
        HDSQuery        m_Query;
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
        uint32_t        m_EndRow;
        uint32_t        m_FieldCount;
        uint32_t        m_RangeIndex;
        uint32_t        m_Remaining;
        uint8_t         m_Flags;
    } DSIterator;

    typedef struct DSRowIterator
    {
        // private
        const DSIterator*   m_Parent;
        uint32_t            m_Index;
        uint32_t            m_NextRow;
    } DSRowIterator;

    // Query batches
    DSIterator DSQueryIterRange(HDSQuery query, uint32_t first, uint32_t count);
    DSIterator DSQueryIter(HDSQuery query);
    bool       DSIterNext(DSIterator* iterator);
    uint64_t   DSIterGetType(const DSIterator* iterator);

    // Rows
    static inline DSRowIterator DSIterRows(const DSIterator* iterator);
    static inline bool          DSRowIterNext(DSRowIterator* iterator);
    DSId                        DSRowIterGetId(const DSRowIterator* iterator);
    DSGroupId                   DSRowIterGetGroupId(const DSRowIterator* iterator);
    static inline const void*   DSRowIterGetData(const DSRowIterator* iterator);
    static inline void*         DSRowIterGetDataMut(const DSRowIterator* iterator);

    // Field pointers
    static inline const double*    DSRowIterGetNumber(const DSRowIterator* iterator, uint32_t field);
    static inline double*          DSRowIterGetNumberMut(const DSRowIterator* iterator, uint32_t field);

    static inline const float*     DSRowIterGetFloat(const DSRowIterator* iterator, uint32_t field);
    static inline float*           DSRowIterGetFloatMut(const DSRowIterator* iterator, uint32_t field);

    static inline const uint32_t*  DSRowIterGetUInt32(const DSRowIterator* iterator, uint32_t field);
    static inline uint32_t*        DSRowIterGetUInt32Mut(const DSRowIterator* iterator, uint32_t field);

    static inline const uint8_t*   DSRowIterGetBoolean(const DSRowIterator* iterator, uint32_t field);
    static inline uint8_t*         DSRowIterGetBooleanMut(const DSRowIterator* iterator, uint32_t field);

    static inline const DSVector3* DSRowIterGetVector3(const DSRowIterator* iterator, uint32_t field);
    static inline DSVector3*       DSRowIterGetVector3Mut(const DSRowIterator* iterator, uint32_t field);

    static inline const DSVector4* DSRowIterGetVector4(const DSRowIterator* iterator, uint32_t field);
    static inline DSVector4*       DSRowIterGetVector4Mut(const DSRowIterator* iterator, uint32_t field);

    static inline const DSMatrix4* DSRowIterGetMatrix4(const DSRowIterator* iterator, uint32_t field);
    static inline DSMatrix4*       DSRowIterGetMatrix4Mut(const DSRowIterator* iterator, uint32_t field);

    // Inline implementations

    // private
    // Address calculation for the public pointer wrappers. Validated field handles and current
    // row indices are caller obligations; no repeated checks occur here.
    static inline const void*   DSFieldGetPointerInternal(const DSIterator* batch, uint32_t row, uint32_t field);
    static inline void*         DSFieldGetPointerMutInternal(const DSIterator* batch, uint32_t row, uint32_t field);

    static inline DSRowIterator DSIterRows(const DSIterator* iterator)
    {
        DSRowIterator rows = { .m_Parent = iterator, .m_Index = UINT32_MAX, .m_NextRow = iterator->m_StartRow };
        return rows;
    }

    static inline bool DSRowIterNext(DSRowIterator* iterator)
    {
        if (iterator->m_NextRow >= iterator->m_Parent->m_EndRow)
        {
            iterator->m_Index = UINT32_MAX;
            return false;
        }
        iterator->m_Index = iterator->m_NextRow++;
        return true;
    }

    static inline const void* DSRowIterGetData(const DSRowIterator* iterator)
    {
        if (!(iterator->m_Parent->m_Flags & DS_ROW_MUTABLE))
            return 0;
        return iterator->m_Parent->m_Values + (size_t)iterator->m_Index * iterator->m_Parent->m_RowStride;
    }

    static inline void* DSRowIterGetDataMut(const DSRowIterator* iterator)
    {
        if (!(iterator->m_Parent->m_Flags & DS_ROW_MUTABLE))
            return 0;
        return iterator->m_Parent->m_Values + (size_t)iterator->m_Index * iterator->m_Parent->m_RowStride;
    }

    static inline const void* DSFieldGetPointerInternal(const DSIterator* batch, uint32_t row, uint32_t field)
    {
        return batch->m_Values + (size_t)row * batch->m_RowStride + batch->m_FieldOffsets[field];
    }

    static inline void* DSFieldGetPointerMutInternal(const DSIterator* batch, uint32_t row, uint32_t field)
    {
        return batch->m_Values + (size_t)row * batch->m_RowStride + batch->m_FieldOffsets[field];
    }

    static inline const double* DSRowIterGetNumber(const DSRowIterator* iterator, uint32_t field)
    {
        return (const double*)DSFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline double* DSRowIterGetNumberMut(const DSRowIterator* iterator, uint32_t field)
    {
        return (double*)DSFieldGetPointerMutInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline const float* DSRowIterGetFloat(const DSRowIterator* iterator, uint32_t field)
    {
        return (const float*)DSFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline float* DSRowIterGetFloatMut(const DSRowIterator* iterator, uint32_t field)
    {
        return (float*)DSFieldGetPointerMutInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline const uint32_t* DSRowIterGetUInt32(const DSRowIterator* iterator, uint32_t field)
    {
        return (const uint32_t*)DSFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline uint32_t* DSRowIterGetUInt32Mut(const DSRowIterator* iterator, uint32_t field)
    {
        return (uint32_t*)DSFieldGetPointerMutInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline const uint8_t* DSRowIterGetBoolean(const DSRowIterator* iterator, uint32_t field)
    {
        return (const uint8_t*)DSFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline uint8_t* DSRowIterGetBooleanMut(const DSRowIterator* iterator, uint32_t field)
    {
        return (uint8_t*)DSFieldGetPointerMutInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline const DSVector3* DSRowIterGetVector3(const DSRowIterator* iterator, uint32_t field)
    {
        return (const DSVector3*)DSFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline DSVector3* DSRowIterGetVector3Mut(const DSRowIterator* iterator, uint32_t field)
    {
        return (DSVector3*)DSFieldGetPointerMutInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline const DSVector4* DSRowIterGetVector4(const DSRowIterator* iterator, uint32_t field)
    {
        return (const DSVector4*)DSFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline DSVector4* DSRowIterGetVector4Mut(const DSRowIterator* iterator, uint32_t field)
    {
        return (DSVector4*)DSFieldGetPointerMutInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline const DSMatrix4* DSRowIterGetMatrix4(const DSRowIterator* iterator, uint32_t field)
    {
        return (const DSMatrix4*)DSFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline DSMatrix4* DSRowIterGetMatrix4Mut(const DSRowIterator* iterator, uint32_t field)
    {
        return (DSMatrix4*)DSFieldGetPointerMutInternal(iterator->m_Parent, iterator->m_Index, field);
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
 * @name DSIter
 * @language C
 */

/*# Query iterator
 *
 * Stack cursor; iterators and fixed-size field access allocate nothing.
 * Keep the query/store alive and its query reservation active.
 * Do not copy active iterators. Parents must stay at the same address; stepping
 * one invalidates its children, including when exhausted. Lifetimes are not checked.
 * Batch and row order are unspecified.
 *
 * @struct
 * @name DSIterator
 */

/*# Row iterator
 *
 * Traverses the current batch. [ref:DSIterator] lifetime rules apply.
 * Fixed-size writes preserve sibling fields and defaults. Reset outside reservations.
 *
 * @struct
 * @name DSRowIterator
 */

/*# Iterate a reserved row range
 *
 * Selects [first, first + count) within reserved rows, possibly across tables.
 * Each job owns its cursor; ranges must be in bounds and disjoint when writing.
 * Keep the reservation active until all iterators and borrowed pointers finish.
 *
 * @name DSQueryIterRange
 * @param query [type:HDSQuery] Query reserved for these jobs.
 * @param first [type:uint32_t] Index in the reserved matching rows.
 * @param count [type:uint32_t] Row count; zero is allowed.
 * @return iterator [type:DSIterator] Cursor to advance with DSIterNext.
 */

/*# Start query iteration
 *
 * Traverses all rows in an active query reservation. The scheduler keeps access
 * reserved until every iterator and job finishes, including early exits.
 *
 * @name DSQueryIter
 * @param query [type:HDSQuery] Query reserved for traversal.
 * @return iterator [type:DSIterator] Iterator with no current batch; advance using DSIterNext.
 */

/*# Advance to the next batch
 *
 * Selects a nonempty contiguous batch. Invalidates child iterators, including when exhausted.
 *
 * @name DSIterNext
 * @param iterator [type:DSIterator*] Iterator to advance.
 * @return has_batch [type:bool] True for a new batch, or false when exhausted.
 */

/*# Get the current table type
 *
 * @name DSIterGetType
 * @param iterator [type:const DSIterator*] Batch iterator.
 * @return type [type:uint64_t] Table type hash, or zero without a valid current batch. Zero may also be a valid type hash.
 */

/*# Start row iteration
 *
 * Borrows the current batch; empty when no batch is selected.
 *
 * @name DSIterRows
 * @param iterator [type:const DSIterator*] Parent query iterator to borrow.
 * @return rows [type:DSRowIterator] Iterator with no current row; advance using DSRowIterNext.
 */

/*# Advance to the next row
 *
 * Invalidates borrowed field pointers; clears the current row when exhausted.
 *
 * @name DSRowIterNext
 * @param iterator [type:DSRowIterator*] Row iterator.
 * @return has_row [type:bool] True for a new row, or false when exhausted.
 */

/*# Get the current row ID
 *
 * @name DSRowIterGetId
 * @param iterator [type:const DSRowIterator*] Row iterator.
 * @return id [type:DSId] Store-local row ID, or zero without a valid current row.
 */

/*# Get the current row group
 *
 * @name DSRowIterGetGroupId
 * @param iterator [type:const DSRowIterator*] Row iterator.
 * @return group [type:DSGroupId] Current group, or zero without a valid row. Zero may also be a valid group.
 */

/*# Borrow the native row bytes
 *
 * Requires a nonzero row stride. Cast only to the registered C layout,
 * including its member offsets and size.
 * Use DSIterGetType to distinguish layouts in queries matching multiple types.
 * Returns zero unless the query flags include DS_ROW_MUTABLE.
 * Otherwise reads any field; permissions are cached in the iterator.
 * Reference slots are internal handles, not native pointers; use container accessors for them.
 * The pointer follows DSIterator lifetime rules. No layout checks occur.
 *
 * @name DSRowIterGetData
 * @param iterator [type:const DSRowIterator*] Iterator on the current row.
 * @return row [type:const void*] Borrowed read-only row start, or zero without whole-row access.
 */

/*# Borrow writable native row bytes
 *
 * Uses the layout and lifetime rules of DSRowIterGetData. Returns zero unless
 * the query flags include DS_ROW_MUTABLE, which reserves reads and writes of the entire row.
 * Never overwrite reference slots or whole rows containing references.
 * Boolean writes must be zero or one. The access flag is cached in the iterator;
 * this getter does not access the query or store.
 *
 * @name DSRowIterGetDataMut
 * @param iterator [type:const DSRowIterator*] Iterator on the current row.
 * @return row [type:void*] Borrowed row start for fixed-size field writes, or zero without DS_ROW_MUTABLE.
 */

/*# Borrow a read-only Number field
 *
 * Uses the handle and pointer lifetime rules of [ref:DSQueryFindField].
 *
 * @name DSRowIterGetNumber
 * @param iterator [type:const DSRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Number handle from DSQueryFindField.
 * @return value [type:const double*] Borrowed read-only value.
 */

/*# Borrow a writable Number field
 *
 * Uses the handle and pointer lifetime rules of [ref:DSQueryFindField].
 *
 * @name DSRowIterGetNumberMut
 * @param iterator [type:const DSRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Number handle from DSQueryFindField.
 * @return value [type:double*] Borrowed writable value.
 */

/*# Borrow a read-only Boolean field
 *
 * Uses the handle and pointer lifetime rules of [ref:DSQueryFindField].
 *
 * @name DSRowIterGetBoolean
 * @param iterator [type:const DSRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Boolean handle from DSQueryFindField.
 * @return value [type:const uint8_t*] Borrowed read-only value.
 */

/*# Borrow a writable Boolean field
 *
 * Uses the handle and pointer lifetime rules of [ref:DSQueryFindField]. Write only zero or one.
 *
 * @name DSRowIterGetBooleanMut
 * @param iterator [type:const DSRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Boolean handle from DSQueryFindField.
 * @return value [type:uint8_t*] Borrowed writable value.
 */

/*# Borrow a read-only Vector3 field
 *
 * Uses the handle and pointer lifetime rules of [ref:DSQueryFindField].
 *
 * @name DSRowIterGetVector3
 * @param iterator [type:const DSRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Vector3 handle from DSQueryFindField.
 * @return value [type:const DSVector3*] Borrowed read-only value.
 */

/*# Borrow a writable Vector3 field
 *
 * Uses the handle and pointer lifetime rules of [ref:DSQueryFindField].
 *
 * @name DSRowIterGetVector3Mut
 * @param iterator [type:const DSRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Vector3 handle from DSQueryFindField.
 * @return value [type:DSVector3*] Borrowed writable value.
 */

/*# Borrow a read-only Vector4 field
 *
 * Uses the handle and pointer lifetime rules of [ref:DSQueryFindField].
 *
 * @name DSRowIterGetVector4
 * @param iterator [type:const DSRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Vector4 handle from DSQueryFindField.
 * @return value [type:const DSVector4*] Borrowed read-only value.
 */

/*# Borrow a writable Vector4 field
 *
 * Uses the handle and pointer lifetime rules of [ref:DSQueryFindField].
 *
 * @name DSRowIterGetVector4Mut
 * @param iterator [type:const DSRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Vector4 handle from DSQueryFindField.
 * @return value [type:DSVector4*] Borrowed writable value.
 */

/*# Borrow a read-only Matrix4 field
 *
 * Uses the handle and pointer lifetime rules of [ref:DSQueryFindField].
 *
 * @name DSRowIterGetMatrix4
 * @param iterator [type:const DSRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Matrix4 handle from DSQueryFindField.
 * @return value [type:const DSMatrix4*] Borrowed read-only value.
 */

/*# Borrow a writable Matrix4 field
 *
 * Uses the handle and pointer lifetime rules of [ref:DSQueryFindField].
 *
 * @name DSRowIterGetMatrix4Mut
 * @param iterator [type:const DSRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Matrix4 handle from DSQueryFindField.
 * @return value [type:DSMatrix4*] Borrowed writable value.
 */

/*# Borrow a read-only Float field
 *
 * Uses the handle and pointer lifetime rules of [ref:DSQueryFindField].
 *
 * @name DSRowIterGetFloat
 * @param iterator [type:const DSRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Float handle from DSQueryFindField.
 * @return value [type:const float*] Borrowed read-only value.
 */

/*# Borrow a read-only UInt32 field
 *
 * Uses the handle and pointer lifetime rules of [ref:DSQueryFindField].
 *
 * @name DSRowIterGetUInt32
 * @param iterator [type:const DSRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] UInt32 handle from DSQueryFindField.
 * @return value [type:const uint32_t*] Borrowed read-only value.
 */

/*# Borrow a writable Float field
 *
 * Uses the handle and pointer lifetime rules of [ref:DSQueryFindField].
 *
 * @name DSRowIterGetFloatMut
 * @param iterator [type:const DSRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] Float handle from DSQueryFindField.
 * @return value [type:float*] Borrowed writable value.
 */

/*# Borrow a writable UInt32 field
 *
 * Uses the handle and pointer lifetime rules of [ref:DSQueryFindField].
 *
 * @name DSRowIterGetUInt32Mut
 * @param iterator [type:const DSRowIterator*] Iterator on the current row.
 * @param field [type:uint32_t] UInt32 handle from DSQueryFindField.
 * @return value [type:uint32_t*] Borrowed writable value.
 */

#endif // DMSDK_DATASTORE_ITER_H
