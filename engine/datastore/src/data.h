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
#error Use the public dmsdk/datastore headers
#endif

#ifndef DM_DATA_H
#define DM_DATA_H

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <dlib/array.h>
#include <dlib/atomic.h>
#include <dlib/mutex.h>
#include <dlib/object_pool.h>
#include <dmsdk/datastore/datastore.h>

// File format, version 1. Integers are little-endian; references are blob offsets.
// Variable arrays follow these fixed headers. DSRow is shared with runtime storage.
struct DSFileHeader
{
    char     m_Magic[4]; // FOURCC "DFDS" (Defold DataStore).
    uint32_t m_Version;  // Format version: DATA_FILE_VERSION.
};

// Followed by m_TableCount uint32_t table offsets, then padding to eight bytes.
struct DSFileDirectory
{
    uint32_t m_TableCount;    // Number of component tables in the blob.
    uint32_t m_StringsOffset; // Blob offset of the shared NUL-terminated string area.
};

// Followed by tags, metadata, row records, fixed-stride values and payloads.
//
// Blob table offset --> [ DSTableHeader                        ]
//                       [ uint64_t tags[m_TagCount]              ]
//                       [ DSFileFieldMeta[m_MetadataCount]     ]
//                       [ DSRow records[m_RowCount]            ]
//                       [ value bytes[m_RowCount * m_RowStride]  ]
//                       [ alignment padding and dynamic payloads ]
// Each fixed row spans m_RowStride bytes. String offsets refer to the blob's
// shared string area, after all tables. Fixed rows store values, with their
// field names and kinds in the shared metadata.
struct DSTableHeader
{
    uint64_t m_TypeHash;      // Table's data type name hash.
    uint16_t m_TagCount;      // Number of following uint64_t tag hashes.
    uint16_t m_FieldCount;    // Root entries at the start of the metadata array.
    uint32_t m_RowCount;      // Number of row records and value rows.
    uint32_t m_RowStride;     // Bytes per row, including alignment padding.
    uint16_t m_MetadataCount; // Total metadata entries, including inline struct members.
    uint16_t m_Reserved;      // Zero.
};

struct DSFileFieldMeta
{
    uint64_t m_NameHash;       // Full name hash, e.g. hash("light.color").
    uint32_t m_Kind;           // DSValueType encoded as uint32_t.
    uint32_t m_ByteOffset;     // Absolute byte offset within its row.
    uint16_t m_ChildIndex;     // First inline member in the same metadata array; zero otherwise.
    uint16_t m_ChildCount;     // Number of immediate inline members.
    uint32_t m_ByteSize;       // Field size; includes trailing padding for inline structs.
    uint64_t m_MemberNameHash; // Local member name hash for structured input/views.
};

// The same row-record layout is used in files and DSTable::m_Rows. Values live
// in a separate array: records[i] describes values[i * m_RowStride].
// Files store the component name hash and zero all runtime fields. Instantiation
// copies each table's record array, then fills group, slot and source indices.
//
// Byte:   0         8             16      20           24               28          32
//         +---------+-------------+-------+------------+----------------+-----------+
//         | m_Group | ComponentId | Slot  | BaseIndex  | InstanceIndex  | Reserved  |
//         | uint64  | uint64      | uint32| uint32     | uint32         | uint32    |
//         +---------+-------------+-------+------------+----------------+-----------+
// Swap removal moves records and values together. Packed source indices stay
// unchanged; owned default rows move with their current values.
struct DSRow
{
    DSGroupId m_Group;         // Caller group (e.g. game object handle); zero in files.
    uint64_t  m_ComponentId;   // Prototype-local component name hash; retained in files.
    uint32_t  m_Slot;          // Runtime stable ID slot in DSStore::m_Slots; zero in files.
    uint32_t  m_BaseIndex;     // Runtime source/default/view row index; zero in files.
    uint32_t  m_InstanceIndex; // Runtime registration slot in the blob pool; zero otherwise.
    uint32_t  m_Reserved;      // Zero; keeps the record size a multiple of eight bytes.
};

// Dynamic structs/lists: optional uint64_t names (structs only), uint32_t kinds,
// uint32_t blob offsets, then child payloads. Each array contains m_Count entries.
struct DSFileContainerHeader
{
    uint32_t m_ByteSize; // Complete serialized container size, including padding.
    uint32_t m_Count;    // Number of immediate children.
};

static const uint32_t DATA_FILE_VERSION = 1;
static const uint32_t DATA_MAX_METADATA_COUNT = UINT16_MAX;
static const uint32_t DATA_MAX_TAG_COUNT = UINT16_MAX;
static const uint32_t DATA_TABLE_OFFSETS_OFFSET = sizeof(DSFileHeader) + sizeof(DSFileDirectory);
static const uint32_t DATA_TABLE_HEADER_SIZE = sizeof(DSTableHeader);
static const uint32_t DATA_FIELD_META_SIZE = sizeof(DSFileFieldMeta);

// Internal construction values and borrowed views; never stored as row/file structs.
struct DSValue;
union DSValueData;
struct DSTable;
struct DSStructTable;
struct DSStructStorage;

// Temporary struct sources. Zero-initialized construction input uses named arrays.
enum DSStructSource
{
    DS_STRUCT_ARRAY,
    DS_STRUCT_PACKED,
    DS_STRUCT_INLINE,
    DS_STRUCT_ROW,
    DS_STRUCT_NATIVE
};

// Borrowed named fields. The source selects one representation; no input pointers
// are retained in stored rows. Read every representation with DSGetStructField.
struct DSStruct
{
    uint32_t         m_Count;  // Number of immediate fields.
    DSStructSource   m_Source; // Selects the active union member.
    union
    {
        struct
        {
            const uint64_t*      m_Names;  // Unique local field hashes.
            const DSValueType*   m_Types;  // Field kinds, shareable across inputs.
            const DSValueData*   m_Values; // Borrowed payloads in name order.
        } m_Array;
        struct
        {
            const uint8_t*         m_Buffer;      // Packed container or inline row bytes.
            uint32_t               m_Offset;      // Container offset, metadata index or child-row index.
            uint32_t               m_FieldOffset; // Native offset of the inline struct, for relative member addressing.
            const DSTable*         m_Table;       // Containing component table for inline/child rows.
            const DSStructTable*   m_StructTable; // Shared child layout for DS_STRUCT_ROW.
        } m_View;
        struct
        {
            const DSStructDesc*   m_Layout; // Borrowed native input layout.
            const uint8_t*        m_Data;   // Borrowed native struct bytes.
        } m_Input;
    };
};

