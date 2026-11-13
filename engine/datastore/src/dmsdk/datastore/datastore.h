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

/*# Data API
 *
 * Typed data storage, queries, field access and scheduled callbacks.
 *
 * @document
 * @name DataStore
 * @language C
 */

/*# Engine datastore context
 *
 * Registry name for the engine-owned [type:HDataStoreContext], available from extension
 * app initialization through app finalization. Borrow the store; do not destroy
 * it. Finish jobs and release your queries and tables before finalization ends.
 * Enable creation with engine.datastore = 1; the default is 0 (disabled).
 * Context lookup returns NULL while disabled.
 *
 * @constant
 * @name DATASTORE_CONTEXT_NAME
 */
#define DATASTORE_CONTEXT_NAME "datastore"

/*# Maximum items in a query descriptor
 * @constant
 * @name DATASTORE_QUERY_MAX_ITEMS
 */
#define DATASTORE_QUERY_MAX_ITEMS 32

/*# Maximum function or event dependencies
 *
 * @constant
 * @name DATASTORE_SYSTEM_MAX_DEPENDENCIES
 * @value 16
 */
#define DATASTORE_SYSTEM_MAX_DEPENDENCIES 16

/*# Before game-object updates
 *
 * Registered in the engine's DATASTORE_CONTEXT_NAME store when engine.datastore
 * is enabled. After input dispatch and before game-object updates,
 * the engine starts the frame's schedule and signals this event. It waits for submitted
 * functions and their dependents before updating components. The same schedule remains
 * active through [ref:DATASTORE_EVENT_GAMEOBJECT_POST_UPDATE]. Callbacks run on a dedicated
 * engine job context; do not access game objects or Lua from workers.
 * This does not refresh world transforms.
 * Standalone stores must register and signal their own events.
 *
 * @constant
 * @name DATASTORE_EVENT_GAMEOBJECT_PRE_UPDATE
 * @value 0xb20391cfdc285ef1
 */
#define DATASTORE_EVENT_GAMEOBJECT_PRE_UPDATE ((dmhash_t)0xb20391cfdc285ef1ULL) // DM_HASH: dmHashString64("engine.gameobject.pre_update")

/*# After game-object updates
 *
 * Registered in the engine's DATASTORE_CONTEXT_NAME store when engine.datastore
 * is enabled. Signaled after dmGameObject::Update, including late updates, and
 * before rendering. The engine waits for ready functions and their dependents,
 * then ends the frame's schedule. Post-update functions can depend on functions
 * from [ref:DATASTORE_EVENT_GAMEOBJECT_PRE_UPDATE]. This precedes game-object PostUpdate
 * deletion processing and does not refresh world transforms after callback writes.
 * Callbacks run on workers; do not access game objects or Lua there.
 * Standalone stores must register and signal their own events.
 *
 * @constant
 * @name DATASTORE_EVENT_GAMEOBJECT_POST_UPDATE
 * @value 0xcfe4de1f015942be
 */
#define DATASTORE_EVENT_GAMEOBJECT_POST_UPDATE ((dmhash_t)0xcfe4de1f015942beULL) // DM_HASH: dmHashString64("engine.gameobject.post_update")

