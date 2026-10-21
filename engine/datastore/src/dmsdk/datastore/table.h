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

#ifndef DMSDK_DATASTORE_TABLE_H
#define DMSDK_DATASTORE_TABLE_H

#include <dmsdk/datastore/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct DSStructDesc DSStructDesc;
    typedef struct DSFieldDesc
    {
        uint64_t            m_Field;  // Field name hash.
        DSValueType         m_Type;   // Declared kind; fixed for the table lifetime.
        uint32_t            m_Offset; // Byte offset within the containing row/struct.
        const DSStructDesc* m_Struct; // Reusable inline layout; NULL for scalar/dynamic values.
        const char*         m_Name;   // Borrowed local name for inline composition; optional for other root fields.
    } DSFieldDesc;

    typedef struct DSStructDesc
    {
        const DSFieldDesc* m_Fields;     // Borrowed member descriptions; copied at registration.
        uint32_t           m_FieldCount; // Number of immediate members.
        uint32_t           m_Size;       // Struct size in bytes, including trailing padding.
    } DSStructDesc;

    typedef enum DSResetPolicy
    {
        DS_RESET_RETAIN = 0,
        DS_RESET_NONE = 1,
    } DSResetPolicy;

    typedef struct DSTableDesc
    {
        uint64_t           m_Type;        // Data type name hash.
        const uint64_t*    m_Tags;        // Borrowed tag hashes; copied at registration.
        uint32_t           m_TagCount;    // Number of tags.
        const DSFieldDesc* m_Fields;      // Borrowed root field descriptions; copied at registration.
        uint32_t           m_FieldCount;  // Number of root fields.
        uint32_t           m_RowStride;   // Bytes per row, including trailing alignment padding.
        DSResetPolicy      m_ResetPolicy; // Retain native initial values by default; NONE disables reset.
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

#ifdef __cplusplus
}
#endif

// API documentation

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
 * @member DS_RESET_RETAIN Keep initial values for row/field reset; the default.
 * @member DS_RESET_NONE Store current values only; reset returns INVALID_ARGUMENT.
 */

/*# Data table and row lifecycle
 *
 * Register native layouts, create rows and remove or reset instances.
 *
 * @document
 * @name DSTable
 * @language C
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
 * @member m_ResetPolicy [type:DSResetPolicy] Retain initial native values, or disable reset with NONE.
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
 * group to every row; groups can share a table. Supplied values become reset
 * defaults unless m_ResetPolicy is DS_RESET_NONE. Native rows have no component name; DSGetComponentId returns zero.
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
 * @param group [type:DSGroupId] Caller-defined group assigned to every row; zero is valid.
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
 * @param group [type:DSGroupId] Group assigned to every row; zero is valid.
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

#endif // DMSDK_DATASTORE_TABLE_H
