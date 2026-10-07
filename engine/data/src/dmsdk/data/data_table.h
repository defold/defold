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

#ifndef DMSDK_DATA_TABLE_H
#define DMSDK_DATA_TABLE_H

#include <dmsdk/data/data_types.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct DataStructDesc DataStructDesc;
    typedef struct DataFieldDesc
    {
        uint64_t              m_Field;  // Field name hash.
        DataValueType         m_Type;   // Declared kind; fixed for the table lifetime.
        uint32_t              m_Offset; // Byte offset within the containing row/struct.
        const DataStructDesc* m_Struct; // Reusable inline layout; NULL for scalar/dynamic values.
        const char*           m_Name;   // Borrowed local name for inline composition; optional for other root fields.
    } DataFieldDesc;

    typedef struct DataStructDesc
    {
        const DataFieldDesc* m_Fields;     // Borrowed member descriptions; copied at registration.
        uint32_t             m_FieldCount; // Number of immediate members.
        uint32_t             m_Size;       // Struct size in bytes, including trailing padding.
    } DataStructDesc;

    typedef struct DataTableDesc
    {
        uint64_t             m_Type;       // Data type name hash.
        const uint64_t*      m_Tags;       // Borrowed tag hashes; copied at registration.
        uint32_t             m_TagCount;   // Number of tags.
        const DataFieldDesc* m_Fields;     // Borrowed root field descriptions; copied at registration.
        uint32_t             m_FieldCount; // Number of root fields.
        uint32_t             m_RowStride;  // Bytes per row, including trailing alignment padding.
    } DataTableDesc;

    typedef struct DataFieldArray
    {
        uint64_t    m_Field;  // Root field name hash.
        const void* m_Values; // Borrowed contiguous array of native field values.
    } DataFieldArray;

    typedef struct DataStructInput
    {
        const DataStructDesc* m_Layout; // Native member layout, borrowed through creation.
        const void*           m_Values; // One native struct matching m_Layout.
    } DataStructInput;

    typedef struct DataListInput
    {
        DataValueType        m_Type;   // Shared element kind when m_Types is NULL.
        uint32_t             m_Count;  // Number of elements.
        const void*          m_Values; // Native array, or array of payload pointers when m_Types is set.
        const DataValueType* m_Types;  // Optional per-element kinds; selects mixed input.
    } DataListInput;

    typedef union DataReference
    {
        const char*            m_String; // NUL-terminated string input.
        const DataStructInput* m_Struct; // Dynamic struct input.
        const DataListInput*   m_List;   // Dynamic list input.
        // private
        uint64_t m_Storage; // Keeps reference slots eight bytes on 32-bit and 64-bit hosts.
    } DataReference;

    DataResult DataRegisterTable(HDataStore store, const DataTableDesc* desc);
    DataResult DataUnregisterTable(HDataStore store, uint64_t type);
    DataResult DataCreateRows(HDataStore store, uint64_t type, DataGroupId group, uint32_t count, const void* rows, DataId* out_ids);
    DataResult DataCreateRowsSoA(HDataStore store, uint64_t type, DataGroupId group, uint32_t count, uint32_t field_count, const DataFieldArray* fields, DataId* out_ids);
    DataResult DataRemoveRow(HDataStore store, DataId id);
    DataResult DataResetRow(HDataStore store, DataId id);
    uint64_t   DataGetComponentId(HDataStore store, DataId id);

#ifdef __cplusplus
}
#endif

// API documentation

/*# Data table and row lifecycle
 *
 * Register native layouts, create rows and remove or reset instances.
 *
 * @document
 * @name DataTable
 * @language C
 */

/*# Field layout
 *
 * Offsets follow C alignment. Inline members and their parents require single-segment
 * names for full-name hashing. Registration copies the compiled metadata.
 *
 * @struct
 * @name DataFieldDesc
 * @member m_Field [type:uint64_t] Local field name hash.
 * @member m_Type [type:DataValueType] Fixed field kind.
 * @member m_Offset [type:uint32_t] Byte offset in the containing row or struct.
 * @member m_Struct [type:const DataStructDesc*] Inline STRUCT layout, otherwise NULL.
 * @member m_Name [type:const char*] Borrowed local name; required for inline parents/members, without dots.
 */

