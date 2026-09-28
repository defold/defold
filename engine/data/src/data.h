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

#ifndef DM_DATA_H
#define DM_DATA_H

#include <dlib/array.h>
#include <dmsdk/data/data.h>

// Internal construction and serialization values. Public callers use typed accessors.
/** Struct value
 *
 * A read-only view of named fields. Read fields using [ref:DataGetStructProperty];
 * returned views may reference packed data directly. To supply a decoded struct to
 * set, zero-initialize this descriptor and provide separate name and value arrays.
 * Names must be unique. The arrays are copied when the struct is set.
 *
 * @struct
 * @name DataStruct
 * @member m_Names [type:const uint64_t*] Input field name hashes; NULL is allowed for zero fields or packed output views.
 * @member m_Values [type:const DataValue*] Input values in name order; NULL is allowed for zero fields or packed output views.
 * @member m_Count [type:uint32_t] Number of fields.
 */
typedef struct DataStruct
{
    const uint64_t*         m_Names;
    const struct DataValue* m_Values;
    uint32_t                m_Count;
    // private
    const uint8_t*          m_Buffer;
    uint32_t                m_Offset;
    const struct DataTable* m_Table; // Non-NULL for inline views: m_Buffer is row bytes, m_Offset is the first child metadata index.
} DataStruct;

/** List value
 *
 * A read-only view of ordered values, which may have different kinds. Read elements
 * using [ref:DataGetListValue]; returned views may reference packed data directly.
 * To supply a decoded list to set, zero-initialize this descriptor and provide
 * m_Values and m_Count. The elements are copied when the list is set.
 *
 * @struct
 * @name DataList
 * @member m_Values [type:const DataValue*] Input elements; NULL is allowed for zero elements or packed output views.
 * @member m_Count [type:uint32_t] Number of elements.
 */
typedef struct DataList
{
    const struct DataValue* m_Values;
    uint32_t                m_Count;
    // private
    const uint8_t* m_Buffer;
    uint32_t       m_Offset;
} DataList;

/** Value payload
 *
 * API input/output payload selected by [ref:DataValue] m_Type. This union is not
 * the row storage format: rows contain only the bytes required by each field.
 * Vector and matrix components use float32; Number remains double precision.
 *
 * @typedef
 * @name DataValueData
 * @member m_Number [type:double] Payload for DATA_VALUE_TYPE_NUMBER.
 * @member m_Boolean [type:uint8_t] Payload for DATA_VALUE_TYPE_BOOLEAN; zero or one.
 * @member m_String [type:const char*] NUL-terminated string for DATA_VALUE_TYPE_STRING.
 * @member m_Struct [type:DataStruct] Struct input or borrowed view for DATA_VALUE_TYPE_STRUCT.
 * @member m_List [type:DataList] List input or borrowed view for DATA_VALUE_TYPE_LIST.
 * @member m_Vector3 [type:float[3]] X, Y, Z components for DATA_VALUE_TYPE_VECTOR3.
 * @member m_Vector4 [type:float[4]] X, Y, Z, W components for DATA_VALUE_TYPE_VECTOR4.
 * @member m_Matrix4 [type:float[16]] Column-major matrix for DATA_VALUE_TYPE_MATRIX4; element at row r, column c is m_Matrix4[c * 4 + r].
 */
typedef union DataValueData
{
    double      m_Number;
    uint8_t     m_Boolean;
    const char* m_String;
    DataStruct  m_Struct;
    DataList    m_List;
    float       m_Vector3[3];
    float       m_Vector4[4];
    float       m_Matrix4[16];
} DataValueData;

/** Tagged API value
 *
 * Carries a value into or out of the API. Tags and property names are not repeated
 * in stored rows: the table's metadata determines their layout. Set requires the
 * declared property kind. Structs/lists support up to 64 container levels; cyclic
 * decoded input is rejected. Returned strings and container views are borrowed,
 * read-only, and must be copied before the next store write, reset or iterator step
 * if longer retention is needed. Getting a packed container does not expand its tree.
 *
 * @struct
 * @name DataValue
 * @member m_Type [type:DataValueType] Value kind, selecting the active union member.
 * @member m_Value [type:DataValueData] Value payload.
 */