#ifdef __cplusplus
extern "C"
{
#endif

/*# Store handle
 *
 * Owned when created with [ref:DataStoreCreate]; engine-supplied handles are borrowed.
 *
 * @typedef
 * @name HDataStoreContext
 */
typedef struct DataStoreContext* HDataStoreContext;

/*# Query handle
 *
 * Owns a live query. Destroy after its iterators finish, before its store shuts down.
 *
 * @typedef
 * @name HDataStoreQuery
 */
typedef struct DataStoreQuery* HDataStoreQuery;

/*# Row or group identifier
 *
 * As a row ID, a store-generated handle with generation checks, valid until
 * removal. Row IDs survive moves, are not serialized, and cannot be zero.
 * As a group ID, a caller-provided 64-bit owner key, such as a game-object handle.
 * Group bookkeeping is created on first addition; the caller controls uniqueness
 * and reuse. Group removal removes its rows and blob registrations. Zero selects
 * ungrouped rows. Query group filters match the same key if it is reused.
 * The datastore does not manage the external owner's lifetime.
 *
 * @typedef
 * @name DataStoreId
 */
typedef uint64_t DataStoreId;

/*# Bound query field identifier
 *
 * Returned by DataStoreQueryFindField and valid for that query until its destruction.
 * UINT32_MAX means no matching field. This is not a field-name hash or byte offset.
 *
 * @typedef
 * @name DataStoreFieldId
 */
typedef uint32_t DataStoreFieldId;

/*# Operation results
 *
 * Output arguments remain unchanged on error unless stated otherwise.
 *
 * @enum
 * @name DataStoreResult
 * @member DATASTORE_RESULT_OK Operation succeeded (0).
 * @member DATASTORE_RESULT_NOT_FOUND Table, row, group, field, system or event was not found (-1).
 * @member DATASTORE_RESULT_LOCKED An operation requires query reservations or a schedule to finish (-3).
 * @member DATASTORE_RESULT_INVALID_ARGUMENT Invalid argument, value or count (-4).
 * @member DATASTORE_RESULT_ALREADY_EXISTS Table type, field name or function/event name is duplicated (-5).
 * @member DATASTORE_RESULT_BUFFER_TOO_SMALL The output buffer is too small (-6).
 * @member DATASTORE_RESULT_INVALID_FORMAT The serialized blob is malformed or has an unsupported version (-7).
 * @member DATASTORE_RESULT_BUSY Unfinished schedule work, query already active, conflicting access, or store mutation in progress (-8).
 */
typedef enum DataStoreResult
{
    DATASTORE_RESULT_OK               = 0,
    DATASTORE_RESULT_NOT_FOUND        = -1,
    DATASTORE_RESULT_LOCKED           = -3,
    DATASTORE_RESULT_INVALID_ARGUMENT = -4,
    DATASTORE_RESULT_ALREADY_EXISTS   = -5,
    DATASTORE_RESULT_BUFFER_TOO_SMALL = -6,
    DATASTORE_RESULT_INVALID_FORMAT   = -7,
    DATASTORE_RESULT_BUSY             = -8,
} DataStoreResult;

/*# Value kinds
 *
 * Declared field kinds. Structs and lists may nest.
 *
 * @enum
 * @name DataStoreValueType
 * @member DATASTORE_TYPE_NUMBER Double-precision number (0).
 * @member DATASTORE_TYPE_BOOLEAN Boolean encoded as zero or one (1).
 * @member DATASTORE_TYPE_STRING NUL-terminated string (2).
 * @member DATASTORE_TYPE_NULL Null value with no payload (3).
 * @member DATASTORE_TYPE_STRUCT Named fields (4).
 * @member DATASTORE_TYPE_LIST Ordered values (5).
 * @member DATASTORE_TYPE_VECTOR3 Three float32 components (6).
 * @member DATASTORE_TYPE_VECTOR4 Four float32 components (7).
 * @member DATASTORE_TYPE_MATRIX4 Sixteen float32 components in column-major order (8).
 * @member DATASTORE_TYPE_FLOAT Single-precision number, stored as float.
 * @member DATASTORE_TYPE_UINT32 Unsigned 32-bit integer.
 */
typedef enum DataStoreValueType
{
    DATASTORE_TYPE_NUMBER  = 0,
    DATASTORE_TYPE_BOOLEAN = 1,
    DATASTORE_TYPE_STRING  = 2,
    DATASTORE_TYPE_NULL    = 3,
    DATASTORE_TYPE_STRUCT  = 4,
    DATASTORE_TYPE_LIST    = 5,
    DATASTORE_TYPE_VECTOR3 = 6,
    DATASTORE_TYPE_VECTOR4 = 7,
    DATASTORE_TYPE_MATRIX4 = 8,
    DATASTORE_TYPE_FLOAT   = 9,
    DATASTORE_TYPE_UINT32  = 10,
} DataStoreValueType;

/*# Vector3 value
 *
 * @struct
 * @name DataStoreVector3
 * @member m_Values [type:float[3]] X, Y and Z components.
 */
typedef struct DataStoreVector3
{
    float m_Values[3];
} DataStoreVector3;

/*# Vector4 value
 *
 * @struct
 * @name DataStoreVector4
 * @member m_Values [type:float[4]] X, Y, Z and W components.
 */
typedef struct DataStoreVector4
{
    float m_Values[4];
} DataStoreVector4;

/*# Matrix4 value
 *
 * @struct
 * @name DataStoreMatrix4
 * @member m_Values [type:float[16]] Column-major components; element at row r, column c is m_Values[c * 4 + r].
 */
typedef struct DataStoreMatrix4
{
    float m_Values[16];
} DataStoreMatrix4;

/*# Store allocation parameters
 *
 * Zero selects the default for each size. Pages are allocated lazily and shared
 * by table value chunks. These are allocator sizes, independent of OS pages.
 *
 * @struct
 * @name DataStoreCreateParams
 * @member m_PageSize [type:uint32_t] Backing allocation bytes; default 524288. Power of two, at least m_RowChunkSize.
 * @member m_RowChunkSize [type:uint32_t] Target current/default value-chunk bytes; default 262144. Power of two, at least 64. Larger rows use dedicated allocations.
 * @member m_MaxSparePages [type:uint32_t] Empty backing pages retained for reuse. Zero releases all; a null params pointer uses 16.
 */
typedef struct DataStoreCreateParams
{
    uint32_t m_PageSize;
    uint32_t m_RowChunkSize;
    uint32_t m_MaxSparePages;
} DataStoreCreateParams;

typedef struct DataStoreStructDesc DataStoreStructDesc;

/*# Field layout
 *
 * Offsets follow C alignment. Inline members and their parents require single-segment
 * names for full-name hashing. Registration copies the compiled metadata.
 *
 * @struct
 * @name DataStoreFieldDesc
 * @member m_Field [type:uint64_t] Local field name hash.
 * @member m_Type [type:DataStoreValueType] Fixed field kind.
 * @member m_Offset [type:uint32_t] Byte offset in the containing row or struct.
 * @member m_Struct [type:const DataStoreStructDesc*] Inline STRUCT layout, otherwise NULL.
 * @member m_Name [type:const char*] Borrowed local name; required for inline parents/members, without dots.
 */
typedef struct DataStoreFieldDesc
{
    uint64_t                   m_Field;  // Field name hash.
    DataStoreValueType         m_Type;   // Declared kind; fixed for the table lifetime.
    uint32_t                   m_Offset; // Byte offset within the containing row/struct.
    const DataStoreStructDesc* m_Struct; // Reusable inline layout; NULL for scalar/dynamic values.
    const char*                m_Name;   // Borrowed local name for inline composition; optional for other root fields.
} DataStoreFieldDesc;

/*# Inline struct layout
 *
 * Reusable metadata, copied by registration. Members must not overlap. Layouts must
 * be acyclic. Inline structs and dynamic containers share a 64-level nesting limit.
 *
 * @struct
 * @name DataStoreStructDesc
 * @member m_Fields [type:const DataStoreFieldDesc*] Borrowed member descriptions.
 * @member m_FieldCount [type:uint32_t] Number of members.
 * @member m_Size [type:uint32_t] Native size including alignment padding.
 */
struct DataStoreStructDesc
{
    const DataStoreFieldDesc* m_Fields;     // Borrowed member descriptions; copied at registration.
    uint32_t                  m_FieldCount; // Number of immediate members.
    uint32_t                  m_Size;       // Struct size in bytes, including trailing padding.
};

/*# Native reset history
 *
 * Controls whether native row creation retains initial values. Blob instances
 * always use their immutable resource as reset defaults. Borrowed views retain none.
 *
 * @enum
 * @name DataStoreResetPolicy
 * @member DATASTORE_RESET_NONE Store current values only; the default. Reset returns INVALID_ARGUMENT.
 * @member DATASTORE_RESET_RETAIN Keep an extra copy of initial values for row/field reset.
 */
typedef enum DataStoreResetPolicy
{
    DATASTORE_RESET_NONE   = 0,
    DATASTORE_RESET_RETAIN = 1,
} DataStoreResetPolicy;

/*# Table layout
 *
 * Tags and metadata are copied. Tags and total metadata entries, including nested
 * members, are each limited to 65,535. Full-name hashes must be unique across the
 * entire layout, including inline members. Rows share this fixed layout.
 *
 * @struct
 * @name DataStoreTableDesc
 * @member m_Type [type:uint64_t] Type name hash, unique among independently registered tables.
 * @member m_Tags [type:const uint64_t*] Borrowed tag hashes.
 * @member m_TagCount [type:uint32_t] Number of tags.
 * @member m_Fields [type:const DataStoreFieldDesc*] Borrowed root field descriptions.
 * @member m_FieldCount [type:uint32_t] Number of root fields.
 * @member m_RowStride [type:uint32_t] Native row size including trailing alignment padding.
 * @member m_ResetPolicy [type:DataStoreResetPolicy] NONE by default; RETAIN keeps an extra copy of initial native values.
 */
typedef struct DataStoreTableDesc
{
    uint64_t                  m_Type;        // Data type name hash.
    const uint64_t*           m_Tags;        // Borrowed tag hashes; copied at registration.
    uint32_t                  m_TagCount;    // Number of tags.
    const DataStoreFieldDesc* m_Fields;      // Borrowed root field descriptions; copied at registration.
    uint32_t                  m_FieldCount;  // Number of root fields.
    uint32_t                  m_RowStride;   // Bytes per row, including trailing alignment padding.
    DataStoreResetPolicy      m_ResetPolicy; // Current values only by default; RETAIN keeps initial values for reset.
} DataStoreTableDesc;

/*# Native field array
 *
 * One input column for DataStoreRowCreateSoA. The registered layout determines the
 * element type and size; an inline struct uses complete native struct elements.
 * STRING, dynamic STRUCT and LIST columns use DataStoreReference elements.
 *
 * @struct
 * @name DataStoreFieldArray
 * @member m_Field [type:uint64_t] Root field name hash.
 * @member m_Values [type:const void*] Borrowed contiguous native values; NULL only for a zero-size field or an empty batch.
 */
typedef struct DataStoreFieldArray
{
    uint64_t    m_Field;      // Root field name hash.
    const void* m_Values;     // Borrowed contiguous array of native field values.
} DataStoreFieldArray;

/*# Dynamic struct input
 *
 * Describes one native struct, with per-input member names and kinds. The table
 * field uses STRUCT without an inline layout. Members may contain DataStoreReference
 * values, including further containers. Inputs must be acyclic, no deeper than
 * 64 container levels including enclosing inline structs; names within each
 * struct must be unique.
 *
 * @struct
 * @name DataStoreStructInput
 * @member m_Layout [type:const DataStoreStructDesc*] Borrowed native member layout; required even for an empty struct.
 * @member m_Values [type:const void*] Borrowed native struct bytes; NULL only when layout size is zero.
 */
typedef struct DataStoreStructInput
{
    const DataStoreStructDesc* m_Layout; // Native member layout, borrowed through creation.
    const void*                m_Values; // One native struct matching m_Layout.
} DataStoreStructInput;

/*# Dynamic list input
 *
 * With m_Types NULL, m_Type describes a contiguous native array. Numeric elements
 * use their native API types; STRING, STRUCT and LIST elements use DataStoreReference.
 * With m_Types set, m_Values is a const void* array of payload pointers: addresses
 * of native scalar/vector/matrix values, string bytes, DataStoreStructInput or
 * DataStoreListInput. NULL-kind elements need no payload. m_Type is ignored.
 * Both arrays contain m_Count entries. Inputs are borrowed through creation;
 * values are copied, including nested payloads. Input rules match DataStoreStructInput.
 *
 * @struct
 * @name DataStoreListInput
 * @member m_Type [type:DataStoreValueType] Shared element kind when m_Types is NULL; otherwise ignored.
 * @member m_Count [type:uint32_t] Number of elements.
 * @member m_Values [type:const void*] Native array or array of payload pointers. NULL only for an empty list or a homogeneous NULL list.
 * @member m_Types [type:const DataStoreValueType*] Optional array of per-element kinds; NULL selects homogeneous input.
 */
typedef struct DataStoreListInput
{
    DataStoreValueType        m_Type;   // Shared element kind when m_Types is NULL.
    uint32_t                  m_Count;  // Number of elements.
    const void*               m_Values; // Native array, or array of payload pointers when m_Types is set.
    const DataStoreValueType* m_Types;  // Optional per-element kinds; selects mixed input.
} DataStoreListInput;

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
 * @name DataStoreReference
 * @member m_String [type:const char*] Borrowed NUL-terminated string.
 * @member m_Struct [type:const DataStoreStructInput*] Borrowed dynamic struct.
 * @member m_List [type:const DataStoreListInput*] Borrowed dynamic list.
 */
typedef union DataStoreReference
{
    const char*                 m_String; // NUL-terminated string input.
    const DataStoreStructInput* m_Struct; // Dynamic struct input.
    const DataStoreListInput*   m_List;   // Dynamic list input.
    // private
    uint64_t                    m_Storage; // Keeps reference slots eight bytes on 32-bit and 64-bit hosts.
} DataStoreReference;

/*# Loaded blob handle
 *
 * Owns a reference to validated metadata, borrowing the caller's immutable bytes.
 * Release with DataStoreBlobDestroy. Store-owned references are synchronized across
 * stores; keep a live reference while other threads call DataStoreBlobAdd.
 *
 * @typedef
 * @name HDataStoreBlob
 */
typedef struct DataStoreBlob* HDataStoreBlob;

/*# Blob instance handle
 *
 * Identifies one grouped registration in a store. Release with DataStoreBlobRemove or DataStoreGroupRemove,
 * or by destroying the store. This does not own the caller's blob bytes.
 *
 * @typedef
 * @name HDataStoreBlobInstance
 */
typedef struct DataStoreBlobInstance* HDataStoreBlobInstance;

/*# Query access flags
 *
 * Pass zero for shared read access, the default.
 *
 * @enum
 * @name DataStoreAccess
 * @member DATASTORE_FIELD_MUTABLE Exclusive read/write access (1).
 */
typedef enum DataStoreAccess
{
    DATASTORE_FIELD_MUTABLE = 1,
} DataStoreAccess;

/*# Query flags
 *
 * Zero keeps field-level access and disables whole-row pointers.
 * Field reads are the default; individual writes require DATASTORE_FIELD_MUTABLE.
 *
 * @enum
 * @name DataStoreQueryFlags
 * @member DATASTORE_ROW_MUTABLE Exclusive read/write access to the entire row; reference slots must not be overwritten (2).
 */
typedef enum DataStoreQueryFlags
{
    DATASTORE_ROW_MUTABLE = 2,
} DataStoreQueryFlags;

/*# Query item kinds
 *
 * @enum
 * @name DataStoreQueryItemKind
 * @member DATASTORE_QUERY_ITEM_NONE Unused entry; ends the item list (0).
 * @member DATASTORE_QUERY_ITEM_FIELD Required field with an exact value kind (1).
 * @member DATASTORE_QUERY_ITEM_TAG Required table tag (2).
 */
typedef enum DataStoreQueryItemKind
{
    DATASTORE_QUERY_ITEM_NONE  = 0,
    DATASTORE_QUERY_ITEM_FIELD = 1,
    DATASTORE_QUERY_ITEM_TAG   = 2,
} DataStoreQueryItemKind;

/*# Required query tag or field
 *
 * Construct with [ref:DataStoreTag] or [ref:DataStoreField], or initialize the kind and hash directly.
 * Field names may include inline members, such as "light.color"; dynamic members cannot bind.
 *
 * @struct
 * @name DataStoreQueryItem
 * @member m_Hash [type:uint64_t] Tag hash or full field name hash; zero is valid.
 * @member m_Type [type:DataStoreValueType] Exact value kind for field items; ignored for tags.
 * @member m_Access [type:uint32_t] Field access: zero for shared reads, or DATASTORE_FIELD_MUTABLE for fixed-size read/write access. Ignored for tags.
 * @member m_Kind [type:DataStoreQueryItemKind] Tag, field, or unused item.
 */
typedef struct DataStoreQueryItem
{
    uint64_t               m_Hash;
    DataStoreValueType     m_Type;
    uint32_t               m_Access;
    DataStoreQueryItemKind m_Kind;
} DataStoreQueryItem;

/*# Query filter descriptor
 *
 * Requires all tags and fields in one table, and matches any listed group.
 * Empty filters impose no restriction. Bind requested fields with DataStoreQueryFindField.
 * Zero-initialize. Items end at the first DATASTORE_QUERY_ITEM_NONE or the array capacity.
 * Filters are copied during creation; group IDs use an explicit count because zero is valid.
 *
 * @struct
 * @name DataStoreQueryDesc
 * @member m_Items [type:DataStoreQueryItem[DATASTORE_QUERY_MAX_ITEMS]] Required tags and fields, in any order.
 * @member m_GroupIds [type:const DataStoreId*] Caller-provided owner IDs; zero selects ungrouped rows. IDs may precede row creation. Optional when count is zero.
 * @member m_GroupIdCount [type:uint32_t] Group count; zero matches all.
 * @member m_Flags [type:uint32_t] DataStoreQueryFlags bits; zero keeps field-level access and disables native row pointers.
 */
typedef struct DataStoreQueryDesc
{
    DataStoreQueryItem m_Items[DATASTORE_QUERY_MAX_ITEMS];
    const DataStoreId* m_GroupIds;
    uint32_t           m_GroupIdCount;
    uint32_t           m_Flags;
} DataStoreQueryDesc;

/*# Process a query range
 *
 * The scheduler reserves the query and calls this function once per job. With
 * m_RowsPerJob set, jobs may run concurrently over disjoint ranges. Otherwise one
 * job covers all matching rows. An empty query still invokes one job with zero rows.
 * Use DataStoreQueryIterRange for the supplied range, not DataStoreQueryIter for the whole query.
 * Returning finishes this range; do not leave child jobs running or retain
 * iterators/pointers. Query access is released and dependents are scheduled only
 * after every range finishes. Compatible registered functions may run concurrently.
 * Synchronize shared context writes and do not call scheduler operations from workers.
 * Disjoint row ranges do not make aliased external or referenced data independent.
 *
 * @typedef
 * @name DataStoreSystemProcess
 * @param query [type:HDataStoreQuery] Registered query with active access.
 * @param first [type:uint32_t] Zero-based index in the query's reserved matching rows, not a row ID.
 * @param num_rows [type:uint32_t] Number of rows in this job's range, possibly spanning tables.
 * @param context [type:void*] Borrowed user context from registration; may be zero.
 */
typedef void (*DataStoreSystemProcess)(HDataStoreQuery query, uint32_t first, uint32_t num_rows, void* context);

/*# System registration
 *
 * Hashes are copied during registration. The query must belong to the supplied store.
 * Keep the query and context alive until unregistration or store teardown. At teardown,
 * finish callbacks, end the schedule and destroy queries before the store.
 * Dependencies name complete updates and may refer to updates registered later.
 * [ref:DataStoreSchedulerBegin] resolves hashes to indices and rejects cycles.
 * Use dmHashString64 during setup; reverse hashes are available for diagnostics.
 * Prerequisites may name functions or events. Every prerequisite must be registered;
 * the engine registers [ref:DATASTORE_EVENT_GAMEOBJECT_PRE_UPDATE] and
 * [ref:DATASTORE_EVENT_GAMEOBJECT_POST_UPDATE] in its own store.
 *
 * @struct
 * @name DataStoreSystemDesc
 * @member m_Name [type:dmhash_t] Nonzero name hash, unique among functions and events in the store.
 * @member m_DependsOn [type:dmhash_t[]] Prerequisite hashes, packed from index zero. Stop at the first zero or use all DATASTORE_SYSTEM_MAX_DEPENDENCIES entries.
 * @member m_Query [type:HDataStoreQuery] Borrowed query.
 * @member m_Process [type:DataStoreSystemProcess] Required processing callback.
 * @member m_Context [type:void*] Borrowed user context passed unchanged to the callback.
 * @member m_RowsPerJob [type:uint32_t] Maximum rows per job. Zero (default) submits one job for the complete query. Nonzero opts into concurrent calls sharing m_Context; callbacks must process only their supplied range and synchronize shared state.
 */
typedef struct DataStoreSystemDesc
{
    dmhash_t               m_Name;
    dmhash_t               m_DependsOn[DATASTORE_SYSTEM_MAX_DEPENDENCIES];
    HDataStoreQuery        m_Query;
    DataStoreSystemProcess m_Process;
    void*                  m_Context;
    uint32_t               m_RowsPerJob;
} DataStoreSystemDesc;

/*# Engine event registration
 *
 * An externally signaled milestone in the same dependency graph as query callbacks.
 * Hashes are copied. Dependencies may name functions or events registered later;
 * DataStoreSchedulerBegin rejects missing prerequisites and cycles.
 *
 * @struct
 * @name DataStoreEventDesc
 * @member m_Name [type:dmhash_t] Nonzero name hash, unique among functions and events in the store.
 * @member m_DependsOn [type:dmhash_t[]] Prerequisite hashes that must complete before signaling succeeds. Packed from index zero; stop at the first zero or use all DATASTORE_SYSTEM_MAX_DEPENDENCIES entries.
 */
typedef struct DataStoreEventDesc
{
    dmhash_t m_Name;
    dmhash_t m_DependsOn[DATASTORE_SYSTEM_MAX_DEPENDENCIES];
} DataStoreEventDesc;

/*# Query iterator
 *
 * Stack cursor; iterators and fixed-size field access allocate nothing.
 * Keep the query/store alive and its query reservation active.
 * Do not copy active iterators. Parents must stay at the same address; stepping
 * one invalidates its children, including when exhausted. Lifetimes are not checked.
 * Batch and row order are unspecified.
 *
 * @struct
 * @name DataStoreIterator
 */
typedef struct DataStoreIterator
{
    // private
    HDataStoreQuery        m_Query;
    uint32_t               m_NextRow;
    uint32_t               m_Count;
    void*                  m_Table;
    void*                  m_Rows;
    const void*            m_Fields;
    const uint32_t*        m_FieldOffsets;
    uint8_t*               m_Values;
    uint32_t               m_RowStride;
    uint32_t               m_FieldCount;
    uint32_t               m_RangeIndex;
    uint32_t               m_Remaining;
    uint8_t                m_Flags;
} DataStoreIterator;

/*# Row iterator
 *
 * Traverses the current batch. [ref:DataStoreIterator] lifetime rules apply.
 * Fixed-size writes preserve sibling fields and defaults. Reset outside reservations.
 *
 * @struct
 * @name DataStoreRowIterator
 */
typedef struct DataStoreRowIterator
{
    // private
    const DataStoreIterator* m_Parent;
    uint32_t                 m_Index;
    uint32_t                 m_NextRow;
} DataStoreRowIterator;

/*# Borrowed value view
 *
 * Created by the view accessors. Copyable; owns no storage. Finish using views
 * and borrowed strings before a store mutation, row/batch advance or
 * query end. Synchronize ID-based reads externally; query views use the current
 * reservation. The private representation is not a serialized/native row layout.
 *
 * @struct
 * @name DataStoreValueView
 */
typedef struct DataStoreValueView
{
    // private
    uint64_t m_Storage[9];
} DataStoreValueView;

/*# Container value iterator
 *
 * Borrows its parent view, which must remain unchanged and outlive iteration.
 * Has the same borrowed-data lifetime and synchronization requirements as DataStoreValueView.
 *
 * @struct
 * @name DataStoreViewIterator
 */
typedef struct DataStoreViewIterator
{
    // private
    const DataStoreValueView* m_Parent;
    uint32_t                  m_Index;
    uint32_t                  m_Count;
} DataStoreViewIterator;

/*# Create an empty store
 *
 * The caller owns the store and releases it with [ref:DataStoreDestroy].
 *
 * @name DataStoreCreate
 * @param params [type:const DataStoreCreateParams*] Allocation settings; zero uses defaults. Copied during creation.
 * @return store [type:HDataStoreContext] New store, or zero for invalid allocation sizes
 * or when linked with datastore_null.
 */
HDataStoreContext DataStoreCreate(const DataStoreCreateParams* params);

/*# Destroy a store
 *
 * Frees the store, tables/rows and callback/event registrations. Finish callbacks and all
 * access, end any schedule, then destroy queries. Borrowed blobs and callback contexts
 * remain caller-owned; destruction does not invoke callbacks.
 *
 * @name DataStoreDestroy
 * @param store [type:HDataStoreContext] Store owned by the caller.
 * @return result [type:DataStoreResult] OK, or LOCKED during a schedule or query reservation; leaves the store intact on failure.
 */
DataStoreResult DataStoreDestroy(HDataStoreContext store);

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
 * @name DataStoreGroupRemove
 * @param store [type:HDataStoreContext] Store owning the group.
 * @param group [type:DataStoreId] Caller-provided owner ID.
 * @return result [type:DataStoreResult] OK, LOCKED during query reservations, or NOT_FOUND for zero or an unknown group. Failure leaves the store unchanged.
 */
DataStoreResult DataStoreGroupRemove(HDataStoreContext store, DataStoreId group);

/*# Add an empty table
 *
 * Copies tags and field metadata. Fields have fixed kinds and non-overlapping byte offsets.
 * Duplicate type hashes and full-name field hashes (including inline members) are rejected. Successful
 * registration updates live queries. Requires a store without active query reservations.
 *
 * @name DataStoreTableAdd
 * @param store [type:HDataStoreContext] Store handle.
 * @param desc [type:const DataStoreTableDesc*] Table type, tags and shared field layout to copy.
 * @return result [type:DataStoreResult] LOCKED while query reservations are active; otherwise OK, ALREADY_EXISTS for a duplicate type/field name, or INVALID_ARGUMENT for invalid tags/count/layout.
 */
DataStoreResult DataStoreTableAdd(HDataStoreContext store, const DataStoreTableDesc* desc);

/*# Register borrowed native rows
 *
 * Creates a runtime table over caller-owned AoS values without copying them.
 * Layouts contain scalar/vector/matrix fields; struct and reference fields are unsupported.
 * Indices select source rows and are copied;
 * NULL selects rows in order. Optional out_ids receives row identities.
 * Keep values alive, aligned and stationary until DataStoreTableRemove or store destruction.
 * The caller synchronizes external access with query reservations. Shared reads are
 * allowed; writable jobs must select distinct source rows, including within one query.
 * Views with overlapping source rows conflict on overlapping writable fields.
 * View rows cannot be created, removed or reset individually, or serialized.
 * Unregistering frees metadata and identities, never the borrowed values.
 *
 * @name DataStoreTableAddView
 * @param store [type:HDataStoreContext] Destination store.
 * @param desc [type:const DataStoreTableDesc*] Native layout and tags; copied.
 * @param values [type:void*] Borrowed native row array.
 * @param value_count [type:uint32_t] Accessible source rows.
 * @param row_count [type:uint32_t] Selected rows.
 * @param indices [type:const uint32_t*] Optional source indices, each below value_count.
 * @param out_ids [type:DataStoreId*] Optional row_count output identities.
 * @return result [type:DataStoreResult] OK, LOCKED, ALREADY_EXISTS or INVALID_ARGUMENT.
 */
DataStoreResult DataStoreTableAddView(HDataStoreContext store, const DataStoreTableDesc* desc, void* values, uint32_t value_count, uint32_t row_count, const uint32_t* indices, DataStoreId* out_ids);

/*# Remove a table
 *
 * Removes an independently registered table and all its rows. Their IDs become stale, even if the type is
 * registered again. Updates live queries. Requires a store without active query reservations.
 *
 * @name DataStoreTableRemove
 * @param store [type:HDataStoreContext] Store handle.
 * @param type [type:uint64_t] Table type hash.
 * @return result [type:DataStoreResult] LOCKED while query reservations are active; otherwise OK on success, or NOT_FOUND if the type is not registered.
 */
DataStoreResult DataStoreTableRemove(HDataStoreContext store, uint64_t type);

/*# Create rows from native input
 *
 * Copies count complete rows using the registered C layout and row size. Assigns
 * group to every row; groups can share a table. DataStoreGroupRemove removes owned rows.
 * Stores current values only unless m_ResetPolicy is DATASTORE_RESET_RETAIN.
 * Native rows have no component name; DataStoreRowGetComponentId returns zero.
 * Supports fixed-size values, inline structs and DataStoreReference fields. Strings
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
 * @name DataStoreRowCreate
 * @param store [type:HDataStoreContext] Destination store.
 * @param type [type:uint64_t] Registered table type.
 * @param group [type:DataStoreId] Caller-provided owner ID; bookkeeping is created lazily. Zero creates ungrouped rows.
 * @param count [type:uint32_t] Rows to add; zero adds nothing.
 * @param rows [type:const void*] Borrowed contiguous native rows; NULL for zero-size rows or an empty batch.
 * @param out_ids [type:DataStoreId*] Optional array receiving count stable IDs on success; NULL skips the output. IDs remain available through row iteration.
 * @return result [type:DataStoreResult] OK, LOCKED, NOT_FOUND for an unknown type, ALREADY_EXISTS for duplicate dynamic member names, or INVALID_ARGUMENT for incompatible input or capacity limits.
 */
DataStoreResult DataStoreRowCreate(HDataStoreContext store, uint64_t type, DataStoreId group, uint32_t count, const void* rows, DataStoreId* out_ids);

/*# Create rows from field arrays
 *
 * Supply every root field exactly once, in any order, with count values per array.
 * Copies into the selected runtime storage and retains native reset rows only
 * when m_ResetPolicy is DATASTORE_RESET_RETAIN.
 * Types and element sizes come from the registered layout. Inline struct fields
 * use arrays of complete structs, not separate arrays for their nested members.
 * Inputs, including struct padding, must be initialized, disjoint from store
 * storage and valid through the call. The table policy controls reset history. Group,
 * locking and reference ownership rules match DataStoreRowCreate. On error no rows are
 * published and output IDs remain unchanged.
 *
 * @name DataStoreRowCreateSoA
 * @param store [type:HDataStoreContext] Destination store.
 * @param type [type:uint64_t] Registered table type.
 * @param group [type:DataStoreId] Caller-provided owner ID; bookkeeping is created lazily. Zero creates ungrouped rows.
 * @param count [type:uint32_t] Rows to add; zero returns OK without accessing the store or arrays.
 * @param field_count [type:uint32_t] Number of root field arrays.
 * @param fields [type:const DataStoreFieldArray*] Borrowed field arrays; NULL for a table without fields or an empty batch.
 * @param out_ids [type:DataStoreId*] Optional array receiving count stable IDs on success; NULL skips the output. IDs remain available through row iteration.
 * @return result [type:DataStoreResult] OK, LOCKED, NOT_FOUND for an unknown table, ALREADY_EXISTS for duplicate dynamic member names, or INVALID_ARGUMENT for invalid fields, input or capacity limits.
 */
DataStoreResult DataStoreRowCreateSoA(HDataStoreContext store, uint64_t type, DataStoreId group, uint32_t count, uint32_t field_count, const DataStoreFieldArray* fields, DataStoreId* out_ids);

/*# Remove a row
 *
 * Uses swap removal; other live IDs remain valid. The removed ID becomes stale.
 * Requires a store without active query reservations. To remove during traversal, collect DataStoreIds in a
 * caller-owned reusable buffer and apply removals after all query reservations finish.
 * Reclaims the retained default slot and owned dynamic child rows. String/list
 * payload chunks are recycled; empty blocks are freed, retaining one small spare
 * per arena. Borrowed blob payloads remain unchanged.
 *
 * @name DataStoreRowRemove
 * @param store [type:HDataStoreContext] Store handle.
 * @param id [type:DataStoreId] ID of the row to remove.
 * @return result [type:DataStoreResult] LOCKED while query reservations are active; otherwise OK on success, NOT_FOUND for an invalid or stale ID, or INVALID_ARGUMENT for a borrowed view.
 */
DataStoreResult DataStoreRowRemove(HDataStoreContext store, DataStoreId id);

/*# Reset one component row
 *
 * Restores all fields, including nested values, to their loaded or added defaults.
 * Other rows are unaffected, including other components with the same group or type.
 * Does not allocate, change IDs or query membership, or invalidate iterators.
 * Mutable row storage stays in place. Owned replacement payloads are released for
 * reuse; borrowed views and strings must no longer be used.
 * Returns LOCKED during query reservations.
 *
 * Tables registered with DATASTORE_RESET_NONE do not retain history and cannot reset.
 *
 * @name DataStoreRowReset
 * @param store [type:HDataStoreContext] Store containing the component row.
 * @param id [type:DataStoreId] Component's runtime row ID.
 * @return result [type:DataStoreResult] OK, LOCKED during query reservations, NOT_FOUND for an invalid or stale ID, or INVALID_ARGUMENT for a borrowed view or a table with DATASTORE_RESET_NONE.
 */
DataStoreResult DataStoreRowReset(HDataStoreContext store, DataStoreId id);

/*# Read a component's prototype-local name hash
 *
 * Used by game object integration to associate a runtime row with its component.
 *
 * Uses the ID-read synchronization requirements of [ref:DataStoreFieldGetNumber].
 *
 * @name DataStoreRowGetComponentId
 * @param store [type:HDataStoreContext] Store containing the row.
 * @param id [type:DataStoreId] Runtime row ID.
 * @return component [type:uint64_t] Component name hash, or zero for an invalid/stale row. Zero may also identify an unnamed row.
 */
uint64_t DataStoreRowGetComponentId(HDataStoreContext store, DataStoreId id);

/*# Load an immutable packed resource
 *
 * Validates all tables and borrows the supplied buffer without copying or modifying it.
 * The input must be eight-byte aligned. Member offsets and row/struct sizes are
 * validated for C-compatible alignment. The caller keeps the buffer unchanged and alive
 * until the blob reference and all its registrations have been released.
 * Loading does not register rows in a store.
 *
 * @name DataStoreBlobLoad
 * @param buffer [type:const void*] Caller-owned complete packed component-data blob.
 * @param buffer_size [type:uint32_t] Blob byte size.
 * @param out_blob [type:HDataStoreBlob*] Receives a caller-owned reference on success; unchanged on error.
 * @return result [type:DataStoreResult] OK or INVALID_FORMAT for malformed/unsupported data or an unaligned buffer.
 */
DataStoreResult DataStoreBlobLoad(const void* buffer, uint32_t buffer_size, HDataStoreBlob* out_blob);

/*# Release a loaded resource reference
 *
 * Resource pools retain a reference while any registrations remain. The last release frees the blob handle,
 * never the caller's buffer. Does not remove any instance's rows. The caller may
 * free the buffer after releasing this reference and removing all registrations.
 *
 * @name DataStoreBlobDestroy
 * @param blob [type:HDataStoreBlob] Reference to release; must not be used afterward.
 */
void DataStoreBlobDestroy(HDataStoreBlob blob);

/*# Add every component table for one resource instance
 *
 * Retains the resource and copies fixed row bytes into shared mutable tables.
 * Registrations of the same loaded resource share metadata, defaults and capacity.
 * Strings and containers retain blob offsets. The caller keeps the immutable
 * buffer alive until all references and registrations are released.
 * Requires a store without active query reservations.
 *
 * @name DataStoreBlobAdd
 * @param store [type:HDataStoreContext] Destination store.
 * @param blob [type:HDataStoreBlob] Loaded resource to retain.
 * @param group [type:DataStoreId] Caller-provided owner ID; bookkeeping is created lazily even for an empty blob. Zero leaves the registration ungrouped.
 * @param out_instance [type:HDataStoreBlobInstance*] Receives the registration handle on success; unchanged on error.
 * @return result [type:DataStoreResult] OK, LOCKED while query reservations are active, or INVALID_ARGUMENT if store capacity would be exceeded. Errors leave the store unchanged.
 */
DataStoreResult DataStoreBlobAdd(HDataStoreContext store, HDataStoreBlob blob, DataStoreId group, HDataStoreBlobInstance* out_instance);

/*# Remove one instance's complete set of component tables
 *
 * Removes its remaining rows, invalidates their IDs, frees its replacement payloads,
 * and returns the registration slot for reuse. Shared capacity remains while other
 * registrations use that resource; the last removal frees the pool and its blob reference.
 * DataStoreGroupRemove also releases registrations owned by that group. Their handles
 * must not be used afterward. The store releases all registrations when destroyed.
 *
 * @name DataStoreBlobRemove
 * @param instance [type:HDataStoreBlobInstance] Registration to remove; must not be used after success.
 * @return result [type:DataStoreResult] OK, or LOCKED while its store has active query reservations (no changes).
 */
DataStoreResult DataStoreBlobRemove(HDataStoreBlobInstance instance);

/*# Reset a game object's grouped component registration
 *
 * Copies loaded values into this registration's remaining mutable rows and frees its replacement payload blocks.
 * Does not restore removed rows or invalidate iterators. Borrowed replacement payloads expire.
 * Other registrations are unaffected, even if they share a blob, type or group ID.
 * The engine retains this handle when registering a game object's component tables;
 * no datastore-wide group search is needed. Independently added rows are not included.
 *
 * @name DataStoreBlobReset
 * @param instance [type:HDataStoreBlobInstance] Registration to reset.
 * @return result [type:DataStoreResult] OK, or LOCKED during query reservations.
 */
DataStoreResult DataStoreBlobReset(HDataStoreBlobInstance instance);

/*# Describe a required table tag
 *
 * Hashes the name immediately; the returned item does not retain the string.
 *
 * @name DataStoreTag
 * @param name [type:const char*] NUL-terminated tag name.
 * @return item [type:DataStoreQueryItem] Tag item for query creation.
 */
DataStoreQueryItem DataStoreTag(const char* name);

/*# Describe a query field by name
 *
 * Hashes the name immediately; the returned descriptor does not retain the string.
 *
 * @name DataStoreField
 * @param name [type:const char*] NUL-terminated full name, such as "light.color".
 * @param type [type:DataStoreValueType] Required value kind.
 * @param access [type:uint32_t] Zero for shared reads, or DATASTORE_FIELD_MUTABLE for read/write access.
 * @return field [type:DataStoreQueryItem] Field item for query creation or binding.
 */
DataStoreQueryItem DataStoreField(const char* name, DataStoreValueType type, uint32_t access);

/*# Create a reusable live query
 *
 * Copies filters and tracks matching tables, including later registrations.
 * Destroy the query before its store.
 *
 * @name DataStoreQueryCreate
 * @param store [type:HDataStoreContext] Store handle.
 * @param desc [type:const DataStoreQueryDesc*] Filters to copy.
 * @param out_query [type:HDataStoreQuery*] Receives the owned query on success.
 * @return result [type:DataStoreResult] OK, INVALID_ARGUMENT for invalid items/access or excessive group count, or LOCKED during query reservations.
 */
DataStoreResult DataStoreQueryCreate(HDataStoreContext store, const DataStoreQueryDesc* desc, HDataStoreQuery* out_query);

/*# Destroy a query
 *
 * Frees the query. Finish using its iterators first.
 *
 * @name DataStoreQueryDestroy
 * @param query [type:HDataStoreQuery] Query whose store is still alive.
 * @return result [type:DataStoreResult] OK, or LOCKED while query reservations are active or this query is registered in an active schedule.
 */
DataStoreResult DataStoreQueryDestroy(HDataStoreQuery query);

/*# Reserve query access
 *
 * Thread-safe, nonblocking reservation that keeps rows and bindings stable.
 * Overlapping fields on the same table allow read/read access; writes conflict.
 * Disjoint group filters permit concurrent writes to the same table. Aliased external
 * views remain conservative. Without fields or whole-row flags, only row identities are read.
 * DATASTORE_ROW_MUTABLE conflicts with any reader or writer of overlapping row values.
 * Field filters still select matching tables.
 * Field writes require DATASTORE_FIELD_MUTABLE, or DATASTORE_ROW_MUTABLE for native row access;
 * string replacement and resets must happen outside reservations.
 * Each query supports one active reservation, shared by its jobs. The caller schedules
 * and synchronizes jobs, then calls DataStoreQueryEnd once, even for an empty query.
 * The scheduler owns this scope for both serial and parallel iteration.
 * Do not bypass reservations with unrelated ID-based access.
 * Matching-table changes invalidate cached compatibility; row changes refresh ranges.
 * First use of a query pair or changed ranges may allocate; later begins reuse storage.
 *
 * @name DataStoreQueryTryBegin
 * @param query [type:HDataStoreQuery] Query to reserve.
 * @return result [type:DataStoreResult] OK, or BUSY without acquiring access if already active or unavailable.
 */
DataStoreResult DataStoreQueryTryBegin(HDataStoreQuery query);

/*# Count reserved rows
 *
 * @name DataStoreQueryGetRowCount
 * @param query [type:HDataStoreQuery] Query with an active reservation.
 * @return count [type:uint32_t] Reserved matching row count.
 */
uint32_t DataStoreQueryGetRowCount(HDataStoreQuery query);

/*# Release query access
 *
 * Thread-safe release. The caller must join all jobs and finish using iterators and pointers first.
 *
 * @name DataStoreQueryEnd
 * @param query [type:HDataStoreQuery] Query with a successful begin to release once.
 */
void DataStoreQueryEnd(HDataStoreQuery query);

/*# Find a query field for direct pointer access
 *
 * Binds a requested full-name hash, kind and access to a handle valid until query destruction.
 * The handle is neither a byte offset nor a query descriptor index. No lock or matching rows
 * are required. Fixed-size kinds support typed pointers; DataStoreRowIterGetView also reads
 * containers, strings and nulls. Table and row iteration order is unspecified.
 * Pointer getters require this query's current row and matching typed handle, with
 * its reservation active. No kind, bounds or lifetime checks occur.
 * Writable pointers require DATASTORE_FIELD_MUTABLE during reservations.
 * Borrowed pointers expire on row/batch advance, query end, or another
 * mutating accessor/reset for the row/table. Never write through a read pointer.
 *
 * @name DataStoreQueryFindField
 * @param query [type:HDataStoreQuery] Query to bind.
 * @param field [type:const DataStoreQueryItem*] Field item to match; borrowed for this call. Access must not exceed the query declaration.
 * @return field [type:DataStoreFieldId] Field handle, or UINT32_MAX for a non-field item or an absent, incompatible or unsupported field.
 */
DataStoreFieldId DataStoreQueryFindField(HDataStoreQuery query, const DataStoreQueryItem* field);

/*# Register a query callback
 *
 * Copies the descriptor and hashes; registration does not invoke the callback.
 *
 * @name DataStoreFunctionAdd
 * @param store [type:HDataStoreContext] Store receiving the registration.
 * @param desc [type:const DataStoreSystemDesc*] Callback, query, context and dependency hashes.
 * @return result [type:DataStoreResult] OK, LOCKED during a schedule, ALREADY_EXISTS for a duplicate name, or INVALID_ARGUMENT for missing required values, a zero name hash or a query from another store.
 */
DataStoreResult DataStoreFunctionAdd(HDataStoreContext store, const DataStoreSystemDesc* desc);

/*# Unregister a query callback
 *
 * Removes the named registration between schedules. The query
 * and user context remain caller-owned. Dependent registrations retain their hashes.
 *
 * @name DataStoreFunctionRemove
 * @param store [type:HDataStoreContext] Store containing the registration.
 * @param name [type:dmhash_t] Name hash supplied when registering the callback.
 * @return result [type:DataStoreResult] OK, LOCKED during a schedule, NOT_FOUND for an unknown name, or INVALID_ARGUMENT if the name identifies an event.
 */
DataStoreResult DataStoreFunctionRemove(HDataStoreContext store, dmhash_t name);

/*# Register an engine event
 *
 * Copies the descriptor and hashes between schedules. Events have no query or callback;
 * the caller signals them explicitly with DataStoreSchedulerEvent.
 *
 * @name DataStoreEventAdd
 * @param store [type:HDataStoreContext] Store receiving the registration.
 * @param desc [type:const DataStoreEventDesc*] Event hash and prerequisite hashes.
 * @return result [type:DataStoreResult] OK, LOCKED during a schedule, ALREADY_EXISTS for a duplicate name, or INVALID_ARGUMENT for a zero name hash.
 */
DataStoreResult DataStoreEventAdd(HDataStoreContext store, const DataStoreEventDesc* desc);

/*# Unregister an engine event
 *
 * Removes the named event between schedules. Dependent registrations retain their hashes.
 *
 * @name DataStoreEventRemove
 * @param store [type:HDataStoreContext] Store containing the event.
 * @param name [type:dmhash_t] Name hash supplied when registering the event.
 * @return result [type:DataStoreResult] OK, LOCKED during a schedule, NOT_FOUND for an unknown name, or INVALID_ARGUMENT if the name identifies a function.
 */
DataStoreResult DataStoreEventRemove(HDataStoreContext store, dmhash_t name);

/*# Begin a schedule
 *
 * Starts one invocation of all registered functions and events. Resolves dependencies
 * after registration changes and clears function completion and event signals from the previous invocation.
 * Reserves each ready function's query and submits its jobs. Borrowed job context must
 * outlive this invocation. Zero-worker contexts execute work during DataStoreSchedulerUpdate.
 * Pair success with DataStoreSchedulerEnd; no threads are created or destroyed here.
 *
 * @name DataStoreSchedulerBegin
 * @param store [type:HDataStoreContext] Store containing the registrations.
 * @param jobs [type:HJobContext] Caller-owned job context. Pump it on the same main thread as scheduler operations.
 * @return result [type:DataStoreResult] OK, LOCKED if already active, NOT_FOUND for a missing prerequisite, or INVALID_ARGUMENT for a null job context or dependency cycle. Failure leaves the schedule inactive except when already active.
 */
DataStoreResult DataStoreSchedulerBegin(HDataStoreContext store, HJobContext jobs);

/*# Signal an engine event
 *
 * Signals an engine milestone, such as the start of an update stage, once its own
 * prerequisites are satisfied. The scheduler immediately submits every ready function,
 * subject to remaining dependencies and query access checks. This call does not wait
 * for jobs. BUSY leaves the event pending: pump DataStoreSchedulerUpdate and retry before
 * entering an engine stage that requires those prerequisites.
 * Signal each event once per invocation; DataStoreSchedulerBegin clears previous signals.
 * Events do not reserve data or protect engine accesses outside registered queries.
 *
 * @name DataStoreSchedulerEvent
 * @param store [type:HDataStoreContext] Store with an active schedule.
 * @param name [type:dmhash_t] Registered event hash.
 * @return result [type:DataStoreResult] OK, BUSY while prerequisites are incomplete, NOT_FOUND for an unknown name, or INVALID_ARGUMENT without an active schedule, for a function name or for an already signaled event.
 */
DataStoreResult DataStoreSchedulerEvent(HDataStoreContext store, dmhash_t name);

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
 * @name DataStoreSchedulerUpdate
 * @param store [type:HDataStoreContext] Store with an active schedule.
 * @return result [type:DataStoreResult] OK when all functions finished, BUSY while functions await jobs, events or reservations, or INVALID_ARGUMENT without an active schedule.
 */
DataStoreResult DataStoreSchedulerUpdate(HDataStoreContext store);

/*# Synchronize ready scheduled work
 *
 * Pumps jobs until running functions and ready functions finish, including their
 * dependents. Functions waiting on unsignaled events remain pending. Keeps the
 * schedule active so later events can use earlier completions without rerunning work.
 * Query reservations are released as functions finish; this does not lock the store.
 * Call on the scheduler's main thread, outside jobs and completion callbacks.
 *
 * @name DataStoreSchedulerSync
 * @param store [type:HDataStoreContext] Store with an active schedule.
 * @return result [type:DataStoreResult] OK after ready work finishes, or INVALID_ARGUMENT without an active schedule.
 */
DataStoreResult DataStoreSchedulerSync(HDataStoreContext store);

/*# End a schedule
 *
 * Ends the invocation, discarding unsignaled events and functions waiting on them.
 * Returns BUSY while jobs await completion or ready functions await query access;
 * keep pumping DataStoreSchedulerUpdate before retrying. A failed end retains reservations
 * and dependencies.
 * Registration changes are allowed after success. The next begin starts fresh.
 *
 * @name DataStoreSchedulerEnd
 * @param store [type:HDataStoreContext] Store with an active schedule.
 * @return result [type:DataStoreResult] OK, BUSY while functions are running or ready to run, or INVALID_ARGUMENT without an active schedule.
 */
DataStoreResult DataStoreSchedulerEnd(HDataStoreContext store);

/*# Iterate a reserved row range
 *
 * Selects [first, first + count) within reserved rows, possibly across tables.
 * Each job owns its cursor; ranges must be in bounds and disjoint when writing.
 * Keep the reservation active until all iterators and borrowed pointers finish.
 *
 * @name DataStoreQueryIterRange
 * @param query [type:HDataStoreQuery] Query reserved for these jobs.
 * @param first [type:uint32_t] Index in the reserved matching rows.
 * @param count [type:uint32_t] Row count; zero is allowed.
 * @return iterator [type:DataStoreIterator] Cursor to advance with DataStoreIterNext.
 */
DataStoreIterator DataStoreQueryIterRange(HDataStoreQuery query, uint32_t first, uint32_t count);

/*# Start query iteration
 *
 * Traverses all rows in an active query reservation. The scheduler keeps access
 * reserved until every iterator and job finishes, including early exits.
 *
 * @name DataStoreQueryIter
 * @param query [type:HDataStoreQuery] Query reserved for traversal.
 * @return iterator [type:DataStoreIterator] Iterator with no current batch; advance using DataStoreIterNext.
 */
DataStoreIterator DataStoreQueryIter(HDataStoreQuery query);

/*# Advance to the next batch
 *
 * Selects a nonempty contiguous batch. Invalidates child iterators, including when exhausted.
 *
 * @name DataStoreIterNext
 * @param iterator [type:DataStoreIterator*] Iterator to advance.
 * @return has_batch [type:bool] True for a new batch, or false when exhausted.
 */
bool DataStoreIterNext(DataStoreIterator* iterator);

/*# Get the current table type
 *
 * @name DataStoreIterGetType
 * @param iterator [type:const DataStoreIterator*] Batch iterator.
 * @return type [type:uint64_t] Table type hash, or zero without a valid current batch. Zero may also be a valid type hash.
 */
uint64_t DataStoreIterGetType(const DataStoreIterator* iterator);

/*# Start row iteration
 *
 * Borrows the current batch; empty when no batch is selected.
 *
 * @name DataStoreIterRows
 * @param iterator [type:const DataStoreIterator*] Parent query iterator to borrow.
 * @return rows [type:DataStoreRowIterator] Iterator with no current row; advance using DataStoreRowIterNext.
 */
static inline DataStoreRowIterator DataStoreIterRows(const DataStoreIterator* iterator);

/*# Advance to the next row
 *
 * Invalidates borrowed field pointers; clears the current row when exhausted.
 *
 * @name DataStoreRowIterNext
 * @param iterator [type:DataStoreRowIterator*] Row iterator.
 * @return has_row [type:bool] True for a new row, or false when exhausted.
 */
static inline bool DataStoreRowIterNext(DataStoreRowIterator* iterator);

/*# Get the current row ID
 *
 * @name DataStoreRowIterGetId
 * @param iterator [type:const DataStoreRowIterator*] Row iterator.
 * @return id [type:DataStoreId] Store-local row ID, or zero without a valid current row.
 */
DataStoreId DataStoreRowIterGetId(const DataStoreRowIterator* iterator);

/*# Get the current row group
 *
 * @name DataStoreRowIterGetGroupId
 * @param iterator [type:const DataStoreRowIterator*] Row iterator.
 * @return group [type:DataStoreId] Caller-provided owner ID, or zero for an ungrouped row or without a valid row.
 */
DataStoreId DataStoreRowIterGetGroupId(const DataStoreRowIterator* iterator);

/*# Borrow the native row bytes
 *
 * Requires a nonzero row stride. Cast only to the registered C layout,
 * including its member offsets and size.
 * Use DataStoreIterGetType to distinguish layouts in queries matching multiple types.
 * Returns zero unless the query flags include DATASTORE_ROW_MUTABLE.
 * Otherwise reads any field; permissions are cached in the iterator.
 * Reference slots are internal handles, not native pointers; use container accessors for them.
 * The pointer follows DataStoreIterator lifetime rules. No layout checks occur.
 *
 * @name DataStoreRowIterGetData
 * @param iterator [type:const DataStoreRowIterator*] Iterator on the current row.
 * @return row [type:const void*] Borrowed read-only row start, or zero without whole-row access.
 */
static inline const void* DataStoreRowIterGetData(const DataStoreRowIterator* iterator);

/*# Borrow writable native row bytes
 *
 * Uses the layout and lifetime rules of DataStoreRowIterGetData. Returns zero unless
 * the query flags include DATASTORE_ROW_MUTABLE, which reserves reads and writes of the entire row.
 * Never overwrite reference slots or whole rows containing references.
 * Boolean writes must be zero or one. The access flag is cached in the iterator;
 * this getter does not access the query or store.
 *
 * @name DataStoreRowIterGetDataMut
 * @param iterator [type:const DataStoreRowIterator*] Iterator on the current row.
 * @return row [type:void*] Borrowed row start for fixed-size field writes, or zero without DATASTORE_ROW_MUTABLE.
 */
static inline void* DataStoreRowIterGetDataMut(const DataStoreRowIterator* iterator);

/*# Borrow a read-only Number field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataStoreQueryFindField].
 *
 * @name DataStoreRowIterGetNumber
 * @param iterator [type:const DataStoreRowIterator*] Iterator on the current row.
 * @param field [type:DataStoreFieldId] Number handle from DataStoreQueryFindField.
 * @return value [type:const double*] Borrowed read-only value.
 */
static inline const double* DataStoreRowIterGetNumber(const DataStoreRowIterator* iterator, DataStoreFieldId field);

/*# Borrow a writable Number field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataStoreQueryFindField].
 *
 * @name DataStoreRowIterGetNumberMut
 * @param iterator [type:const DataStoreRowIterator*] Iterator on the current row.
 * @param field [type:DataStoreFieldId] Number handle from DataStoreQueryFindField.
 * @return value [type:double*] Borrowed writable value.
 */
static inline double* DataStoreRowIterGetNumberMut(const DataStoreRowIterator* iterator, DataStoreFieldId field);

/*# Borrow a read-only Float field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataStoreQueryFindField].
 *
 * @name DataStoreRowIterGetFloat
 * @param iterator [type:const DataStoreRowIterator*] Iterator on the current row.
 * @param field [type:DataStoreFieldId] Float handle from DataStoreQueryFindField.
 * @return value [type:const float*] Borrowed read-only value.
 */
static inline const float* DataStoreRowIterGetFloat(const DataStoreRowIterator* iterator, DataStoreFieldId field);

/*# Borrow a writable Float field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataStoreQueryFindField].
 *
 * @name DataStoreRowIterGetFloatMut
 * @param iterator [type:const DataStoreRowIterator*] Iterator on the current row.
 * @param field [type:DataStoreFieldId] Float handle from DataStoreQueryFindField.
 * @return value [type:float*] Borrowed writable value.
 */
static inline float* DataStoreRowIterGetFloatMut(const DataStoreRowIterator* iterator, DataStoreFieldId field);

/*# Borrow a read-only UInt32 field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataStoreQueryFindField].
 *
 * @name DataStoreRowIterGetUInt32
 * @param iterator [type:const DataStoreRowIterator*] Iterator on the current row.
 * @param field [type:DataStoreFieldId] UInt32 handle from DataStoreQueryFindField.
 * @return value [type:const uint32_t*] Borrowed read-only value.
 */
static inline const uint32_t* DataStoreRowIterGetUInt32(const DataStoreRowIterator* iterator, DataStoreFieldId field);

/*# Borrow a writable UInt32 field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataStoreQueryFindField].
 *
 * @name DataStoreRowIterGetUInt32Mut
 * @param iterator [type:const DataStoreRowIterator*] Iterator on the current row.
 * @param field [type:DataStoreFieldId] UInt32 handle from DataStoreQueryFindField.
 * @return value [type:uint32_t*] Borrowed writable value.
 */
static inline uint32_t* DataStoreRowIterGetUInt32Mut(const DataStoreRowIterator* iterator, DataStoreFieldId field);

/*# Borrow a read-only Boolean field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataStoreQueryFindField].
 *
 * @name DataStoreRowIterGetBoolean
 * @param iterator [type:const DataStoreRowIterator*] Iterator on the current row.
 * @param field [type:DataStoreFieldId] Boolean handle from DataStoreQueryFindField.
 * @return value [type:const uint8_t*] Borrowed read-only value.
 */
static inline const uint8_t* DataStoreRowIterGetBoolean(const DataStoreRowIterator* iterator, DataStoreFieldId field);

/*# Borrow a writable Boolean field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataStoreQueryFindField]. Write only zero or one.
 *
 * @name DataStoreRowIterGetBooleanMut
 * @param iterator [type:const DataStoreRowIterator*] Iterator on the current row.
 * @param field [type:DataStoreFieldId] Boolean handle from DataStoreQueryFindField.
 * @return value [type:uint8_t*] Borrowed writable value.
 */
static inline uint8_t* DataStoreRowIterGetBooleanMut(const DataStoreRowIterator* iterator, DataStoreFieldId field);

/*# Borrow a read-only Vector3 field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataStoreQueryFindField].
 *
 * @name DataStoreRowIterGetVector3
 * @param iterator [type:const DataStoreRowIterator*] Iterator on the current row.
 * @param field [type:DataStoreFieldId] Vector3 handle from DataStoreQueryFindField.
 * @return value [type:const DataStoreVector3*] Borrowed read-only value.
 */
static inline const DataStoreVector3* DataStoreRowIterGetVector3(const DataStoreRowIterator* iterator, DataStoreFieldId field);

/*# Borrow a writable Vector3 field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataStoreQueryFindField].
 *
 * @name DataStoreRowIterGetVector3Mut
 * @param iterator [type:const DataStoreRowIterator*] Iterator on the current row.
 * @param field [type:DataStoreFieldId] Vector3 handle from DataStoreQueryFindField.
 * @return value [type:DataStoreVector3*] Borrowed writable value.
 */
static inline DataStoreVector3* DataStoreRowIterGetVector3Mut(const DataStoreRowIterator* iterator, DataStoreFieldId field);

/*# Borrow a read-only Vector4 field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataStoreQueryFindField].
 *
 * @name DataStoreRowIterGetVector4
 * @param iterator [type:const DataStoreRowIterator*] Iterator on the current row.
 * @param field [type:DataStoreFieldId] Vector4 handle from DataStoreQueryFindField.
 * @return value [type:const DataStoreVector4*] Borrowed read-only value.
 */
static inline const DataStoreVector4* DataStoreRowIterGetVector4(const DataStoreRowIterator* iterator, DataStoreFieldId field);

/*# Borrow a writable Vector4 field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataStoreQueryFindField].
 *
 * @name DataStoreRowIterGetVector4Mut
 * @param iterator [type:const DataStoreRowIterator*] Iterator on the current row.
 * @param field [type:DataStoreFieldId] Vector4 handle from DataStoreQueryFindField.
 * @return value [type:DataStoreVector4*] Borrowed writable value.
 */
static inline DataStoreVector4* DataStoreRowIterGetVector4Mut(const DataStoreRowIterator* iterator, DataStoreFieldId field);

/*# Borrow a read-only Matrix4 field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataStoreQueryFindField].
 *
 * @name DataStoreRowIterGetMatrix4
 * @param iterator [type:const DataStoreRowIterator*] Iterator on the current row.
 * @param field [type:DataStoreFieldId] Matrix4 handle from DataStoreQueryFindField.
 * @return value [type:const DataStoreMatrix4*] Borrowed read-only value.
 */
static inline const DataStoreMatrix4* DataStoreRowIterGetMatrix4(const DataStoreRowIterator* iterator, DataStoreFieldId field);

/*# Borrow a writable Matrix4 field
 *
 * Uses the handle and pointer lifetime rules of [ref:DataStoreQueryFindField].
 *
 * @name DataStoreRowIterGetMatrix4Mut
 * @param iterator [type:const DataStoreRowIterator*] Iterator on the current row.
 * @param field [type:DataStoreFieldId] Matrix4 handle from DataStoreQueryFindField.
 * @return value [type:DataStoreMatrix4*] Borrowed writable value.
 */
static inline DataStoreMatrix4* DataStoreRowIterGetMatrix4Mut(const DataStoreRowIterator* iterator, DataStoreFieldId field);

/*# Read a Number field
 *
 * ID-based reads acquire no lock or query reservation. Prevent structural changes
 * and conflicting writes with a matching query reservation or caller synchronization.
 * Keep that protection while using borrowed strings or views.
 *
 * @name DataStoreFieldGetNumber
 * @param store [type:HDataStoreContext] Store handle.
 * @param id [type:DataStoreId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param out_value [type:double*] Receives the value on success.
 * @return result [type:DataStoreResult] OK, NOT_FOUND for an absent row/field, or INVALID_ARGUMENT for a kind mismatch.
 */
DataStoreResult DataStoreFieldGetNumber(HDataStoreContext store, DataStoreId id, uint64_t field, double* out_value);

/*# Read Number fields by row ID
 *
 * Reads IDs in input order without allocation. Stops at the first error; outputs
 * may be partially written on failure; use them only after OK. Synchronize as
 * for DataStoreFieldGetNumber. Input/output arrays must not overlap and must contain
 * count entries. Zero count accesses nothing.
 *
 * @name DataStoreFieldGetNumberBatch
 * @param store [type:HDataStoreContext] Store handle.
 * @param count [type:uint32_t] Number of IDs and output values.
 * @param ids [type:const DataStoreId*] Row IDs in requested order; duplicates are allowed.
 * @param field [type:uint64_t] Shared field name hash.
 * @param out_values [type:double*] Receives the values on success.
 * @return result [type:DataStoreResult] OK (including zero count), or the first NOT_FOUND for an absent row/field or INVALID_ARGUMENT for a kind mismatch.
 */
DataStoreResult DataStoreFieldGetNumberBatch(HDataStoreContext store, uint32_t count, const DataStoreId* ids, uint64_t field, double* out_values);

/*# Write a Number field
 *
 * @name DataStoreFieldSetNumber
 * @param store [type:HDataStoreContext] Store handle.
 * @param id [type:DataStoreId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param value [type:double] Replacement value.
 * @return result [type:DataStoreResult] OK, NOT_FOUND for an absent row/field, INVALID_ARGUMENT for an invalid value/kind, or LOCKED during query reservations.
 */
DataStoreResult DataStoreFieldSetNumber(HDataStoreContext store, DataStoreId id, uint64_t field, double value);

/*# Read a Float field
 *
 * Uses the ID-read synchronization requirements of [ref:DataStoreFieldGetNumber].
 *
 * @name DataStoreFieldGetFloat
 * @param store [type:HDataStoreContext] Store handle.
 * @param id [type:DataStoreId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param out_value [type:float*] Receives the value on success.
 * @return result [type:DataStoreResult] OK, NOT_FOUND for an absent row/field, or INVALID_ARGUMENT for a kind mismatch.
 */
DataStoreResult DataStoreFieldGetFloat(HDataStoreContext store, DataStoreId id, uint64_t field, float* out_value);

/*# Read Float fields by row ID
 *
 * Reads IDs in input order without allocation. Stops at the first error; outputs
 * may be partially written on failure; use them only after OK. Synchronize as
 * for DataStoreFieldGetFloat. Input/output arrays must not overlap and must contain
 * count entries. Zero count accesses nothing.
 *
 * @name DataStoreFieldGetFloatBatch
 * @param store [type:HDataStoreContext] Store handle.
 * @param count [type:uint32_t] Number of IDs and output values.
 * @param ids [type:const DataStoreId*] Row IDs in requested order; duplicates are allowed.
 * @param field [type:uint64_t] Shared field name hash.
 * @param out_values [type:float*] Receives the values on success.
 * @return result [type:DataStoreResult] OK (including zero count), or the first NOT_FOUND for an absent row/field or INVALID_ARGUMENT for a kind mismatch.
 */
DataStoreResult DataStoreFieldGetFloatBatch(HDataStoreContext store, uint32_t count, const DataStoreId* ids, uint64_t field, float* out_values);

/*# Write a Float field
 *
 * @name DataStoreFieldSetFloat
 * @param store [type:HDataStoreContext] Store handle.
 * @param id [type:DataStoreId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param value [type:float] Replacement value.
 * @return result [type:DataStoreResult] OK, NOT_FOUND for an absent row/field, INVALID_ARGUMENT for an invalid value/kind, or LOCKED during query reservations.
 */
DataStoreResult DataStoreFieldSetFloat(HDataStoreContext store, DataStoreId id, uint64_t field, float value);

/*# Read a UInt32 field
 *
 * Uses the ID-read synchronization requirements of [ref:DataStoreFieldGetNumber].
 *
 * @name DataStoreFieldGetUInt32
 * @param store [type:HDataStoreContext] Store handle.
 * @param id [type:DataStoreId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param out_value [type:uint32_t*] Receives the value on success.
 * @return result [type:DataStoreResult] OK, NOT_FOUND for an absent row/field, or INVALID_ARGUMENT for a kind mismatch.
 */
DataStoreResult DataStoreFieldGetUInt32(HDataStoreContext store, DataStoreId id, uint64_t field, uint32_t* out_value);

/*# Read UInt32 fields by row ID
 *
 * Reads IDs in input order without allocation. Stops at the first error; outputs
 * may be partially written on failure; use them only after OK. Synchronize as
 * for DataStoreFieldGetUInt32. Input/output arrays must not overlap and must contain
 * count entries. Zero count accesses nothing.
 *
 * @name DataStoreFieldGetUInt32Batch
 * @param store [type:HDataStoreContext] Store handle.
 * @param count [type:uint32_t] Number of IDs and output values.
 * @param ids [type:const DataStoreId*] Row IDs in requested order; duplicates are allowed.
 * @param field [type:uint64_t] Shared field name hash.
 * @param out_values [type:uint32_t*] Receives the values on success.
 * @return result [type:DataStoreResult] OK (including zero count), or the first NOT_FOUND for an absent row/field or INVALID_ARGUMENT for a kind mismatch.
 */
DataStoreResult DataStoreFieldGetUInt32Batch(HDataStoreContext store, uint32_t count, const DataStoreId* ids, uint64_t field, uint32_t* out_values);

/*# Write a UInt32 field
 *
 * @name DataStoreFieldSetUInt32
 * @param store [type:HDataStoreContext] Store handle.
 * @param id [type:DataStoreId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param value [type:uint32_t] Replacement value.
 * @return result [type:DataStoreResult] OK, NOT_FOUND for an absent row/field, INVALID_ARGUMENT for an invalid value/kind, or LOCKED during query reservations.
 */
DataStoreResult DataStoreFieldSetUInt32(HDataStoreContext store, DataStoreId id, uint64_t field, uint32_t value);

/*# Read a Boolean field
 *
 * Uses the ID-read synchronization requirements of [ref:DataStoreFieldGetNumber].
 *
 * @name DataStoreFieldGetBoolean
 * @param store [type:HDataStoreContext] Store handle.
 * @param id [type:DataStoreId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param out_value [type:uint8_t*] Receives the value on success.
 * @return result [type:DataStoreResult] OK, NOT_FOUND for an absent row/field, or INVALID_ARGUMENT for a kind mismatch.
 */
DataStoreResult DataStoreFieldGetBoolean(HDataStoreContext store, DataStoreId id, uint64_t field, uint8_t* out_value);

/*# Read Boolean fields by row ID
 *
 * Reads IDs in input order without allocation. Stops at the first error; outputs
 * may be partially written on failure; use them only after OK. Synchronize as
 * for DataStoreFieldGetBoolean. Input/output arrays must not overlap and must contain
 * count entries. Zero count accesses nothing.
 *
 * @name DataStoreFieldGetBooleanBatch
 * @param store [type:HDataStoreContext] Store handle.
 * @param count [type:uint32_t] Number of IDs and output values.
 * @param ids [type:const DataStoreId*] Row IDs in requested order; duplicates are allowed.
 * @param field [type:uint64_t] Shared field name hash.
 * @param out_values [type:uint8_t*] Receives the values on success.
 * @return result [type:DataStoreResult] OK (including zero count), or the first NOT_FOUND for an absent row/field or INVALID_ARGUMENT for a kind mismatch.
 */
DataStoreResult DataStoreFieldGetBooleanBatch(HDataStoreContext store, uint32_t count, const DataStoreId* ids, uint64_t field, uint8_t* out_values);

/*# Write a Boolean field
 *
 * @name DataStoreFieldSetBoolean
 * @param store [type:HDataStoreContext] Store handle.
 * @param id [type:DataStoreId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param value [type:uint8_t] Zero or one.
 * @return result [type:DataStoreResult] OK, NOT_FOUND for an absent row/field, INVALID_ARGUMENT for an invalid value/kind, or LOCKED during query reservations.
 */
DataStoreResult DataStoreFieldSetBoolean(HDataStoreContext store, DataStoreId id, uint64_t field, uint8_t value);

/*# Read a String field
 *
 * Uses the ID-read synchronization requirements of [ref:DataStoreFieldGetNumber].
 *
 * @name DataStoreFieldGetString
 * @param store [type:HDataStoreContext] Store handle.
 * @param id [type:DataStoreId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param out_value [type:const char**] Borrowed string; copy before a store write, reset or iterator step.
 * @return result [type:DataStoreResult] OK, NOT_FOUND for an absent row/field, or INVALID_ARGUMENT for a kind mismatch.
 */
DataStoreResult DataStoreFieldGetString(HDataStoreContext store, DataStoreId id, uint64_t field, const char** out_value);

/*# Read String fields by row ID
 *
 * Reads IDs in input order without allocation. Stops at the first error; outputs
 * may be partially written on failure; use them only after OK. Synchronize as
 * for DataStoreFieldGetString. Input/output arrays must not overlap and must contain
 * count entries. Zero count accesses nothing.
 * Returned strings are borrowed; finish using them before a store write or reset.
 *
 * @name DataStoreFieldGetStringBatch
 * @param store [type:HDataStoreContext] Store handle.
 * @param count [type:uint32_t] Number of IDs and output values.
 * @param ids [type:const DataStoreId*] Row IDs in requested order; duplicates are allowed.
 * @param field [type:uint64_t] Shared field name hash.
 * @param out_values [type:const char**] Receives the values on success.
 * @return result [type:DataStoreResult] OK (including zero count), or the first NOT_FOUND for an absent row/field or INVALID_ARGUMENT for a kind mismatch.
 */
DataStoreResult DataStoreFieldGetStringBatch(HDataStoreContext store, uint32_t count, const DataStoreId* ids, uint64_t field, const char** out_values);

/*# Write a String field
 *
 * Copies the NUL-terminated string into owned storage; self-assignment is supported.
 *
 * @name DataStoreFieldSetString
 * @param store [type:HDataStoreContext] Store handle.
 * @param id [type:DataStoreId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param value [type:const char*] Value to copy; pointer not retained.
 * @return result [type:DataStoreResult] OK, NOT_FOUND for an absent row/field, INVALID_ARGUMENT for an invalid value/kind, or LOCKED during query reservations.
 */
DataStoreResult DataStoreFieldSetString(HDataStoreContext store, DataStoreId id, uint64_t field, const char* value);

/*# Read a Vector3 field
 *
 * Uses the ID-read synchronization requirements of [ref:DataStoreFieldGetNumber].
 *
 * @name DataStoreFieldGetVector3
 * @param store [type:HDataStoreContext] Store handle.
 * @param id [type:DataStoreId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param out_value [type:DataStoreVector3*] Receives the value on success.
 * @return result [type:DataStoreResult] OK, NOT_FOUND for an absent row/field, or INVALID_ARGUMENT for a kind mismatch.
 */
DataStoreResult DataStoreFieldGetVector3(HDataStoreContext store, DataStoreId id, uint64_t field, DataStoreVector3* out_value);

/*# Read Vector3 fields by row ID
 *
 * Reads IDs in input order without allocation. Stops at the first error; outputs
 * may be partially written on failure; use them only after OK. Synchronize as
 * for DataStoreFieldGetVector3. Input/output arrays must not overlap and must contain
 * count entries. Zero count accesses nothing.
 *
 * @name DataStoreFieldGetVector3Batch
 * @param store [type:HDataStoreContext] Store handle.
 * @param count [type:uint32_t] Number of IDs and output values.
 * @param ids [type:const DataStoreId*] Row IDs in requested order; duplicates are allowed.
 * @param field [type:uint64_t] Shared field name hash.
 * @param out_values [type:DataStoreVector3*] Receives the values on success.
 * @return result [type:DataStoreResult] OK (including zero count), or the first NOT_FOUND for an absent row/field or INVALID_ARGUMENT for a kind mismatch.
 */
DataStoreResult DataStoreFieldGetVector3Batch(HDataStoreContext store, uint32_t count, const DataStoreId* ids, uint64_t field, DataStoreVector3* out_values);

/*# Write a Vector3 field
 *
 * @name DataStoreFieldSetVector3
 * @param store [type:HDataStoreContext] Store handle.
 * @param id [type:DataStoreId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param value [type:const DataStoreVector3*] Value to copy; pointer not retained.
 * @return result [type:DataStoreResult] OK, NOT_FOUND for an absent row/field, INVALID_ARGUMENT for an invalid value/kind, or LOCKED during query reservations.
 */
DataStoreResult DataStoreFieldSetVector3(HDataStoreContext store, DataStoreId id, uint64_t field, const DataStoreVector3* value);

/*# Read a Vector4 field
 *
 * Uses the ID-read synchronization requirements of [ref:DataStoreFieldGetNumber].
 *
 * @name DataStoreFieldGetVector4
 * @param store [type:HDataStoreContext] Store handle.
 * @param id [type:DataStoreId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param out_value [type:DataStoreVector4*] Receives the value on success.
 * @return result [type:DataStoreResult] OK, NOT_FOUND for an absent row/field, or INVALID_ARGUMENT for a kind mismatch.
 */
DataStoreResult DataStoreFieldGetVector4(HDataStoreContext store, DataStoreId id, uint64_t field, DataStoreVector4* out_value);

/*# Read Vector4 fields by row ID
 *
 * Reads IDs in input order without allocation. Stops at the first error; outputs
 * may be partially written on failure; use them only after OK. Synchronize as
 * for DataStoreFieldGetVector4. Input/output arrays must not overlap and must contain
 * count entries. Zero count accesses nothing.
 *
 * @name DataStoreFieldGetVector4Batch
 * @param store [type:HDataStoreContext] Store handle.
 * @param count [type:uint32_t] Number of IDs and output values.
 * @param ids [type:const DataStoreId*] Row IDs in requested order; duplicates are allowed.
 * @param field [type:uint64_t] Shared field name hash.
 * @param out_values [type:DataStoreVector4*] Receives the values on success.
 * @return result [type:DataStoreResult] OK (including zero count), or the first NOT_FOUND for an absent row/field or INVALID_ARGUMENT for a kind mismatch.
 */
DataStoreResult DataStoreFieldGetVector4Batch(HDataStoreContext store, uint32_t count, const DataStoreId* ids, uint64_t field, DataStoreVector4* out_values);

/*# Write a Vector4 field
 *
 * @name DataStoreFieldSetVector4
 * @param store [type:HDataStoreContext] Store handle.
 * @param id [type:DataStoreId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param value [type:const DataStoreVector4*] Value to copy; pointer not retained.
 * @return result [type:DataStoreResult] OK, NOT_FOUND for an absent row/field, INVALID_ARGUMENT for an invalid value/kind, or LOCKED during query reservations.
 */
DataStoreResult DataStoreFieldSetVector4(HDataStoreContext store, DataStoreId id, uint64_t field, const DataStoreVector4* value);

/*# Read a Matrix4 field
 *
 * Uses the ID-read synchronization requirements of [ref:DataStoreFieldGetNumber].
 *
 * @name DataStoreFieldGetMatrix4
 * @param store [type:HDataStoreContext] Store handle.
 * @param id [type:DataStoreId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param out_value [type:DataStoreMatrix4*] Receives the value on success.
 * @return result [type:DataStoreResult] OK, NOT_FOUND for an absent row/field, or INVALID_ARGUMENT for a kind mismatch.
 */
DataStoreResult DataStoreFieldGetMatrix4(HDataStoreContext store, DataStoreId id, uint64_t field, DataStoreMatrix4* out_value);

/*# Read Matrix4 fields by row ID
 *
 * Reads IDs in input order without allocation. Stops at the first error; outputs
 * may be partially written on failure; use them only after OK. Synchronize as
 * for DataStoreFieldGetMatrix4. Input/output arrays must not overlap and must contain
 * count entries. Zero count accesses nothing.
 *
 * @name DataStoreFieldGetMatrix4Batch
 * @param store [type:HDataStoreContext] Store handle.
 * @param count [type:uint32_t] Number of IDs and output values.
 * @param ids [type:const DataStoreId*] Row IDs in requested order; duplicates are allowed.
 * @param field [type:uint64_t] Shared field name hash.
 * @param out_values [type:DataStoreMatrix4*] Receives the values on success.
 * @return result [type:DataStoreResult] OK (including zero count), or the first NOT_FOUND for an absent row/field or INVALID_ARGUMENT for a kind mismatch.
 */
DataStoreResult DataStoreFieldGetMatrix4Batch(HDataStoreContext store, uint32_t count, const DataStoreId* ids, uint64_t field, DataStoreMatrix4* out_values);

/*# Write a Matrix4 field
 *
 * @name DataStoreFieldSetMatrix4
 * @param store [type:HDataStoreContext] Store handle.
 * @param id [type:DataStoreId] Row ID.
 * @param field [type:uint64_t] Field name hash.
 * @param value [type:const DataStoreMatrix4*] Value to copy; pointer not retained.
 * @return result [type:DataStoreResult] OK, NOT_FOUND for an absent row/field, INVALID_ARGUMENT for an invalid value/kind, or LOCKED during query reservations.
 */
DataStoreResult DataStoreFieldSetMatrix4(HDataStoreContext store, DataStoreId id, uint64_t field, const DataStoreMatrix4* value);

/*# Read a field as a view
 *
 * Uses the ID-read synchronization requirements of [ref:DataStoreFieldGetNumber].
 *
 * @name DataStoreFieldGetView
 * @param store [type:HDataStoreContext] Store handle.
 * @param id [type:DataStoreId] Row ID.
 * @param field [type:uint64_t] Full field-name hash.
 * @param out_view [type:DataStoreValueView*] Receives a borrowed view on success.
 * @return result [type:DataStoreResult] OK, or NOT_FOUND for an absent row/field.
 */
DataStoreResult DataStoreFieldGetView(HDataStoreContext store, DataStoreId id, uint64_t field, DataStoreValueView* out_view);

/*# Read a bound query field as a view
 *
 * Uses the same valid-row and matching-handle contract as typed pointer getters.
 * Containers and strings require read-only access. No name lookup or allocation.
 *
 * @name DataStoreRowIterGetView
 * @param iterator [type:const DataStoreRowIterator*] Current row of a reserved query.
 * @param field [type:DataStoreFieldId] Handle returned by DataStoreQueryFindField.
 * @param out_view [type:DataStoreValueView*] Receives a borrowed view.
 */
void DataStoreRowIterGetView(const DataStoreRowIterator* iterator, DataStoreFieldId field, DataStoreValueView* out_view);

/*# Read a view's kind
 *
 * @name DataStoreViewGetType
 * @param view [type:const DataStoreValueView*] Valid view.
 * @return type [type:DataStoreValueType] Value kind, including NULL for a null element.
 */
DataStoreValueType DataStoreViewGetType(const DataStoreValueView* view);

/*# Count container entries
 *
 * @name DataStoreViewGetCount
 * @param view [type:const DataStoreValueView*] Struct or list view.
 * @param out_count [type:uint32_t*] Receives the immediate member/element count.
 * @return result [type:DataStoreResult] OK, or INVALID_ARGUMENT for a non-container.
 */
DataStoreResult DataStoreViewGetCount(const DataStoreValueView* view, uint32_t* out_count);

/*# Read a struct member
 *
 * Uses a local member hash; repeat for further nesting. No member enumeration.
 *
 * @name DataStoreViewGetField
 * @param view [type:const DataStoreValueView*] Struct view, including an inline struct.
 * @param field [type:uint64_t] Local member-name hash.
 * @param out_view [type:DataStoreValueView*] Receives the member view; may alias view.
 * @return result [type:DataStoreResult] OK, NOT_FOUND, or INVALID_ARGUMENT for a non-struct.
 */
DataStoreResult DataStoreViewGetField(const DataStoreValueView* view, uint64_t field, DataStoreValueView* out_view);

/*# Read a list element
 *
 * @name DataStoreViewGetElement
 * @param view [type:const DataStoreValueView*] List view.
 * @param index [type:uint32_t] Zero-based element index.
 * @param out_view [type:DataStoreValueView*] Receives the element view; may alias view.
 * @return result [type:DataStoreResult] OK, NOT_FOUND outside the list, or INVALID_ARGUMENT for a non-list.
 */
DataStoreResult DataStoreViewGetElement(const DataStoreValueView* view, uint32_t index, DataStoreValueView* out_view);

/*# Iterate container values
 *
 * Visits list elements in order or immediate struct values in storage order.
 * Struct order is unspecified. Empty containers and non-container views yield no values.
 * No allocation; count is resolved once.
 *
 * @name DataStoreViewIter
 * @param view [type:const DataStoreValueView*] Parent view; keep unchanged through iteration.
 * @return iterator [type:DataStoreViewIterator] Iterator positioned before the first value.
 */
DataStoreViewIterator DataStoreViewIter(const DataStoreValueView* view);

/*# Read the next container value
 *
 * Returned views use the existing typed view getters. Nested containers can have
 * their own iterators. Exhaustion leaves the output unchanged.
 *
 * @name DataStoreViewIterNext
 * @param iterator [type:DataStoreViewIterator*] Container iterator.
 * @param out_view [type:DataStoreValueView*] Receives the next value; must not alias the parent view.
 * @return next [type:bool] True for a value, including NULL; false at the end.
 */
bool DataStoreViewIterNext(DataStoreViewIterator* iterator, DataStoreValueView* out_view);

/*# Read a Number view
 *
 * Copies the value without conversion. Errors leave the output unchanged.
 *
 * @name DataStoreViewGetNumber
 * @param view [type:const DataStoreValueView*] Valid view.
 * @param out_value [type:double*] Receives the value on success.
 * @return result [type:DataStoreResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */
DataStoreResult DataStoreViewGetNumber(const DataStoreValueView* view, double* out_value);

/*# Read a Float view
 *
 * Copies the value without conversion. Errors leave the output unchanged.
 *
 * @name DataStoreViewGetFloat
 * @param view [type:const DataStoreValueView*] Valid view.
 * @param out_value [type:float*] Receives the value on success.
 * @return result [type:DataStoreResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */
DataStoreResult DataStoreViewGetFloat(const DataStoreValueView* view, float* out_value);

/*# Read a UInt32 view
 *
 * Copies the value without conversion. Errors leave the output unchanged.
 *
 * @name DataStoreViewGetUInt32
 * @param view [type:const DataStoreValueView*] Valid view.
 * @param out_value [type:uint32_t*] Receives the value on success.
 * @return result [type:DataStoreResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */
DataStoreResult DataStoreViewGetUInt32(const DataStoreValueView* view, uint32_t* out_value);

/*# Read a Boolean view
 *
 * Copies the value without conversion. Errors leave the output unchanged.
 *
 * @name DataStoreViewGetBoolean
 * @param view [type:const DataStoreValueView*] Valid view.
 * @param out_value [type:uint8_t*] Receives the value on success.
 * @return result [type:DataStoreResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */
DataStoreResult DataStoreViewGetBoolean(const DataStoreValueView* view, uint8_t* out_value);

/*# Read a String view
 *
 * Borrows the NUL-terminated string until the view expires. Errors leave the output unchanged.
 *
 * @name DataStoreViewGetString
 * @param view [type:const DataStoreValueView*] Valid view.
 * @param out_value [type:const char**] Receives the value on success.
 * @return result [type:DataStoreResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */
DataStoreResult DataStoreViewGetString(const DataStoreValueView* view, const char** out_value);

/*# Read a Vector3 view
 *
 * Copies the value without conversion. Errors leave the output unchanged.
 *
 * @name DataStoreViewGetVector3
 * @param view [type:const DataStoreValueView*] Valid view.
 * @param out_value [type:DataStoreVector3*] Receives the value on success.
 * @return result [type:DataStoreResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */
DataStoreResult DataStoreViewGetVector3(const DataStoreValueView* view, DataStoreVector3* out_value);

/*# Read a Vector4 view
 *
 * Copies the value without conversion. Errors leave the output unchanged.
 *
 * @name DataStoreViewGetVector4
 * @param view [type:const DataStoreValueView*] Valid view.
 * @param out_value [type:DataStoreVector4*] Receives the value on success.
 * @return result [type:DataStoreResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */
DataStoreResult DataStoreViewGetVector4(const DataStoreValueView* view, DataStoreVector4* out_value);

/*# Read a Matrix4 view
 *
 * Copies the value without conversion. Errors leave the output unchanged.
 *
 * @name DataStoreViewGetMatrix4
 * @param view [type:const DataStoreValueView*] Valid view.
 * @param out_value [type:DataStoreMatrix4*] Receives the value on success.
 * @return result [type:DataStoreResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */
DataStoreResult DataStoreViewGetMatrix4(const DataStoreValueView* view, DataStoreMatrix4* out_value);

// Inline implementations

// private
// Address calculation for the public pointer wrappers. Validated field handles and current
// row indices are caller obligations; no repeated checks occur here.
static inline void* DataStoreFieldGetPointerInternal(const DataStoreIterator* batch, uint32_t row, DataStoreFieldId field);

static inline DataStoreRowIterator DataStoreIterRows(const DataStoreIterator* iterator)
{
    DataStoreRowIterator rows;
    rows.m_Parent = iterator;
    rows.m_Index = UINT32_MAX;
    rows.m_NextRow = 0;
    return rows;
}

static inline bool DataStoreRowIterNext(DataStoreRowIterator* iterator)
{
    if (iterator->m_NextRow >= iterator->m_Parent->m_Count)
    {
        iterator->m_Index = UINT32_MAX;
        return false;
    }
    iterator->m_Index = iterator->m_NextRow++;
    return true;
}

static inline const void* DataStoreRowIterGetData(const DataStoreRowIterator* iterator)
{
    if (!(iterator->m_Parent->m_Flags & DATASTORE_ROW_MUTABLE))
    {
        return 0;
    }
    return iterator->m_Parent->m_Values + (size_t)iterator->m_Index * iterator->m_Parent->m_RowStride;
}

static inline void* DataStoreRowIterGetDataMut(const DataStoreRowIterator* iterator)
{
    if (!(iterator->m_Parent->m_Flags & DATASTORE_ROW_MUTABLE))
    {
        return 0;
    }
    return iterator->m_Parent->m_Values + (size_t)iterator->m_Index * iterator->m_Parent->m_RowStride;
}

static inline void* DataStoreFieldGetPointerInternal(const DataStoreIterator* batch, uint32_t row, DataStoreFieldId field)
{
    return batch->m_Values + (size_t)row * batch->m_RowStride + batch->m_FieldOffsets[field];
}

static inline const double* DataStoreRowIterGetNumber(const DataStoreRowIterator* iterator, DataStoreFieldId field)
{
    return (const double*)DataStoreFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
}

static inline double* DataStoreRowIterGetNumberMut(const DataStoreRowIterator* iterator, DataStoreFieldId field)
{
    return (double*)DataStoreFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
}

static inline const float* DataStoreRowIterGetFloat(const DataStoreRowIterator* iterator, DataStoreFieldId field)
{
    return (const float*)DataStoreFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
}

static inline float* DataStoreRowIterGetFloatMut(const DataStoreRowIterator* iterator, DataStoreFieldId field)
{
    return (float*)DataStoreFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
}

static inline const uint32_t* DataStoreRowIterGetUInt32(const DataStoreRowIterator* iterator, DataStoreFieldId field)
{
    return (const uint32_t*)DataStoreFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
}

static inline uint32_t* DataStoreRowIterGetUInt32Mut(const DataStoreRowIterator* iterator, DataStoreFieldId field)
{
    return (uint32_t*)DataStoreFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
}

static inline const uint8_t* DataStoreRowIterGetBoolean(const DataStoreRowIterator* iterator, DataStoreFieldId field)
{
    return (const uint8_t*)DataStoreFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
}

static inline uint8_t* DataStoreRowIterGetBooleanMut(const DataStoreRowIterator* iterator, DataStoreFieldId field)
{
    return (uint8_t*)DataStoreFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
}

static inline const DataStoreVector3* DataStoreRowIterGetVector3(const DataStoreRowIterator* iterator, DataStoreFieldId field)
{
    return (const DataStoreVector3*)DataStoreFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
}

static inline DataStoreVector3* DataStoreRowIterGetVector3Mut(const DataStoreRowIterator* iterator, DataStoreFieldId field)
{
    return (DataStoreVector3*)DataStoreFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
}

static inline const DataStoreVector4* DataStoreRowIterGetVector4(const DataStoreRowIterator* iterator, DataStoreFieldId field)
{
    return (const DataStoreVector4*)DataStoreFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
}

static inline DataStoreVector4* DataStoreRowIterGetVector4Mut(const DataStoreRowIterator* iterator, DataStoreFieldId field)
{
    return (DataStoreVector4*)DataStoreFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
}

static inline const DataStoreMatrix4* DataStoreRowIterGetMatrix4(const DataStoreRowIterator* iterator, DataStoreFieldId field)
{
    return (const DataStoreMatrix4*)DataStoreFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
}

static inline DataStoreMatrix4* DataStoreRowIterGetMatrix4Mut(const DataStoreRowIterator* iterator, DataStoreFieldId field)
{
    return (DataStoreMatrix4*)DataStoreFieldGetPointerInternal(iterator->m_Parent, iterator->m_Index, field);
}

#ifdef __cplusplus
}
#endif

#endif // DMSDK_DATASTORE_H
