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

#ifdef DATA_PUBLIC_API_ONLY
#error Use the public dmsdk/data headers in benchmarks and API examples
#endif

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
//
// Blob table offset --> [ DataTableHeader                        ]
//                       [ uint64_t tags[m_TagCount]              ]
//                       [ DataFileFieldMeta[m_MetadataCount]     ]
//                       [ uint64_t component IDs[m_RowCount]     ]
//                       [ row bytes[m_RowCount * m_RowStride]    ]
//                       [ alignment padding and dynamic payloads ]
// Each fixed row spans m_RowStride bytes. String offsets refer to the blob's
// shared string area, after all tables. Fixed rows store values, with their
// field names and kinds in the shared metadata.
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
    uint64_t m_NameHash;       // Full name hash, e.g. hash("light.color").
    uint32_t m_Kind;           // DataValueType encoded as uint32_t.
    uint32_t m_ByteOffset;     // Absolute byte offset within its row.
    uint16_t m_ChildIndex;     // First inline member in the same metadata array; zero otherwise.
    uint16_t m_ChildCount;     // Number of immediate inline members.
    uint32_t m_ByteSize;       // Field size; includes trailing padding for inline structs.
    uint64_t m_MemberNameHash; // Local member name hash for structured input/views.
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
struct DataStructTable;
struct DataStructStorage;

// Temporary struct sources. Zero-initialized construction input uses named arrays.
enum DataStructSource
{
    DATA_STRUCT_ARRAY,
    DATA_STRUCT_PACKED,
    DATA_STRUCT_INLINE,
    DATA_STRUCT_ROW,
    DATA_STRUCT_NATIVE
};

// Borrowed named fields. The source selects one representation; no input pointers
// are retained in stored rows. Read every representation with DataGetStructField.
struct DataStruct
{
    uint32_t         m_Count;  // Number of immediate fields.
    DataStructSource m_Source; // Selects the active union member.
    union
    {
        struct
        {
            const uint64_t*      m_Names;  // Unique local field hashes.
            const DataValueType* m_Types;  // Field kinds, shareable across inputs.
            const DataValueData* m_Values; // Borrowed payloads in name order.
        } m_Array;
        struct
        {
            const uint8_t*         m_Buffer;      // Packed container or inline row bytes.
            uint32_t               m_Offset;      // Container offset, metadata index or child-row index.
            const DataTable*       m_Table;       // Containing component table for inline/child rows.
            const DataStructTable* m_StructTable; // Shared child layout for DATA_STRUCT_ROW.
        } m_View;
        struct
        {
            const DataStructDesc* m_Layout; // Borrowed native input layout.
            const uint8_t*        m_Data;   // Borrowed native struct bytes.
        } m_Input;
    };
};

