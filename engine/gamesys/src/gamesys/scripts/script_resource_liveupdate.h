// Copyright 2020 The Defold Foundation
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

#ifndef H_SCRIPT_RESOURCE_LIVEUPDATE
#define H_SCRIPT_RESOURCE_LIVEUPDATE

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
}

namespace dmLiveUpdate
{
    /*# Resource API documentation
     *
     * Functions and constants to access resources.
     *
     * @document
     * @name Resource
     * @namespace resource
     */

    /*# return a reference to the Manifest that is currently loaded
     *
     * Return a reference to the Manifest that is currently loaded.
     *
     * @name resource.get_current_manifest
     * @return manifest_reference [type:number] reference to the Manifest that is currently loaded
     */
    int Resource_GetCurrentManifest(lua_State* L);

    /*# add a resource to the data archive and runtime index
     *
     * add a resource to the data archive and runtime index. The resource will be verified
     * internally before being added to the data archive.
     *
     * @name resource.store_resource
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
     *     self.manifest = resource.get_current_manifest()
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
     *                     resource.store_resource(self.manifest, response.response, resource_hash, callback_store_resource)
     *                else
     *                     print("Failed to download resource: " .. resource_hash)
     *                end
     *           end)
     *      end
     * end
     * ```
     */
    int Resource_StoreResource(lua_State* L);

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
     * @name resource.store_manifest
     * @param manifest_buffer [type:string] the binary data that represents the manifest
     * @param callback [type:function(self, status)] the callback function
     * executed once the engine has attempted to store the manifest.

     * `self`
     * : [type:object] The current object.
     *
     * `status`
     * : [type:constant] the status of the store operation:
     *
     * - `resource.LIVEUPATE_OK`
     * - `resource.LIVEUPATE_INVALID_RESOURCE`
     * - `resource.LIVEUPATE_VERSION_MISMATCH`
     * - `resource.LIVEUPATE_ENGINE_VERSION_MISMATCH`
     * - `resource.LIVEUPATE_SIGNATURE_MISMATCH`
     * - `resource.LIVEUPDATE_BUNDLED_RESOURCE_MISMATCH`
     * - `resource.LIVEUPDATE_FORMAT_ERROR`
     *
     * @examples
     *
     * How to download a manifest with HTTP and store it on device.
     *
     * ```lua
     * local function store_manifest_cb(self, status)
     *   if status == resource.LIVEUPATE_OK then
     *     pprint("Successfully stored manifest. This manifest will be loaded instead of the bundled manifest the next time the engine starts.")
     *   else
     *     pprint("Failed to store manifest")
     *   end
     * end
     *
     * local function download_and_store_manifest(self)
     *   http.request(MANIFEST_URL, "GET", function(self, id, response)
     *       if response.status == 200 then
     *         resource.store_manifest(response.response, store_manifest_cb)
     *       end
     *     end)
     * end
     * ```
     */
    int Resource_StoreManifest(lua_State* L);


    /*# register and store a live update zip file
     *
     * Stores a zip file and uses it for live update content.
     * The path is renamed and stored in the (internal) live update location
     *
     * @name resource.store_archive
     * @param path [type:string] the path to the original file on disc
     * @param callback [type:function(self, status)] the callback function
     * executed after the storage has completed
     *
     * `self`
     * : [type:object] The current object.
     *
     * `status`
     * : [type:constant] the status of the store operation (See resource.store_manifest)
     *
     * @examples
     *
     * How to download an archive with HTTP and store it on device.
     *
     * ```lua
     * local function store_archive_cb(self, status, path)
     *   if status == resource.LIVEUPATE_OK then
     *     pprint("Successfully stored archive.")
     *   else
     *     pprint("Failed to store archive")
     *   end
     * end
     *
     * local function download_and_store_manifest(self)
     *   local path = sys.get_save_file("test", "/save.html")
     *   http.request(ARCHIVE_URL, "GET", function(self, id, response)
     *       if response.status == 200 then
     *         if response.error == nil then
     *             -- register the path to the live update system
     *             resource.store_archive(response.path, store_archive_cb)
     *         else
     *             print("Error when downloading", path, ":", response.error)
     *         end
     *       end
     *     end, nil, nil, {path = path})
     * end
     * ```
     *
     */
    int Resource_StoreArchive(lua_State* L);


    /*# is any liveupdate data mounted and currently in use
     *
     * Is any liveupdate data mounted and currently in use?
     * This can be used to determine if a new manifest or zip file should be downloaded.
     *
     * @note: Old downloaded files are automatically discarded upon startup, if their signatures mismatch with the bundled manifest.
     */
    int Resource_IsUsingLiveUpdateData(lua_State* L);
};

#endif // H_SCRIPT_RESOURCE_LIVEUPDATE