/*# Inline struct layout
 *
 * Reusable metadata, copied by registration. Members must not overlap. Layouts must
 * be acyclic. Inline structs and dynamic containers share a 64-level nesting limit.
 *
 * @struct
 * @name DataStructDesc
 * @member m_Fields [type:const DataFieldDesc*] Borrowed member descriptions.
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
 * @name DataTableDesc
 * @member m_Type [type:uint64_t] Type name hash, unique among independently registered tables.
 * @member m_Tags [type:const uint64_t*] Borrowed tag hashes.
 * @member m_TagCount [type:uint32_t] Number of tags.
 * @member m_Fields [type:const DataFieldDesc*] Borrowed root field descriptions.
 * @member m_FieldCount [type:uint32_t] Number of root fields.
 * @member m_RowStride [type:uint32_t] Native row size including trailing alignment padding.
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
 * @name DataReference
 * @member m_String [type:const char*] Borrowed NUL-terminated string.
 * @member m_Struct [type:const DataStructInput*] Borrowed dynamic struct.
 * @member m_List [type:const DataListInput*] Borrowed dynamic list.
 */

/*# Dynamic struct input
 *
 * Describes one native struct, with per-input member names and kinds. The table
 * field uses STRUCT without an inline layout. Members may contain DataReference
 * values, including further containers. Inputs must be acyclic, no deeper than
 * 64 container levels including enclosing inline structs; names within each
 * struct must be unique.
 *
 * @struct
 * @name DataStructInput
 * @member m_Layout [type:const DataStructDesc*] Borrowed native member layout; required even for an empty struct.
 * @member m_Values [type:const void*] Borrowed native struct bytes; NULL only when layout size is zero.
 */

/*# Dynamic list input
 *
 * With m_Types NULL, m_Type describes a contiguous native array. Numeric elements
 * use their native API types; STRING, STRUCT and LIST elements use DataReference.
 * With m_Types set, m_Values is a const void* array of payload pointers: addresses
 * of native scalar/vector/matrix values, string bytes, DataStructInput or
 * DataListInput. NULL-kind elements need no payload. m_Type is ignored.
 * Both arrays contain m_Count entries. Inputs are borrowed through creation;
 * values are copied, including nested payloads. Input rules match DataStructInput.
 *
 * @struct
 * @name DataListInput
 * @member m_Type [type:DataValueType] Shared element kind when m_Types is NULL; otherwise ignored.
 * @member m_Count [type:uint32_t] Number of elements.
 * @member m_Values [type:const void*] Native array or array of payload pointers. NULL only for an empty list or a homogeneous NULL list.
 * @member m_Types [type:const DataValueType*] Optional array of per-element kinds; NULL selects homogeneous input.
 */

/*# Create rows from native input
 *
 * Copies count complete rows using the registered C layout and row size. Assigns
 * group to every row; groups can share a table. Supplied values become reset
 * defaults. Native rows have no component name; DataGetComponentId returns zero.
 * Supports fixed-size values, inline structs and DataReference fields. Strings
 * and dynamic containers are copied into table-owned storage, including nested
 * payloads; all input memory can be released after the call. Dynamic struct fields
 * share layout metadata and own separate child rows. Removal releases those rows
 * for reuse; reset releases replacements and restores the original children.
 * Removed rows' default slots are reused. String/list arena storage remains
 * until the table is empty or destroyed.
 * Booleans must be 0 or 1. Inputs, including padding, must be initialized, disjoint
 * from store storage and valid through the call. On error no rows are published
 * and output IDs remain unchanged. Nonempty batches require an unlocked,
 * unreserved store. An empty batch returns OK without accessing the store or arrays.
 *
 * @name DataCreateRows
 * @param store [type:HDataStore] Destination store.
 * @param type [type:uint64_t] Registered table type.
 * @param group [type:DataGroupId] Caller-defined group assigned to every row; zero is valid.
 * @param count [type:uint32_t] Rows to add; zero adds nothing.
 * @param rows [type:const void*] Borrowed contiguous native rows; NULL for zero-size rows or an empty batch.
 * @param out_ids [type:DataId*] Optional array receiving count stable IDs on success; NULL skips the output. IDs remain available through row iteration.
 * @return result [type:DataResult] OK, LOCKED, NOT_FOUND for an unknown type, ALREADY_EXISTS for duplicate dynamic member names, or INVALID_ARGUMENT for incompatible input or capacity limits.
 */

/*# Native field array
 *
 * One input column for DataCreateRowsSoA. The registered layout determines the
 * element type and size; an inline struct uses complete native struct elements.
 * STRING, dynamic STRUCT and LIST columns use DataReference elements.
 *
 * @struct
 * @name DataFieldArray
 * @member m_Field [type:uint64_t] Root field name hash.
 * @member m_Values [type:const void*] Borrowed contiguous native values; NULL only for a zero-size field or an empty batch.
 */

