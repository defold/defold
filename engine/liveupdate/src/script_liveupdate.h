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

#ifndef DM_SCRIPT_LIVEUPDATE_H
#define DM_SCRIPT_LIVEUPDATE_H

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
}

#include <resource/resource.h>

namespace dmLiveUpdate
{
    void ScriptInit(lua_State* L, dmResource::HFactory factory);
}

/*# LiveUpdate API documentation
 *
 * Functions and constants to access resources.
 *
 * @document
 * @name LiveUpdate
 * @namespace liveupdate
 * @language Lua
 */

/*# Get current mounts
 *
 * Get an array of the current mounts
 * This can be used to determine if a new mount is needed or not
 *
 * @name liveupdate.get_mounts
 * @return mounts [type:table] Array of mounts
 * @note: Any mount with priority < 0 is considered a base archive and it cannot be removed. All other mounts are considered "live update" content
 * @examples
 *
 * Output the current resource mounts
 *
 * ```lua
 * pprint("MOUNTS", liveupdate.get_mounts())
 * ```
 *
 * Give an output like:
 *
 * ```lua
 * DEBUG:SCRIPT: MOUNTS,
 * { --[[0x119667bf0]]
 *   1 = { --[[0x119667c50]]
 *     name = hash: [liveupdate],
 *     uri = "zip:/device/path/to/acchives/liveupdate.zip",
 *     priority = 5
 *   },
 *   2 = { --[[0x119667d50]]
 *     name = hash: [_base],
 *     uri = "archive:build/default/game.dmanifest",
 *     priority = -10
 *   }
 * }
 * ```
 */

/*# Add resource mount
 *
 * Adds a resource mount to the resource system.
 * After the mount succeeded, the resources are available to load. (i.e. no reboot required)
 *
 * @note The request is asynchronous
 * @note Mounts are active for the current session only
 * @note Names cannot start with '_'
 * @note Priority must be >= 0
 *
 * @name liveupdate.add_mount
 * @param name [type:string] Unique name of the mount
 * @param uri [type:string] The uri of the mount, including the scheme. Currently supported schemes are 'zip' and 'archive'.
 * @param priority [type:number] Priority of mount. Larger priority takes prescedence
 * @param callback [type:function(self, name, uri, result)] Callback after the asynchronous request completed
 * - `name` [type:hash] Unique name of the mount
 * - `uri` [type:string] The uri of the mount
 * - `result` [type:number] The result of the request
 *
 * @return result [type:number] The result of the request
 *
 * @examples
 *
 * Add multiple mounts. Higher priority takes precedence.
 *
 * ```lua
 * liveupdate.add_mount("common", "zip:/path/to/common_stuff.zip", 10, function (self, name, uri, result) end) -- base pack
 * liveupdate.add_mount("levelpack_1", "zip:/path/to/levels_1_to_20.zip", 20, function (self, name, uri, result) end) -- level pack
 * liveupdate.add_mount("season_pack_1", "zip:/path/to/easter_pack_1.zip", 30, function (self, name, uri, result) end) -- season pack, overriding content in the other packs
 * ```
 */


/*# Remove resource mount
 *
 * Remove a mount the resource system.
 * Removing a mount does not affect any loaded resources.
 *
 * @note The call is synchronous
 *
 * @name liveupdate.remove_mount
 * @param name [type:string] Unique name of the mount
 * @return result [type:number] The result of the call
 *
 * @examples
 *
 * Add multiple mounts. Higher priority takes precedence.
 *
 * ```lua
 * liveupdate.remove_mount("season_pack_1")
 * ```
 */

/*# check if the build contains excluded live update resources
 *
 * Checks if the bundled application was built with one or more resources
 * excluded from the main bundle, through a collection proxy with
 * `Exclude` enabled.
 *
 * This value is based on metadata in the bundled manifest. It does not check
 * whether any live update archive has been mounted or whether the excluded
 * resources are currently available on device.
 *
 * @name liveupdate.is_built_with_excluded_files
 * @return is_built_with_excluded_files [type:boolean] true if the bundled application was built with excluded files
 * @examples
 *
 * ```lua
 * if liveupdate.is_built_with_excluded_files() then
 *     print("The bundle expects live update content.")
 * end
 * ```
 */

 /*# LIVEUPDATE_OK
 *
 * @name liveupdate.LIVEUPDATE_OK
 * @constant
 */

 /*# LIVEUPDATE_INVALID_HEADER
 * The handled resource is invalid.
 *
 * @name liveupdate.LIVEUPDATE_INVALID_HEADER
 * @constant
 */

 /*# LIVEUPDATE_MEM_ERROR
 * Memory wasn't allocated
 *
 * @name liveupdate.LIVEUPDATE_MEM_ERROR
 * @constant
 */

 /*# LIVEUPDATE_INVALID_RESOURCE
 * The header of the resource is invalid.
 *
 * @name liveupdate.LIVEUPDATE_INVALID_RESOURCE
 * @constant
 */

 /*# LIVEUPDATE_VERSION_MISMATCH
 * Mismatch between manifest expected version and actual version.
 *
 * @name liveupdate.LIVEUPDATE_VERSION_MISMATCH
 * @constant
 */

 /*# LIVEUPDATE_ENGINE_VERSION_MISMATCH
 * Mismatch between running engine version and engine versions supported by manifest.
 *
 * @name liveupdate.LIVEUPDATE_ENGINE_VERSION_MISMATCH
 * @constant
 */

 /*# LIVEUPDATE_SIGNATURE_MISMATCH
 * Mismatch between expected and actual integrity data for legacy liveupdate verification.
 *
 * @name liveupdate.LIVEUPDATE_SIGNATURE_MISMATCH
 * @constant
 */

 /*# LIVEUPDATE_SCHEME_MISMATCH
 * Mismatch between scheme used to load resources. Resources are loaded with a different scheme than from manifest, for example over HTTP or directly from file. This is typically the case when running the game directly from the editor instead of from a bundle.
 *
 * @name liveupdate.LIVEUPDATE_SCHEME_MISMATCH
 * @constant
 */

 /*# LIVEUPDATE_BUNDLED_RESOURCE_MISMATCH
 * Mismatch between between expected bundled resources and actual bundled resources. The manifest expects a resource to be in the bundle, but it was not found in the bundle. This is typically the case when a non-excluded resource was modified between publishing the bundle and publishing the manifest.
 *
 * @name liveupdate.LIVEUPDATE_BUNDLED_RESOURCE_MISMATCH
 * @constant
 */

 /*# LIVEUPDATE_FORMAT_ERROR
 * Failed to parse manifest data buffer. The manifest was probably produced by a different engine version.
 *
 * @name liveupdate.LIVEUPDATE_FORMAT_ERROR
 * @constant
 */

 /*# LIVEUPDATE_IO_ERROR
 * I/O operation failed
 *
 * @name liveupdate.LIVEUPDATE_IO_ERROR
 * @constant
 */

 /*# LIVEUPDATE_INVAL
 * Argument was invalid
 *
 * @name liveupdate.LIVEUPDATE_INVAL
 * @constant
 */

 /*# LIVEUPDATE_UNKNOWN
 * Unspecified error
 *
 * @name liveupdate.LIVEUPDATE_UNKNOWN
 * @constant
 */


#endif // DM_SCRIPT_LIVEUPDATE_H
