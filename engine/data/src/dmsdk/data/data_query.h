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

#ifndef DMSDK_DATA_QUERY_H
#define DMSDK_DATA_QUERY_H

#include <dmsdk/data/data_types.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum DataAccess
    {
        DATA_ACCESS_READ = 0,
        DATA_ACCESS_READ_WRITE = 1,
    } DataAccess;

    typedef struct DataQueryField
    {
        uint64_t      m_Field;
        DataValueType m_Type;
        DataAccess    m_Access;
    } DataQueryField;

    typedef struct DataQueryDesc
    {
        const DataGroupId*    m_GroupIds;
        uint32_t              m_GroupIdCount;
        const uint64_t*       m_AllTags;
        uint32_t              m_AllTagCount;
        const DataQueryField* m_Fields;
        uint32_t              m_FieldCount;
    } DataQueryDesc;

    DataResult DataCreateQuery(HDataStore store, const DataQueryDesc* desc, HDataQuery* out_query);
    DataResult DataDestroyQuery(HDataQuery query);

    DataResult DataQueryTryBegin(HDataQuery query);
    uint32_t   DataQueryGetRowCount(HDataQuery query);
    void       DataQueryEnd(HDataQuery query);

    uint32_t   DataQueryFindField(HDataQuery query, const DataQueryField* field);

#ifdef __cplusplus
}
#endif

// API documentation

/*# Data query API
 *
 * Query filters, field bindings and concurrent access reservations.
 *
 * @document
 * @name DataQuery
 * @language C
 */

/*# Query access mode
 *
 * @enum
 * @name DataAccess
 * @member DATA_ACCESS_READ Shared read access (0), the default.
 * @member DATA_ACCESS_READ_WRITE Exclusive read/write access (1).
 */

/*# Required query field
 *
 * Matches a full-name hash and exact kind. For an inline member use, for example,
 * dmHashString64("light.color"). Dynamic containers cannot bind members. Zero-initialize.
 *
 * @struct
 * @name DataQueryField
 * @member m_Field [type:uint64_t] Full field name hash, including dot-separated inline member names.
 * @member m_Type [type:DataValueType] Exact leaf kind.
 * @member m_Access [type:DataAccess] Reserved access; defaults to read. Writes require fixed-size fields.
 */

/*# Query filter descriptor
 *
 * Requires all tags and fields in one table, and matches any listed group.
 * Empty filters impose no restriction. Bind requested fields with DataQueryFindField.
 * Zero-initialize; filter arrays are copied during creation.
 *
 * @struct
 * @name DataQueryDesc
 * @member m_GroupIds [type:const DataGroupId*] Group IDs; optional when count is zero.
 * @member m_GroupIdCount [type:uint32_t] Group count; zero matches all.
 * @member m_AllTags [type:const uint64_t*] Required tags; optional when count is zero.
 * @member m_AllTagCount [type:uint32_t] Tag count; zero matches all.
 * @member m_Fields [type:const DataQueryField*] Required fields; optional when count is zero.
 * @member m_FieldCount [type:uint32_t] Field count; zero matches all.
 */

/*# Create a reusable live query
 *
 * Copies filters and tracks matching tables, including later registrations.
 * Destroy the query before its store.
 *
 * @name DataCreateQuery
 * @param store [type:HDataStore] Store handle.
 * @param desc [type:const DataQueryDesc*] Filters to copy.
 * @param out_query [type:HDataQuery*] Receives the owned query on success.
 * @return result [type:DataResult] OK, INVALID_ARGUMENT for invalid filters/access, or LOCKED during query reservations.
 */

/*# Destroy a query
 *
 * Frees the query. Finish using its iterators first.
 *
 * @name DataDestroyQuery
 * @param query [type:HDataQuery] Query whose store is still alive.
 * @return result [type:DataResult] OK, or LOCKED while query reservations are active.
 */

/*# Reserve query access
 *
 * Thread-safe, nonblocking reservation that keeps rows and bindings stable.
 * Overlapping fields on the same table allow read/read access; writes conflict.
 * Group filters do not narrow reservations. Empty field filters reserve whole-row reads.
 * Writes require fixed-size fields with DATA_ACCESS_READ_WRITE;
 * string replacement and resets must happen outside reservations.
 * Each query supports one active reservation, shared by its jobs. The caller schedules
 * and synchronizes jobs, then calls DataQueryEnd once, even for an empty query.
 * Do not mix reservations with DataStoreLock or ID-based access.
 * The first begin after structural changes may allocate; later begins reuse storage.
 *
 * @name DataQueryTryBegin
 * @param query [type:HDataQuery] Query to reserve.
 * @return result [type:DataResult] OK, or BUSY without acquiring access if already active or unavailable.
 */

/*# Count reserved rows
 *
 * @name DataQueryGetRowCount
 * @param query [type:HDataQuery] Query with an active reservation.
 * @return count [type:uint32_t] Reserved matching row count.
 */

/*# Release query access
 *
 * Thread-safe release. The caller must join all jobs and finish using iterators and pointers first.
 *
 * @name DataQueryEnd
 * @param query [type:HDataQuery] Query with a successful begin to release once.
 */

/*# Find a query field for direct pointer access
 *
 * Binds a requested full-name hash, kind and access to a handle valid until query destruction.
 * The handle is neither a byte offset nor a query descriptor index. No lock or matching rows
 * are required. Supports Number, Boolean, Vector3, Vector4 and Matrix4.
 * Pointer getters require this query's current row and matching typed handle, with
 * its store locked or reservation active. No kind, bounds or lifetime checks occur.
 * Writable pointers require DATA_ACCESS_READ_WRITE during reservations.
 * Borrowed pointers expire on row/batch advance, unlock, query end, or another
 * mutating accessor/reset for the row/table. Never write through a read pointer.
 *
 * @name DataQueryFindField
 * @param query [type:HDataQuery] Query to bind.
 * @param field [type:const DataQueryField*] Full-name hash, kind and access to match; borrowed for this call. Access must not exceed the query declaration.
 * @return field [type:uint32_t] Field handle, or UINT32_MAX if absent, incompatible or unsupported.
 */

#endif // DMSDK_DATA_QUERY_H
