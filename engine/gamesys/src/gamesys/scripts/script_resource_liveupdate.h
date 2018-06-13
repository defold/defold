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
     *       if response.status > 199 and response.status < 400 then
     *         resource.store_manifest(response.response, store_manifest_cb)
     *       end
     *     end)
     * end
     * ```
     */
    int Resource_StoreManifest(lua_State* L);

};

#endif // H_SCRIPT_RESOURCE_LIVEUPDATE