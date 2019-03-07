#ifndef H_SCRIPT_COLLECTIONPROXY
#define H_SCRIPT_COLLECTIONPROXY

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
}

namespace dmGameSystem
{

    void ScriptCollectionProxyRegister(const struct ScriptLibContext& context);
    void ScriptCollectionProxyFinalize(const struct ScriptLibContext& context);

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
     * @param collectionproxy [type:url] the collectionproxy to check for missing
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
     *         print("Successfully downloaded resource: " .. expected)
     *         resource.store_resource(response.response)
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
};

#endif // H_SCRIPT_COLLECTIONPROXY