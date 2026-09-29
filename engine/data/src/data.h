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

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <dlib/array.h>
#include <dlib/mutex.h>
#include <dlib/object_pool.h>
#include <dmsdk/data/data.h>

// File format, version 1. Integers are little-endian; references are blob offsets.
// Variable arrays follow these fixed headers. Runtime structures below are separate.
struct DataFileHeader
{
    char     m_Magic[4]; // FOURCC "DMDT".
    uint32_t m_Version;  // Format version: DATA_FILE_VERSION.
};

// Followed by m_TableCount uint32_t table offsets, then padding to eight bytes.
struct DataFileDirectory
{
    uint32_t m_TableCount;    // Number of component tables in the blob.
    uint32_t m_StringsOffset; // Blob offset of the shared NUL-terminated string area.
};

// Followed by tags, metadata, component name hashes, fixed-stride rows and payloads.
struct DataTableHeader
{
    uint64_t m_TypeHash;      // Table's data type name hash.
    uint16_t m_TagCount;      // Number of following uint64_t tag hashes.
    uint16_t m_FieldCount;    // Root entries at the start of the metadata array.
    uint32_t m_RowCount;      // Component name count and number of value rows.
    uint32_t m_RowStride;     // Bytes per row, including alignment padding.
    uint16_t m_MetadataCount; // Total metadata entries, including inline struct members.
    uint16_t m_Reserved;      // Zero.
};

struct DataFileFieldMeta
{
    uint64_t m_NameHash;   // Field or member name hash.
    uint32_t m_Kind;       // DataValueType encoded as uint32_t.
    uint32_t m_ByteOffset; // Field offset within its row or containing inline struct.
    uint16_t m_ChildIndex; // First inline member in the same metadata array; zero otherwise.
    uint16_t m_ChildCount; // Number of immediate inline members.
    uint32_t m_ByteSize;   // Field size; includes trailing padding for inline structs.
};

// Dynamic structs/lists: optional uint64_t names (structs only), uint32_t kinds,
// uint32_t blob offsets, then child payloads. Each array contains m_Count entries.
struct DataFileContainerHeader
{
    uint32_t m_ByteSize; // Complete serialized container size, including padding.
    uint32_t m_Count;    // Number of immediate children.
};

static const uint32_t DATA_FILE_VERSION = 1;
static const uint32_t DATA_MAX_METADATA_COUNT = UINT16_MAX;
static const uint32_t DATA_MAX_TAG_COUNT = UINT16_MAX;
static const uint32_t DATA_TABLE_OFFSETS_OFFSET = sizeof(DataFileHeader) + sizeof(DataFileDirectory);
static const uint32_t DATA_TABLE_HEADER_SIZE = sizeof(DataTableHeader);
static const uint32_t DATA_FIELD_META_SIZE = sizeof(DataFileFieldMeta);

// Internal construction values and borrowed views; never stored as row/file structs.
struct DataValue;
union DataValueData;
struct DataTable;

// Borrowed named fields, backed by input arrays, a packed container or inline row bytes.
// Decoded input uses separate name, kind and payload arrays; names must be unique.
// Zero-initialize input; set copies names, kinds and values during the call.
// Use DataGetStructField to read either representation.
struct DataStruct
{
    const uint64_t*      m_Names;  // Decoded field hashes; unused for packed/inline views.
    const DataValueType* m_Types;  // Decoded field kinds; shareable across inputs, unused for packed/inline views.
    const DataValueData* m_Values; // Untagged decoded payloads in name order; unused for packed/inline views.
    uint32_t             m_Count;  // Number of fields.
    // private
    const uint8_t*   m_Buffer; // Borrowed blob/container bytes, or inline struct bytes when m_Table is set.
    uint32_t         m_Offset; // Container offset in m_Buffer, or first child metadata index for inline views.
    const DataTable* m_Table;  // Borrowed table for inline views; NULL for decoded/dynamic structs.
};

// Borrowed ordered values; elements may have different kinds.
// Zero-initialize decoded input; set copies the values. Read with DataGetListValue.
struct DataList
{
    const DataValue* m_Values; // Decoded elements; unused for packed views.
    uint32_t         m_Count;  // Number of elements.
    // private
    const uint8_t* m_Buffer; // Borrowed bytes backing a packed container; NULL for decoded input.
    uint32_t       m_Offset; // Container header offset in m_Buffer.
};

// Untagged payload selected by the input's m_Types entry or DataValue::m_Type.
// Stored rows contain native field bytes instead.
union DataValueData
{
    double      m_Number;      // Double-precision number.
    uint8_t     m_Boolean;     // Zero or one.
    const char* m_String;      // Borrowed NUL-terminated string.
    DataStruct  m_Struct;      // Named fields or a borrowed packed/inline view.
    DataList    m_List;        // Ordered elements or a borrowed packed view.
    float       m_Vector3[3];  // X, Y and Z.
    float       m_Vector4[4];  // X, Y, Z and W.
    float       m_Matrix4[16]; // Column-major; element (r, c) is [c * 4 + r].
};

