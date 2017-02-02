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
     * Return a reference to the Manifest that is currently loaded. This
     * reference should be passed on to the `verify_resource` function when
     * downloading content that was selected for LiveUpdate during the build
     * process.
     *
     * @name resource.get_current_manifest
     * @return manifest_reference [type:number] reference to the Manifest that is currently loaded
     */
    int Resource_GetCurrentManifest(lua_State* L);

    /*# create a new manifest from a buffer
     *
     * Create a new manifest from a buffer and return a reference to the
     * manifest. Before storing a manifest that has been downloaded it is
     * important to verify that the manifest was created using the correct
     * private key during the bundle process, this is done by passing the
     * reference returned by `create_manifest` to the function `verify_manifest`.
     *
     * Once the manifest has been verified, this reference should be passed on
     * to the functions `verify_resource` and `store_manifest` when updating a game.
     * It is possible to create up to eight different manifests at the same
     * time, and it is important that the manifests are removed by passing the
     * reference to `destroy_manifest` once all processing has been done to free
     * up memory.
     *
     * @name resource.create_manifest
     * @param buffer [type:string] the binary data that represents the manifest
     * @return manifest_reference [type:number] a reference to the manifest
     * @error an error occurs when too many manifests have been created, or when
     * the manifest could not be parsed correctly.
     *
     * @examples
     *
     * ```lua
     * local manifest = resource.create_manifest(buffer)
     * ...
     * resource.destroy_manifest(manifest)
     * ```
     */
    int Resource_CreateManifest(lua_State* L);

    /*# remove a manifest that has been created
     *
     * remove a manifest that has been created using `create_manifest`. This will
     * free up the memory that was allocated when creating the manifest. The
     * manifest that is currently loaded, and retrieved through
     * `get_current_manifest` cannot be destroyed.
     *
     * @name resource.destroy_manifest
     * @param manifest_reference [type:number] the reference that should be destroyed
     * @error An error occurs if a reference to the current manifest is
     * supplied, or if the reference supplied does not exist.
     *
     * @examples
     *
     * ```lua
     * local manifest = resource.create_manifest(buffer)
     * ...
     * resource.destroy_manifest(manifest)
     * ```
     */
    int Resource_DestroyManifest(lua_State* L);

    /*# return an indexed table of missing resources for a collection proxy
     *
     * return an indexed table of missing resources for a collection proxy. Each
     * entry is a hexadecimal string that represents the data of the specific
     * resource. This representation corresponds with the filename for each
     * individual resource that is exported when you bundle an application with
     * LiveUpdate functionality. It should be considered good practise to always
     * check whether or not there are any missing resources in a collection proxy
     * before attempting to load the collection proxy.
     *
     * @namespace collectionproxy
     * @name collectionproxy.missing_resources
     * @param collectionproxy [type:hash|string|url] the collectionproxy to check for missing
     * resources.
     * @return resources [type:table] the missing resources
     *
     * @examples
     *
     * ```lua
     * function init(self)
     *     self.manifest = resource.get_current_manifest()
     * end
     *
     * local function callback(self, id, response)
     *     local expected = self.resources[id]
     *     if response ~= nil and response.status == 200 then
     *         if resource.verify_resource(self.manifest, expected, response.response) then
     *             print("Successfully downloaded resource: " .. expected)
     *             resource.store_resource(response.response)
     *         else
     *             print("Resource could not be verified: " .. expected)
     *             -- error handling
     *         end
     *     else
     *         print("Failed to download resource: " .. expected)
     *         -- error handling
     *     end
     * end
     *
     * local function download_resources(self, cproxy)
     *     self.resources = {}
     *     local resources = collectionproxy.missing_resources(cproxy)
     *     for _, v in ipairs(resources) do
     *         print("Downloading resource: " .. v)
     *
     *         local uri = "http://example.defold.com/" .. v
     *         local id = http.request(uri, "GET", callback)
     *         self.resources[id] = v
     *     end
     * end
     * ```
     */
    int CollectionProxy_MissingResources(lua_State* L);

    /*# checks whether a resource matches the expected resource hash or not
     *
     * Checks whether a resource matches the expected resource hash or not. This
     * function should always be used before storing a resource using the
     * function `store_resource` to ensure that the resource was not corrupted,
     * or modified by a third party, during transfer.
     *
     * @name resource.verify_resource
     * @param manifest_reference [type:number] the manifest to check against
     * @param expected_hash [type:string] the expected hash for the resource,
     * retrieved through `collectionproxy.missing_resources`.
     * @param buffer [type:string] the resource data to verify.
     * @return matches [type:boolean] `true` if the resource matches the expected hash, `false`
     * otherwise.
     * @error An error occurs if the manifest_reference is invalid.
     * @examples
     *
     * ```lua
     * function init(self)
     *     self.manifest = resource.get_current_manifest()
     * end
     *
     * local function store_resource(self, expected_hash, buffer)
     *     if resource.verify_resource(self.manifest, expected_hash, buffer) then
     *         resource.store_resource(buffer)
     *     end
     * end
     * ```
     */
    int Resource_VerifyResource(lua_State* L);

    /*# add a resource to the data archive and runtime index
     *
     * Add a resource to the data archive and runtime index. The resource that
     * is added must already exist in the manifest, and should be verified using
     * `verify_resource`. Adding corrupted content or resources that doesn't exist
     * in the manifest will increase the size of the data archive, but the
     * resources will not be accessible.
     *
     * @name resource.store_resource
     * @param buffer [type:string) the resource data to store.
     * @return stored [type:boolean] `true` if the resource could be stored, `false` otherwise.
     */
    int Resource_StoreResource(lua_State* L);

    /*# check if a manifest has been created with the correct private key
     *
     * Check if a manifest has been created with the correct private key. This
     * function should always be used before storing a new manifest using the
     * function `store_manifest` to ensure that the manifest was not corrupted,
     * or modified by a third party, during transfer.
     *
     * @name resource.verify_manifest
     * @param manifest_reference [type:number] the reference that should be verified.
     * @return verified [type:boolean] `true` if the manifest signature could be correctly
     * verified, `false` otherwise.
     *
     * @examples
     *
     * ```lua
     * local function callback_update_manifest(self, id, response)
     *     if response.status == 200 then
     *         local manifest = resource.create_manifest(response.response)
     *         if resource.verify_manifest(manifest) then
     *             resource.store_manifest(manifest)
     *             msg.post("@system", "reboot")
     *         else
     *             print("Manifest could not be verified")
     *             -- error handling
     *         end
     *         resource.destroy_manifest(manifest)
     *     else
     *         print("Could not retrieve new manifest from server")
     *         -- error handling
     *     end
     * end
     *
     * local function callback_manifest_info(self, id, response)
     *     if response.status == 200 then
     *         error, data = pcall(json.decode, response.response)
     *         if not error then
     *             if data.version > self.version then
     *                 http.request(data.uri, "GET", callback_update_manifest)
     *             end
     *         else
     *             print("Could not parse manifest information from server")
     *             -- error handling
     *         end
     *     else
     *         print("Could not retrieve manifest information from server")
     *         -- error handling
     *     end
     * end
     *
     * local function check_new_manifest(self)
     *     local uri = "http://example.defold.com/latest_manifest.json"
     *     http.request(uri, "GET", callback_manifest_info)
     * end
     * ```
     */
    int Resource_VerifyManifest(lua_State* L);

    /*# store a manifest to device
     *
     * Store a manifest to device. The manifest that is stored should be
     * verified using `verify_manifest` before the manifest is stored. The next
     * time the engine starts (or is rebooted) it will look for the latest
     * manifest before loading resources. Storing a new manifest allows the
     * developer to update the game, modify existing resources, or add new
     * resources to the game through LiveUpdate.
     *
     * @name resource.store_manifest
     * @param manifest_reference [type:number] the reference that should be verified.
     * @return stored [type:boolean] `true` if the manifest could be stored, `false` otherwise.
     */
    int Resource_StoreManifest(lua_State* L);

};

#endif // H_SCRIPT_RESOURCE_LIVEUPDATE