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

#ifndef DMSDK_DATA_BLOB_H
#define DMSDK_DATA_BLOB_H

#include <dmsdk/data/data_types.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct DataBlob*         HDataBlob;
    typedef struct DataBlobInstance* HDataBlobInstance;

    DataResult                       DataLoadBlob(const void* buffer, uint32_t buffer_size, HDataBlob* out_blob);
    void                             DataDestroyBlob(HDataBlob blob);
    DataResult                       DataAddBlob(HDataStore store, HDataBlob blob, DataGroupId group, HDataBlobInstance* out_instance);
    DataResult                       DataRemoveBlob(HDataBlobInstance instance);
    DataResult                       DataResetBlob(HDataBlobInstance instance);

#ifdef __cplusplus
}
#endif

// API documentation

/*# Data blob lifecycle
 *
 * Load borrowed resource bytes and instantiate or remove grouped component rows.
 *
 * @document
 * @name DataBlob
 * @language C
 */

/*# Loaded blob handle
 *
 * Owns a reference to validated metadata, borrowing the caller's immutable bytes.
 * Release with DataDestroyBlob.
 *
 * @typedef
 * @name HDataBlob
 */

/*# Blob instance handle
 *
 * Identifies one grouped registration in a store. Release with DataRemoveBlob,
 * or by destroying the store. This does not own the caller's blob bytes.
 *
 * @typedef
 * @name HDataBlobInstance
 */

/*# Load an immutable packed resource
 *
 * Validates all tables and borrows the supplied buffer without copying or modifying it.
 * The input must be eight-byte aligned. Member offsets and row/struct sizes are
 * validated for C-compatible alignment. The caller keeps the buffer unchanged and alive
 * until the blob reference and all its registrations have been released.
 * Loading does not register rows in a store.
 *
 * @name DataLoadBlob
 * @param buffer [type:const void*] Caller-owned complete packed component-data blob.
 * @param buffer_size [type:uint32_t] Blob byte size.
 * @param out_blob [type:HDataBlob*] Receives a caller-owned reference on success; unchanged on error.
 * @return result [type:DataResult] OK or INVALID_FORMAT for malformed/unsupported data or an unaligned buffer.
 */

/*# Release a loaded resource reference
 *
 * Resource pools retain a reference while any registrations remain. The last release frees the blob handle,
 * never the caller's buffer. Does not remove any instance's rows. The caller may
 * free the buffer after releasing this reference and removing all registrations.
 *
 * @name DataDestroyBlob
 * @param blob [type:HDataBlob] Reference to release; must not be used afterward.
 */

/*# Add every component table for one game object instance
 *
 * Retains the resource and copies fixed row bytes into shared mutable tables.
 * Registrations of the same loaded resource share metadata, defaults and capacity.
 * Strings and containers retain blob offsets. The caller keeps the immutable
 * buffer alive until all references and registrations are released.
 * Requires an unlocked store without active query reservations.
 *
 * @name DataAddBlob
 * @param store [type:HDataStore] Destination store.
 * @param blob [type:HDataBlob] Loaded resource to retain.
 * @param group [type:DataGroupId] Runtime game object group assigned to every added row.
 * @param out_instance [type:HDataBlobInstance*] Receives the registration handle on success; unchanged on error.
 * @return result [type:DataResult] OK, LOCKED while the store is locked or reserved, or INVALID_ARGUMENT if store capacity would be exceeded. Errors leave the store unchanged.
 */

/*# Remove one instance's complete set of component tables
 *
 * Removes its remaining rows, invalidates their IDs, frees its replacement payloads,
 * and returns the registration slot for reuse. Shared capacity remains while other
 * registrations use that resource; the last removal frees the pool and its blob reference.
 * The store also removes its instances automatically when destroyed.
 *
 * @name DataRemoveBlob
 * @param instance [type:HDataBlobInstance] Registration to remove; must not be used after success.
 * @return result [type:DataResult] OK, or LOCKED while its store is locked or reserved (no changes).
 */

/*# Reset a game object's grouped component registration
 *
 * Copies loaded values into this registration's remaining mutable rows and frees its replacement payload blocks.
 * Does not restore removed rows or invalidate iterators. Borrowed replacement payloads expire.
 * Other registrations are unaffected, even if they share a blob, type or group ID.
 * The engine retains this handle when registering a game object's component tables;
 * no datastore-wide group search is needed. Independently added rows are not included.
 *
 * @name DataResetBlob
 * @param instance [type:HDataBlobInstance] Registration to reset.
 * @return result [type:DataResult] OK, or LOCKED during query reservations.
 */

#endif // DMSDK_DATA_BLOB_H
