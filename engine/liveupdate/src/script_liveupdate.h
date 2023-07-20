// Copyright 2020-2023 The Defold Foundation
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
 */

/*# return a reference to the Manifest that is currently loaded
 *
 * Return a reference to the Manifest that is currently loaded.
 *
 * @name liveupdate.get_current_manifest
 * @return manifest_reference [type:number] reference to the Manifest that is currently loaded
 */

/*# add a resource to the data archive and runtime index
 *
 * add a resource to the data archive and runtime index. The resource will be verified
 * internally before being added to the data archive.
 *
 * @name liveupdate.store_resource
 * @param manifest_reference [type:number] The manifest to check against.
 * @param data [type:string] The resource data that should be stored.
 * @param hexdigest [type:string] The expected hash for the resource,
 * retrieved through collectionproxy.missing_resources.
 * @param callback [type:function(self, hexdigest, status)] The callback
 * function that is executed once the engine has been attempted to store
 * the resource.
 *
 * `self`
 * : [type:object] The current object.
 *
 * `hexdigest`
 * : [type:string] The hexdigest of the resource.
 *
 * `status`
 * : [type:boolean] Whether or not the resource was successfully stored.
 *
 * @examples
 *
 * ```lua
 * function init(self)
 *     self.manifest = liveupdate.get_current_manifest()
 * end
 *
 * local function callback_store_resource(self, hexdigest, status)
 *      if status == true then
 *           print("Successfully stored resource: " .. hexdigest)
 *      else
 *           print("Failed to store resource: " .. hexdigest)
 *      end
 * end
 *
 * local function load_resources(self, target)
 *      local resources = collectionproxy.missing_resources(target)
 *      for _, resource_hash in ipairs(resources) do
 *           local baseurl = "http://example.defold.com:8000/"
 *           http.request(baseurl .. resource_hash, "GET", function(self, id, response)
 *                if response.status == 200 then
 *                     liveupdate.store_resource(self.manifest, response.response, resource_hash, callback_store_resource)
 *                else
 *                     print("Failed to download resource: " .. resource_hash)
 *                end
 *           end)
 *      end
 * end
 * ```
 */

/*# create, verify, and store a manifest to device
 *
 * Create a new manifest from a buffer. The created manifest is verified
 * by ensuring that the manifest was signed using the bundled public/private
 * key-pair during the bundle process and that the manifest supports the current
 * running engine version. Once the manifest is verified it is stored on device.
 * The next time the engine starts (or is rebooted) it will look for the stored
 * manifest before loading resources. Storing a new manifest allows the
 * developer to update the game, modify existing resources, or add new
 * resources to the game through LiveUpdate.
 *
 * @name liveupdate.store_manifest
 * @param manifest_buffer [type:string] the binary data that represents the manifest
 * @param callback [type:function(self, status)] the callback function
 * executed once the engine has attempted to store the manifest.

 * `self`
 * : [type:object] The current object.
 *
 * `status`
 * : [type:constant] the status of the store operation:
 *
 * - `liveupdate.LIVEUPDATE_OK`
 * - `liveupdate.LIVEUPDATE_INVALID_RESOURCE`
 * - `liveupdate.LIVEUPDATE_VERSION_MISMATCH`
 * - `liveupdate.LIVEUPDATE_ENGINE_VERSION_MISMATCH`
 * - `liveupdate.LIVEUPDATE_SIGNATURE_MISMATCH`
 * - `liveupdate.LIVEUPDATE_BUNDLED_RESOURCE_MISMATCH`
 * - `liveupdate.LIVEUPDATE_FORMAT_ERROR`
 *
 * @examples
 *
 * How to download a manifest with HTTP and store it on device.
 *
 * ```lua
 * local function store_manifest_cb(self, status)
 *   if status == liveupdate.LIVEUPATE_OK then
 *     pprint("Successfully stored manifest. This manifest will be loaded instead of the bundled manifest the next time the engine starts.")
 *   else
 *     pprint("Failed to store manifest")
 *   end
 * end
 *
 * local function download_and_store_manifest(self)
 *   http.request(MANIFEST_URL, "GET", function(self, id, response)
 *       if response.status == 200 then
 *         liveupdate.store_manifest(response.response, store_manifest_cb)
 *       end
 *     end)
 * end
 * ```
 */