/*# Create rows from field arrays
 *
 * Supply every root field exactly once, in any order, with count values per array.
 * Copies directly into stored byte rows without a temporary array of native rows.
 * Types and element sizes come from the registered layout. Inline struct fields
 * use arrays of complete structs, not separate arrays for their nested members.
 * Inputs, including struct padding, must be initialized, disjoint from store
 * storage and valid through the call. Values become reset defaults. Group,
 * locking and reference ownership rules match DataCreateRows. On error no rows are
 * published and output IDs remain unchanged.
 *
 * @name DataCreateRowsSoA
 * @param store [type:HDataStore] Destination store.
 * @param type [type:uint64_t] Registered table type.
 * @param group [type:DataGroupId] Group assigned to every row; zero is valid.
 * @param count [type:uint32_t] Rows to add; zero returns OK without accessing the store or arrays.
 * @param field_count [type:uint32_t] Number of root field arrays.
 * @param fields [type:const DataFieldArray*] Borrowed field arrays; NULL for a table without fields or an empty batch.
 * @param out_ids [type:DataId*] Optional array receiving count stable IDs on success; NULL skips the output. IDs remain available through row iteration.
 * @return result [type:DataResult] OK, LOCKED, NOT_FOUND for an unknown table, ALREADY_EXISTS for duplicate dynamic member names, or INVALID_ARGUMENT for invalid fields, input or capacity limits.
 */

/*# Register an empty table
 *
 * Copies tags and field metadata. Fields have fixed kinds and non-overlapping byte offsets.
 * Duplicate type hashes and full-name field hashes (including inline members) are rejected. Successful
 * registration updates live queries. Requires an unlocked store without active query reservations.
 *
 * @name DataRegisterTable
 * @param store [type:HDataStore] Store handle.
 * @param desc [type:const DataTableDesc*] Table type, tags and shared field layout to copy.
 * @return result [type:DataResult] LOCKED while the store is locked or reserved; otherwise OK, ALREADY_EXISTS for a duplicate type/field name, or INVALID_ARGUMENT for invalid tags/count/layout.
 */

/*# Unregister a table
 *
 * Removes an independently registered table and all its rows. Their IDs become stale, even if the type is
 * registered again. Updates live queries. Requires an unlocked store without active query reservations.
 *
 * @name DataUnregisterTable
 * @param store [type:HDataStore] Store handle.
 * @param type [type:uint64_t] Table type hash.
 * @return result [type:DataResult] LOCKED while the store is locked or reserved; otherwise OK on success, or NOT_FOUND if the type is not registered.
 */

/*# Remove a row
 *
 * Uses swap removal; other live IDs remain valid. The removed ID becomes stale.
 * Requires an unlocked store without active query reservations. To remove during traversal, collect DataIds in a
 * caller-owned reusable buffer and apply removals after the outermost unlock.
 * Reclaims the default row slot in independently registered tables. Their string/list
 * arenas are released when the table becomes empty; loaded instances retain replacement
 * arenas until instance reset or destruction.
 *
 * @name DataRemoveRow
 * @param store [type:HDataStore] Store handle.
 * @param id [type:DataId] ID of the row to remove.
 * @return result [type:DataResult] LOCKED while the store is locked or reserved; otherwise OK on success, or NOT_FOUND for an invalid or stale ID.
 */

/*# Reset one component row
 *
 * Restores all fields, including nested values, to their loaded or added defaults.
 * Other rows are unaffected, including other components with the same group or type.
 * Does not allocate, change IDs or query membership, or invalidate iterators.
 * Mutable row storage stays in place. Registration/table-owned replacement payloads
 * remain allocated until DataResetBlob, table/instance destruction or removal of
 * the last independently created row.
 * Returns LOCKED during query reservations.
 *
 * @name DataResetRow
 * @param store [type:HDataStore] Store containing the component row.
 * @param id [type:DataId] Component's runtime row ID.
 * @return result [type:DataResult] OK, LOCKED during query reservations, or NOT_FOUND for an invalid or stale ID.
 */

/*# Read a component's prototype-local name hash
 *
 * Used by game object integration to associate a runtime row with its component.
 *
 * @name DataGetComponentId
 * @param store [type:HDataStore] Store containing the row.
 * @param id [type:DataId] Runtime row ID.
 * @return component [type:uint64_t] Component name hash, or zero for an invalid/stale row. Zero may also identify an unnamed row.
 */

#endif // DMSDK_DATA_TABLE_H