// Borrowed ordered values; elements may have different kinds.
// Zero-initialize decoded input; set copies the values. Read with DataGetListValue.
struct DataList
{
    const DataValue* m_Values; // Decoded elements; unused for packed views.
    uint32_t         m_Count;  // Number of elements.
    // private
    const uint8_t*       m_Buffer; // Borrowed bytes backing a packed container; NULL for decoded input.
    uint32_t             m_Offset; // Container header offset in m_Buffer.
    const DataListInput* m_Input;  // Borrowed native list input; NULL for decoded/packed views.
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

// Compiled field layout, shared by every row of a table type. Fixed structs point
// into the same metadata array by index; all offsets are relative to the row.
// Root fields occupy the first m_FieldCount entries. A zero m_ChildIndex denotes
// an ordinary scalar/reference, including a dynamic STRUCT.
struct DataFieldMeta
{
    uint64_t      m_Field;      // Full name hash; root fields retain their declared hash.
    DataValueType m_Type;       // Declared kind.
    uint32_t      m_Offset;     // Absolute byte offset within the row.
    uint16_t      m_ChildIndex; // First inline child metadata entry; zero for scalar/dynamic fields.
    uint16_t      m_ChildCount; // Number of immediate inline members.
    uint32_t      m_Size;       // Field byte size, including inline struct padding.
    uint64_t      m_Name;       // Local member hash, used by structured input/views.
};

// Dynamic structs share field metadata within their containing component table.
// No child DataId or per-row metadata is needed. Slots remain stable until freed:
// A reference: [ row index: high 32 bits | table index: 30 bits | tag: 2 bits ]
//                        |                       |
//                        |        m_Structs -> m_Tables[t]
//                        |                       |
//                        +---------------------> m_Values[r * m_RowStride]
//                            [ B values ][ free ][ B values ] ...
// A freed slot holds the next free index in its first four bytes. Slots are reused
// without moving other children; capacity remains until the component table dies.
struct DataStructField
{
    uint64_t      m_Name;   // Local member hash, shared by all child rows of this layout.
    DataValueType m_Type;   // Member kind; nested structs use another child-row reference.
    uint32_t      m_Offset; // C-aligned byte offset within the child row.
};

struct DataStructTable
{
    dmArray<DataStructField> m_Fields;    // Shared member layout; input order may differ.
    dmArray<uint8_t>         m_Values;    // Fixed-stride owned child rows, including reusable free slots.
    uint32_t                 m_RowStride; // Row size aligned to eight bytes; at least eight for the free link.
    uint32_t                 m_FreeRow;   // First free row index, or UINT32_MAX.
    uint32_t                 m_RowCount;  // Live child rows, including retained reset defaults.
};

struct DataStructStorage
{
    dmArray<DataStructTable*> m_Tables; // Owned layouts, allocated only for owned dynamic struct fields.
};

/** Decoded construction input, copied by DataAddRows as reset defaults.
 * Separate kind and payload arrays follow metadata order and must match its count
 * and kinds. Rows of the same layout may share one kind array. Input arrays
 * and their referenced strings/containers only need to live through insertion.
 * Component IDs identify components within a prototype; groups associate runtime rows.
 */
// Decoded inputs use parallel arrays, not the native byte-row layout:
//              field 0        field 1        ...
// m_Types  --> [ kind        ][ kind        ][...]
// m_Values --> [DataValueData][DataValueData][...]
//                     | insertion uses shared metadata offsets
//                     v
//              [ native field bytes + alignment padding ]
struct DataRowDesc
{
    DataGroupId          m_Group;       // Runtime game object or caller-defined group.
    const DataValueType* m_Types;       // Borrowed kinds in root metadata order; may be shared across rows.
    const DataValueData* m_Values;      // Borrowed untagged payloads in root metadata order; copied on insertion.
    uint32_t             m_ValueCount;  // Number of root kinds and payloads.
    uint64_t             m_ComponentId; // Prototype-local component name hash.
};

// Row identity, source row and pooled registration indices. Mutable values follow
// the current row order; swap removal moves these bytes and preserves registration membership.
//
// DataRow byte offsets (identity only; field values are in DataTable::m_Values):
//  0                   8                 16                 20                      24
//  +-------------------+-----------------+------------------+-----------------------+
//  | m_Group: uint64_t | union (8 bytes) | m_Slot: uint32_t | m_BaseIndex: uint32_t |
//  +-------------------+-----------------+------------------+-----------------------+
//                      |                 |                  +--> reset source row
//                      |                 +--> DataStore::m_Slots
//                      +--> m_ComponentId (uint64_t) or m_InstanceIndex (uint32_t)
// Packed m_BaseIndex stays unchanged on swap removal; owned defaults move with the row.
struct DataRow
{
    DataGroupId m_Group; // Caller-defined group shared by any number of rows.
    union
    {
        uint64_t m_ComponentId;   // Prototype-local name for independently added rows.
        uint32_t m_InstanceIndex; // Packed rows: registration slot in the resource pool.
    };
    uint32_t m_Slot;      // Stable ID slot in DataStore::m_Slots.
    uint32_t m_BaseIndex; // Packed source row index, or the current dense index for owned defaults.
};

static const uint32_t DATA_MAX_NESTING = 64;
// One allocation in a registration, table or query arena. Payload follows the
// header at an eight-byte boundary. Used bytes include padding; capacity excludes
// the header. Blocks never move. Registration/table reset frees replacement
// payloads; independent tables release payloads when emptied or destroyed.
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
    dmArray<uint8_t>       m_BaseRows;   // Original fixed row bytes in the same dense order as live rows.
    DataBlock*             m_BaseValues; // Arena blocks for original strings and dynamic containers.
};