/*# register and store a live update zip file
 *
 * Stores a zip file and uses it for live update content. The contents of the
 * zip file will be verified against the manifest to ensure file integrity.
 * It is possible to opt out of the resource verification using an option passed
 * to this function.
 * The path is stored in the (internal) live update location.
 *
 * @name liveupdate.store_archive
 * @param path [type:string] the path to the original file on disc
 * @param callback [type:function(self, status)] the callback function
 * executed after the storage has completed
 *
 * `self`
 * : [type:object] The current object.
 *
 * `status`
 * : [type:constant] the status of the store operation (See liveupdate.store_manifest)
 *
 * @param [options] [type:table] optional table with extra parameters. Supported entries:
 *
 * - [type:boolean] `verify`: if archive should be verified as well as stored (defaults to true)
 *
 * @examples
 *
 * How to download an archive with HTTP and store it on device.
 *
 * ```lua
 * local LIVEUPDATE_URL = <a file server url>
 *
 * -- This can be anything, but you should keep the platform bundles apart
 * local ZIP_FILENAME = 'defold.resourcepack.zip'
 *
 * local APP_SAVE_DIR = "LiveUpdateDemo"
 *
 * function init(self)
 *     self.proxy = "levels#level1"
 *
 *     print("INIT: is_using_liveupdate_data:", liveupdate.is_using_liveupdate_data())
 *     -- let's download the archive
 *     msg.post("#", "attempt_download_archive")
 * end
 *
 * -- helper function to store headers from the http request (e.g. the ETag)
 * local function store_http_response_headers(name, data)
 *     local path = sys.get_save_file(APP_SAVE_DIR, name)
 *     sys.save(path, data)
 * end
 *
 * local function load_http_response_headers(name)
 *     local path = sys.get_save_file(APP_SAVE_DIR, name)
 *     return sys.load(path)
 * end
 *
 * -- returns headers that can potentially generate a 304
 * -- without redownloading the file again
 * local function get_http_request_headers(name)
 *     local data = load_http_response_headers(name)
 *     local headers = {}
 *     for k, v in pairs(data) do
 *         if string.lower(k) == 'etag' then
 *             headers['If-None-Match'] = v
 *         elseif string.lower(k) == 'last-modified' then
 *             headers['If-Modified-Since'] = v
 *         end
 *     end
 *     return headers
 * end
 *
 * local function store_archive_cb(self, path, status)
 *     if status == true then
 *         print("Successfully stored live update archive!", path)
 *         sys.reboot()
 *     else
 *         print("Failed to store live update archive, ", path)
 *         -- remove the path
 *     end
 * end
 *
 * function on_message(self, message_id, message, sender)
 *     if message_id == hash("attempt_download_archive") then
 *
 *         -- by supplying the ETag, we don't have to redownload the file again
 *         -- if we already have downloaded it.
 *         local headers = get_http_request_headers(ZIP_FILENAME .. '.json')
 *         if not liveupdate.is_using_liveupdate_data() then
 *             headers = {} -- live update data has been purged, and we need do a fresh download
 *         end
 *
 *         local path = sys.get_save_file(APP_SAVE_DIR, ZIP_FILENAME)
 *         local options = {
 *             path = path,        -- a temporary file on disc. will be removed upon successful liveupdate storage
 *             ignore_cache = true -- we don't want to store a (potentially large) duplicate in our http cache
 *         }
 *
 *         local url = LIVEUPDATE_URL .. ZIP_FILENAME
 *         print("Downloading", url)
 *         http.request(url, "GET", function(self, id, response)
 *             if response.status == 304 then
 *                 print(string.format("%d: Archive zip file up-to-date", response.status))
 *             elseif response.status == 200 and response.error == nil then
 *                 -- register the path to the live update system
 *                 liveupdate.store_archive(response.path, store_archive_cb)
 *                 -- at this point, the "path" has been moved internally to a different location
 *
 *                 -- save the ETag for the next run
 *                 store_http_response_headers(ZIP_FILENAME .. '.json', response.headers)
 *             else
 *                 print("Error when downloading", url, "to", path, ":", response.status, response.error)
 *             end
 *
 *             -- If we got a 200, we would call store_archive_cb() then reboot
 *             -- Second time, if we get here, it should be after a 304, and then
 *             -- we can load the missing resources from the liveupdate archive
 *             if liveupdate.is_using_liveupdate_data() then
 *                 msg.post(self.proxy, "load")
 *             end
 *         end,
 *         headers, nil, options)
 * ```
 *
 */

/*# is any liveupdate data mounted and currently in use
 *
 * Is any liveupdate data mounted and currently in use?
 * This can be used to determine if a new manifest or zip file should be downloaded.
 *
 * @name liveupdate.is_using_liveupdate_data
 * @return bool [type:bool] true if a liveupdate archive (any format) has been loaded
 * @note: Old downloaded files are automatically discarded upon startup, if their signatures mismatch with the bundled manifest.
 */


/*# Get current mounts
 *
 * Get an array of the current mounts
 * This can be used to determine if a new mount is needed or not
 *
 * @name liveupdate.get_mounts
 * @return mounts [type:array] Array of mounts
 * @note: Any mount with priority < 0 is considered a base archive and it cannot be removed. All other mounts are considered "live update" content
 * @example
 *
 * Output the current resource mounts
 *
 * ```lua
 * pprint("MOUNTS", liveupdate.get_mounts())
 * ```
 */