typedef struct DataValue
{
    DataValueType m_Type;
    DataValueData m_Value;
} DataValue;

// Engine-owned packed resource and one registration of its component rows.
typedef struct DataBlob*         HDataBlob;
typedef struct DataBlobInstance* HDataBlobInstance;

struct DataStructDesc;

/** Construction field. Offsets are bytes relative to the containing row/struct.
 * Offsets must respect field alignment: Boolean/Null 1, float math values 4,
 * Number/references 8, inline structs the maximum alignment of their members.
 * m_Struct declares a fixed inline STRUCT layout; NULL keeps dynamic container storage.
 * Registration validates and copies the complete description. The same struct
 * description can be reused in multiple parent types and need not outlive registration.
 */
struct DataPropertyDesc
{
    uint64_t              m_Property;
    DataValueType         m_Type;
    uint32_t              m_Offset;
    const DataStructDesc* m_Struct;
};

/** Reusable fixed composition layout. All rows have these members and kinds.
 * Names must be unique; nonempty fields must not overlap or exceed m_Size.
 * m_Size includes trailing padding to the maximum member alignment.
 * Members may include fixed structs, strings and dynamic containers. Cycles and
 * nesting deeper than DATA_MAX_NESTING are rejected during table registration.
 */
struct DataStructDesc
{
    const DataPropertyDesc* m_Properties;
    uint32_t                m_PropertyCount;
    uint32_t                m_Size;
};

// Compiled field layout, shared by every row of a table type. Fixed structs point
// into the same metadata array by index; their member offsets are relative to the
// struct's bytes. Root fields occupy the first m_PropertyCount entries. A zero
// m_ChildIndex denotes an ordinary scalar/reference, including a dynamic STRUCT.
struct DataPropertyMeta
{
    uint64_t      m_Property;
    DataValueType m_Type;
    uint32_t      m_Offset;
    uint32_t      m_ChildIndex;
    uint32_t      m_ChildCount;
    uint32_t      m_Size;
};

static const uint32_t DATA_TABLE_HEADER_SIZE = 32;
static const uint32_t DATA_PROPERTY_META_SIZE = 32;

/** Construction table description. Registration copies the tags and metadata.
 * Row stride includes trailing padding to the maximum field alignment and must
 * contain every field. Invalid alignment is rejected at registration. All inserted
 * rows use these same field kinds and offsets. Zero fields/stride are allowed.
 */
struct DataTableDesc
{
    uint64_t                m_Type;
    const uint64_t*         m_Tags;
    uint32_t                m_TagCount;
    const DataPropertyDesc* m_Properties;
    uint32_t                m_PropertyCount;
    uint32_t                m_RowStride;
};

/** Decoded construction input, copied by DataAddRows as reset defaults.
 * Values follow metadata order and must match its count and kinds. Input arrays
 * and their referenced strings/containers only need to live through insertion.
 * Component IDs identify components within a prototype; owners identify instances.
 */
struct DataRowDesc
{
    DataOwnerId      m_Owner;
    const DataValue* m_Values;
    uint32_t         m_ValueCount;
    uint64_t         m_ComponentId;
};

// Row identity, source row and pooled registration indices. Mutable values follow
// the current row order; swap removal moves these bytes and preserves registration membership.
struct DataRow
{
    DataOwnerId m_Owner;
    union
    {
        uint64_t m_ComponentId;   // Independently added rows.
        uint32_t m_InstanceIndex; // Packed rows: registration slot in the resource pool.
    };
    uint32_t m_Slot;
    uint32_t m_BaseIndex;
};

static const uint32_t DATA_MAX_NESTING = 64;
struct DataBlock;
struct DataBlobPool;

