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

    HDSContext DSCreateStore(void);
    DSResult   DSDestroyStore(HDSContext store);

    void       DSStoreLock(HDSContext store);
    void       DSStoreUnlock(HDSContext store);

#ifdef __cplusplus
}
#endif

// API documentation

/*# Data API
 *
 * Datastore lifetime and locking. Includes the query, iterator and field APIs.
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

/*# Create an empty store
 *
 * The caller owns the store and releases it with [ref:DSDestroyStore].
 *
 * @name DSCreateStore
 * @return store [type:HDSContext] New store.
 */

/*# Destroy a store
 *
 * Frees the store and its tables/rows. Destroy queries and finish all access first.
 * Borrowed blob memory remains caller-owned.
 *
 * @name DSDestroyStore
 * @param store [type:HDSContext] Store owned by the caller.
 * @return result [type:DSResult] OK, or LOCKED while locked or reserved by queries; leaves the store intact on failure.
 */

/*# Lock store structure
 *
 * Keeps rows and query bindings stable while allowing value writes and resets.
 * Structural changes return LOCKED. Locks nest and must be unlocked on the same
 * thread after iteration. Use query reservations separately for concurrent jobs.
 *
 * @name DSStoreLock
 * @param store [type:HDSContext] Store to lock.
 */

/*# Unlock store structure
 *
 * Releases one lock. Finish iteration before the outermost unlock.
 *
 * @name DSStoreUnlock
 * @param store [type:HDSContext] Store locked by this thread.
 */

#endif // DMSDK_DATASTORE_H