// Temporary tagged construction/result value. Set requires the declared field kind.
// Strings and container views are borrowed until a store write, reset or iterator step.
// Decoded trees must be acyclic and no deeper than DATA_MAX_NESTING.
struct DataValue
{
    DataValueType m_Type;  // Kind selecting the active payload member.
    DataValueData m_Value; // Input value or borrowed result; not a stored row.
};

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
struct DataFieldDesc
{
    uint64_t              m_Field;  // Field name hash.
    DataValueType         m_Type;   // Declared kind; fixed for the table lifetime.
    uint32_t              m_Offset; // Byte offset within the containing row/struct.
    const DataStructDesc* m_Struct; // Reusable inline layout; NULL for scalar/dynamic values.
};

/** Reusable fixed composition layout. All rows have these members and kinds.
 * Names must be unique; nonempty fields must not overlap or exceed m_Size.
 * m_Size includes trailing padding to the maximum member alignment.
 * Members may include fixed structs, strings and dynamic containers. Cycles and
 * nesting deeper than DATA_MAX_NESTING are rejected during table registration.
 */
struct DataStructDesc
{
    const DataFieldDesc* m_Fields;     // Borrowed member descriptions; copied at registration.
    uint32_t             m_FieldCount; // Number of immediate members.
    uint32_t             m_Size;       // Struct size in bytes, including trailing padding.
};

// Compiled field layout, shared by every row of a table type. Fixed structs point
// into the same metadata array by index; their member offsets are relative to the
// struct's bytes. Root fields occupy the first m_FieldCount entries. A zero
// m_ChildIndex denotes an ordinary scalar/reference, including a dynamic STRUCT.
struct DataFieldMeta
{
    uint64_t      m_Field;      // Field name hash.
    DataValueType m_Type;       // Declared kind.
    uint32_t      m_Offset;     // Byte offset within the containing row/struct.
    uint16_t      m_ChildIndex; // First inline child metadata entry; zero for scalar/dynamic fields.
    uint16_t      m_ChildCount; // Number of immediate inline members.
    uint32_t      m_Size;       // Field byte size, including inline struct padding.
};

/** Construction table description. Registration copies the tags and metadata.
 * Row stride includes trailing padding to the maximum field alignment and must
 * contain every field. Invalid alignment is rejected at registration. All inserted
 * rows use these same field kinds and offsets. Zero fields/stride are allowed.
 * Tags and total metadata entries (including nested members) are limited to 65,535.
 */
struct DataTableDesc
{
    uint64_t             m_Type;       // Data type name hash.
    const uint64_t*      m_Tags;       // Borrowed tag hashes; copied at registration.
    uint32_t             m_TagCount;   // Number of tags.
    const DataFieldDesc* m_Fields;     // Borrowed root field descriptions; copied at registration.
    uint32_t             m_FieldCount; // Number of root fields.
    uint32_t             m_RowStride;  // Bytes per row, including trailing alignment padding.
};

/** Decoded construction input, copied by DataAddRows as reset defaults.
 * Separate kind and payload arrays follow metadata order and must match its count
 * and kinds. Rows of the same layout may share one kind array. Input arrays
 * and their referenced strings/containers only need to live through insertion.
 * Component IDs identify components within a prototype; owners identify instances.
 */
struct DataRowDesc
{
    DataOwnerId          m_Owner;       // Runtime game object or caller-defined owner.
    const DataValueType* m_Types;       // Borrowed kinds in root metadata order; may be shared across rows.
    const DataValueData* m_Values;      // Borrowed untagged payloads in root metadata order; copied on insertion.
    uint32_t             m_ValueCount;  // Number of root kinds and payloads.
    uint64_t             m_ComponentId; // Prototype-local component name hash.
};

// Native scalar overrides, resolved against the registered layout once per batch.
struct DataRowOverride
{
    DataQueryField m_Field;  // Name/path and exact kind; access mode is unused.
    const void*    m_Values; // First native scalar value, copied during insertion.
    uint32_t       m_Stride; // Byte stride between values; zero repeats one value.
};

struct DataRowOverrideBinding
{
    const uint8_t* m_Values; // Borrowed first source value.
    uint32_t       m_Stride; // Source byte stride; zero repeats one value.
    uint32_t       m_Offset; // Destination offset relative to the row.
    uint32_t       m_Size;   // Validated native scalar size.
};

// Row identity, source row and pooled registration indices. Mutable values follow
// the current row order; swap removal moves these bytes and preserves registration membership.
struct DataRow
{
    DataOwnerId m_Owner; // Runtime owner shared by this game object's components.
    union
    {
        uint64_t m_ComponentId;   // Prototype-local name for independently added rows.
        uint32_t m_InstanceIndex; // Packed rows: registration slot in the resource pool.
    };
    uint32_t m_Slot;      // Stable ID slot in DataStore::m_Slots.
    uint32_t m_BaseIndex; // Source row index in packed or owned reset defaults.
};

