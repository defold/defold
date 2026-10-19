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

#ifndef DMSDK_DATASTORE_BLOB_H
#define DMSDK_DATASTORE_BLOB_H

#include <dmsdk/datastore/datastore_types.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct DSBlob*         HDSBlob;
    typedef struct DSBlobInstance* HDSBlobInstance;

    DSResult                       DSLoadBlob(const void* buffer, uint32_t buffer_size, HDSBlob* out_blob);
    void                           DSDestroyBlob(HDSBlob blob);
    DSResult                       DSAddBlob(HDSContext store, HDSBlob blob, DSGroupId group, HDSBlobInstance* out_instance);
    DSResult                       DSRemoveBlob(HDSBlobInstance instance);
    DSResult                       DSResetBlob(HDSBlobInstance instance);

#ifdef __cplusplus
}
#endif

// API documentation

/*# Data blob lifecycle
 *
 * Load borrowed resource bytes and instantiate or remove grouped component rows.
 *
 * @document
 * @name DSBlob
 * @language C
 */

/*# Loaded blob handle
 *
 * Owns a reference to validated metadata, borrowing the caller's immutable bytes.
 * Release with DSDestroyBlob. Store-owned references are synchronized across
 * stores; keep a live reference while other threads call DSAddBlob.
 *
 * @typedef
 * @name HDSBlob
 */

/*# Blob instance handle
 *
 * Identifies one grouped registration in a store. Release with DSRemoveBlob,
 * or by destroying the store. This does not own the caller's blob bytes.
 *
 * @typedef
 * @name HDSBlobInstance
 */

/*# Load an immutable packed resource
 *
 * Validates all tables and borrows the supplied buffer without copying or modifying it.
 * The input must be eight-byte aligned. Member offsets and row/struct sizes are
 * validated for C-compatible alignment. The caller keeps the buffer unchanged and alive
 * until the blob reference and all its registrations have been released.
 * Loading does not register rows in a store.
 *
 * @name DSLoadBlob
 * @param buffer [type:const void*] Caller-owned complete packed component-data blob.
 * @param buffer_size [type:uint32_t] Blob byte size.
 * @param out_blob [type:HDSBlob*] Receives a caller-owned reference on success; unchanged on error.
 * @return result [type:DSResult] OK or INVALID_FORMAT for malformed/unsupported data or an unaligned buffer.
 */

/*# Release a loaded resource reference
 *
 * Resource pools retain a reference while any registrations remain. The last release frees the blob handle,
 * never the caller's buffer. Does not remove any instance's rows. The caller may
 * free the buffer after releasing this reference and removing all registrations.
 *
 * @name DSDestroyBlob
 * @param blob [type:HDSBlob] Reference to release; must not be used afterward.
 */

/*# Add every component table for one game object instance
 *
 * Retains the resource and copies fixed row bytes into shared mutable tables.
 * Registrations of the same loaded resource share metadata, defaults and capacity.
 * Strings and containers retain blob offsets. The caller keeps the immutable
 * buffer alive until all references and registrations are released.
 * Requires an unlocked store without active query reservations.
 *
 * @name DSAddBlob
 * @param store [type:HDSContext] Destination store.
 * @param blob [type:HDSBlob] Loaded resource to retain.
 * @param group [type:DSGroupId] Runtime game object group assigned to every added row.
 * @param out_instance [type:HDSBlobInstance*] Receives the registration handle on success; unchanged on error.
 * @return result [type:DSResult] OK, LOCKED while the store is locked or reserved, or INVALID_ARGUMENT if store capacity would be exceeded. Errors leave the store unchanged.
 */

/*# Remove one instance's complete set of component tables
 *
 * Removes its remaining rows, invalidates their IDs, frees its replacement payloads,
 * and returns the registration slot for reuse. Shared capacity remains while other
 * registrations use that resource; the last removal frees the pool and its blob reference.
 * The store also removes its instances automatically when destroyed.
 *
 * @name DSRemoveBlob
 * @param instance [type:HDSBlobInstance] Registration to remove; must not be used after success.
 * @return result [type:DSResult] OK, or LOCKED while its store is locked or reserved (no changes).
 */

/*# Reset a game object's grouped component registration
 *
 * Copies loaded values into this registration's remaining mutable rows and frees its replacement payload blocks.
 * Does not restore removed rows or invalidate iterators. Borrowed replacement payloads expire.
 * Other registrations are unaffected, even if they share a blob, type or group ID.
 * The engine retains this handle when registering a game object's component tables;
 * no datastore-wide group search is needed. Independently added rows are not included.
 *
 * @name DSResetBlob
 * @param instance [type:HDSBlobInstance] Registration to reset.
 * @return result [type:DSResult] OK, or LOCKED during query reservations.
 */

#endif // DMSDK_DATASTORE_BLOB_H
