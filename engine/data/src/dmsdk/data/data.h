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

#ifndef DMSDK_DATA_H
#define DMSDK_DATA_H

#include <dmsdk/data/data_types.h>
#include <dmsdk/data/data_query.h>
#include <dmsdk/data/data_iter.h>
#include <dmsdk/data/data_field.h>

#ifdef __cplusplus
extern "C"
{
#endif

    HDataStore DataCreateStore(void);
    DataResult DataDestroyStore(HDataStore store);

    void       DataStoreLock(HDataStore store);
    void       DataStoreUnlock(HDataStore store);

#ifdef __cplusplus
}
#endif

// API documentation

/*# Data API
 *
 * Datastore lifetime and locking. Includes the query, iterator and field APIs.
 *
 * @document
 * @name Data
 * @language C
 */

/*# Create an empty store
 *
 * The caller owns the store and releases it with [ref:DataDestroyStore].
 *
 * @name DataCreateStore
 * @return store [type:HDataStore] New store.
 */

/*# Destroy a store
 *
 * Frees the store and its tables/rows. Destroy queries and finish all access first.
 * Borrowed blob memory remains caller-owned.
 *
 * @name DataDestroyStore
 * @param store [type:HDataStore] Store owned by the caller.
 * @return result [type:DataResult] OK, or LOCKED while locked or reserved by queries; leaves the store intact on failure.
 */

/*# Lock store structure
 *
 * Keeps rows and query bindings stable while allowing value writes and resets.
 * Structural changes return LOCKED. Locks nest and must be unlocked on the same
 * thread after iteration. Use query reservations separately for concurrent jobs.
 *
 * @name DataStoreLock
 * @param store [type:HDataStore] Store to lock.
 */

/*# Unlock store structure
 *
 * Releases one lock. Finish iteration before the outermost unlock.
 *
 * @name DataStoreUnlock
 * @param store [type:HDataStore] Store locked by this thread.
 */

#endif // DMSDK_DATA_H