// Only decoded tables own metadata/default storage. Packed tables borrow it.
struct DataOwnedTable
{
    dmArray<uint64_t>         m_Tags;
    dmArray<DataPropertyMeta> m_Properties;
    dmArray<uint8_t>          m_BaseRows;
    DataBlock*                m_BaseValues;
    uint32_t                  m_BaseCount;
};

struct DataTable
{
    uint64_t         m_Type;
    uint32_t         m_StoreIndex;
    uint32_t         m_TagCount;
    uint32_t         m_PropertyCount;
    uint32_t         m_MetadataCount;
    uint32_t         m_RowStride;
    dmArray<DataRow> m_Rows;
    dmArray<uint8_t> m_Values; // Mutable fixed-stride rows, in the same order as m_Rows.
    DataBlobPool*    m_Pool;
    const uint8_t*   m_Blob;
    union
    {
        DataOwnedTable* m_Owned; // Present only when m_Pool is NULL.
        struct
        {
            uint32_t m_Table;
            uint32_t m_Rows;
            uint32_t m_FirstSlot; // First source row in a registration's slot array.
        } m_Offsets;              // Relative to m_Blob; no pointer fixup of file bytes.
    };
    DataBlock* m_Payloads; // Replacements for independently added rows; packed replacements belong to their registration.
};

struct DataSlot
{
    DataTable* m_Table;
    uint32_t   m_Generation;
    union
    {
        uint32_t m_Row;
        uint32_t m_NextFree; // Shares the row index when m_Table is NULL.
    };
};

struct DataStore
{
    dmArray<DataTable*> m_Tables;
    dmArray<DataSlot>   m_Slots;
    dmArray<HDataQuery> m_Queries;
    uint32_t            m_FreeSlot;
    uint32_t            m_LockCount;
    DataBlobPool*       m_Pools;
};

struct DataBlob
{
    const uint8_t* m_Data;
    uint32_t       m_TableCount;
    uint32_t       m_RowCount;
    uint32_t       m_RefCount;
};

// Shared runtime tables for one loaded resource in this store. The first
// registration page and initial table buffers follow this header in one allocation.
// Later registrations reuse slots; dense row arrays grow geometrically.
struct DataBlobPool
{
    HDataStore        m_Store;
    HDataBlob         m_Blob;
    DataBlobPool*     m_Next;
    dmArray<uint8_t*> m_Pages; // Additional registration pages; never moved.
    uint8_t*          m_FirstPage;
    uint64_t          m_AllocationSize;
    uint32_t          m_InstanceStride;
    uint32_t          m_PageShift;
    uint32_t          m_Issued;
    uint32_t          m_Live;
    uint32_t          m_FreeInstance;
};

// Stable pooled handle. A uint32_t slot index for each source row follows it;
// removed rows use UINT32_MAX. Only replacement payloads belong to this instance.
struct DataBlobInstance
{
    DataBlobPool* m_Pool;
    DataBlock*    m_Payloads;
    uint32_t      m_Index;
    uint32_t      m_NextFree;
};

static inline DataTable* GetPoolTable(DataBlobPool* pool, uint32_t index)
{
    return (DataTable*)(pool + 1) + index;
}

static inline DataTable* GetInstanceTable(HDataBlobInstance instance, uint32_t index)
{
    return GetPoolTable(instance->m_Pool, index);
}

static inline HDataBlobInstance GetPoolInstance(DataBlobPool* pool, uint32_t index)
{
    uint32_t page = index >> pool->m_PageShift;
    uint8_t* bytes = page ? pool->m_Pages[page - 1] : pool->m_FirstPage;
    return (HDataBlobInstance)(bytes + (size_t)(index & ((1u << pool->m_PageShift) - 1)) * pool->m_InstanceStride);
}

static inline uint32_t* GetInstanceSlots(HDataBlobInstance instance)
{
    return (uint32_t*)(instance + 1);
}