static const uint32_t DATA_MAX_NESTING = 64;
// One allocation in a registration, table or query arena. Payload follows the
// header at an eight-byte boundary. Used bytes include padding; capacity excludes
// the header. Blocks never move. Registration/table reset frees replacement
// payloads; independently added default values remain until table destruction.
struct DataBlock
{
    DataBlock* m_Next;     // Next owned arena block.
    size_t     m_Size;     // Used payload bytes, including padding.
    size_t     m_Capacity; // Payload capacity; excludes this header.
};

struct DataBlobPool;

// Only decoded tables own metadata/default storage. Packed tables borrow it.
struct DataOwnedTable
{
    dmArray<uint64_t>      m_Tags;       // Owned table tag hashes.
    dmArray<DataFieldMeta> m_Fields;     // Owned root and inline-member metadata.
    dmArray<uint8_t>       m_BaseRows;   // Original fixed row bytes used by reset.
    DataBlock*             m_BaseValues; // Arena blocks for original strings and dynamic containers.
    uint32_t               m_BaseCount;  // Number of allocated default rows; removal does not reclaim them.
};

static const uint32_t DATA_FIELD_LOOKUP_SIZE = 16;

struct DataTable
{
    uint64_t         m_Type;          // Data type name hash.
    uint32_t         m_StoreIndex;    // Index in DataStore::m_Tables for swap removal.
    uint16_t         m_TagCount;      // Number of table tags.
    uint16_t         m_FieldCount;    // Number of root fields.
    uint16_t         m_MetadataCount; // Root fields plus all inline-member entries.
    bool             m_HasReferences; // Row bytes contain strings or dynamic containers.
    uint32_t         m_RowStride;     // Bytes per fixed row, including padding.
    dmArray<DataRow> m_Rows;          // Dense runtime row identities.
    dmArray<uint8_t> m_Values;        // Mutable fixed row bytes in m_Rows order.
    DataBlobPool*    m_Pool;          // Owning resource pool; NULL for independently registered tables.
    const uint8_t*   m_Blob;          // Borrowed serialized resource bytes; NULL for independent tables.
    union
    {
        DataOwnedTable* m_Owned; // Metadata/default storage when m_Pool is NULL.
        struct
        {
            uint32_t m_Table;     // DataTableHeader offset in m_Blob.
            uint32_t m_Rows;      // Default row bytes offset in m_Blob.
            uint32_t m_FirstSlot; // First source row in each registration's slot array.
        } m_Offsets;              // Packed defaults and registration membership locations.
    };
    DataBlock*     m_Payloads;                            // Replacement arena for independent rows; packed rows use their registration.
    const uint8_t* m_FieldMetadata;                       // Borrowed owned/blob metadata; fixed for the table lifetime.
    uint16_t       m_FieldLookup[DATA_FIELD_LOOKUP_SIZE]; // First root metadata index per hash bucket; UINT16_MAX means empty.
};

struct DataSlot
{
    DataTable* m_Table;      // Borrowed owning table; NULL when the slot is free.
    uint32_t   m_Generation; // ID generation incremented on release.
    union
    {
        uint32_t m_Row;      // Current dense row index in m_Table.
        uint32_t m_NextFree; // Next free slot when m_Table is NULL.
    };
};

struct DataStore
{
    dmArray<DataTable*>      m_Tables;        // Owned runtime tables.
    dmArray<DataSlot>        m_Slots;         // Stable row ID slots and free-list links.
    dmArray<HDataQuery>      m_Queries;       // Borrowed live queries; callers destroy them before the store.
    uint32_t                 m_FreeSlotCount; // Reusable slots; excludes retired generations.
    uint32_t                 m_FreeSlot;      // First unused row ID slot, or UINT32_MAX.
    uint32_t                 m_LockCount;     // Nested structural locks on the owning thread.
    DataBlobPool*            m_Pools;         // Owned resource pools, linked by DataBlobPool::m_Next.
    dmMutex::HMutex          m_Mutex;         // Protects structural changes and query admission/release.
    dmObjectPool<HDataQuery> m_ActiveQueries; // Dense pool of currently reserved query handles.
    uint64_t                 m_Revision;      // Structural revision used to refresh cached query ranges.
};

// One requested field resolved for one matching table. Cached metadata
// supplies the field offset and kind without a name lookup per row.
struct DataQueryBinding
{
    DataFieldMeta m_Meta; // Resolved leaf metadata; offset is relative to the row.
};

// Bindings are followed by one uint32_t byte offset per field for inline pointer access.
static inline uint32_t* GetQueryFieldOffsets(DataQueryBinding* fields, uint32_t count)
{
    return (uint32_t*)(fields + count);
}

// A borrowed table and its query-owned bindings, in query field order.
// m_Fields is NULL for tag-only queries. Table removal releases these bindings
// to the query's free list before the table storage is destroyed.
struct DataQueryTable
{
    DataTable*        m_Table;  // Borrowed matching runtime table.
    DataQueryBinding* m_Fields; // Query-owned bindings; NULL when no fields were requested.
};

