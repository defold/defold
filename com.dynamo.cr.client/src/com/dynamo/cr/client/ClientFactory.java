package com.dynamo.cr.client;

import java.net.URI;
import java.util.HashMap;
import java.util.Map;

import com.dynamo.cr.protocol.proto.Protocol.ResourceInfo;
import com.sun.jersey.api.client.Client;

public class ClientFactory implements IClientFactory {
    private Map<URI, ResourceInfo> resourceInfoCache = new HashMap<URI, ResourceInfo>();

    private Client client;

    public ClientFactory(Client client) {
        this.client = client;
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.client.IClientFactory#getProjectClient(java.net.URI)
     */
    @Override
    public IProjectClient getProjectClient(URI uri) {
        ProjectClient pc = new ProjectClient(this, uri, client);
        return pc;
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.client.IClientFactory#getProjectsClient(java.net.URI)
     */
    @Override
    public IProjectsClient getProjectsClient(URI uri) {
        ProjectsClient pc = new ProjectsClient(this, uri, client);
        return pc;
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.client.IClientFactory#getBranchClient(java.net.URI)
     */
    @Override
    public IBranchClient getBranchClient(URI uri) {
        return new BranchClient(this, uri, client);
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.client.IClientFactory#getUsersClient(java.net.URI)
     */
    @Override
    public IUsersClient getUsersClient(URI uri) {
        return new UsersClient(uri, client);
    }

    synchronized ResourceInfo getCachedResourceInfo(URI uri) {
        return resourceInfoCache.get(uri.normalize());
    }

    synchronized void cacheResourceInfo(URI uri, ResourceInfo resource_info) {
        resourceInfoCache.put(uri.normalize(), resource_info);
    }

    synchronized void discardResourceInfo(URI uri) {
        resourceInfoCache.remove(uri.normalize());
    }

    synchronized void flushResourceInfoCache() {
        resourceInfoCache.clear();
    }

}