// Snapshot for internal profiling. Arena used bytes include alignment padding;
// capacity excludes allocation headers. Sampling walks tables, rows and blocks.
struct DataMemoryStats
{
    uint64_t m_Tables;
    uint64_t m_Instances;
    uint64_t m_Rows;
    uint64_t m_ValueBytes;
    uint64_t m_PayloadBlocks;
    uint64_t m_PayloadUsed;
    uint64_t m_PayloadCapacity;
    uint64_t m_BaseBlocks;
    uint64_t m_BaseUsed;
    uint64_t m_BaseCapacity;
    uint64_t m_StoreBytes;
    uint64_t m_TableBytes;
    uint64_t m_RowMetadataBytes;
    uint64_t m_SlotBytes;
    uint64_t m_BaseRowBytes;
    uint64_t m_InstanceBytes;
    uint64_t m_QueryBytes;
    uint64_t m_TotalBytes;
};

void             GetDataMemoryStats(HDataStore store, DataMemoryStats* out_stats);

uint64_t         ReadDataInteger(const uint8_t* bytes, uint32_t size);
void             WriteDataInteger(uint8_t* bytes, uint64_t value, uint32_t size);
uint32_t         DataTypeSize(DataValueType type);
uint32_t         DataTypeAlignment(DataValueType type);
DataPropertyMeta GetPropertyMeta(const DataTable* table, uint32_t index);
uint64_t         GetTableTag(const DataTable* table, uint32_t index);
uint64_t         GetRowComponentId(const DataTable* table, const DataRow* row);
DataValue        ReadRowValue(const DataTable* table, const DataRow* row, uint32_t property_index);
DataValue        GetChildValue(const DataValue* value, uint32_t index);
uint64_t         GetChildName(const DataStruct* object, uint32_t index);

