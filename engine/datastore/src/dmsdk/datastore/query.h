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

#ifndef DMSDK_DATASTORE_QUERY_H
#define DMSDK_DATASTORE_QUERY_H

#include <dmsdk/datastore/types.h>
#include <dmsdk/dlib/hash.h>

#define DS_QUERY_MAX_ITEMS 32

#ifdef __cplusplus
extern "C"
{
#endif

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

#ifdef __cplusplus
}
#endif

// API documentation

/*# Data query API
 *
 * Query filters, field bindings and concurrent access reservations.
 *
 * @document
 * @name DSQuery
 * @language C
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

#endif // DMSDK_DATASTORE_QUERY_H