// Borrowed ordered values; elements may have different kinds.
// Zero-initialize decoded input; set copies the values. Read with DSGetListValue.
struct DSList
{
    const DSValue*   m_Values; // Decoded elements; unused for packed views.
    uint32_t         m_Count;  // Number of elements.
    // private
    const uint8_t*       m_Buffer; // Borrowed bytes backing a packed container; NULL for decoded input.
    uint32_t             m_Offset; // Container header offset in m_Buffer.
    const DSListInput*   m_Input;  // Borrowed native list input; NULL for decoded/packed views.
};

// Untagged payload selected by the input's m_Types entry or DSValue::m_Type.
// Stored rows contain native field bytes instead.
union DSValueData
{
    double      m_Number;      // Double-precision number.
    float       m_Float;       // Single-precision number.
    uint32_t    m_UInt32;      // Unsigned 32-bit integer.
    uint8_t     m_Boolean;     // Zero or one.
    const char* m_String;      // Borrowed NUL-terminated string.
    DSStruct    m_Struct;      // Named fields or a borrowed packed/inline view.
    DSList      m_List;        // Ordered elements or a borrowed packed view.
    float       m_Vector3[3];  // X, Y and Z.
    float       m_Vector4[4];  // X, Y, Z and W.
    float       m_Matrix4[16]; // Column-major; element (r, c) is [c * 4 + r].
};

// Temporary tagged construction/result value. Set requires the declared field kind.
// Strings and container views are borrowed until a store write, reset or iterator step.
// Decoded trees must be acyclic and no deeper than DATA_MAX_NESTING.
struct DSValue
{
    DSValueType m_Type;  // Kind selecting the active payload member.
    DSValueData m_Value; // Input value or borrowed result; not a stored row.
};

// Compiled field layout, shared by every row of a table type. Fixed structs point
// into the same metadata array by index; all offsets are relative to the row.
// Root fields occupy the first m_FieldCount entries. A zero m_ChildIndex denotes
// an ordinary scalar/reference, including a dynamic STRUCT.
struct DSFieldMeta
{
    uint64_t      m_Field;      // Full name hash; root fields retain their declared hash.
    DSValueType   m_Type;       // Declared kind.
    uint32_t      m_Offset;     // Absolute byte offset within the row.
    uint16_t      m_ChildIndex; // First inline child metadata entry; zero for scalar/dynamic fields.
    uint16_t      m_ChildCount; // Number of immediate inline members.
    uint32_t      m_Size;       // Field byte size, including inline struct padding.
    uint64_t      m_Name;       // Local member hash, used by structured input/views.
};

// Dynamic structs share field metadata within their containing component table.
// No child DSId or per-row metadata is needed. Slots remain stable until freed:
// A reference: [ row index: high 32 bits | table index: 30 bits | tag: 2 bits ]
//                        |                       |
//                        |        m_Structs -> m_Tables[t]
//                        |                       |
//                        +---------------------> m_Values[r * m_RowStride]
//                            [ B values ][ free ][ B values ] ...
// A freed slot holds the next free index in its first four bytes. Slots are reused
// without moving other children; capacity remains until the component table dies.
struct DSStructField
{
    uint64_t      m_Name;   // Local member hash, shared by all child rows of this layout.
    DSValueType   m_Type;   // Member kind; nested structs use another child-row reference.
    uint32_t      m_Offset; // C-aligned byte offset within the child row.
};

struct DSStructTable
{
    dmArray<DSStructField>   m_Fields;    // Shared member layout; input order may differ.
    dmArray<uint8_t>         m_Values;    // Fixed-stride owned child rows, including reusable free slots.
    uint32_t                 m_RowStride; // Row size aligned to eight bytes; at least eight for the free link.
    uint32_t                 m_FreeRow;   // First free row index, or UINT32_MAX.
    uint32_t                 m_RowCount;  // Live child rows, including retained reset defaults.
};

struct DSStructStorage
{
    dmArray<DSStructTable*> m_Tables; // Owned layouts, allocated only for owned dynamic struct fields.
};

/** Decoded construction input; DSAddRows applies the table's reset policy.
 * Separate kind and payload arrays follow metadata order and must match its count
 * and kinds. Rows of the same layout may share one kind array. Input arrays
 * and their referenced strings/containers only need to live through insertion.
 * Component IDs identify components within a prototype; groups associate runtime rows.
 */
// Decoded inputs use parallel arrays, not the native byte-row layout:
//              field 0        field 1        ...
// m_Types  --> [ kind        ][ kind        ][...]
// m_Values --> [DSValueData][DSValueData][...]
//                     | insertion uses shared metadata offsets
//                     v
//              [ native field bytes + alignment padding ]
struct DSRowDesc
{
    DSGroupId            m_Group;       // Runtime game object or caller-defined group.
    const DSValueType*   m_Types;       // Borrowed kinds in root metadata order; may be shared across rows.
    const DSValueData*   m_Values;      // Borrowed untagged payloads in root metadata order; copied on insertion.
    uint32_t             m_ValueCount;  // Number of root kinds and payloads.
    uint64_t             m_ComponentId; // Prototype-local component name hash.
};

static const uint32_t DATA_MAX_NESTING = 64;
// One allocation in a query binding arena. Payload follows the header at an
// eight-byte boundary. Used bytes include padding; capacity excludes the header.
// Blocks never move; query destruction releases the arena.
struct DSBlock
{
    DSBlock*   m_Next;     // Next owned arena block.
    size_t     m_Size;     // Used payload bytes, including padding.
    size_t     m_Capacity; // Payload capacity; excludes this header.
};

