package com.dynamo.cr.client;

import java.net.URI;
import java.util.HashMap;
import java.util.Map;

import com.dynamo.cr.protocol.proto.Protocol.ResourceInfo;
import com.sun.jersey.api.client.Client;

/**
 * Factory for constructing {@link IBranchClient} and {@link IProjectClient}.
 * Internally the implementation of IBranchClient can cache meta-data. In order to ensure
 * cache coherence all instances should be created using the same {@link #ClientFactory(Client)}
 * @author chmu
 */

public class ClientFactory {
    private Map<URI, ResourceInfo> resourceInfoCache = new HashMap<URI, ResourceInfo>();

    private Client client;

    public ClientFactory(Client client) {
        this.client = client;
    }

    /**
     * Get {@link IProjectClient} from uri
     * @param uri URI to get {@link IProjectClient} from
     * @return A new {@link IProjectClient}
     */
    public IProjectClient getProjectClient(URI uri) {
        ProjectClient pc = new ProjectClient(this, uri, client);
        return pc;
    }

    /**
     * Get {@link IBranchClient} from #uri
     * @param uri URI to get {@link IBranchClient} from
     * @return A new {@link IBranchClient}
     */
    public BranchClient getBranchClient(URI uri) {
        return new BranchClient(this, uri, client);
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