// Contiguous matching rows. Prefix indices let jobs find their first batch with
// a binary search, including owner-filtered queries. Rebuilt only after mutation.
struct DataQueryRange
{
    uint32_t m_TableIndex; // Index in DataQuery::m_Tables.
    uint32_t m_Start;      // First dense row in this table.
    uint32_t m_Count;      // Number of contiguous matching rows.
    uint32_t m_First;      // First row index in the complete reserved query result.
};

// Validated copy of a public query field. Paths have at most DATA_MAX_NESTING
// members; access is READ or READ_WRITE. Public input remains wide for validation.
struct DataQueryFieldInfo
{
    uint64_t        m_Field;     // Requested root name hash.
    const uint64_t* m_Path;      // Borrowed slice of the query's owned path array.
    DataValueType   m_Type;      // Required leaf kind.
    uint16_t        m_PathCount; // Number of inline members in the path, at most 64.
    uint8_t         m_Access;    // Validated DataAccess mode.
};

// Live query owned by its caller and registered with m_Store. Owns copies of
// filters, the matching-table array and the arena holding field bindings;
// it borrows the store/tables and must be destroyed before the store.
// Table changes update matches; owner filtering happens during row iteration.
// While active, m_ActiveSlot identifies its reservation in the store's object pool.
struct DataQuery
{
    HDataStore                  m_Store;      // Borrowed store; must outlive this query.
    dmArray<DataQueryTable>     m_Tables;     // Matching tables and their field bindings.
    dmArray<DataQueryFieldInfo> m_Fields;     // Copied requested fields and access modes.
    dmArray<uint64_t>           m_Paths;      // Owned member hashes referenced by m_Fields.
    dmArray<DataOwnerId>        m_Owners;     // Copied owner filter; empty matches every owner.
    dmArray<uint64_t>           m_Tags;       // Copied all-required tag filter.
    DataBlock*                  m_Bindings;   // Owned arena for per-table field bindings.
    DataQueryBinding*           m_FreeFields; // Recycled binding arrays; first bytes hold the next pointer.
    dmArray<DataQueryRange>     m_Ranges;     // Cached contiguous ranges for job splitting.
    uint64_t                    m_Revision;   // Store revision used to build m_Ranges.
    uint32_t                    m_RowCount;   // Total reserved matching rows.
    uint32_t                    m_ActiveSlot; // Store reservation pool slot; UINT32_MAX when inactive.
};

struct DataBlob
{
    const uint8_t* m_Data;       // Borrowed validated file bytes; never modified or freed here.
    uint32_t       m_TableCount; // Number of serialized tables.
    uint32_t       m_RowCount;   // Total source rows across the tables.
    uint32_t       m_RefCount;   // Resource references, including one per runtime pool.
};

// Shared runtime tables for one loaded resource in this store. The first
// registration page and initial table buffers follow this header in one allocation.
// Later registrations reuse slots; dense row arrays grow geometrically.
struct DataBlobPool
{
    HDataStore        m_Store;          // Borrowed destination store.
    HDataBlob         m_Blob;           // Retained immutable resource handle.
    DataBlobPool*     m_Next;           // Next pool owned by the store.
    dmArray<uint8_t*> m_Pages;          // Owned extra registration pages; never moved.
    uint8_t*          m_FirstPage;      // Registration page embedded in the initial allocation.
    uint64_t          m_AllocationSize; // Bytes in the initial pool/table/row/page allocation.
    uint32_t          m_InstanceStride; // Bytes per registration, including its source-row slot indices.
    uint32_t          m_PageShift;      // log2 of registrations per page.
    uint32_t          m_Issued;         // Number of registration slots initialized so far.
    uint32_t          m_Live;           // Number of registrations still present.
    uint32_t          m_FreeInstance;   // First reusable registration index, or UINT32_MAX.
};

// Stable pooled handle. A uint32_t slot index for each source row follows it;
// removed rows use UINT32_MAX. Only replacement payloads belong to this instance.
struct DataBlobInstance
{
    DataBlobPool* m_Pool;     // Borrowed owning pool; NULL when this slot is free.
    DataBlock*    m_Payloads; // Owned replacement string/container arena.
    uint32_t      m_Index;    // Stable registration slot within the pool.
    uint32_t      m_NextFree; // Next reusable registration index while free.
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
    uint64_t m_Tables;           // Live runtime table count.
    uint64_t m_Instances;        // Live blob registration count.
    uint64_t m_Rows;             // Live component row count.
    uint64_t m_ValueBytes;       // Mutable row capacity in bytes.
    uint64_t m_PayloadBlocks;    // Replacement arena block count.
    uint64_t m_PayloadUsed;      // Used replacement payload bytes, including padding.
    uint64_t m_PayloadCapacity;  // Replacement payload capacity, excluding block headers.
    uint64_t m_BaseBlocks;       // Owned default-value arena block count.
    uint64_t m_BaseUsed;         // Used default payload bytes, including padding.
    uint64_t m_BaseCapacity;     // Default payload capacity, excluding block headers.
    uint64_t m_StoreBytes;       // Store and table/query registry capacity in bytes.
    uint64_t m_TableBytes;       // Table objects and owned metadata/tag capacity in bytes.
    uint64_t m_RowMetadataBytes; // Runtime row identity capacity in bytes.
    uint64_t m_SlotBytes;        // Stable row ID slot capacity in bytes.
    uint64_t m_BaseRowBytes;     // Owned default row capacity in bytes.
    uint64_t m_InstanceBytes;    // Pools, registration pages and unused embedded storage in bytes.
    uint64_t m_QueryBytes;       // Query objects, copied filters, ranges and binding arenas in bytes.
    uint64_t m_TotalBytes;       // Retained store memory; excludes borrowed resource blobs.
};