// Recyclable payloads keep stable addresses. Free chunks live inside their block;
// empty blocks are released, retaining at most one small spare block per arena.
struct DSPayloadHeader;
struct DSPayloadBlock
{
    DSPayloadBlock*    m_Next;     // Next block owned by the arena.
    DSPayloadBlock*    m_Previous; // Previous block; NULL at the head.
    DSPayloadBlock**   m_Owner;    // Stable arena head in a table or blob instance.
    DSPayloadHeader*   m_Free;     // Reusable chunks; next links occupy freed payload bytes.
    size_t             m_Size;     // End of the initialized chunks, including headers.
    size_t             m_Capacity; // Payload allocation capacity, excluding this block header.
    size_t             m_LiveSize; // Bytes held by live chunks, including their headers.
};

struct DSPayloadHeader
{
    DSPayloadBlock*   m_Block; // Owning block, used for release without searching the arena.
    size_t            m_Size;  // Aligned payload capacity; excludes this header.
};

struct DSBlobPool;

// Native/decoded tables own metadata and optional reset storage. Packed tables borrow it.
struct DSOwnedTable
{
    dmArray<uint64_t>      m_Tags;       // Owned table tag hashes.
    dmArray<DSFieldMeta>   m_Fields;     // Owned root and inline-member metadata.
    DSPayloadBlock*        m_BaseValues; // Recyclable original payloads; empty when reset is disabled.
};

// Value chunks are suballocated from store pages. Oversized rows own a dedicated
// allocation (m_Page == UINT32_MAX). Records remain in a separate dense array.
struct DSRowChunk
{
    uint8_t* m_Data;   // Current rows, followed by optional retained default rows.
    uint32_t m_Page;   // Index in DSStore::m_Pages; independent of table/chunk lifetime.
    uint32_t m_Offset; // Byte offset of this chunk within its page.
};

// Pages with free chunks form an index-linked list. A free chunk stores the byte
// offset of the next free chunk in its first uint32_t. Full pages leave the list.
struct DSRowPage
{
    uint8_t* m_Data;      // Owned backing allocation; zero for a recycled page slot.
    uint32_t m_FreeChunk; // First free chunk offset, or UINT32_MAX.
    uint32_t m_FreeCount; // Number of reusable chunks.
    uint32_t m_Next;      // Next available page, or next recycled page slot.
    uint32_t m_Prev;      // Previous available page, or UINT32_MAX.
};

static const uint32_t DATA_FIELD_LOOKUP_SIZE = 16;

// Row identities stay dense; values occupy fixed-capacity chunks in shared pages:
// m_Rows   --> [ DSRow 0 ][ DSRow 1 ][ ... ]
// m_Chunks --> [ chunk 0* ][ chunk 1* ][ ... ][ partial tail* ]
// page     --> [ current rows | retained defaults ][ another table's chunk ]
// A row index selects chunk = row >> m_ChunkShift, local = row & m_ChunkMask.
// Only the tail has unused rows. Swap removal keeps all earlier chunks full.
// A borrowed view uses m_Rows[i].m_BaseIndex to select its external row:
// view m_Rows --> [source 2][source 3][source 7]   m_Values --> caller-owned rows
// Reset defaults are addressed by m_Rows[r].m_BaseIndex * m_RowStride:
//   m_Blob + m_Offsets.m_Rows   (loaded defaults)
//   chunk.m_Data + m_DefaultOffset (native/decoded defaults, indexed within chunk)
//
// Example PointLight value row from DATASTORE.md; offsets are bytes:
//  0                       12        16                        24              32
//  +-----------------------+---------+-------------------------+---------------+
//  | light.color: float[3] | padding | light.intensity: double | range: double |
//  +-----------------------+---------+-------------------------+---------------+
// One shared metadata array describes all rows; inline Light needs no pointer.
struct DSTable
{
    uint64_t                  m_Type;           // Data type name hash.
    uint64_t                  m_Serial;         // Store-local registration order, stable across swap removal.
    uint32_t                  m_StoreIndex;     // Index in DSStore::m_Tables for swap removal.
    uint16_t                  m_TagCount;       // Number of table tags.
    uint16_t                  m_FieldCount;     // Number of root fields.
    uint16_t                  m_MetadataCount;  // Root fields plus all inline-member entries.
    bool                      m_HasReferences;  // Row bytes contain strings or dynamic containers.
    bool                      m_IsView;         // Values are caller-owned; row base indices select source rows.
    bool                      m_RetainDefaults; // Native rows retain initial values; blobs always have resource defaults.
    dmArray<DSTable*>         m_ViewAliases;    // Borrowed views whose selected source rows overlap this view.
    uint32_t                  m_RowStride;      // Row bytes, including native alignment padding.
    dmArray<DSRow>            m_Rows;           // Dense row records; same layout as in the blob.
    dmArray<uint8_t>          m_Values;         // Borrowed external values for views; empty for owned tables.
    dmArray<DSRowChunk>       m_Chunks;         // Dense value chunks; only the last can have free rows.
    uint32_t                  m_ChunkShift;     // log2 rows per chunk; row index uses shift and mask.
    uint32_t                  m_ChunkMask;      // Rows per chunk minus one.
    uint32_t                  m_DefaultOffset;  // Retained defaults within a chunk; zero when none.
    DSBlobPool*               m_Pool;           // Owning resource pool; NULL for independently registered tables.
    const uint8_t*            m_Blob;           // Borrowed serialized resource bytes; NULL for independent tables.
    union
    {
        DSOwnedTable* m_Owned; // Metadata/default storage when m_Pool is NULL.
        struct
        {
            uint32_t m_Table;     // DSTableHeader offset in m_Blob.
            uint32_t m_Rows;      // Default row bytes offset in m_Blob.
            uint32_t m_FirstSlot; // First source row in each registration's slot array.
        } m_Offsets;              // Packed defaults and registration membership locations.
    };
    DSStructStorage*   m_Structs;                             // Owned dynamic child tables; NULL until a dynamic struct is copied.
    DSPayloadBlock*    m_Payloads;                            // Recyclable current/replacement payloads; packed rows use their registration.
    const uint8_t*     m_FieldMetadata;                       // Borrowed owned/blob metadata; fixed for the table lifetime.
    uint16_t           m_FieldLookup[DATA_FIELD_LOOKUP_SIZE]; // First metadata index per hash bucket; UINT16_MAX means empty.
};