/*# Add resource mount
 *
 * Adds a resource mount to the resource system.
 * The mounts are persisted between sessions.
 *
 * After the mount succeeded, the resources are available to load. (i.e. no reboot required)
 *
 * @note The request is asynchronous
 * @note Names cannot start with '_'
 * @note Priority must be >= 0
 *
 * @name liveupdate.add_mount
 * @param name [type:string] Unique name of the mount
 * @param uri [type:string] The uri of the mount, including the scheme. Currently supported schemes are 'zip' and 'archive'.
 * @param priority [type:integer] Priority of mount. Larger priority takes prescedence
 * @param callback [type:function] Callback after the asynchronous request completed
 *
 * @return result [type:number] The result of the request
 *
 * @example
 *
 * Add multiple mounts. Higher priority takes precedence.
 *
 * ```lua
 * liveupdate.add_mount("common", "zip:/path/to/common_stuff.zip", 10) -- base pack
 * liveupdate.add_mount("levelpack_1", "zip:/path/to/levels_1_to_20.zip", 20) -- level pack
 * liveupdate.add_mount("season_pack_1", "zip:/path/to/easter_pack_1.zip", 30) -- season pack, overriding content in the other packs
 * ```
 */


/*# Remove resource mount
 *
 * Remove a mount the resource system.
 * The remaining mounts are persisted between sessions.
 *
 * Removing a mount does not affect any loaded resources.
 *
 * @note The call is synchronous
 *
 * @name liveupdate.remove_mount
 * @param name [type:string] Unique name of the mount
 * @return result [type:number] The result of the call
 *
 * @example
 *
 * Add multiple mounts. Higher priority takes precedence.
 *
 * ```lua
 * liveupdate.remove_mount("season_pack_1")
 * ```
 */

 /*# LIVEUPDATE_OK
 *
 * @name liveupdate.LIVEUPDATE_OK
 * @variable
 */

 /*# LIVEUPDATE_INVALID_HEADER
 * The handled resource is invalid.
 *
 * @name liveupdate.LIVEUPDATE_INVALID_HEADER
 * @variable
 */

 /*# LIVEUPDATE_MEM_ERROR
 * Memory wasn't allocated
 *
 * @name liveupdate.LIVEUPDATE_MEM_ERROR
 * @variable
 */

 /*# LIVEUPDATE_INVALID_RESOURCE
 * The header of the resource is invalid.
 *
 * @name liveupdate.LIVEUPDATE_INVALID_RESOURCE
 * @variable
 */

 /*# LIVEUPDATE_VERSION_MISMATCH
 * Mismatch between manifest expected version and actual version.
 *
 * @name liveupdate.LIVEUPDATE_VERSION_MISMATCH
 * @variable
 */

 /*# LIVEUPDATE_ENGINE_VERSION_MISMATCH
 * Mismatch between running engine version and engine versions supported by manifest.
 *
 * @name liveupdate.LIVEUPDATE_ENGINE_VERSION_MISMATCH
 * @variable
 */

 /*# LIVEUPDATE_SIGNATURE_MISMATCH
 * Mismatch between manifest expected signature and actual signature.
 *
 * @name liveupdate.LIVEUPDATE_SIGNATURE_MISMATCH
 * @variable
 */

 /*# LIVEUPDATE_SCHEME_MISMATCH
 * Mismatch between scheme used to load resources. Resources are loaded with a different scheme than from manifest, for example over HTTP or directly from file. This is typically the case when running the game directly from the editor instead of from a bundle.
 *
 * @name liveupdate.LIVEUPDATE_SCHEME_MISMATCH
 * @variable
 */

 /*# LIVEUPDATE_BUNDLED_RESOURCE_MISMATCH
 * Mismatch between between expected bundled resources and actual bundled resources. The manifest expects a resource to be in the bundle, but it was not found in the bundle. This is typically the case when a non-excluded resource was modified between publishing the bundle and publishing the manifest.
 *
 * @name liveupdate.LIVEUPDATE_BUNDLED_RESOURCE_MISMATCH
 * @variable
 */

 /*# LIVEUPDATE_FORMAT_ERROR
 * Failed to parse manifest data buffer. The manifest was probably produced by a different engine version.
 *
 * @name liveupdate.LIVEUPDATE_FORMAT_ERROR
 * @variable
 */

 /*# LIVEUPDATE_IO_ERROR
 * I/O operation failed
 *
 * @name liveupdate.LIVEUPDATE_IO_ERROR
 * @variable
 */

 /*# LIVEUPDATE_INVAL
 * Argument was invalid
 *
 * @name liveupdate.LIVEUPDATE_INVAL
 * @variable
 */

 /*# LIVEUPDATE_UNKNOWN
 * Unspecified error
 *
 * @name liveupdate.LIVEUPDATE_UNKNOWN
 * @variable
 */


#endif // DM_SCRIPT_LIVEUPDATE_H