void GetDataMemoryStats(HDataStore store, DataMemoryStats* out_stats);

// Shared allocation and lookup helpers. Arena blocks belong to their table,
// registration or query; borrowed values remain valid until that owner resets/frees them.
void       ReserveData(DataBlock** blocks, size_t size, size_t minimum_capacity = 4096);
void*      AllocateData(DataBlock** blocks, size_t size, size_t minimum_capacity = 4096);
void       DeleteBlocks(DataBlock** blocks);
DataTable* FindTable(HDataStore store, uint64_t type);

// Query table matching runs while the caller holds the store's structural mutex.
bool ResolveFieldPath(const DataTable* table, uint64_t field, DataValueType type, const uint64_t* path, uint32_t path_count, DataFieldMeta* out_meta);
void MatchTable(HDataQuery query, DataTable* table);
void UnmatchTable(HDataQuery query, DataTable* table);

// Value layout, construction and access shared by storage, iteration and I/O.
uint32_t   DataTypeSize(DataValueType type);
uint32_t   DataTypeAlignment(DataValueType type);
uint64_t   GetRowComponentId(const DataTable* table, const DataRow* row);
DataValue  ReadRowValue(const DataTable* table, const DataRow* row, uint32_t field_index);
DataValue  GetChildValue(DataValueType type, const DataValueData* value, uint32_t index);
uint64_t   GetChildName(const DataStruct* object, uint32_t index);
void       InitializeBlobValues(DataTable* table, uint32_t start, uint32_t count);
DataValue  ReadBoundValue(const DataTable* table, const DataRow* row, const DataFieldMeta& meta);
DataResult ReadField(DataTable* table, uint32_t row, uint64_t field, DataValue* out_value);
DataResult SetRowField(DataTable* table, DataRow* row, const DataFieldMeta& meta, const DataValue* value);
// Decoded batch construction; no table/row identities are changed by these helpers.
DataResult ValidateRowValues(const DataTable* table, const DataRowDesc* rows, uint32_t count, size_t* payload_size);
void       StoreRowValues(DataTable* table, const DataRowDesc* rows, uint32_t count, uint8_t* bytes);
DataResult BindRowOverrides(const DataTable* table, const DataRowOverride* fields, uint32_t field_count, uint32_t count, DataRowOverrideBinding* bindings);
void       StoreRowOverrides(uint8_t* bytes, uint32_t stride, uint32_t count, const DataRowOverrideBinding* fields, uint32_t field_count);
DataResult ValidateStoredValue(const DataTable* table, const DataFieldMeta& meta, DataValueType type, const DataValueData* value, uint32_t depth, size_t* payload_size);
void       StoreStoredValue(const DataTable* table, const DataFieldMeta& meta, DataBlock** blocks, uint8_t* bytes, const DataValueData* value);

// Keep shared byte/row access inline across translation units.
inline uint64_t ReadDataInteger(const uint8_t* bytes, uint32_t size)
{
    uint64_t value = 0;
    for (uint32_t i = 0; i < size; ++i)
        value |= (uint64_t)bytes[i] << (i * 8);
    return value;
}

inline void WriteDataInteger(uint8_t* bytes, uint64_t value, uint32_t size)
{
    for (uint32_t i = 0; i < size; ++i)
        bytes[i] = (uint8_t)(value >> (i * 8));
}

// Fixed-size copies preserve float bits and support unaligned source bytes.
static inline void ReadDataFloats(void* out_value, const uint8_t* bytes, uint32_t count)
{
    memcpy(out_value, bytes, count * sizeof(float));
}

inline DataFieldMeta ReadFileFieldMeta(const uint8_t* metadata, uint32_t index)
{
    const uint8_t* meta = metadata + (size_t)index * DATA_FIELD_META_SIZE;
    DataFieldMeta  result = {
         .m_Field = ReadDataInteger(meta + offsetof(DataFileFieldMeta, m_NameHash), 8),
         .m_Type = (DataValueType)ReadDataInteger(meta + offsetof(DataFileFieldMeta, m_Kind), 4),
         .m_Offset = (uint32_t)ReadDataInteger(meta + offsetof(DataFileFieldMeta, m_ByteOffset), 4),
         .m_ChildIndex = (uint16_t)ReadDataInteger(meta + offsetof(DataFileFieldMeta, m_ChildIndex), 2),
         .m_ChildCount = (uint16_t)ReadDataInteger(meta + offsetof(DataFileFieldMeta, m_ChildCount), 2),
         .m_Size = (uint32_t)ReadDataInteger(meta + offsetof(DataFileFieldMeta, m_ByteSize), 4)
    };
    return result;
}