// DSId: [ generation: high 32 bits | slot index: low 32 bits ]
//                                           |
//                                           v
//                      m_Slots[index] --> m_Table->m_Rows[m_Row]
// The generation rejects stale IDs; swap removal updates m_Row, not the ID.
struct DSSlot
{
    DSTable*   m_Table;      // Borrowed owning table; NULL when the slot is free.
    uint32_t   m_Generation; // ID generation incremented on release.
    union
    {
        uint32_t m_Row;      // Current dense row index in m_Table.
        uint32_t m_NextFree; // Next free slot when m_Table is NULL.
    };
};

struct DSSystemRegistration
{
    DSSystemDesc m_Desc;            // Copied declaration; event nodes have no query/process. Query/context are borrowed.
    char*        m_Names;           // Owned contiguous storage for all declaration strings.
    uint32_t     m_DependencyStart; // First resolved index in DSStore::m_SystemDependencies.
    uint8_t      m_DependencyCount; // Number of packed dependency names, at most DS_SYSTEM_MAX_DEPENDENCIES.
    uint8_t      m_State;           // Pending, running or complete in the current schedule.
};

struct DSStore
{
    dmArray<DSRowPage>            m_Pages;              // Owned backing pages and reusable descriptor slots.
    uint32_t                      m_PageSize;           // Configured backing allocation bytes.
    uint32_t                      m_RowChunkSize;       // Configured chunk allocation bytes.
    uint32_t                      m_FreePage;           // First page with free chunks, or UINT32_MAX.
    uint32_t                      m_FreePageSlot;       // First recycled page descriptor, or UINT32_MAX.
    uint32_t                      m_SparePageCount;     // Empty pages currently available for reuse.
    uint32_t                      m_MaxSparePages;      // Limit on retained empty pages; excess pages are freed.
    dmArray<DSTable*>             m_Tables;             // Owned runtime tables.
    dmArray<DSSlot>               m_Slots;              // Stable row ID slots and free-list links.
    dmArray<HDSQuery>             m_Queries;            // Borrowed live queries; callers destroy them before the store.
    uint32_t                      m_FreeSlotCount;      // Reusable slots; excludes retired generations.
    uint32_t                      m_FreeSlot;           // First unused row ID slot, or UINT32_MAX.
    DSBlobPool*                   m_Pools;              // Owned resource pools, linked by DSBlobPool::m_Next.
    dmMutex::HMutex               m_Mutex;              // Protects structural changes and query admission/release.
    dmObjectPool<HDSQuery>        m_ActiveQueries;      // Dense pool of currently reserved query handles.
    uint64_t                      m_Revision;           // Structural revision used to refresh cached query ranges.
    uint64_t                      m_NextTableSerial;    // Next unique table registration number.
    dmArray<DSSystemRegistration> m_Systems;            // Owned callback/event registrations; removal swaps the last entry.
    dmArray<uint32_t>             m_SystemDependencies; // Resolved registration indices, rebuilt after registration changes.
    HJobContext                   m_ScheduleJobs;       // Borrowed execution context for the current schedule; never owned by the store.
    bool                          m_ScheduleDirty;      // Registration changes require dependency resolution and cycle detection.
    bool                          m_ScheduleActive;     // Invocation open; published under m_Mutex for query/store lifetime checks.
};

// One requested field resolved for one matching table. Cached metadata
// supplies the field offset and kind without a name lookup per row.
//
// One binding allocation, count = DSQuery::m_Fields.Size():
// DSQueryTable::m_Fields:
// [ DSQueryBinding x count ][ uint32_t offset x count ]
//                             ^ GetQueryFieldOffsets
// A handle selects an offset; value = m_Values + offset[handle] + row * m_RowStride.
struct DSQueryBinding
{
    DSFieldMeta m_Meta; // Resolved leaf metadata; offset is relative to the row.
};

// Bindings are followed by byte offsets for inline pointer access.
static inline uint32_t* GetQueryFieldOffsets(DSQueryBinding* fields, uint32_t count)
{
    return (uint32_t*)(fields + count);
}

// A borrowed table and its query-owned bindings, in query field order.
// m_Fields is NULL for tag-only queries. Table removal releases these bindings
// to the query's free list before the table storage is destroyed.
struct DSQueryTable
{
    DSTable*        m_Table;  // Borrowed matching runtime table.
    DSQueryBinding* m_Fields; // Query-owned bindings; NULL when no fields were requested.
};

// Contiguous matching rows. Prefix indices let jobs find their first batch with
// a binary search, including group-filtered queries. Rebuilt only after mutation.
struct DSQueryRange
{
    uint32_t m_TableIndex; // Index in DSQuery::m_Tables.
    uint32_t m_Start;      // First dense row in this table.
    uint32_t m_Count;      // Number of contiguous matching rows.
    uint32_t m_First;      // First row index in the complete reserved query result.
};

// Validated copy of a public query field. Access is READ or READ_WRITE.
// Public input remains wide for validation.
struct DSQueryFieldInfo
{
    uint64_t      m_Field;  // Requested full-name hash.
    DSValueType   m_Type;   // Required leaf kind.
    uint8_t       m_Access; // Validated query access flags; zero reads, DS_FIELD_MUTABLE writes.
};

// Compatibility depends on matching layouts and immutable filters, not row values.
struct DSQueryConflict
{
    HDSQuery   m_Query;    // Other live query; removed when that query is destroyed.
    uint64_t   m_Revision; // Other query's matching-table revision.
    bool       m_Conflict; // Overlapping declared access with at least one writer.
};

