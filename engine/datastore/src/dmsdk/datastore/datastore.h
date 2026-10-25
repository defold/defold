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

#ifndef DMSDK_DATASTORE_H
#define DMSDK_DATASTORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/jobsystem.h>

#define DATASTORE_CONTEXT_NAME "datastore"
#define DS_QUERY_MAX_ITEMS 32
#define DS_SYSTEM_MAX_DEPENDENCIES 16
#define DS_EVENT_GAMEOBJECT_PRE_UPDATE dmHashString64("engine.gameobject.pre_update")
#define DS_EVENT_GAMEOBJECT_POST_UPDATE dmHashString64("engine.gameobject.post_update")

#ifdef __cplusplus
extern "C"
{
#endif

    // Shared types

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

    // Store lifetime

    typedef struct DSCreateParams
    {
        uint32_t m_PageSize;
        uint32_t m_RowChunkSize;
        uint32_t m_MaxSparePages;
    } DSCreateParams;

    HDSContext DSCreateStore(const DSCreateParams* params);
    DSResult   DSDestroyStore(HDSContext store);
    DSResult   DSRemoveGroup(HDSContext store, DSGroupId group);

    // Native tables and rows

    typedef struct DSStructDesc DSStructDesc;
    typedef struct DSFieldDesc
    {
        uint64_t            m_Field;  // Field name hash.
        DSValueType         m_Type;   // Declared kind; fixed for the table lifetime.
        uint32_t            m_Offset; // Byte offset within the containing row/struct.
        const DSStructDesc* m_Struct; // Reusable inline layout; NULL for scalar/dynamic values.
        const char*         m_Name;   // Borrowed local name for inline composition; optional for other root fields.
    } DSFieldDesc;

    struct DSStructDesc
    {
        const DSFieldDesc* m_Fields;     // Borrowed member descriptions; copied at registration.
        uint32_t           m_FieldCount; // Number of immediate members.
        uint32_t           m_Size;       // Struct size in bytes, including trailing padding.
    };

    typedef enum DSResetPolicy
    {
        DS_RESET_NONE = 0,
        DS_RESET_RETAIN = 1,
    } DSResetPolicy;

    typedef struct DSTableDesc
    {
        uint64_t           m_Type;        // Data type name hash.
        const uint64_t*    m_Tags;        // Borrowed tag hashes; copied at registration.
        uint32_t           m_TagCount;    // Number of tags.
        const DSFieldDesc* m_Fields;      // Borrowed root field descriptions; copied at registration.
        uint32_t           m_FieldCount;  // Number of root fields.
        uint32_t           m_RowStride;   // Bytes per row, including trailing alignment padding.
        DSResetPolicy      m_ResetPolicy; // Current values only by default; RETAIN keeps initial values for reset.
    } DSTableDesc;

    typedef struct DSFieldArray
    {
        uint64_t    m_Field;  // Root field name hash.
        const void* m_Values; // Borrowed contiguous array of native field values.
    } DSFieldArray;

    typedef struct DSStructInput
    {
        const DSStructDesc* m_Layout; // Native member layout, borrowed through creation.
        const void*         m_Values; // One native struct matching m_Layout.
    } DSStructInput;

    typedef struct DSListInput
    {
        DSValueType        m_Type;   // Shared element kind when m_Types is NULL.
        uint32_t           m_Count;  // Number of elements.
        const void*        m_Values; // Native array, or array of payload pointers when m_Types is set.
        const DSValueType* m_Types;  // Optional per-element kinds; selects mixed input.
    } DSListInput;

    typedef union DSReference
    {
        const char*          m_String; // NUL-terminated string input.
        const DSStructInput* m_Struct; // Dynamic struct input.
        const DSListInput*   m_List;   // Dynamic list input.
        // private
        uint64_t m_Storage; // Keeps reference slots eight bytes on 32-bit and 64-bit hosts.
    } DSReference;

    DSResult DSRegisterTable(HDSContext store, const DSTableDesc* desc);
    DSResult DSRegisterTableView(HDSContext store, const DSTableDesc* desc, void* values, uint32_t value_count, uint32_t row_count, const uint32_t* indices, DSId* out_ids);
    DSResult DSUnregisterTable(HDSContext store, uint64_t type);
    DSResult DSCreateRows(HDSContext store, uint64_t type, DSGroupId group, uint32_t count, const void* rows, DSId* out_ids);
    DSResult DSCreateRowsSoA(HDSContext store, uint64_t type, DSGroupId group, uint32_t count, uint32_t field_count, const DSFieldArray* fields, DSId* out_ids);
    DSResult DSRemoveRow(HDSContext store, DSId id);
    DSResult DSResetRow(HDSContext store, DSId id);
    uint64_t DSGetComponentId(HDSContext store, DSId id);

    // Resource instances

    typedef struct DSBlob*         HDSBlob;
    typedef struct DSBlobInstance* HDSBlobInstance;

    DSResult                       DSLoadBlob(const void* buffer, uint32_t buffer_size, HDSBlob* out_blob);
    void                           DSDestroyBlob(HDSBlob blob);
    DSResult                       DSAddBlob(HDSContext store, HDSBlob blob, DSGroupId group, HDSBlobInstance* out_instance);
    DSResult                       DSRemoveBlob(HDSBlobInstance instance);
    DSResult                       DSResetBlob(HDSBlobInstance instance);

    // Queries

    typedef enum DSAccess
    {
        DS_FIELD_MUTABLE = 1,
    } DSAccess;

    typedef enum DSQueryFlags
    {
        DS_ROW_MUTABLE = 2,
    } DSQueryFlags;

    typedef enum DSQueryItemKind
    {
        DS_QUERY_ITEM_NONE = 0,
        DS_QUERY_ITEM_FIELD = 1,
        DS_QUERY_ITEM_TAG = 2,
    } DSQueryItemKind;

    typedef struct DSQueryItem
    {
        uint64_t        m_Hash;
        DSValueType     m_Type;
        uint32_t        m_Access;
        DSQueryItemKind m_Kind;
    } DSQueryItem;

    typedef struct DSQueryDesc
    {
        const DSGroupId* m_GroupIds;
        uint32_t         m_GroupIdCount;
        uint32_t         m_Flags;
        DSQueryItem      m_Items[DS_QUERY_MAX_ITEMS];
    } DSQueryDesc;

    static inline DSQueryItem DSTag(const char* name);
    static inline DSQueryItem DSField(const char* name, DSValueType type, uint32_t access);

    DSResult                  DSCreateQuery(HDSContext store, const DSQueryDesc* desc, HDSQuery* out_query);
    DSResult                  DSDestroyQuery(HDSQuery query);

    DSResult                  DSQueryTryBegin(HDSQuery query);
    uint32_t                  DSQueryGetRowCount(HDSQuery query);
    void                      DSQueryEnd(HDSQuery query);

    uint32_t                  DSQueryFindField(HDSQuery query, const DSQueryItem* field);

    // Scheduled callbacks

    typedef void (*DSSystemProcess)(HDSQuery query, uint32_t first, uint32_t num_rows, void* context);

    typedef struct DSSystemDesc
    {
        dmhash_t        m_Name;
        dmhash_t        m_DependsOn[DS_SYSTEM_MAX_DEPENDENCIES];
        HDSQuery        m_Query;
        DSSystemProcess m_Process;
        void*           m_Context;
        uint32_t        m_RowsPerJob;
    } DSSystemDesc;

    typedef struct DSEventDesc
    {
        dmhash_t m_Name;
        dmhash_t m_DependsOn[DS_SYSTEM_MAX_DEPENDENCIES];
    } DSEventDesc;

    DSResult DSRegisterFunction(HDSContext store, const DSSystemDesc* desc);
    DSResult DSUnregisterFunction(HDSContext store, dmhash_t name);
    DSResult DSRegisterEvent(HDSContext store, const DSEventDesc* desc);
    DSResult DSUnregisterEvent(HDSContext store, dmhash_t name);
    DSResult DSBeginSchedule(HDSContext store, HJobContext jobs);
    DSResult DSSchedulerEvent(HDSContext store, dmhash_t name);
    DSResult DSSchedulerUpdate(HDSContext store);
    DSResult DSSchedulerSync(HDSContext store);
    DSResult DSEndSchedule(HDSContext store);

    // Iteration

    typedef struct DSIterator
    {
        // private
        HDSQuery        m_Query;
        uint32_t        m_NextRow;
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
        uint8_t         m_Flags;
    } DSIterator;

    typedef struct DSRowIterator
    {
        // private
        const DSIterator* m_Parent;
        uint32_t          m_Index;
        uint32_t          m_NextRow;
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

    // Field values

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

    // Dynamic containers

    typedef struct DSValueView
    {
        // private
        uint64_t m_Storage[9];
    } DSValueView;

    DSResult    DSFieldGetView(HDSContext store, DSId id, uint64_t field, DSValueView* out_view);
    void        DSRowIterGetView(const DSRowIterator* iterator, uint32_t field, DSValueView* out_view);
    DSValueType DSViewGetType(const DSValueView* view);
    DSResult    DSViewGetCount(const DSValueView* view, uint32_t* out_count);
    DSResult    DSViewGetField(const DSValueView* view, uint64_t field, DSValueView* out_view);
    DSResult    DSViewGetElement(const DSValueView* view, uint32_t index, DSValueView* out_view);
    DSResult    DSViewGetNumber(const DSValueView* view, double* out_value);
    DSResult    DSViewGetFloat(const DSValueView* view, float* out_value);
    DSResult    DSViewGetUInt32(const DSValueView* view, uint32_t* out_value);
    DSResult    DSViewGetBoolean(const DSValueView* view, uint8_t* out_value);
    DSResult    DSViewGetString(const DSValueView* view, const char** out_value);
    DSResult    DSViewGetVector3(const DSValueView* view, DSVector3* out_value);
    DSResult    DSViewGetVector4(const DSValueView* view, DSVector4* out_value);
    DSResult    DSViewGetMatrix4(const DSValueView* view, DSMatrix4* out_value);

    // Inline implementations

    static inline DSQueryItem DSTag(const char* name)
    {
        DSQueryItem tag = { .m_Hash = dmHashString64(name), .m_Kind = DS_QUERY_ITEM_TAG };
        return tag;
    }

    static inline DSQueryItem DSField(const char* name, DSValueType type, uint32_t access)
    {
        DSQueryItem field = { .m_Hash = dmHashString64(name), .m_Type = type, .m_Access = access, .m_Kind = DS_QUERY_ITEM_FIELD };
        return field;
    }

    // private
    // Address calculation for the public pointer wrappers. Validated field handles and current
    // row indices are caller obligations; no repeated checks occur here.
    static inline void*         DSFieldGetPointerInternal(const DSIterator* batch, uint32_t row, uint32_t field);

    static inline DSRowIterator DSIterRows(const DSIterator* iterator)
    {
        DSRowIterator rows = { .m_Parent = iterator, .m_Index = UINT32_MAX };
        return rows;
    }

    static inline bool DSRowIterNext(DSRowIterator* iterator)
    {
        if (iterator->m_NextRow >= iterator->m_Parent->m_Count)
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

    static inline void* DSFieldGetPointerInternal(const DSIterator* batch, uint32_t row, uint32_t field)
    {
        return batch->m_Values + (size_t)row * batch->m_RowStride + batch->m_FieldOffsets[field];
    }

    static inline const double* DSRowIterGetNumber(const DSRowIterator* iterator, uint32_t field)
    {
        return (const double*)DSFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline double* DSRowIterGetNumberMut(const DSRowIterator* iterator, uint32_t field)
    {
        return (double*)DSFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline const float* DSRowIterGetFloat(const DSRowIterator* iterator, uint32_t field)
    {
        return (const float*)DSFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline float* DSRowIterGetFloatMut(const DSRowIterator* iterator, uint32_t field)
    {
        return (float*)DSFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline const uint32_t* DSRowIterGetUInt32(const DSRowIterator* iterator, uint32_t field)
    {
        return (const uint32_t*)DSFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline uint32_t* DSRowIterGetUInt32Mut(const DSRowIterator* iterator, uint32_t field)
    {
        return (uint32_t*)DSFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline const uint8_t* DSRowIterGetBoolean(const DSRowIterator* iterator, uint32_t field)
    {
        return (const uint8_t*)DSFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline uint8_t* DSRowIterGetBooleanMut(const DSRowIterator* iterator, uint32_t field)
    {
        return (uint8_t*)DSFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline const DSVector3* DSRowIterGetVector3(const DSRowIterator* iterator, uint32_t field)
    {
        return (const DSVector3*)DSFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline DSVector3* DSRowIterGetVector3Mut(const DSRowIterator* iterator, uint32_t field)
    {
        return (DSVector3*)DSFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline const DSVector4* DSRowIterGetVector4(const DSRowIterator* iterator, uint32_t field)
    {
        return (const DSVector4*)DSFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline DSVector4* DSRowIterGetVector4Mut(const DSRowIterator* iterator, uint32_t field)
    {
        return (DSVector4*)DSFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline const DSMatrix4* DSRowIterGetMatrix4(const DSRowIterator* iterator, uint32_t field)
    {
        return (const DSMatrix4*)DSFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    static inline DSMatrix4* DSRowIterGetMatrix4Mut(const DSRowIterator* iterator, uint32_t field)
    {
        return (DSMatrix4*)DSFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

#ifdef __cplusplus
}
#endif

// API documentation

/*# Data API
 *
 * Typed data storage, queries, field access and scheduled callbacks.
 *
 * @document
 * @name DS
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

/*# Engine datastore context
 *
 * Registry name for the engine-owned [type:HDSContext], available from extension
 * app initialization through app finalization. Borrow the store; do not destroy
 * it. Finish jobs and release your queries and tables before finalization ends.
 * Setting engine.datastore to 0 disables creation; context lookup returns NULL.
 * The default is 1 (enabled).
 *
 * @constant
 * @name DATASTORE_CONTEXT_NAME
 */

/*# Store allocation parameters
 *
 * Zero selects the default for each size. Pages are allocated lazily and shared
 * by table value chunks. These are allocator sizes, independent of OS pages.
 *
 * @struct
 * @name DSCreateParams
 * @member m_PageSize [type:uint32_t] Backing allocation bytes; default 524288. Power of two, at least m_RowChunkSize.
 * @member m_RowChunkSize [type:uint32_t] Target current/default value-chunk bytes; default 262144. Power of two, at least 64. Larger rows use dedicated allocations.
 * @member m_MaxSparePages [type:uint32_t] Empty backing pages retained for reuse. Zero releases all; a null params pointer uses 16.
 */

/*# Create an empty store
 *
 * The caller owns the store and releases it with [ref:DSDestroyStore].
 *
 * @name DSCreateStore
 * @param params [type:const DSCreateParams*] Allocation settings; zero uses defaults. Copied during creation.
 * @return store [type:HDSContext] New store, or zero for invalid allocation sizes.
 */

/*# Destroy a store
 *
 * Frees the store, tables/rows and callback/event registrations. Finish callbacks and all
 * access, end any schedule, then destroy queries. Borrowed blobs and callback contexts
 * remain caller-owned; destruction does not invoke callbacks.
 *
 * @name DSDestroyStore
 * @param store [type:HDSContext] Store owned by the caller.
 * @return result [type:DSResult] OK, or LOCKED during a schedule or query reservation; leaves the store intact on failure.
 */

/*# Remove a group and its rows
 *
 * Removes the group's rows across all tables, owned payloads and blob registrations,
 * including empty registrations. Invalidates their row IDs and blob-instance
 * handles and releases group bookkeeping. Other groups and their row IDs remain
 * valid. Independent table registrations remain available for reuse. Does not scan unrelated rows.
 * Requires a store without active query reservations; queue requests in caller-owned
 * storage and apply them after jobs finish. Zero is not a removable group.
 *
 * Bookkeeping is created on first use and kept until group removal, including
 * when rows are removed individually. Reusing the caller ID later recreates the
 * group; existing query filters then match its new rows.
 *
 * @name DSRemoveGroup
 * @param store [type:HDSContext] Store owning the group.
 * @param group [type:DSGroupId] Caller-provided owner ID.
 * @return result [type:DSResult] OK, LOCKED during query reservations, or NOT_FOUND for zero or an unknown group. Failure leaves the store unchanged.
 */

/*# Register borrowed native rows
 *
 * Creates a runtime table over caller-owned AoS values without copying them.
 * Layouts contain scalar/vector/matrix fields; struct and reference fields are unsupported.
 * Indices select source rows and are copied;
 * NULL selects rows in order. Optional out_ids receives row identities.
 * Keep values alive, aligned and stationary until DSUnregisterTable or store destruction.
 * The caller synchronizes external access with query reservations. Shared reads are
 * allowed; writable jobs must select distinct source rows, including within one query.
 * Views with overlapping source rows conflict on overlapping writable fields.
 * View rows cannot be created, removed or reset individually, or serialized.
 * Unregistering frees metadata and identities, never the borrowed values.
 *
 * @name DSRegisterTableView
 * @param store [type:HDSContext] Destination store.
 * @param desc [type:const DSTableDesc*] Native layout and tags; copied.
 * @param values [type:void*] Borrowed native row array.
 * @param value_count [type:uint32_t] Accessible source rows.
 * @param row_count [type:uint32_t] Selected rows.
 * @param indices [type:const uint32_t*] Optional source indices, each below value_count.
 * @param out_ids [type:DSId*] Optional row_count output identities.
 * @return result [type:DSResult] OK, LOCKED, ALREADY_EXISTS or INVALID_ARGUMENT.
 */

/*# Native reset history
 *
 * Controls whether native row creation retains initial values. Blob instances
 * always use their immutable resource as reset defaults. Borrowed views retain none.
 *
 * @enum
 * @name DSResetPolicy
 * @member DS_RESET_NONE Store current values only; the default. Reset returns INVALID_ARGUMENT.
 * @member DS_RESET_RETAIN Keep an extra copy of initial values for row/field reset.
 */



/*# Field layout
 *
 * Offsets follow C alignment. Inline members and their parents require single-segment
 * names for full-name hashing. Registration copies the compiled metadata.
 *
 * @struct
 * @name DSFieldDesc
 * @member m_Field [type:uint64_t] Local field name hash.
 * @member m_Type [type:DSValueType] Fixed field kind.
 * @member m_Offset [type:uint32_t] Byte offset in the containing row or struct.
 * @member m_Struct [type:const DSStructDesc*] Inline STRUCT layout, otherwise NULL.
 * @member m_Name [type:const char*] Borrowed local name; required for inline parents/members, without dots.
 */

/*# Inline struct layout
 *
 * Reusable metadata, copied by registration. Members must not overlap. Layouts must
 * be acyclic. Inline structs and dynamic containers share a 64-level nesting limit.
 *
 * @struct
 * @name DSStructDesc
 * @member m_Fields [type:const DSFieldDesc*] Borrowed member descriptions.
 * @member m_FieldCount [type:uint32_t] Number of members.
 * @member m_Size [type:uint32_t] Native size including alignment padding.
 */

/*# Table layout
 *
 * Tags and metadata are copied. Tags and total metadata entries, including nested
 * members, are each limited to 65,535. Full-name hashes must be unique across the
 * entire layout, including inline members. Rows share this fixed layout.
 *
 * @struct
 * @name DSTableDesc
 * @member m_Type [type:uint64_t] Type name hash, unique among independently registered tables.
 * @member m_Tags [type:const uint64_t*] Borrowed tag hashes.
 * @member m_TagCount [type:uint32_t] Number of tags.
 * @member m_Fields [type:const DSFieldDesc*] Borrowed root field descriptions.
 * @member m_FieldCount [type:uint32_t] Number of root fields.
 * @member m_RowStride [type:uint32_t] Native row size including trailing alignment padding.
 * @member m_ResetPolicy [type:DSResetPolicy] NONE by default; RETAIN keeps an extra copy of initial native values.
 */

/*# Native reference input
 *
 * Use for STRING, dynamic STRUCT and LIST fields in native rows or field arrays,
 * including nested members. Select the union member matching the declared kind.
 * Payloads and descriptors are borrowed through creation and copied into the
 * table. References must be non-NULL. This is an input descriptor, not a stored
 * pointer view; read strings through the typed API. Reference slots occupy eight
 * bytes on all supported hosts and follow the registered C alignment rules.
 *
 * @struct
 * @name DSReference
 * @member m_String [type:const char*] Borrowed NUL-terminated string.
 * @member m_Struct [type:const DSStructInput*] Borrowed dynamic struct.
 * @member m_List [type:const DSListInput*] Borrowed dynamic list.
 */

/*# Dynamic struct input
 *
 * Describes one native struct, with per-input member names and kinds. The table
 * field uses STRUCT without an inline layout. Members may contain DSReference
 * values, including further containers. Inputs must be acyclic, no deeper than
 * 64 container levels including enclosing inline structs; names within each
 * struct must be unique.
 *
 * @struct
 * @name DSStructInput
 * @member m_Layout [type:const DSStructDesc*] Borrowed native member layout; required even for an empty struct.
 * @member m_Values [type:const void*] Borrowed native struct bytes; NULL only when layout size is zero.
 */

/*# Dynamic list input
 *
 * With m_Types NULL, m_Type describes a contiguous native array. Numeric elements
 * use their native API types; STRING, STRUCT and LIST elements use DSReference.
 * With m_Types set, m_Values is a const void* array of payload pointers: addresses
 * of native scalar/vector/matrix values, string bytes, DSStructInput or
 * DSListInput. NULL-kind elements need no payload. m_Type is ignored.
 * Both arrays contain m_Count entries. Inputs are borrowed through creation;
 * values are copied, including nested payloads. Input rules match DSStructInput.
 *
 * @struct
 * @name DSListInput
 * @member m_Type [type:DSValueType] Shared element kind when m_Types is NULL; otherwise ignored.
 * @member m_Count [type:uint32_t] Number of elements.
 * @member m_Values [type:const void*] Native array or array of payload pointers. NULL only for an empty list or a homogeneous NULL list.
 * @member m_Types [type:const DSValueType*] Optional array of per-element kinds; NULL selects homogeneous input.
 */

/*# Create rows from native input
 *
 * Copies count complete rows using the registered C layout and row size. Assigns
 * group to every row; groups can share a table. DSRemoveGroup removes owned rows.
 * Stores current values only unless m_ResetPolicy is DS_RESET_RETAIN.
 * Native rows have no component name; DSGetComponentId returns zero.
 * Supports fixed-size values, inline structs and DSReference fields. Strings
 * and dynamic containers are copied into table-owned storage, including nested
 * payloads; all input memory can be released after the call. Dynamic struct fields
 * share layout metadata and own separate child rows. Removal releases those rows
 * for reuse; reset releases replacements and restores the original children.
 * Removed rows' default slots and string/list payload chunks are reused. Empty
 * payload blocks are released, keeping at most one small spare per arena.
 * Booleans must be 0 or 1. Inputs, including padding, must be initialized, disjoint
 * from store storage and valid through the call. On error no rows are published
 * and output IDs remain unchanged. Nonempty batches require a store without active
 * query reservations. An empty batch returns OK without accessing the store or arrays.
 *
 * @name DSCreateRows
 * @param store [type:HDSContext] Destination store.
 * @param type [type:uint64_t] Registered table type.
 * @param group [type:DSGroupId] Caller-provided owner ID; bookkeeping is created lazily. Zero creates ungrouped rows.
 * @param count [type:uint32_t] Rows to add; zero adds nothing.
 * @param rows [type:const void*] Borrowed contiguous native rows; NULL for zero-size rows or an empty batch.
 * @param out_ids [type:DSId*] Optional array receiving count stable IDs on success; NULL skips the output. IDs remain available through row iteration.
 * @return result [type:DSResult] OK, LOCKED, NOT_FOUND for an unknown type, ALREADY_EXISTS for duplicate dynamic member names, or INVALID_ARGUMENT for incompatible input or capacity limits.
 */

/*# Native field array
 *
 * One input column for DSCreateRowsSoA. The registered layout determines the
 * element type and size; an inline struct uses complete native struct elements.
 * STRING, dynamic STRUCT and LIST columns use DSReference elements.
 *
 * @struct
 * @name DSFieldArray
 * @member m_Field [type:uint64_t] Root field name hash.
 * @member m_Values [type:const void*] Borrowed contiguous native values; NULL only for a zero-size field or an empty batch.
 */

/*# Create rows from field arrays
 *
 * Supply every root field exactly once, in any order, with count values per array.
 * Copies into the selected runtime storage and retains native reset rows only
 * when m_ResetPolicy is DS_RESET_RETAIN.
 * Types and element sizes come from the registered layout. Inline struct fields
 * use arrays of complete structs, not separate arrays for their nested members.
 * Inputs, including struct padding, must be initialized, disjoint from store
 * storage and valid through the call. The table policy controls reset history. Group,
 * locking and reference ownership rules match DSCreateRows. On error no rows are
 * published and output IDs remain unchanged.
 *
 * @name DSCreateRowsSoA
 * @param store [type:HDSContext] Destination store.
 * @param type [type:uint64_t] Registered table type.
 * @param group [type:DSGroupId] Caller-provided owner ID; bookkeeping is created lazily. Zero creates ungrouped rows.
 * @param count [type:uint32_t] Rows to add; zero returns OK without accessing the store or arrays.
 * @param field_count [type:uint32_t] Number of root field arrays.
 * @param fields [type:const DSFieldArray*] Borrowed field arrays; NULL for a table without fields or an empty batch.
 * @param out_ids [type:DSId*] Optional array receiving count stable IDs on success; NULL skips the output. IDs remain available through row iteration.
 * @return result [type:DSResult] OK, LOCKED, NOT_FOUND for an unknown table, ALREADY_EXISTS for duplicate dynamic member names, or INVALID_ARGUMENT for invalid fields, input or capacity limits.
 */

/*# Register an empty table
 *
 * Copies tags and field metadata. Fields have fixed kinds and non-overlapping byte offsets.
 * Duplicate type hashes and full-name field hashes (including inline members) are rejected. Successful
 * registration updates live queries. Requires a store without active query reservations.
 *
 * @name DSRegisterTable
 * @param store [type:HDSContext] Store handle.
 * @param desc [type:const DSTableDesc*] Table type, tags and shared field layout to copy.
 * @return result [type:DSResult] LOCKED while query reservations are active; otherwise OK, ALREADY_EXISTS for a duplicate type/field name, or INVALID_ARGUMENT for invalid tags/count/layout.
 */

/*# Unregister a table
 *
 * Removes an independently registered table and all its rows. Their IDs become stale, even if the type is
 * registered again. Updates live queries. Requires a store without active query reservations.
 *
 * @name DSUnregisterTable
 * @param store [type:HDSContext] Store handle.
 * @param type [type:uint64_t] Table type hash.
 * @return result [type:DSResult] LOCKED while query reservations are active; otherwise OK on success, or NOT_FOUND if the type is not registered.
 */

/*# Remove a row
 *
 * Uses swap removal; other live IDs remain valid. The removed ID becomes stale.
 * Requires a store without active query reservations. To remove during traversal, collect DSIds in a
 * caller-owned reusable buffer and apply removals after all query reservations finish.
 * Reclaims the retained default slot and owned dynamic child rows. String/list
 * payload chunks are recycled; empty blocks are freed, retaining one small spare
 * per arena. Borrowed blob payloads remain unchanged.
 *
 * @name DSRemoveRow
 * @param store [type:HDSContext] Store handle.
 * @param id [type:DSId] ID of the row to remove.
 * @return result [type:DSResult] LOCKED while query reservations are active; otherwise OK on success, NOT_FOUND for an invalid or stale ID, or INVALID_ARGUMENT for a borrowed view.
 */

/*# Reset one component row
 *
 * Restores all fields, including nested values, to their loaded or added defaults.
 * Other rows are unaffected, including other components with the same group or type.
 * Does not allocate, change IDs or query membership, or invalidate iterators.
 * Mutable row storage stays in place. Owned replacement payloads are released for
 * reuse; borrowed views and strings must no longer be used.
 * Returns LOCKED during query reservations.
 *
 * Tables registered with DS_RESET_NONE do not retain history and cannot reset.
 *
 * @name DSResetRow
 * @param store [type:HDSContext] Store containing the component row.
 * @param id [type:DSId] Component's runtime row ID.
 * @return result [type:DSResult] OK, LOCKED during query reservations, NOT_FOUND for an invalid or stale ID, or INVALID_ARGUMENT for a borrowed view or a table with DS_RESET_NONE.
 */

/*# Read a component's prototype-local name hash
 *
 * Used by game object integration to associate a runtime row with its component.
 *
 * @name DSGetComponentId
 * @param store [type:HDSContext] Store containing the row.
 * @param id [type:DSId] Runtime row ID.
 * @return component [type:uint64_t] Component name hash, or zero for an invalid/stale row. Zero may also identify an unnamed row.
 */

/*# Loaded blob handle
 *
 * Owns a reference to validated metadata, borrowing the caller's immutable bytes.
 * Release with DSDestroyBlob. Store-owned references are synchronized across
 * stores; keep a live reference while other threads call DSAddBlob.
 *
 * @typedef
 * @name HDSBlob
 */

/*# Blob instance handle
 *
 * Identifies one grouped registration in a store. Release with DSRemoveBlob or DSRemoveGroup,
 * or by destroying the store. This does not own the caller's blob bytes.
 *
 * @typedef
 * @name HDSBlobInstance
 */

/*# Load an immutable packed resource
 *
 * Validates all tables and borrows the supplied buffer without copying or modifying it.
 * The input must be eight-byte aligned. Member offsets and row/struct sizes are
 * validated for C-compatible alignment. The caller keeps the buffer unchanged and alive
 * until the blob reference and all its registrations have been released.
 * Loading does not register rows in a store.
 *
 * @name DSLoadBlob
 * @param buffer [type:const void*] Caller-owned complete packed component-data blob.
 * @param buffer_size [type:uint32_t] Blob byte size.
 * @param out_blob [type:HDSBlob*] Receives a caller-owned reference on success; unchanged on error.
 * @return result [type:DSResult] OK or INVALID_FORMAT for malformed/unsupported data or an unaligned buffer.
 */

/*# Release a loaded resource reference
 *
 * Resource pools retain a reference while any registrations remain. The last release frees the blob handle,
 * never the caller's buffer. Does not remove any instance's rows. The caller may
 * free the buffer after releasing this reference and removing all registrations.
 *
 * @name DSDestroyBlob
 * @param blob [type:HDSBlob] Reference to release; must not be used afterward.
 */

/*# Add every component table for one resource instance
 *
 * Retains the resource and copies fixed row bytes into shared mutable tables.
 * Registrations of the same loaded resource share metadata, defaults and capacity.
 * Strings and containers retain blob offsets. The caller keeps the immutable
 * buffer alive until all references and registrations are released.
 * Requires a store without active query reservations.
 *
 * @name DSAddBlob
 * @param store [type:HDSContext] Destination store.
 * @param blob [type:HDSBlob] Loaded resource to retain.
 * @param group [type:DSGroupId] Caller-provided owner ID; bookkeeping is created lazily even for an empty blob. Zero leaves the registration ungrouped.
 * @param out_instance [type:HDSBlobInstance*] Receives the registration handle on success; unchanged on error.
 * @return result [type:DSResult] OK, LOCKED while query reservations are active, or INVALID_ARGUMENT if store capacity would be exceeded. Errors leave the store unchanged.
 */

/*# Remove one instance's complete set of component tables
 *
 * Removes its remaining rows, invalidates their IDs, frees its replacement payloads,
 * and returns the registration slot for reuse. Shared capacity remains while other
 * registrations use that resource; the last removal frees the pool and its blob reference.
 * DSRemoveGroup also releases registrations owned by that group. Their handles
 * must not be used afterward. The store releases all registrations when destroyed.
 *
 * @name DSRemoveBlob
 * @param instance [type:HDSBlobInstance] Registration to remove; must not be used after success.
 * @return result [type:DSResult] OK, or LOCKED while its store has active query reservations (no changes).
 */

/*# Reset a game object's grouped component registration
 *
 * Copies loaded values into this registration's remaining mutable rows and frees its replacement payload blocks.
 * Does not restore removed rows or invalidate iterators. Borrowed replacement payloads expire.
 * Other registrations are unaffected, even if they share a blob, type or group ID.
 * The engine retains this handle when registering a game object's component tables;
 * no datastore-wide group search is needed. Independently added rows are not included.
 *
 * @name DSResetBlob
 * @param instance [type:HDSBlobInstance] Registration to reset.
 * @return result [type:DSResult] OK, or LOCKED during query reservations.
 */

/*# Maximum items in a query descriptor
 * @constant
 * @name DS_QUERY_MAX_ITEMS
 */

/*# Query access flags
 *
 * Pass zero for shared read access, the default.
 *
 * @enum
 * @name DSAccess
 * @member DS_FIELD_MUTABLE Exclusive read/write access (1).
 */

/*# Query flags
 *
 * Zero keeps field-level access and disables whole-row pointers.
 * Field reads are the default; individual writes require DS_FIELD_MUTABLE.
 *
 * @enum
 * @name DSQueryFlags
 * @member DS_ROW_MUTABLE Exclusive read/write access to the entire row; reference slots must not be overwritten (2).
 */

/*# Query item kinds
 *
 * @enum
 * @name DSQueryItemKind
 * @member DS_QUERY_ITEM_NONE Unused entry; ends the item list (0).
 * @member DS_QUERY_ITEM_FIELD Required field with an exact value kind (1).
 * @member DS_QUERY_ITEM_TAG Required table tag (2).
 */

/*# Required query tag or field
 *
 * Construct with [ref:DSTag] or [ref:DSField], or initialize the kind and hash directly.
 * Field names may include inline members, such as "light.color"; dynamic members cannot bind.
 *
 * @struct
 * @name DSQueryItem
 * @member m_Hash [type:uint64_t] Tag hash or full field name hash; zero is valid.
 * @member m_Type [type:DSValueType] Exact value kind for field items; ignored for tags.
 * @member m_Access [type:uint32_t] Field access: zero for shared reads, or DS_FIELD_MUTABLE for fixed-size read/write access. Ignored for tags.
 * @member m_Kind [type:DSQueryItemKind] Tag, field, or unused item.
 */

/*# Describe a required table tag
 *
 * Hashes the name immediately; the returned item does not retain the string.
 *
 * @name DSTag
 * @param name [type:const char*] NUL-terminated tag name.
 * @return item [type:DSQueryItem] Tag item for query creation.
 */

/*# Describe a query field by name
 *
 * Hashes the name immediately; the returned descriptor does not retain the string.
 *
 * @name DSField
 * @param name [type:const char*] NUL-terminated full name, such as "light.color".
 * @param type [type:DSValueType] Required value kind.
 * @param access [type:uint32_t] Zero for shared reads, or DS_FIELD_MUTABLE for read/write access.
 * @return field [type:DSQueryItem] Field item for query creation or binding.
 */

/*# Query filter descriptor
 *
 * Requires all tags and fields in one table, and matches any listed group.
 * Empty filters impose no restriction. Bind requested fields with DSQueryFindField.
 * Zero-initialize. Items end at the first DS_QUERY_ITEM_NONE or the array capacity.
 * Filters are copied during creation; group IDs use an explicit count because zero is valid.
 *
 * @struct
 * @name DSQueryDesc
 * @member m_GroupIds [type:const DSGroupId*] Caller-provided owner IDs; zero selects ungrouped rows. IDs may precede row creation. Optional when count is zero.
 * @member m_GroupIdCount [type:uint32_t] Group count; zero matches all.
 * @member m_Flags [type:uint32_t] DSQueryFlags bits; zero keeps field-level access and disables native row pointers.
 * @member m_Items [type:DSQueryItem[DS_QUERY_MAX_ITEMS]] Required tags and fields, in any order.
 */

/*# Create a reusable live query
 *
 * Copies filters and tracks matching tables, including later registrations.
 * Destroy the query before its store.
 *
 * @name DSCreateQuery
 * @param store [type:HDSContext] Store handle.
 * @param desc [type:const DSQueryDesc*] Filters to copy.
 * @param out_query [type:HDSQuery*] Receives the owned query on success.
 * @return result [type:DSResult] OK, INVALID_ARGUMENT for invalid items/access or excessive group count, or LOCKED during query reservations.
 */

/*# Destroy a query
 *
 * Frees the query. Finish using its iterators first.
 *
 * @name DSDestroyQuery
 * @param query [type:HDSQuery] Query whose store is still alive.
 * @return result [type:DSResult] OK, or LOCKED while query reservations are active or this query is registered in an active schedule.
 */

/*# Reserve query access
 *
 * Thread-safe, nonblocking reservation that keeps rows and bindings stable.
 * Overlapping fields on the same table allow read/read access; writes conflict.
 * Disjoint group filters permit concurrent writes to the same table. Aliased external
 * views remain conservative. Without fields or whole-row flags, only row identities are read.
 * DS_ROW_MUTABLE conflicts with any reader or writer of overlapping row values.
 * Field filters still select matching tables.
 * Field writes require DS_FIELD_MUTABLE, or DS_ROW_MUTABLE for native row access;
 * string replacement and resets must happen outside reservations.
 * Each query supports one active reservation, shared by its jobs. The caller schedules
 * and synchronizes jobs, then calls DSQueryEnd once, even for an empty query.
 * The scheduler owns this scope for both serial and parallel iteration.
 * Do not bypass reservations with unrelated ID-based access.
 * Matching-table changes invalidate cached compatibility; row changes refresh ranges.
 * First use of a query pair or changed ranges may allocate; later begins reuse storage.
 *
 * @name DSQueryTryBegin
 * @param query [type:HDSQuery] Query to reserve.
 * @return result [type:DSResult] OK, or BUSY without acquiring access if already active or unavailable.
 */

/*# Count reserved rows
 *
 * @name DSQueryGetRowCount
 * @param query [type:HDSQuery] Query with an active reservation.
 * @return count [type:uint32_t] Reserved matching row count.
 */

/*# Release query access
 *
 * Thread-safe release. The caller must join all jobs and finish using iterators and pointers first.
 *
 * @name DSQueryEnd
 * @param query [type:HDSQuery] Query with a successful begin to release once.
 */

/*# Find a query field for direct pointer access
 *
 * Binds a requested full-name hash, kind and access to a handle valid until query destruction.
 * The handle is neither a byte offset nor a query descriptor index. No lock or matching rows
 * are required. Fixed-size kinds support typed pointers; DSRowIterGetView also reads
 * containers, strings and nulls. Table and row iteration order is unspecified.
 * Pointer getters require this query's current row and matching typed handle, with
 * its reservation active. No kind, bounds or lifetime checks occur.
 * Writable pointers require DS_FIELD_MUTABLE during reservations.
 * Borrowed pointers expire on row/batch advance, query end, or another
 * mutating accessor/reset for the row/table. Never write through a read pointer.
 *
 * @name DSQueryFindField
 * @param query [type:HDSQuery] Query to bind.
 * @param field [type:const DSQueryItem*] Field item to match; borrowed for this call. Access must not exceed the query declaration.
 * @return field [type:uint32_t] Field handle, or UINT32_MAX for a non-field item or an absent, incompatible or unsupported field.
 */

/*# Maximum function or event dependencies
 *
 * @constant
 * @name DS_SYSTEM_MAX_DEPENDENCIES
 * @value 16
 */

/*# Before game-object updates
 *
 * Registered in the engine's DATASTORE_CONTEXT_NAME store when engine.datastore
 * is enabled (the default). After input dispatch and before game-object updates,
 * the engine starts the frame's schedule and signals this event. It waits for submitted
 * functions and their dependents before updating components. The same schedule remains
 * active through [ref:DS_EVENT_GAMEOBJECT_POST_UPDATE]. Callbacks run on a dedicated
 * engine job context; do not access game objects or Lua from workers.
 * This does not refresh world transforms.
 * This macro computes the event hash; cache it during initialization when signaling
 * repeatedly. Standalone stores must register and signal their own events.
 *
 * @constant
 * @name DS_EVENT_GAMEOBJECT_PRE_UPDATE
 * @value dmHashString64("engine.gameobject.pre_update")
 */

/*# After game-object updates
 *
 * Registered in the engine's DATASTORE_CONTEXT_NAME store when engine.datastore
 * is enabled. Signaled after dmGameObject::Update, including late updates, and
 * before rendering. The engine waits for ready functions and their dependents,
 * then ends the frame's schedule. Post-update functions can depend on functions
 * from [ref:DS_EVENT_GAMEOBJECT_PRE_UPDATE]. This precedes game-object PostUpdate
 * deletion processing and does not refresh world transforms after callback writes.
 * Callbacks run on workers; do not access game objects or Lua there.
 * Standalone stores must register and signal their own events.
 *
 * @constant
 * @name DS_EVENT_GAMEOBJECT_POST_UPDATE
 * @value dmHashString64("engine.gameobject.post_update")
 */

/*# Process a query range
 *
 * The scheduler reserves the query and calls this function once per job. With
 * m_RowsPerJob set, jobs may run concurrently over disjoint ranges. Otherwise one
 * job covers all matching rows. An empty query still invokes one job with zero rows.
 * Use DSQueryIterRange for the supplied range, not DSQueryIter for the whole query.
 * Returning finishes this range; do not leave child jobs running or retain
 * iterators/pointers. Query access is released and dependents are scheduled only
 * after every range finishes. Compatible registered functions may run concurrently.
 * Synchronize shared context writes and do not call scheduler operations from workers.
 * Disjoint row ranges do not make aliased external or referenced data independent.
 *
 * @typedef
 * @name DSSystemProcess
 * @param query [type:HDSQuery] Registered query with active access.
 * @param first [type:uint32_t] Zero-based index in the query's reserved matching rows, not a row ID.
 * @param num_rows [type:uint32_t] Number of rows in this job's range, possibly spanning tables.
 * @param context [type:void*] Borrowed user context from registration; may be zero.
 */

/*# System registration
 *
 * Hashes are copied during registration. The query must belong to the supplied store.
 * Keep the query and context alive until unregistration or store teardown. At teardown,
 * finish callbacks, end the schedule and destroy queries before the store.
 * Dependencies name complete updates and may refer to updates registered later.
 * [ref:DSBeginSchedule] resolves hashes to indices and rejects cycles.
 * Use dmHashString64 during setup; reverse hashes are available for diagnostics.
 * Prerequisites may name functions or events. Every prerequisite must be registered;
 * the engine registers [ref:DS_EVENT_GAMEOBJECT_PRE_UPDATE] and
 * [ref:DS_EVENT_GAMEOBJECT_POST_UPDATE] in its own store.
 *
 * @struct
 * @name DSSystemDesc
 * @member m_Name [type:dmhash_t] Nonzero name hash, unique among functions and events in the store.
 * @member m_DependsOn [type:dmhash_t[]] Prerequisite hashes, packed from index zero. Stop at the first zero or use all DS_SYSTEM_MAX_DEPENDENCIES entries.
 * @member m_Query [type:HDSQuery] Borrowed query.
 * @member m_Process [type:DSSystemProcess] Required processing callback.
 * @member m_Context [type:void*] Borrowed user context passed unchanged to the callback.
 * @member m_RowsPerJob [type:uint32_t] Maximum rows per job. Zero (default) submits one job for the complete query. Nonzero opts into concurrent calls sharing m_Context; callbacks must process only their supplied range and synchronize shared state.
 */

/*# Engine event registration
 *
 * An externally signaled milestone in the same dependency graph as query callbacks.
 * Hashes are copied. Dependencies may name functions or events registered later;
 * DSBeginSchedule rejects missing prerequisites and cycles.
 *
 * @struct
 * @name DSEventDesc
 * @member m_Name [type:dmhash_t] Nonzero name hash, unique among functions and events in the store.
 * @member m_DependsOn [type:dmhash_t[]] Prerequisite hashes that must complete before signaling succeeds. Packed from index zero; stop at the first zero or use all DS_SYSTEM_MAX_DEPENDENCIES entries.
 */

/*# Register a query callback
 *
 * Copies the descriptor and hashes; registration does not invoke the callback.
 *
 * @name DSRegisterFunction
 * @param store [type:HDSContext] Store receiving the registration.
 * @param desc [type:const DSSystemDesc*] Callback, query, context and dependency hashes.
 * @return result [type:DSResult] OK, LOCKED during a schedule, ALREADY_EXISTS for a duplicate name, or INVALID_ARGUMENT for missing required values, a zero name hash or a query from another store.
 */

/*# Unregister a query callback
 *
 * Removes the named registration between schedules. The query
 * and user context remain caller-owned. Dependent registrations retain their hashes.
 *
 * @name DSUnregisterFunction
 * @param store [type:HDSContext] Store containing the registration.
 * @param name [type:dmhash_t] Name hash supplied when registering the callback.
 * @return result [type:DSResult] OK, LOCKED during a schedule, NOT_FOUND for an unknown name, or INVALID_ARGUMENT if the name identifies an event.
 */

/*# Register an engine event
 *
 * Copies the descriptor and hashes between schedules. Events have no query or callback;
 * the caller signals them explicitly with DSSchedulerEvent.
 *
 * @name DSRegisterEvent
 * @param store [type:HDSContext] Store receiving the registration.
 * @param desc [type:const DSEventDesc*] Event hash and prerequisite hashes.
 * @return result [type:DSResult] OK, LOCKED during a schedule, ALREADY_EXISTS for a duplicate name, or INVALID_ARGUMENT for a zero name hash.
 */

/*# Unregister an engine event
 *
 * Removes the named event between schedules. Dependent registrations retain their hashes.
 *
 * @name DSUnregisterEvent
 * @param store [type:HDSContext] Store containing the event.
 * @param name [type:dmhash_t] Name hash supplied when registering the event.
 * @return result [type:DSResult] OK, LOCKED during a schedule, NOT_FOUND for an unknown name, or INVALID_ARGUMENT if the name identifies a function.
 */

/*# Begin a schedule
 *
 * Starts one invocation of all registered functions and events. Resolves dependencies
 * after registration changes and clears function completion and event signals from the previous invocation.
 * Reserves each ready function's query and submits its jobs. Borrowed job context must
 * outlive this invocation. Zero-worker contexts execute work during DSSchedulerUpdate.
 * Pair success with DSEndSchedule; no threads are created or destroyed here.
 *
 * @name DSBeginSchedule
 * @param store [type:HDSContext] Store containing the registrations.
 * @param jobs [type:HJobContext] Caller-owned job context. Pump it on the same main thread as scheduler operations.
 * @return result [type:DSResult] OK, LOCKED if already active, NOT_FOUND for a missing prerequisite, or INVALID_ARGUMENT for a null job context or dependency cycle. Failure leaves the schedule inactive except when already active.
 */

/*# Signal an engine event
 *
 * Signals an engine milestone, such as the start of an update stage, once its own
 * prerequisites are satisfied. The scheduler immediately submits every ready function,
 * subject to remaining dependencies and query access checks. This call does not wait
 * for jobs. BUSY leaves the event pending: pump DSSchedulerUpdate and retry before
 * entering an engine stage that requires those prerequisites.
 * Signal each event once per invocation; DSBeginSchedule clears previous signals.
 * Events do not reserve data or protect engine accesses outside registered queries.
 *
 * @name DSSchedulerEvent
 * @param store [type:HDSContext] Store with an active schedule.
 * @param name [type:dmhash_t] Registered event hash.
 * @return result [type:DSResult] OK, BUSY while prerequisites are incomplete, NOT_FOUND for an unknown name, or INVALID_ARGUMENT without an active schedule, for a function name or for an already signaled event.
 */

/*# Update scheduled work
 *
 * Pumps the borrowed job context's completion callbacks on the main thread. In a
 * zero-worker context, also executes at most one queued job. This may service other
 * users of the same job context. Do not call recursively from a job or completion callback.
 *
 * The last job of each function releases query access and submits newly ready functions.
 * Also retries functions blocked by external query reservations. Read/read and disjoint
 * writes may overlap; conflicting accesses wait. Use m_DependsOn when order matters.
 * The same query handle cannot have two active reservations. Unsignaled events do
 * not keep this call BUSY unless a pending function depends on them.
 *
 * @name DSSchedulerUpdate
 * @param store [type:HDSContext] Store with an active schedule.
 * @return result [type:DSResult] OK when all functions finished, BUSY while functions await jobs, events or reservations, or INVALID_ARGUMENT without an active schedule.
 */

/*# Synchronize ready scheduled work
 *
 * Pumps jobs until running functions and ready functions finish, including their
 * dependents. Functions waiting on unsignaled events remain pending. Keeps the
 * schedule active so later events can use earlier completions without rerunning work.
 * Query reservations are released as functions finish; this does not lock the store.
 * Call on the scheduler's main thread, outside jobs and completion callbacks.
 *
 * @name DSSchedulerSync
 * @param store [type:HDSContext] Store with an active schedule.
 * @return result [type:DSResult] OK after ready work finishes, or INVALID_ARGUMENT without an active schedule.
 */

/*# End a schedule
 *
 * Ends the invocation, discarding unsignaled events and functions waiting on them.
 * Returns BUSY while jobs await completion or ready functions await query access;
 * keep pumping DSSchedulerUpdate before retrying. A failed end retains reservations
 * and dependencies.
 * Registration changes are allowed after success. The next begin starts fresh.
 *
 * @name DSEndSchedule
 * @param store [type:HDSContext] Store with an active schedule.
 * @return result [type:DSResult] OK, BUSY while functions are running or ready to run, or INVALID_ARGUMENT without an active schedule.
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
 * @return group [type:DSGroupId] Caller-provided owner ID, or zero for an ungrouped row or without a valid row.
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

/*# Borrowed value view
 *
 * Created by the view accessors. Copyable; owns no storage. Finish using views
 * and borrowed strings before a store mutation, row/batch advance or
 * query end. Synchronize ID-based reads externally; query views use the current
 * reservation. The private representation is not a serialized/native row layout.
 *
 * @struct
 * @name DSValueView
 */

/*# Read a field as a view
 *
 * @name DSFieldGetView
 * @param store [type:HDSContext] Store handle.
 * @param id [type:DSId] Row ID.
 * @param field [type:uint64_t] Full field-name hash.
 * @param out_view [type:DSValueView*] Receives a borrowed view on success.
 * @return result [type:DSResult] OK, or NOT_FOUND for an absent row/field.
 */

/*# Read a bound query field as a view
 *
 * Uses the same valid-row and matching-handle contract as typed pointer getters.
 * Containers and strings require read-only access. No name lookup or allocation.
 *
 * @name DSRowIterGetView
 * @param iterator [type:const DSRowIterator*] Current row of a reserved query.
 * @param field [type:uint32_t] Handle returned by DSQueryFindField.
 * @param out_view [type:DSValueView*] Receives a borrowed view.
 */

/*# Read a view's kind
 *
 * @name DSViewGetType
 * @param view [type:const DSValueView*] Valid view.
 * @return type [type:DSValueType] Value kind, including NULL for a null element.
 */

/*# Count container entries
 *
 * @name DSViewGetCount
 * @param view [type:const DSValueView*] Struct or list view.
 * @param out_count [type:uint32_t*] Receives the immediate member/element count.
 * @return result [type:DSResult] OK, or INVALID_ARGUMENT for a non-container.
 */

/*# Read a struct member
 *
 * Uses a local member hash; repeat for further nesting. No member enumeration.
 *
 * @name DSViewGetField
 * @param view [type:const DSValueView*] Struct view, including an inline struct.
 * @param field [type:uint64_t] Local member-name hash.
 * @param out_view [type:DSValueView*] Receives the member view; may alias view.
 * @return result [type:DSResult] OK, NOT_FOUND, or INVALID_ARGUMENT for a non-struct.
 */

/*# Read a list element
 *
 * @name DSViewGetElement
 * @param view [type:const DSValueView*] List view.
 * @param index [type:uint32_t] Zero-based element index.
 * @param out_view [type:DSValueView*] Receives the element view; may alias view.
 * @return result [type:DSResult] OK, NOT_FOUND outside the list, or INVALID_ARGUMENT for a non-list.
 */

/*# Read a Number view
 *
 * Copies the value without conversion. Errors leave the output unchanged.
 *
 * @name DSViewGetNumber
 * @param view [type:const DSValueView*] Valid view.
 * @param out_value [type:double*] Receives the value on success.
 * @return result [type:DSResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a Float view
 *
 * Copies the value without conversion. Errors leave the output unchanged.
 *
 * @name DSViewGetFloat
 * @param view [type:const DSValueView*] Valid view.
 * @param out_value [type:float*] Receives the value on success.
 * @return result [type:DSResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a UInt32 view
 *
 * Copies the value without conversion. Errors leave the output unchanged.
 *
 * @name DSViewGetUInt32
 * @param view [type:const DSValueView*] Valid view.
 * @param out_value [type:uint32_t*] Receives the value on success.
 * @return result [type:DSResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a Boolean view
 *
 * Copies the value without conversion. Errors leave the output unchanged.
 *
 * @name DSViewGetBoolean
 * @param view [type:const DSValueView*] Valid view.
 * @param out_value [type:uint8_t*] Receives the value on success.
 * @return result [type:DSResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a String view
 *
 * Borrows the NUL-terminated string until the view expires. Errors leave the output unchanged.
 *
 * @name DSViewGetString
 * @param view [type:const DSValueView*] Valid view.
 * @param out_value [type:const char**] Receives the value on success.
 * @return result [type:DSResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a Vector3 view
 *
 * Copies the value without conversion. Errors leave the output unchanged.
 *
 * @name DSViewGetVector3
 * @param view [type:const DSValueView*] Valid view.
 * @param out_value [type:DSVector3*] Receives the value on success.
 * @return result [type:DSResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a Vector4 view
 *
 * Copies the value without conversion. Errors leave the output unchanged.
 *
 * @name DSViewGetVector4
 * @param view [type:const DSValueView*] Valid view.
 * @param out_value [type:DSVector4*] Receives the value on success.
 * @return result [type:DSResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a Matrix4 view
 *
 * Copies the value without conversion. Errors leave the output unchanged.
 *
 * @name DSViewGetMatrix4
 * @param view [type:const DSValueView*] Valid view.
 * @param out_value [type:DSMatrix4*] Receives the value on success.
 * @return result [type:DSResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */

#endif // DMSDK_DATASTORE_H