inline DataFieldMeta GetFieldMeta(const DataTable* table, uint32_t index)
{
    if (!table->m_Pool)
        return table->m_Owned->m_Fields.Begin()[index];
    const uint8_t* metadata = table->m_Blob + table->m_Offsets.m_Table + DATA_TABLE_HEADER_SIZE + table->m_TagCount * 8;
    return ReadFileFieldMeta(metadata, index);
}

inline uint64_t GetTableTag(const DataTable* table, uint32_t index)
{
    return table->m_Pool ? ReadDataInteger(table->m_Blob + table->m_Offsets.m_Table + DATA_TABLE_HEADER_SIZE + index * 8, 8) : table->m_Owned->m_Tags[index];
}

static inline const uint8_t* GetFieldBytes(const DataTable* table, const DataRow* row, uint32_t offset)
{
    return table->m_Values.Begin() + (size_t)(row - table->m_Rows.Begin()) * table->m_RowStride + offset;
}

static inline uint8_t* GetFieldBytes(DataTable* table, const DataRow* row, uint32_t offset)
{
    return table->m_Values.Begin() + (size_t)(row - table->m_Rows.Begin()) * table->m_RowStride + offset;
}

template <typename T>
static inline void Reserve(dmArray<T>& array, uint32_t count)
{
    if (count > array.Capacity())
    {
        const uint32_t max_capacity = UINT32_MAX / sizeof(T);
        assert(count <= max_capacity);
        uint32_t capacity = array.Capacity() < 16 ? 16 : array.Capacity();
        while (capacity < count)
        {
            capacity = capacity > max_capacity / 2 ? count : capacity * 2;
        }
        array.SetCapacity(capacity);
    }
}

template <typename T>
static inline bool Contains(const dmArray<T>& array, T value)
{
    for (uint32_t i = 0; i < array.Size(); ++i)
    {
        if (array[i] == value)
            return true;
    }
    return false;
}

static inline uint32_t FindField(const DataTable* table, uint64_t field)
{
    for (uint32_t i = 0; i < table->m_FieldCount; ++i)
    {
        if (GetFieldMeta(table, i).m_Field == field)
            return i;
    }
    return UINT32_MAX;
}

static inline uint32_t FindMember(const DataTable* table, uint32_t first, uint32_t count, uint64_t field)
{
    for (uint32_t i = 0; i < count; ++i)
        if (GetFieldMeta(table, first + i).m_Field == field)
            return first + i;
    return UINT32_MAX;
}

static inline bool MatchesOwner(HDataQuery query, DataOwnerId owner)
{
    return query->m_Owners.Empty() || Contains(query->m_Owners, owner);
}

static inline DataId MakeId(HDataStore store, uint32_t index)
{
    return ((uint64_t)store->m_Slots[index].m_Generation << 32) | index;
}

static inline DataSlot* FindSlot(HDataStore store, DataId id)
{
    uint32_t index = (uint32_t)id;
    if (index >= store->m_Slots.Size())
        return 0;
    DataSlot* slot = &store->m_Slots[index];
    return slot->m_Table && slot->m_Generation == (uint32_t)(id >> 32) ? slot : 0;
}

// Specialize the payload copy at compile time; public typed reads have no runtime type dispatch.
template <DataValueType TYPE>
static inline DataResult CopyTypedValue(const DataTable* table, const uint8_t* bytes, void* out_value)
{
    switch (TYPE)
    {
        case DATA_VALUE_TYPE_NUMBER:
        {
            uint64_t bits = ReadDataInteger(bytes, 8);
            memcpy(out_value, &bits, 8);
            break;
        }
        case DATA_VALUE_TYPE_BOOLEAN:
            *(uint8_t*)out_value = *bytes;
            break;
        case DATA_VALUE_TYPE_STRING:
        {
            uint64_t    reference = ReadDataInteger(bytes, 8);
            const char* string = (reference & 1) ? (const char*)table->m_Blob + (reference >> 1) : (const char*)(uintptr_t)reference;
            *(const char**)out_value = string;
            break;
        }
        case DATA_VALUE_TYPE_VECTOR3:
            ReadDataFloats(out_value, bytes, 3);
            break;
        case DATA_VALUE_TYPE_VECTOR4:
            ReadDataFloats(out_value, bytes, 4);
            break;
        case DATA_VALUE_TYPE_MATRIX4:
            ReadDataFloats(out_value, bytes, 16);
            break;
        default:
            break;
    }
    return DATA_RESULT_OK;
}