// Live query owned by its caller and registered with m_Store. Owns copies of
// filters, the matching-table array and the arena holding field bindings;
// it borrows the store/tables and must be destroyed before the store.
// Table changes update matches; group filtering happens during row iteration.
// While active, m_ActiveSlot identifies its reservation in the store's object pool.
struct DSQuery
{
    HDSContext                  m_Store;         // Borrowed store; must outlive this query.
    dmArray<DSQueryTable>       m_Tables;        // Matching tables and bindings, sorted by registration serial.
    dmArray<DSQueryFieldInfo>   m_Fields;        // Copied requested fields and access modes.
    dmArray<DSGroupId>          m_Groups;        // Copied group filter; empty matches every group.
    dmArray<uint64_t>           m_Tags;          // Copied all-required tag filter.
    DSBlock*                    m_Bindings;      // Owned arena for per-table field bindings.
    DSQueryBinding*             m_FreeFields;    // Recycled binding arrays; first bytes hold the next pointer.
    dmArray<DSQueryRange>       m_Ranges;        // Cached contiguous ranges for job splitting.
    dmArray<DSQueryConflict>    m_Conflicts;     // Lazily cached compatibility with other queries.
    uint64_t                    m_MatchRevision; // Invalidates compatibility when matching tables change.
    bool                        m_HasWrites;     // Any field or the complete row has read/write access.
    uint8_t                     m_Flags;         // Validated DSQueryFlags for whole-row access.
    uint64_t                    m_Revision;      // Store revision used to build m_Ranges.
    uint32_t                    m_RowCount;      // Total reserved matching rows.
    uint32_t                    m_ActiveSlot;    // Store reservation pool slot; UINT32_MAX when inactive.
};

struct DSBlob
{
    const uint8_t* m_Data;       // Borrowed validated file bytes; never modified or freed here.
    uint32_t       m_TableCount; // Number of serialized tables.
    uint32_t       m_RowCount;   // Total source rows across the tables.
    int32_atomic_t m_RefCount;   // Thread-safe resource references, including one per runtime pool.
};