static const uint32_t DATA_FIELD_LOOKUP_SIZE = 16;

// Parallel dense arrays; each identity selects the value bytes at the same index:
// m_Rows   --> [ DataRow 0 ][ DataRow 1 ][ ... ]
// m_Values --> [ row bytes ][ row bytes ][ ... ]  (m_RowStride bytes per row)
//                   ^
//                   | reset source: m_Rows[r].m_BaseIndex * m_RowStride
//                   +-- m_Blob + m_Offsets.m_Rows     (loaded defaults)
//                   +-- m_Owned->m_BaseRows.Begin()   (decoded defaults)
//
// Example PointLight value row from DATASTORE.md; offsets are bytes:
//  0                       12        16                        24              32
//  +-----------------------+---------+-------------------------+---------------+
//  | light.color: float[3] | padding | light.intensity: double | range: double |
//  +-----------------------+---------+-------------------------+---------------+
// One shared metadata array describes all rows; inline Light needs no pointer.
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
    DataStructStorage* m_Structs;                             // Owned dynamic child tables; NULL until a dynamic struct is copied.
    DataBlock*         m_Payloads;                            // Replacement arena for independent rows; packed rows use their registration.
    const uint8_t*     m_FieldMetadata;                       // Borrowed owned/blob metadata; fixed for the table lifetime.
    uint16_t           m_FieldLookup[DATA_FIELD_LOOKUP_SIZE]; // First metadata index per hash bucket; UINT16_MAX means empty.
};

// DataId: [ generation: high 32 bits | slot index: low 32 bits ]
//                                           |
//                                           v
//                      m_Slots[index] --> m_Table->m_Rows[m_Row]
// The generation rejects stale IDs; swap removal updates m_Row, not the ID.
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
//
// One binding allocation, count = DataQuery::m_Fields.Size():
// DataQueryTable::m_Fields:
//              [ DataQueryBinding x count ][ uint32_t offset x count ]
//                                         ^
//                                         GetQueryFieldOffsets(m_Fields, count)
// A field handle selects offset[handle]; value = row bytes + offset[handle].
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
// a binary search, including group-filtered queries. Rebuilt only after mutation.
struct DataQueryRange
{
    uint32_t m_TableIndex; // Index in DataQuery::m_Tables.
    uint32_t m_Start;      // First dense row in this table.
    uint32_t m_Count;      // Number of contiguous matching rows.
    uint32_t m_First;      // First row index in the complete reserved query result.
};

// Validated copy of a public query field. Access is READ or READ_WRITE.
// Public input remains wide for validation.
struct DataQueryFieldInfo
{
    uint64_t      m_Field;  // Requested full-name hash.
    DataValueType m_Type;   // Required leaf kind.
    uint8_t       m_Access; // Validated DataAccess mode.
};