template <DataValueType TYPE>
static inline DataResult ReadTypedValue(const DataTable* table, const DataRow* row, const DataFieldMeta& meta, void* out_value)
{
    if (meta.m_Type != TYPE)
        return DATA_RESULT_INVALID_ARGUMENT;
    const uint8_t* bytes = GetFieldBytes(table, row, meta.m_Offset);
    return CopyTypedValue<TYPE>(table, bytes, out_value);
}

// Engine store/table/row management, reset and binary I/O; not part of the extension SDK.
extern "C"
{
    /** Read a struct field
     *
     * Reads decoded input arrays or a packed view without allocating or expanding children.
     *
     * @name DataGetStructField
     * @param object [type:const DataStruct*] Struct input or borrowed view.
     * @param field [type:uint64_t] Field name hash.
     * @param out_value [type:DataValue*] Receives the field on success; unchanged on error.
     * @return result [type:DataResult] OK or NOT_FOUND when the field is absent.
     */
    DataResult DataGetStructField(const DataStruct* object, uint64_t field, DataValue* out_value);

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

    /** Read a field
     *
     * Borrowed strings and containers remain valid until the next store write or iterator step;
     * callers must copy them for longer retention.
     *
     * @name DataFieldGet
     * @param store [type:HDataStore] Store handle.
     * @param id [type:DataId] Row ID.
     * @param field [type:uint64_t] Field name hash.
     * @param out_value [type:DataValue*] Receives the value on success; unchanged on error. Strings and containers are borrowed.
     * @return result [type:DataResult] OK on success, or NOT_FOUND if the row or field does not exist.
     */
    DataResult DataFieldGet(HDataStore store, DataId id, uint64_t field, DataValue* out_value);

    /** Replace a field value
     *
     * Requires the kind declared in the table metadata. Does not add missing fields or change reset defaults.
     * Fixed-size values overwrite the existing mutable row without allocation.
     * Strings and containers are copied recursively into registration-owned payload blocks (table-owned for independently added rows), including self-assignment.
     * Replaces the complete field value; nested fields and elements are read-only views.
     * Replacement payload blocks are retained until their registration/table is reset or destroyed; individual replacements
     * do not reclaim them. Does not change query membership or invalidate iterators.
     * On error, the existing value is unchanged. Returns LOCKED during query reservations.
     *
     * @name DataSetField
     * @param store [type:HDataStore] Store handle.
     * @param id [type:DataId] Row ID.
     * @param field [type:uint64_t] Field name hash.
     * @param value [type:const DataValue*] New value tree to copy.
     * @return result [type:DataResult] OK, NOT_FOUND if the row or field does not exist, ALREADY_EXISTS for duplicate nested field names, or INVALID_ARGUMENT for a kind mismatch, invalid value or excessive nesting.
     */
    DataResult DataSetField(HDataStore store, DataId id, uint64_t field, const DataValue* value);

    // Internal batch helpers for engine construction and generic inspection.
    uint32_t    DataIterGetCount(const DataIterator* iterator);
    DataId      DataIterGetId(const DataIterator* iterator, uint32_t row);
    DataOwnerId DataIterGetOwnerId(const DataIterator* iterator, uint32_t row);

    /** Read a field from the current batch
     *
     * Borrowed strings and containers have the same lifetime as those returned by [ref:DataFieldGet].
     *
     * @name DataIterGetFieldByHash
     * @param iterator [type:const DataIterator*] Iterator whose query and store are still alive.
     * @param row [type:uint32_t] Zero-based row index within the current batch.
     * @param field [type:uint64_t] Field name hash.
     * @param out_value [type:DataValue*] Receives the value on success; unchanged on error. Strings and containers are borrowed.
     * @return result [type:DataResult] OK, INVALID_ARGUMENT for an out-of-range row, or NOT_FOUND for a missing field.
     */
    DataResult DataIterGetFieldByHash(const DataIterator* iterator, uint32_t row, uint64_t field, DataValue* out_value);

    /** Read a bound query field
     *
     * Uses the cached table offset for a requested field without a name lookup.
     * Selects original or overridden bytes on each call. Does not allocate.
     * Borrowed values have the lifetime described by [ref:DataValue].
     *
     * @name DataIterGetField
     * @param iterator [type:const DataIterator*] Iterator whose query and store are still alive.
     * @param row [type:uint32_t] Zero-based row index within the current batch.
     * @param field [type:uint32_t] Zero-based index in DataQueryDesc.m_Fields, not the table's field order.
     * @param out_value [type:DataValue*] Receives the value on success; unchanged on error.
     * @return result [type:DataResult] OK, or INVALID_ARGUMENT without a current row or for an out-of-range field.
     */
    DataResult DataIterGetField(const DataIterator* iterator, uint32_t row, uint32_t field, DataValue* out_value);

    /** Write a bound query field
     *
     * Uses a cached field binding. Has the same validation, payload allocation,
     * copying and reset semantics as [ref:DataSetField]. Preserves iteration.
     *
     * @name DataIterSetField
     * @param iterator [type:const DataIterator*] Iterator whose query and store are still alive.
     * @param row [type:uint32_t] Zero-based row index within the current batch.
     * @param field [type:uint32_t] Zero-based index in DataQueryDesc.m_Fields.
     * @param value [type:const DataValue*] Replacement value of the bound kind, copied on success.
     * @return result [type:DataResult] OK, INVALID_ARGUMENT for an invalid row/field, mismatched kind or invalid value, or ALREADY_EXISTS for duplicate nested names. Existing data is unchanged on error.
     */
    DataResult DataIterSetField(const DataIterator* iterator, uint32_t row, uint32_t field, const DataValue* value);

    /** Register an empty table
     *
     * Copies tags and field metadata. Fields have fixed kinds and non-overlapping byte offsets. Duplicate independently registered type hashes are rejected. Successful
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
     * @param types [type:const DataValueType*] Kinds in table metadata order; may be shared across rows. NULL when value_count is zero.
     * @param values [type:const DataValueData*] Untagged payloads in table metadata order; may be NULL when value_count is zero.
     * @param value_count [type:uint32_t] Must equal the table field count; kinds must match.
     * @param out_id [type:DataId*] Receives the new store-local ID on success; unchanged on error.
     * @param component_id [type:uint64_t] Component name hash to save in a packed blob; defaults to zero for unnamed rows.
     * @return result [type:DataResult] LOCKED while the store is locked; otherwise OK, NOT_FOUND for an unknown table, INVALID_ARGUMENT for invalid input/counts or mismatched kinds.
     */
    DataResult DataAddRow(HDataStore store, uint64_t type, DataOwnerId owner, const DataValueType* types, const DataValueData* values, uint32_t value_count, DataId* out_id, uint64_t component_id = 0);

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

    /** Construct rows from one decoded default and strided native scalar overrides.
     * Defaults are validated/copied once; named paths and kinds bind once per batch.
     * All inputs must remain valid and disjoint from destination storage through
     * the call. Only owner/component identity is read from rows. Final values,
     * including overrides, become each row's reset defaults. Validation failure
     * publishes no rows and leaves out_ids unchanged. Repeated overrides apply in
     * input order. Strings/containers come from defaults, not native overrides.
     */
    DataResult DataAddRowsFromTemplate(HDataStore store, uint64_t type, const DataRowDesc* defaults, const DataRowOverride* fields, uint32_t field_count, const DataRowDesc* rows, uint32_t count, DataId* out_ids);

    /** Reset a field
     *
     * Copies the loaded or added field value into its mutable row. Does not allocate,
     * change query membership or invalidate iterators. Replacement payload blocks remain
     * allocated until [ref:DataResetTable], [ref:DataResetBlob] or table destruction.
     * Returns LOCKED during query reservations.
     *
     * @param store [type:HDataStore] Store handle.
     * @param id [type:DataId] Row ID.
     * @param field [type:uint64_t] Field name hash.
     * @return result [type:DataResult] OK on success, or NOT_FOUND if the row or field does not exist.
     */
    DataResult DataResetField(HDataStore store, DataId id, uint64_t field);

    /** Reset one component row
     *
     * Restores all fields, including nested values, to their loaded or added defaults.
     * Other rows are unaffected, including other components with the same owner or type.
     * Does not allocate, change IDs or query membership, or invalidate iterators.
     * Mutable row storage stays in place. Registration/table-owned replacement payloads
     * remain allocated until DataResetBlob, DataResetTable or table/instance destruction.
     * Returns LOCKED during query reservations.
     *
     * @param store [type:HDataStore] Store containing the component row.
     * @param id [type:DataId] Component's runtime row ID.
     * @return result [type:DataResult] OK on success, or NOT_FOUND for an invalid or stale ID.
     */
    DataResult DataResetRow(HDataStore store, DataId id);

    /** Reset an independently registered table for bulk maintenance
     *
     * Restores every remaining row's fields to their loaded or added values,
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
     * uint32 version 1. Fixed rows preserve the validated C-compatible layout,
     * including member and trailing padding. One directory addresses every table; all tables and nested
     * values share one packed string area. All offsets are relative to the blob start.
     *
     * @param store [type:HDataStore] Store containing the tables to write.
     * @param buffer [type:void*] Caller-owned destination; must not overlap any borrowed blob. NULL with size zero queries required size.
     * @param buffer_size [type:uint32_t] Destination capacity in bytes.
     * @param out_size [type:uint32_t*] Required byte size on OK or BUFFER_TOO_SMALL; unchanged on other errors.
     * @return result [type:DataResult] OK, BUFFER_TOO_SMALL, INVALID_ARGUMENT for an invalid size/buffer, or LOCKED during query reservations. Buffer is unchanged on errors.
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
     * containers retain blob offsets; no field trees are expanded. Addition requires an unlocked store.
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
     * @return result [type:DataResult] OK, or LOCKED during query reservations.
     */
    DataResult DataResetBlob(HDataBlobInstance instance);

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