// Shared runtime tables for one loaded resource in this store. The first
// registration page and initial table buffers follow this header in one allocation.
// Later registrations reuse slots; row records grow geometrically. Values use store pages.
//
// Initial allocation (alignment padding omitted):
// [ DSBlobPool ][ DSTable x table count ][ DSRow record buffers ]
// [ first registration page: instance 0 | instance 1 | ... ]
//   ^ m_FirstPage              (records are m_InstanceStride bytes apart)
// m_Pages --> [ extra page 1* ][ extra page 2* ][ ... ]
// m_Blob  --> DSBlob --> immutable caller-owned file bytes
// Identity arrays may move to larger allocations; values and registration records stay put.
struct DSBlobPool
{
    HDSContext        m_Store;          // Borrowed destination store.
    HDSBlob           m_Blob;           // Retained immutable resource handle.
    DSBlobPool*       m_Next;           // Next pool owned by the store.
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
// [ DSBlobInstance ][ uint32_t slots x blob row count ][ alignment padding ]
//                     ^ GetInstanceSlots(instance)
// slots[table.m_Offsets.m_FirstSlot + row.m_BaseIndex] --> DSStore::m_Slots
// m_Payloads --> [ DSPayloadBlock | chunk header + payload | ... ] --> next block
struct DSBlobInstance
{
    DSBlobPool*       m_Pool;     // Borrowed owning pool; NULL when this slot is free.
    DSPayloadBlock*   m_Payloads; // Owned replacement string/container arena.
    uint32_t          m_Index;    // Stable registration slot within the pool.
    uint32_t          m_NextFree; // Next reusable registration index while free.
};

static inline DSTable* GetPoolTable(DSBlobPool* pool, uint32_t index)
{
    return (DSTable*)(pool + 1) + index;
}

static inline DSTable* GetInstanceTable(HDSBlobInstance instance, uint32_t index)
{
    return GetPoolTable(instance->m_Pool, index);
}

static inline HDSBlobInstance GetPoolInstance(DSBlobPool* pool, uint32_t index)
{
    uint32_t page = index >> pool->m_PageShift;
    uint8_t* bytes = page ? pool->m_Pages[page - 1] : pool->m_FirstPage;
    return (HDSBlobInstance)(bytes + (size_t)(index & ((1u << pool->m_PageShift) - 1)) * pool->m_InstanceStride);
}

static inline uint32_t* GetInstanceSlots(HDSBlobInstance instance)
{
    return (uint32_t*)(instance + 1);
}

// Shared allocation and lookup helpers. Arena blocks belong to their table,
// registration or query; borrowed values remain valid until that owner resets/frees them.
void                             ReserveData(DSBlock** blocks, size_t size, size_t minimum_capacity = 4096);
void*                            AllocateData(DSBlock** blocks, size_t size, size_t minimum_capacity = 4096);
void                             DeleteBlocks(DSBlock** blocks);
void                             ReservePayloads(DSPayloadBlock** blocks, size_t size);
void*                            AllocatePayload(DSPayloadBlock** blocks, size_t size);
void                             ReleasePayload(void* bytes);
void                             DeletePayloads(DSPayloadBlock** blocks);
void                             ReleaseReference(DSTable* table, DSValueType type, uint64_t reference);
DSTable*                         FindTable(HDSContext store, uint64_t type);
void                             InitializeRowStorage(HDSContext store, DSTable* table);
void                             ReserveRowStorage(HDSContext store, DSTable* table, uint32_t num_rows);
void                             TrimRowStorage(HDSContext store, DSTable* table, uint32_t num_rows);
void                             DeleteRowPages(HDSContext store);

static inline DSPayloadBlock**   GetNativePayloads(DSTable* table)
{
    return table->m_RetainDefaults ? &table->m_Owned->m_BaseValues : &table->m_Payloads;
}

// Query table matching runs while the caller holds the store's structural mutex.
bool ResolveField(const DSTable* table, uint64_t field, DSValueType type, DSFieldMeta* out_meta);
void MatchTable(HDSQuery query, DSTable* table);
void UnmatchTable(HDSQuery query, DSTable* table);

// Value layout, construction and access shared by storage, iteration and I/O.
uint32_t    DSTypeSize(DSValueType type);
uint32_t    DSTypeAlignment(DSValueType type);
DSValue     ReadRowValue(const DSTable* table, const DSRow* row, uint32_t field_index);
DSValue     GetChildValue(DSValueType type, const DSValueData* value, uint32_t index);
uint64_t    GetChildName(const DSStruct* object, uint32_t index);
DSValueType GetStructFieldType(const DSStruct* object, uint32_t index);
void        InitializeBlobValues(DSTable* table, uint32_t start, uint32_t count);
DSValue     ReadBoundValue(const DSTable* table, const DSRow* row, const DSFieldMeta& meta);
DSResult    ReadField(DSTable* table, uint32_t row, uint64_t field, DSValue* out_value);
DSResult    SetRowField(DSTable* table, DSRow* row, const DSFieldMeta& meta, const DSValue* value);
// Decoded batch construction; no table/row identities are changed by these helpers.
DSResult   ValidateLayout(const DSFieldDesc* fields, uint32_t count, uint32_t size, uint32_t depth, uint64_t* metadata_count, uint32_t* out_alignment);
bool       ValidateFieldNames(const uint8_t* metadata, uint32_t count, dmArray<uint64_t>& scratch);
DSResult   ValidateNativeReferences(const DSTable* table, const DSFieldMeta& meta, const uint8_t* values, uint32_t stride, uint32_t count, uint32_t depth, size_t* payload_size);
void       StoreNativeReferences(DSTable* table, uint32_t count, uint8_t* rows);
DSResult   ValidateRowValues(const DSTable* table, const DSRowDesc* rows, uint32_t count, size_t* payload_size);
void       StoreRowValues(DSTable* table, const DSRowDesc* rows, uint32_t count, uint8_t* bytes);
DSResult   ValidateStoredValue(const DSTable* table, const DSFieldMeta& meta, DSValueType type, const DSValueData* value, uint32_t depth, size_t* payload_size);
void       StoreStoredValue(DSTable* table, const DSFieldMeta& meta, DSPayloadBlock** blocks, uint8_t* bytes, const DSValueData* value);

// Owned dynamic structs use table/row indices. Lists retain packed containers.
void               StoreValue(DSPayloadBlock** blocks, uint8_t* bytes, DSValueType type, const DSValueData* value);
uint64_t           StoreStructRow(DSTable* table, DSPayloadBlock** blocks, const DSStruct* object);
DSStruct           ReadStructRow(const DSTable* table, uint64_t reference);
void               ReleaseStructRow(DSTable* table, uint64_t reference);
void               DeleteStructStorage(DSTable* table);
void               ReleaseRowValues(DSTable* table, uint32_t row, bool defaults);

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

inline DSFieldMeta ReadFileFieldMeta(const uint8_t* metadata, uint32_t index)
{
    const uint8_t* meta = metadata + (size_t)index * DATA_FIELD_META_SIZE;
    DSFieldMeta    result = {
           .m_Field = ReadDataInteger(meta + offsetof(DSFileFieldMeta, m_NameHash), 8),
           .m_Type = (DSValueType)ReadDataInteger(meta + offsetof(DSFileFieldMeta, m_Kind), 4),
           .m_Offset = (uint32_t)ReadDataInteger(meta + offsetof(DSFileFieldMeta, m_ByteOffset), 4),
           .m_ChildIndex = (uint16_t)ReadDataInteger(meta + offsetof(DSFileFieldMeta, m_ChildIndex), 2),
           .m_ChildCount = (uint16_t)ReadDataInteger(meta + offsetof(DSFileFieldMeta, m_ChildCount), 2),
           .m_Size = (uint32_t)ReadDataInteger(meta + offsetof(DSFileFieldMeta, m_ByteSize), 4),
           .m_Name = ReadDataInteger(meta + offsetof(DSFileFieldMeta, m_MemberNameHash), 8)
    };
    return result;
}

inline DSFieldMeta GetFieldMeta(const DSTable* table, uint32_t index)
{
    if (!table->m_Pool)
        return table->m_Owned->m_Fields.Begin()[index];
    const uint8_t* metadata = table->m_Blob + table->m_Offsets.m_Table + DATA_TABLE_HEADER_SIZE + table->m_TagCount * 8;
    return ReadFileFieldMeta(metadata, index);
}

inline uint64_t GetTableTag(const DSTable* table, uint32_t index)
{
    return table->m_Pool ? ReadDataInteger(table->m_Blob + table->m_Offsets.m_Table + DATA_TABLE_HEADER_SIZE + index * 8, 8) : table->m_Owned->m_Tags[index];
}

static inline uint8_t* GetRowBytes(const DSTable* table, uint32_t row)
{
    if (table->m_IsView)
        return (uint8_t*)table->m_Values.Begin() + (size_t)table->m_Rows[row].m_BaseIndex * table->m_RowStride;
    if (!table->m_RowStride)
        return 0;
    return table->m_Chunks.Begin()[row >> table->m_ChunkShift].m_Data + (size_t)(row & table->m_ChunkMask) * table->m_RowStride;
}

static inline uint32_t GetRowSpan(const DSTable* table, uint32_t row, uint32_t count)
{
    if (table->m_IsView || !table->m_RowStride)
        return count;
    uint32_t available = table->m_ChunkMask + 1 - (row & table->m_ChunkMask);
    return count < available ? count : available;
}

static inline uint8_t* GetNativeDefaultRow(const DSTable* table, uint32_t row)
{
    return GetRowBytes(table, row) + table->m_DefaultOffset;
}

static inline const uint8_t* GetFieldBytes(const DSTable* table, const DSRow* row, uint32_t offset)
{
    return GetRowBytes(table, (uint32_t)(row - table->m_Rows.Begin())) + offset;
}

static inline uint8_t* GetFieldBytes(DSTable* table, const DSRow* row, uint32_t offset)
{
    return GetRowBytes(table, (uint32_t)(row - table->m_Rows.Begin())) + offset;
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

static inline uint32_t FindField(const DSTable* table, uint64_t field)
{
    for (uint32_t i = 0; i < table->m_MetadataCount; ++i)
    {
        if (GetFieldMeta(table, i).m_Field == field)
            return i;
    }
    return UINT32_MAX;
}

static inline bool MatchesGroup(HDSQuery query, DSGroupId group)
{
    return query->m_Groups.Empty() || Contains(query->m_Groups, group);
}

static inline DSId MakeId(HDSContext store, uint32_t index)
{
    return ((uint64_t)store->m_Slots[index].m_Generation << 32) | index;
}

static inline DSSlot* FindSlot(HDSContext store, DSId id)
{
    uint32_t index = (uint32_t)id;
    if (index >= store->m_Slots.Size())
        return 0;
    DSSlot* slot = &store->m_Slots[index];
    return slot->m_Table && slot->m_Generation == (uint32_t)(id >> 32) ? slot : 0;
}

// Specialize the payload copy at compile time; public typed reads have no runtime type dispatch.
template <DSValueType TYPE>
static inline DSResult CopyTypedValue(const DSTable* table, const uint8_t* bytes, void* out_value)
{
    switch (TYPE)
    {
        case DS_TYPE_NUMBER:
        {
            uint64_t bits = ReadDataInteger(bytes, 8);
            memcpy(out_value, &bits, 8);
            break;
        }
        case DS_TYPE_FLOAT:
        case DS_TYPE_UINT32:
            memcpy(out_value, bytes, 4);
            break;
        case DS_TYPE_BOOLEAN:
            *(uint8_t*)out_value = *bytes;
            break;
        case DS_TYPE_STRING:
        {
            uint64_t    reference = ReadDataInteger(bytes, 8);
            const char* string = (reference & 1) ? (const char*)table->m_Blob + (reference >> 1) : (const char*)(uintptr_t)reference;
            *(const char**)out_value = string;
            break;
        }
        case DS_TYPE_VECTOR3:
            ReadDataFloats(out_value, bytes, 3);
            break;
        case DS_TYPE_VECTOR4:
            ReadDataFloats(out_value, bytes, 4);
            break;
        case DS_TYPE_MATRIX4:
            ReadDataFloats(out_value, bytes, 16);
            break;
        default:
            break;
    }
    return DS_RESULT_OK;
}

// Engine store/table/row management, reset and binary I/O; not part of the extension SDK.
extern "C"
{
    /** Read a struct field
     *
     * Reads decoded input arrays or a packed view without allocating or expanding children.
     *
     * @name DSGetStructField
     * @param object [type:const DSStruct*] Struct input or borrowed view.
     * @param field [type:uint64_t] Field name hash.
     * @param out_value [type:DSValue*] Receives the field on success; unchanged on error.
     * @return result [type:DSResult] OK or NOT_FOUND when the field is absent.
     */
    DSResult DSGetStructField(const DSStruct* object, uint64_t field, DSValue* out_value);

    /** Read a list element
     *
     * Reads decoded input arrays or a packed view without allocating or expanding children.
     *
     * @name DSGetListValue
     * @param list [type:const DSList*] List input or borrowed view.
     * @param index [type:uint32_t] Zero-based element index.
     * @param out_value [type:DSValue*] Receives the element on success; unchanged on error.
     * @return result [type:DSResult] OK or NOT_FOUND when index is out of range.
     */
    DSResult DSGetListValue(const DSList* list, uint32_t index, DSValue* out_value);

    /** Read a field
     *
     * Borrowed strings and containers remain valid until the next store write or iterator step;
     * callers must copy them for longer retention.
     *
     * @name DSFieldGet
     * @param store [type:HDSContext] Store handle.
     * @param id [type:DSId] Row ID.
     * @param field [type:uint64_t] Field name hash.
     * @param out_value [type:DSValue*] Receives the value on success; unchanged on error. Strings and containers are borrowed.
     * @return result [type:DSResult] OK on success, or NOT_FOUND if the row or field does not exist.
     */
    DSResult DSFieldGet(HDSContext store, DSId id, uint64_t field, DSValue* out_value);

    /** Replace a field value
     *
     * Requires the kind declared in the table metadata. Does not add missing fields or change reset defaults.
     * Fixed-size values overwrite the existing mutable row without allocation.
     * Strings and containers are copied recursively into registration-owned payload blocks (table-owned for independently added rows), including self-assignment.
     * Replaces the complete field value; nested fields and elements are read-only views.
     * Replaced payload chunks are recycled; empty blocks are reclaimed. Does not change query membership or invalidate iterators.
     * On error, the existing value is unchanged. Returns LOCKED during query reservations.
     *
     * @name DSSetField
     * @param store [type:HDSContext] Store handle.
     * @param id [type:DSId] Row ID.
     * @param field [type:uint64_t] Field name hash.
     * @param value [type:const DSValue*] New value tree to copy.
     * @return result [type:DSResult] OK, NOT_FOUND if the row or field does not exist, ALREADY_EXISTS for duplicate nested field names, or INVALID_ARGUMENT for a kind mismatch, invalid value or excessive nesting.
     */
    DSResult DSSetField(HDSContext store, DSId id, uint64_t field, const DSValue* value);

    // Internal batch helpers for engine construction and generic inspection.
    uint32_t  DSIterGetCount(const DSIterator* iterator);
    DSId      DSIterGetId(const DSIterator* iterator, uint32_t row);
    DSGroupId DSIterGetGroupId(const DSIterator* iterator, uint32_t row);

    /** Read a field from the current batch
     *
     * Borrowed strings and containers have the same lifetime as those returned by [ref:DSFieldGet].
     *
     * @name DSIterGetFieldByHash
     * @param iterator [type:const DSIterator*] Iterator whose query and store are still alive.
     * @param row [type:uint32_t] Zero-based row index within the current batch.
     * @param field [type:uint64_t] Field name hash.
     * @param out_value [type:DSValue*] Receives the value on success; unchanged on error. Strings and containers are borrowed.
     * @return result [type:DSResult] OK, INVALID_ARGUMENT for an out-of-range row, or NOT_FOUND for a missing field.
     */
    DSResult DSIterGetFieldByHash(const DSIterator* iterator, uint32_t row, uint64_t field, DSValue* out_value);

    /** Read a bound query field
     *
     * Uses the cached table offset for a requested field without a name lookup.
     * Selects original or overridden bytes on each call. Does not allocate.
     * Borrowed values have the lifetime described by [ref:DSValue].
     *
     * @name DSIterGetField
     * @param iterator [type:const DSIterator*] Iterator whose query and store are still alive.
     * @param row [type:uint32_t] Zero-based row index within the current batch.
     * @param field [type:uint32_t] Index among the query's requested fields, excluding tags.
     * @param out_value [type:DSValue*] Receives the value on success; unchanged on error.
     * @return result [type:DSResult] OK, or INVALID_ARGUMENT without a current row or for an out-of-range field.
     */
    DSResult DSIterGetField(const DSIterator* iterator, uint32_t row, uint32_t field, DSValue* out_value);

    /** Write a bound query field
     *
     * Uses a cached field binding. Has the same validation, payload allocation,
     * copying and reset semantics as [ref:DSSetField]. Preserves iteration.
     *
     * @name DSIterSetField
     * @param iterator [type:const DSIterator*] Iterator whose query and store are still alive.
     * @param row [type:uint32_t] Zero-based row index within the current batch.
     * @param field [type:uint32_t] Index among the query's requested fields, excluding tags.
     * @param value [type:const DSValue*] Replacement value of the bound kind, copied on success.
     * @return result [type:DSResult] OK, INVALID_ARGUMENT for an invalid row/field, mismatched kind or invalid value, or ALREADY_EXISTS for duplicate nested names. Existing data is unchanged on error.
     */
    DSResult DSIterSetField(const DSIterator* iterator, uint32_t row, uint32_t field, const DSValue* value);

    /** Insert a row
     *
     * Packs values in metadata order; the table reset policy controls retention of defaults.
     * Strings and containers are allocated in table-owned blocks. Empty rows are allowed.
     * Requires a store without active query reservations. On error, the store is unchanged.
     *
     * @param store [type:HDSContext] Store handle.
     * @param type [type:uint64_t] Independently registered table type hash.
     * @param group [type:DSGroupId] Logical row group.
     * @param types [type:const DSValueType*] Kinds in table metadata order; may be shared across rows. NULL when value_count is zero.
     * @param values [type:const DSValueData*] Untagged payloads in table metadata order; may be NULL when value_count is zero.
     * @param value_count [type:uint32_t] Must equal the table field count; kinds must match.
     * @param out_id [type:DSId*] Optional output receiving the new store-local ID on success; unchanged on error.
     * @param component_id [type:uint64_t] Component name hash to save in a packed blob; defaults to zero for unnamed rows.
     * @return result [type:DSResult] LOCKED while query reservations are active; otherwise OK, NOT_FOUND for an unknown table, INVALID_ARGUMENT for invalid input/counts or mismatched kinds.
     */
    DSResult DSAddRow(HDSContext store, uint64_t type, DSGroupId group, const DSValueType* types, const DSValueData* values, uint32_t value_count, DSId* out_id, uint64_t component_id = 0);

    /** Insert decoded rows in bulk
     *
     * Validates the entire batch before adding anything. Value arrays must match the shared metadata. Values and nested trees are
     * copied, with optional reset retention; output IDs are independent of input lifetimes. Successful
     * insertion requires no active query reservations. Zero count returns OK without accessing the store or arrays.
     *
     * @param store [type:HDSContext] Store handle.
     * @param type [type:uint64_t] Independently registered table type hash.
     * @param rows [type:const DSRowDesc*] Array of count row descriptors; may be NULL when count is zero.
     * @param count [type:uint32_t] Number of rows to insert.
     * @param out_ids [type:DSId*] Optional array receiving count IDs on success; unchanged on error.
     * @return result [type:DSResult] LOCKED while query reservations are active; otherwise OK, NOT_FOUND for an unknown table, INVALID_ARGUMENT for invalid input/counts or mismatched kinds.
     */
    DSResult DSAddRows(HDSContext store, uint64_t type, const DSRowDesc* rows, uint32_t count, DSId* out_ids);

    /** Reset a field
     *
     * Copies the loaded or added field value into its mutable row. Does not allocate,
     * change query membership or invalidate iterators. Replacement payloads are released for reuse.
     * Tables without retained defaults return INVALID_ARGUMENT.
     * Returns LOCKED during query reservations.
     *
     * @param store [type:HDSContext] Store handle.
     * @param id [type:DSId] Row ID.
     * @param field [type:uint64_t] Field name hash.
     * @return result [type:DSResult] OK on success, or NOT_FOUND if the row or field does not exist.
     */
    DSResult DSResetField(HDSContext store, DSId id, uint64_t field);

    /** Reset an independently registered table for bulk maintenance
     *
     * Restores every remaining row's fields to their loaded or added values,
     * then releases all replacement payload blocks. Keeps row IDs, groups and tags unchanged.
     * Does not undo row additions or removals, change query membership or invalidate iterators.
     * Borrowed strings and containers from replacement payload blocks must no longer be used after this call.
     * This spans groups. Engine component resets use DSResetRow;
     * a game object's grouped registration uses DSResetBlob.
     *
     * @param store [type:HDSContext] Store handle.
     * @param type [type:uint64_t] Independently registered table type hash.
     * @return result [type:DSResult] OK on success, or NOT_FOUND for an unknown table.
     */
    DSResult DSResetTable(HDSContext store, uint64_t type);

    /** Serialize all tables as one packed component-data blob
     *
     * Writes type/tag metadata, row records and current value trees. Row records
     * retain component IDs; groups and runtime indices are written as zero.
     * Row IDs and reset history are not saved. The header is FOURCC "DFDS" and
     * uint32 version 1. Fixed rows preserve the validated C-compatible layout,
     * including member and trailing padding. One directory addresses every table; all tables and nested
     * values share one packed string area. All offsets are relative to the blob start.
     *
     * @param store [type:HDSContext] Store containing the tables to write.
     * @param buffer [type:void*] Caller-owned destination; must not overlap any borrowed blob. NULL with size zero queries required size.
     * @param buffer_size [type:uint32_t] Destination capacity in bytes.
     * @param out_size [type:uint32_t*] Required byte size on OK or BUFFER_TOO_SMALL; unchanged on other errors.
     * @return result [type:DSResult] OK, BUFFER_TOO_SMALL, INVALID_ARGUMENT for an invalid size/buffer, or LOCKED during query reservations. Buffer is unchanged on errors.
     */
    DSResult DSWriteBlob(HDSContext store, void* buffer, uint32_t buffer_size, uint32_t* out_size);
}

#endif // DM_DATA_H
