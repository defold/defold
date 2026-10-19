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

#include <dmsdk/datastore/datastore_types.h>
#include <dmsdk/datastore/datastore_query.h>
#include <dmsdk/datastore/datastore_iter.h>
#include <dmsdk/datastore/datastore_field.h>
#include <dmsdk/datastore/datastore_container.h>
#include <dmsdk/datastore/datastore_table.h>
#include <dmsdk/datastore/datastore_blob.h>

#define DATASTORE_CONTEXT_NAME "datastore"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct DSCreateParams
    {
        uint32_t m_PageSize;
        uint32_t m_RowChunkSize;
        uint32_t m_MaxSparePages;
    } DSCreateParams;

    HDSContext DSCreateStore(const DSCreateParams* params);
    DSResult   DSDestroyStore(HDSContext store);

#ifdef __cplusplus
}
#endif

// API documentation

/*# Data API
 *
 * Datastore lifetime. Includes the query, iterator and field APIs.
 *
 * @document
 * @name DS
 * @language C
 */

/*# Engine datastore context
 *
 * Registry name for the engine-owned [type:HDSContext], available from extension
 * app initialization through app finalization. Borrow the store; do not destroy
 * it. Finish jobs and release your queries and tables before finalization ends.
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
 * Frees the store and its tables/rows. Destroy queries and finish all access first.
 * Borrowed blob memory remains caller-owned.
 *
 * @name DSDestroyStore
 * @param store [type:HDSContext] Store owned by the caller.
 * @return result [type:DSResult] OK, or LOCKED while reserved by queries; leaves the store intact on failure.
 */

#endif // DMSDK_DATASTORE_H