// Engine store/table/row management, reset and binary I/O; not part of the extension SDK.
extern "C"
{
    /** Read a struct field
     *
     * Reads decoded input arrays or a packed view without allocating or expanding children.
     *
     * @name DataGetStructProperty
     * @param object [type:const DataStruct*] Struct input or borrowed view.
     * @param property [type:uint64_t] Field name hash.
     * @param out_value [type:DataValue*] Receives the field on success; unchanged on error.
     * @return result [type:DataResult] OK or NOT_FOUND when the field is absent.
     */
    DataResult DataGetStructProperty(const DataStruct* object, uint64_t property, DataValue* out_value);

    /** Read a list element
     *
     * Reads decoded input arrays or a packed view without allocating or expanding children.
     *
     * @name DataGetListValue
     * @param list [type:const DataList*] List input or borrowed view.
     * @param index [type:uint32_t] Zero-based element index.
     * @param out_value [type:DataValue*] Receives the element on success; unchanged on error.
     * @return result [type:DataResult] OK or NOT_FOUND when index is out of range.
     */
    DataResult DataGetListValue(const DataList* list, uint32_t index, DataValue* out_value);

    /** Read a property
     *
     * Borrowed strings and containers remain valid until the next store write or iterator step;
     * callers must copy them for longer retention.
     *
     * @name DataGetProperty
     * @param store [type:HDataStore] Store handle.
     * @param id [type:DataId] Row ID.
     * @param property [type:uint64_t] Property name hash.
     * @param out_value [type:DataValue*] Receives the value on success; unchanged on error. Strings and containers are borrowed.
     * @return result [type:DataResult] OK on success, or NOT_FOUND if the row or property does not exist.
     */
    DataResult DataGetProperty(HDataStore store, DataId id, uint64_t property, DataValue* out_value);

    /** Replace a property value
     *
     * Requires the kind declared in the table metadata. Does not add missing properties or change reset defaults.
     * Fixed-size values overwrite the existing mutable row without allocation.
     * Strings and containers are copied recursively into registration-owned payload blocks (table-owned for independently added rows), including self-assignment.
     * Replaces the complete property value; nested fields and elements are read-only views.
     * Replacement payload blocks are retained until their registration/table is reset or destroyed; individual replacements
     * do not reclaim them. Does not change query membership or invalidate iterators.
     * On error, the existing value is unchanged.
     *
     * @name DataSetProperty
     * @param store [type:HDataStore] Store handle.
     * @param id [type:DataId] Row ID.
     * @param property [type:uint64_t] Property name hash.
     * @param value [type:const DataValue*] New value tree to copy.
     * @return result [type:DataResult] OK, NOT_FOUND if the row or property does not exist, ALREADY_EXISTS for duplicate nested field names, or INVALID_ARGUMENT for a kind mismatch, invalid value or excessive nesting.
     */
    DataResult DataSetProperty(HDataStore store, DataId id, uint64_t property, const DataValue* value);

    // Internal batch helpers for engine construction and generic inspection.
    uint32_t    DataIterGetCount(const DataIterator* iterator);
    DataId      DataIterGetId(const DataIterator* iterator, uint32_t row);
    DataOwnerId DataIterGetOwnerId(const DataIterator* iterator, uint32_t row);

    /** Read a property from the current batch
     *
     * Borrowed strings and containers have the same lifetime as those returned by [ref:DataGetProperty].
     *
     * @name DataIterGetProperty
     * @param iterator [type:const DataIterator*] Iterator whose query and store are still alive.
     * @param row [type:uint32_t] Zero-based row index within the current batch.
     * @param property [type:uint64_t] Property name hash.
     * @param out_value [type:DataValue*] Receives the value on success; unchanged on error. Strings and containers are borrowed.
     * @return result [type:DataResult] OK, INVALID_ARGUMENT for an out-of-range row, or NOT_FOUND for a missing property.
     */
    DataResult DataIterGetProperty(const DataIterator* iterator, uint32_t row, uint64_t property, DataValue* out_value);

    /** Read a bound query field
     *
     * Uses the cached table offset for a requested property without a name lookup.
     * Selects original or overridden bytes on each call. Does not allocate.
     * Borrowed values have the lifetime described by [ref:DataValue].
     *
     * @name DataIterGetField
     * @param iterator [type:const DataIterator*] Iterator whose query and store are still alive.
     * @param row [type:uint32_t] Zero-based row index within the current batch.
     * @param field [type:uint32_t] Zero-based index in DataQueryDesc.m_Properties, not the table's field order.
     * @param out_value [type:DataValue*] Receives the value on success; unchanged on error.
     * @return result [type:DataResult] OK, or INVALID_ARGUMENT without a current row or for an out-of-range field.
     */
    DataResult DataIterGetField(const DataIterator* iterator, uint32_t row, uint32_t field, DataValue* out_value);

    /** Write a bound query field
     *
     * Uses a cached property binding. Has the same validation, payload allocation,
     * copying and reset semantics as [ref:DataSetProperty]. Preserves iteration.
     *
     * @name DataIterSetField
     * @param iterator [type:const DataIterator*] Iterator whose query and store are still alive.
     * @param row [type:uint32_t] Zero-based row index within the current batch.
     * @param field [type:uint32_t] Zero-based index in DataQueryDesc.m_Properties.
     * @param value [type:const DataValue*] Replacement value of the bound kind, copied on success.
     * @return result [type:DataResult] OK, INVALID_ARGUMENT for an invalid row/field, mismatched kind or invalid value, or ALREADY_EXISTS for duplicate nested names. Existing data is unchanged on error.
     */
    DataResult DataIterSetField(const DataIterator* iterator, uint32_t row, uint32_t field, const DataValue* value);

    /** Create an empty store
     *
     * The caller owns the store and destroys it with [ref:DataDestroyStore].
     *
     * @return store [type:HDataStore] New store handle.
     */
    HDataStore DataCreateStore(void);

    /** Destroy a store
     *
     * Frees the store, tables, rows and copied value trees. Destroy all queries first.
     * Requires an unlocked store; LOCKED leaves it intact.
     *
     * @param store [type:HDataStore] Store handle.
     * @return result [type:DataResult] OK, or LOCKED while the store is locked.
     */
    DataResult DataDestroyStore(HDataStore store);

    /** Register an empty table
     *
     * Copies tags and property metadata. Fields have fixed kinds and non-overlapping byte offsets. Duplicate independently registered type hashes are rejected. Successful
     * registration updates live queries. Requires an unlocked store.
     *
     * @param store [type:HDataStore] Store handle.
     * @param desc [type:const DataTableDesc*] Table type, tags and shared field layout to copy.
     * @return result [type:DataResult] LOCKED while the store is locked; otherwise OK, ALREADY_EXISTS for a duplicate type/field name, or INVALID_ARGUMENT for invalid tags/count/layout.
     */
    DataResult DataRegisterTable(HDataStore store, const DataTableDesc* desc);

    /** Unregister a table
     *
     * Removes an independently registered table and all its rows. Their IDs become stale, even if the type is
     * registered again. Updates live queries. Requires an unlocked store.
     *
     * @param store [type:HDataStore] Store handle.
     * @param type [type:uint64_t] Table type hash.
     * @return result [type:DataResult] LOCKED while the store is locked; otherwise OK on success, or NOT_FOUND if the type is not registered.
     */
    DataResult DataUnregisterTable(HDataStore store, uint64_t type);

    /** Insert a row
     *
     * Packs values in metadata order into the table's byte rows, retaining added values as reset defaults.
     * Strings and containers are allocated in table-owned blocks. Empty rows are allowed.
     * Requires an unlocked store. On error, the store is unchanged.
     *
     * @param store [type:HDataStore] Store handle.
     * @param type [type:uint64_t] Independently registered table type hash.
     * @param owner [type:DataOwnerId] Logical row owner.
     * @param values [type:const DataValue*] Values in table metadata order; may be NULL when value_count is zero.
     * @param value_count [type:uint32_t] Must equal the table property count; kinds must match.
     * @param out_id [type:DataId*] Receives the new store-local ID on success; unchanged on error.
     * @param component_id [type:uint64_t] Component name hash to save in a packed blob; defaults to zero for unnamed rows.
     * @return result [type:DataResult] LOCKED while the store is locked; otherwise OK, NOT_FOUND for an unknown table, INVALID_ARGUMENT for invalid input/counts or mismatched kinds.
     */
    DataResult DataAddRow(HDataStore store, uint64_t type, DataOwnerId owner, const DataValue* values, uint32_t value_count, DataId* out_id, uint64_t component_id = 0);

    /** Remove a row
     *
     * Uses swap removal; other live IDs remain valid. The removed ID becomes stale.
     * Requires an unlocked store. To remove during traversal, collect DataIds in a
     * caller-owned reusable buffer and apply removals after the outermost unlock.
     * Nested value storage is retained
     * until table/instance reset (replacement payloads) or table destruction (base values).
     * The caller keeps packed bytes alive while any resource or instance references the blob.
     *
     * @param store [type:HDataStore] Store handle.
     * @param id [type:DataId] ID of the row to remove.
     * @return result [type:DataResult] LOCKED while the store is locked; otherwise OK on success, or NOT_FOUND for an invalid or stale ID.
     */
    DataResult DataRemoveRow(HDataStore store, DataId id);

    /** Insert decoded rows in bulk
     *
     * Validates the entire batch before adding anything. Value arrays must match the shared metadata. Values and nested trees are
     * copied and retained as reset defaults; output IDs are independent of the input arrays' lifetimes. Successful
     * insertion requires an unlocked store. Zero count adds nothing.
     *
     * @param store [type:HDataStore] Store handle.
     * @param type [type:uint64_t] Independently registered table type hash.
     * @param rows [type:const DataRowDesc*] Array of count row descriptors; may be NULL when count is zero.
     * @param count [type:uint32_t] Number of rows to insert.
     * @param out_ids [type:DataId*] Array receiving count IDs; may be NULL when count is zero. Unchanged on error.
     * @return result [type:DataResult] LOCKED while the store is locked; otherwise OK, NOT_FOUND for an unknown table, INVALID_ARGUMENT for invalid input/counts or mismatched kinds.
     */
    DataResult DataAddRows(HDataStore store, uint64_t type, const DataRowDesc* rows, uint32_t count, DataId* out_ids);

    /** Reset a property
     *
     * Copies the loaded or added property value into its mutable row. Does not allocate,
     * change query membership or invalidate iterators. Replacement payload blocks remain
     * allocated until [ref:DataResetTable], [ref:DataResetBlob] or table destruction.
     *
     * @param store [type:HDataStore] Store handle.
     * @param id [type:DataId] Row ID.
     * @param property [type:uint64_t] Property name hash.
     * @return result [type:DataResult] OK on success, or NOT_FOUND if the row or property does not exist.
     */
    DataResult DataResetProperty(HDataStore store, DataId id, uint64_t property);

    /** Reset one component row
     *
     * Restores all properties, including nested values, to their loaded or added defaults.
     * Other rows are unaffected, including other components with the same owner or type.
     * Does not allocate, change IDs or query membership, or invalidate iterators.
     * Mutable row storage stays in place. Registration/table-owned replacement payloads
     * remain allocated until DataResetBlob, DataResetTable or table/instance destruction.
     *
     * @param store [type:HDataStore] Store containing the component row.
     * @param id [type:DataId] Component's runtime row ID.
     * @return result [type:DataResult] OK on success, or NOT_FOUND for an invalid or stale ID.
     */
    DataResult DataResetRow(HDataStore store, DataId id);

    /** Reset an independently registered table for bulk maintenance
     *
     * Restores every remaining row's properties to their loaded or added values,
     * then releases all replacement payload blocks. Keeps row IDs, owners and tags unchanged.
     * Does not undo row additions or removals, change query membership or invalidate iterators.
     * Borrowed strings and containers from replacement payload blocks must no longer be used after this call.
     * This spans owners and is used by benchmark setup. Engine component resets use
     * DataResetRow; a game object's grouped registration uses DataResetBlob.
     *
     * @param store [type:HDataStore] Store handle.
     * @param type [type:uint64_t] Independently registered table type hash.
     * @return result [type:DataResult] OK on success, or NOT_FOUND for an unknown table.
     */
    DataResult DataResetTable(HDataStore store, uint64_t type);

    /** Serialize all tables as one packed component-data blob
     *
     * Writes type/tag metadata, component IDs and current value trees. Runtime owner
     * IDs, row IDs and reset history are not saved. The header is FOURCC "DMDT" and
     * uint32 version 8. Fixed rows preserve the validated C-compatible layout,
     * including member and trailing padding. One directory addresses every table; all tables and nested
     * values share one packed string area. All offsets are relative to the blob start.
     *
     * @param store [type:HDataStore] Store containing the tables to write.
     * @param buffer [type:void*] Caller-owned destination; must not overlap any borrowed blob. NULL with size zero queries required size.
     * @param buffer_size [type:uint32_t] Destination capacity in bytes.
     * @param out_size [type:uint32_t*] Required byte size on OK or BUFFER_TOO_SMALL; unchanged on other errors.
     * @return result [type:DataResult] OK, BUFFER_TOO_SMALL, or INVALID_ARGUMENT for an unrepresentable size or invalid buffer arguments. Buffer is unchanged on errors.
     */
    DataResult DataWriteBlob(HDataStore store, void* buffer, uint32_t buffer_size, uint32_t* out_size);

    /** Load an immutable packed resource
     *
     * Validates all tables and borrows the supplied buffer without copying or modifying it.
     * The input must be eight-byte aligned. Member offsets and row/struct sizes are
     * validated for C-compatible alignment. The caller keeps the buffer unchanged and alive
     * until the blob reference and all its registrations have been released.
     * Loading does not register rows in a store.
     *
     * @param buffer [type:const void*] Caller-owned complete packed component-data blob.
     * @param buffer_size [type:uint32_t] Blob byte size.
     * @param out_blob [type:HDataBlob*] Receives a caller-owned reference on success; unchanged on error.
     * @return result [type:DataResult] OK or INVALID_FORMAT for malformed/unsupported data or an unaligned buffer.
     */
    DataResult DataLoadBlob(const void* buffer, uint32_t buffer_size, HDataBlob* out_blob);

    /** Release a loaded resource reference
     *
     * Resource pools retain a reference while any registrations remain. The last release frees the blob handle,
     * never the caller's buffer. Does not remove any instance's rows. The caller may
     * free the buffer after releasing this reference and removing all registrations.
     *
     * @param blob [type:HDataBlob] Reference to release; must not be used afterward.
     */
    void DataDestroyBlob(HDataBlob blob);

    /** Add every component table for one game object instance
     *
     * Registers row IDs and the supplied owner, borrowing shared metadata/defaults
     * and copying fixed row bytes into shared dense runtime tables. Registrations of
     * the same loaded resource reuse those tables and query bindings. Different loaded
     * resources remain separate, even if their layouts match. Strings and dynamic
     * containers retain blob offsets; no property trees are expanded. Addition requires an unlocked store.
     * Registration handles and row membership indices use reusable pooled slots.
     * The first registration includes tables, initial rows and a handle page in one allocation;
     * later registrations allocate only when shared capacity grows. Store registries and
     * live query caches may grow separately. Packed arrays split into another pool at their size limit.
     *
     * @param store [type:HDataStore] Destination store.
     * @param blob [type:HDataBlob] Loaded resource to retain.
     * @param owner [type:DataOwnerId] Runtime game object owner assigned to every added row.
     * @param out_instance [type:HDataBlobInstance*] Receives the registration handle on success; unchanged on error.
     * @return result [type:DataResult] OK, LOCKED while the store is locked, or INVALID_ARGUMENT if store capacity would be exceeded. Errors leave the store unchanged.
     */
    DataResult DataAddBlob(HDataStore store, HDataBlob blob, DataOwnerId owner, HDataBlobInstance* out_instance);

    /** Remove one instance's complete set of component tables
     *
     * Removes its remaining rows, invalidates their IDs, frees its replacement payloads,
     * and returns the registration slot for reuse. Shared capacity remains while other
     * registrations use that resource; the last removal frees the pool and its blob reference.
     * The store also removes its instances automatically when destroyed.
     *
     * @param instance [type:HDataBlobInstance] Registration to remove; must not be used after success.
     * @return result [type:DataResult] OK, or LOCKED while its store is locked (no changes).
     */
    DataResult DataRemoveBlob(HDataBlobInstance instance);

    /** Reset a game object's grouped component registration
     *
     * Copies loaded values into this registration's remaining mutable rows and frees its replacement payload blocks.
     * Does not restore removed rows or invalidate iterators. Borrowed replacement payloads expire.
     * Other registrations are unaffected, even if they share a blob, type or owner ID.
     * The engine retains this handle when registering a game object's component tables;
     * no datastore-wide owner search is needed. Independently added rows are not included.
     *
     * @param instance [type:HDataBlobInstance] Registration to reset.
     */
    void DataResetBlob(HDataBlobInstance instance);

    /** Read a component's prototype-local name hash
     *
     * Used by game object integration to associate a runtime row with its component.
     *
     * @param store [type:HDataStore] Store containing the row.
     * @param id [type:DataId] Runtime row ID.
     * @return component [type:uint64_t] Component name hash, or zero for an invalid/stale row. Zero may also identify an unnamed row.
     */
    uint64_t DataGetComponentId(HDataStore store, DataId id);
}

#endif // DM_DATA_H