// Live query owned by its caller and registered with m_Store. Owns copies of
// filters, the matching-table array and the arena holding field bindings;
// it borrows the store/tables and must be destroyed before the store.
// Table changes update matches; group filtering happens during row iteration.
// While active, m_ActiveSlot identifies its reservation in the store's object pool.
struct DataQuery
{
    HDataStore                  m_Store;      // Borrowed store; must outlive this query.
    dmArray<DataQueryTable>     m_Tables;     // Matching tables and their field bindings.
    dmArray<DataQueryFieldInfo> m_Fields;     // Copied requested fields and access modes.
    dmArray<DataGroupId>        m_Groups;     // Copied group filter; empty matches every group.
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
//
// Initial allocation (alignment padding omitted):
// [ DataBlobPool ][ DataTable x table count ][ DataRow buffers ][ value buffers ]
// [ first registration page: instance 0 | instance 1 | ... ]
//   ^ m_FirstPage              (records are m_InstanceStride bytes apart)
// m_Pages --> [ extra page 1* ][ extra page 2* ][ ... ]
// m_Blob  --> DataBlob --> immutable caller-owned file bytes
// Row/value arrays may move to larger allocations; registration records stay put.
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
//
// [ DataBlobInstance ][ uint32_t slots x blob row count ][ alignment padding ]
//                     ^ GetInstanceSlots(instance)
// slots[table.m_Offsets.m_FirstSlot + row.m_BaseIndex] --> DataStore::m_Slots
// m_Payloads --> [ DataBlock | replacement payload bytes ] --> next block
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
    uint64_t m_StructRows;       // Live exclusively owned child rows, including reset defaults.
    uint64_t m_StructRowBytes;   // Child row capacity; included in m_ValueBytes.
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
bool ResolveField(const DataTable* table, uint64_t field, DataValueType type, DataFieldMeta* out_meta);
void MatchTable(HDataQuery query, DataTable* table);
void UnmatchTable(HDataQuery query, DataTable* table);

// Value layout, construction and access shared by storage, iteration and I/O.
uint32_t      DataTypeSize(DataValueType type);
uint32_t      DataTypeAlignment(DataValueType type);
uint64_t      GetRowComponentId(const DataTable* table, const DataRow* row);
DataValue     ReadRowValue(const DataTable* table, const DataRow* row, uint32_t field_index);
DataValue     GetChildValue(DataValueType type, const DataValueData* value, uint32_t index);
uint64_t      GetChildName(const DataStruct* object, uint32_t index);
DataValueType GetStructFieldType(const DataStruct* object, uint32_t index);
void          InitializeBlobValues(DataTable* table, uint32_t start, uint32_t count);
DataValue     ReadBoundValue(const DataTable* table, const DataRow* row, const DataFieldMeta& meta);
DataResult    ReadField(DataTable* table, uint32_t row, uint64_t field, DataValue* out_value);
DataResult    SetRowField(DataTable* table, DataRow* row, const DataFieldMeta& meta, const DataValue* value);
// Decoded batch construction; no table/row identities are changed by these helpers.
DataResult ValidateLayout(const DataFieldDesc* fields, uint32_t count, uint32_t size, uint32_t depth, uint64_t* metadata_count, uint32_t* out_alignment);
bool       ValidateFieldNames(const uint8_t* metadata, uint32_t count, dmArray<uint64_t>& scratch);
DataResult ValidateNativeReferences(const DataTable* table, const DataFieldMeta& meta, const uint8_t* values, uint32_t stride, uint32_t count, uint32_t depth, size_t* payload_size);
void       StoreNativeReferences(DataTable* table, uint32_t count, uint8_t* rows);
DataResult ValidateRowValues(const DataTable* table, const DataRowDesc* rows, uint32_t count, size_t* payload_size);
void       StoreRowValues(DataTable* table, const DataRowDesc* rows, uint32_t count, uint8_t* bytes);
DataResult ValidateStoredValue(const DataTable* table, const DataFieldMeta& meta, DataValueType type, const DataValueData* value, uint32_t depth, size_t* payload_size);
void       StoreStoredValue(DataTable* table, const DataFieldMeta& meta, DataBlock** blocks, uint8_t* bytes, const DataValueData* value);

// Owned dynamic structs use table/row indices. Lists retain packed containers.
void               StoreValue(DataBlock** blocks, uint8_t* bytes, DataValueType type, const DataValueData* value);
uint64_t           StoreStructRow(DataTable* table, DataBlock** blocks, const DataStruct* object);
DataStruct         ReadStructRow(const DataTable* table, uint64_t reference);
void               ReleaseStructRow(DataTable* table, uint64_t reference);
void               DeleteStructStorage(DataTable* table);
void               ReleaseRowStructs(DataTable* table, uint32_t row, bool defaults);

static inline bool IsStructRow(uint64_t reference)
{
    return (reference & 3) == 2;
}

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
         .m_Size = (uint32_t)ReadDataInteger(meta + offsetof(DataFileFieldMeta, m_ByteSize), 4),
         .m_Name = ReadDataInteger(meta + offsetof(DataFileFieldMeta, m_MemberNameHash), 8)
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
    for (uint32_t i = 0; i < table->m_MetadataCount; ++i)
    {
        if (GetFieldMeta(table, i).m_Field == field)
            return i;
    }
    return UINT32_MAX;
}

