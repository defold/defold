package com.dynamo.cr.client;

import java.net.URI;
import java.util.HashMap;
import java.util.Map;

import javax.ws.rs.core.UriBuilder;

/**
 * Delegating implementation of {@link IClientFactory} This implementation
 * ensures that for a given URI (canonicalized) the same client is returned for subsequent calls,
 * ie client instances are kept in map from uri to instance. This behavior is useful for coherence in caching clients.
 *
 * @author chmu
 *
 */
public class DelegatingClientFactory implements IClientFactory {

    private IClientFactory factory;
    private Map<URI, IProjectClient> projectClients = new HashMap<URI, IProjectClient>();
    private Map<URI, IProjectsClient> projectsClients = new HashMap<URI, IProjectsClient>();
    private Map<URI, IBranchClient> branchClients = new HashMap<URI, IBranchClient>();
    private Map<URI, IUsersClient> usersClients = new HashMap<URI, IUsersClient>();

    public DelegatingClientFactory(IClientFactory factory) {
        this.factory = factory;
    }

    private static URI normalize(URI uri) {
        uri = uri.normalize();
        String path = uri.getPath();
        if (path.endsWith("/")) {
            path = path.substring(0, path.lastIndexOf('/'));
        }
        return UriBuilder.fromUri(uri).replacePath(path).build();
    }

    @Override
    public IProjectClient getProjectClient(URI uri) throws RepositoryException {
        uri = normalize(uri);
        if (!projectClients.containsKey(uri)) {
            projectClients.put(uri, factory.getProjectClient(uri));
        }
        return projectClients.get(uri);
    }

    @Override
    public IProjectsClient getProjectsClient(URI uri) {
        uri = normalize(uri);
        if (!projectsClients.containsKey(uri)) {
            projectsClients.put(uri, factory.getProjectsClient(uri));
        }
        return projectsClients.get(uri);
    }

    @Override
    public IBranchClient getBranchClient(URI uri) throws RepositoryException {
        uri = normalize(uri);
        if (!branchClients.containsKey(uri)) {
            branchClients.put(uri, factory.getBranchClient(uri));
        }
        return branchClients.get(uri);
    }

    @Override
    public IUsersClient getUsersClient(URI uri) {
        uri = normalize(uri);
        if (!usersClients.containsKey(uri)) {
            usersClients.put(uri, factory.getUsersClient(uri));
        }
        return usersClients.get(uri);
    }
}