static inline bool MatchesGroup(HDataQuery query, DataGroupId group)
{
    return query->m_Groups.Empty() || Contains(query->m_Groups, group);
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
        case DATA_TYPE_NUMBER:
        {
            uint64_t bits = ReadDataInteger(bytes, 8);
            memcpy(out_value, &bits, 8);
            break;
        }
        case DATA_TYPE_BOOLEAN:
            *(uint8_t*)out_value = *bytes;
            break;
        case DATA_TYPE_STRING:
        {
            uint64_t    reference = ReadDataInteger(bytes, 8);
            const char* string = (reference & 1) ? (const char*)table->m_Blob + (reference >> 1) : (const char*)(uintptr_t)reference;
            *(const char**)out_value = string;
            break;
        }
        case DATA_TYPE_VECTOR3:
            ReadDataFloats(out_value, bytes, 3);
            break;
        case DATA_TYPE_VECTOR4:
            ReadDataFloats(out_value, bytes, 4);
            break;
        case DATA_TYPE_MATRIX4:
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
    DataGroupId DataIterGetGroupId(const DataIterator* iterator, uint32_t row);

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

    /** Insert a row
     *
     * Packs values in metadata order into the table's byte rows, retaining added values as reset defaults.
     * Strings and containers are allocated in table-owned blocks. Empty rows are allowed.
     * Requires an unlocked store. On error, the store is unchanged.
     *
     * @param store [type:HDataStore] Store handle.
     * @param type [type:uint64_t] Independently registered table type hash.
     * @param group [type:DataGroupId] Logical row group.
     * @param types [type:const DataValueType*] Kinds in table metadata order; may be shared across rows. NULL when value_count is zero.
     * @param values [type:const DataValueData*] Untagged payloads in table metadata order; may be NULL when value_count is zero.
     * @param value_count [type:uint32_t] Must equal the table field count; kinds must match.
     * @param out_id [type:DataId*] Optional output receiving the new store-local ID on success; unchanged on error.
     * @param component_id [type:uint64_t] Component name hash to save in a packed blob; defaults to zero for unnamed rows.
     * @return result [type:DataResult] LOCKED while the store is locked; otherwise OK, NOT_FOUND for an unknown table, INVALID_ARGUMENT for invalid input/counts or mismatched kinds.
     */
    DataResult DataAddRow(HDataStore store, uint64_t type, DataGroupId group, const DataValueType* types, const DataValueData* values, uint32_t value_count, DataId* out_id, uint64_t component_id = 0);

    /** Insert decoded rows in bulk
     *
     * Validates the entire batch before adding anything. Value arrays must match the shared metadata. Values and nested trees are
     * copied and retained as reset defaults; output IDs are independent of the input arrays' lifetimes. Successful
     * insertion requires an unlocked store. Zero count returns OK without accessing the store or arrays.
     *
     * @param store [type:HDataStore] Store handle.
     * @param type [type:uint64_t] Independently registered table type hash.
     * @param rows [type:const DataRowDesc*] Array of count row descriptors; may be NULL when count is zero.
     * @param count [type:uint32_t] Number of rows to insert.
     * @param out_ids [type:DataId*] Optional array receiving count IDs on success; unchanged on error.
     * @return result [type:DataResult] LOCKED while the store is locked; otherwise OK, NOT_FOUND for an unknown table, INVALID_ARGUMENT for invalid input/counts or mismatched kinds.
     */
    DataResult DataAddRows(HDataStore store, uint64_t type, const DataRowDesc* rows, uint32_t count, DataId* out_ids);

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

    /** Reset an independently registered table for bulk maintenance
     *
     * Restores every remaining row's fields to their loaded or added values,
     * then releases all replacement payload blocks. Keeps row IDs, groups and tags unchanged.
     * Does not undo row additions or removals, change query membership or invalidate iterators.
     * Borrowed strings and containers from replacement payload blocks must no longer be used after this call.
     * This spans groups and is used by benchmark setup. Engine component resets use
     * DataResetRow; a game object's grouped registration uses DataResetBlob.
     *
     * @param store [type:HDataStore] Store handle.
     * @param type [type:uint64_t] Independently registered table type hash.
     * @return result [type:DataResult] OK on success, or NOT_FOUND for an unknown table.
     */
    DataResult DataResetTable(HDataStore store, uint64_t type);

    /** Serialize all tables as one packed component-data blob
     *
     * Writes type/tag metadata, component IDs and current value trees. Runtime group
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
}

#endif // DM_DATA_H
