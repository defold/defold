package com.dynamo.cr.client;

import java.net.URI;

import com.sun.jersey.api.client.Client;

/**
 * Factory for constructing {@link IBranchClient} and {@link IProjectClient}.
 * Internally the implementation of IBranchClient can cache meta-data. In order to ensure
 * cache coherence all instances should be created using the same {@link #IClientFactory(Client)}
 * @author chmu
 */
public interface IClientFactory {

    /**
     * Get {@link IProjectClient} from uri
     * @param uri URI to get {@link IProjectClient} from
     * @return A new {@link IProjectClient}
     */
    public IProjectClient getProjectClient(URI uri);

    /**
     * Get {@link IProjectsClient} from uri
     * @param uri URI to get {@link IProjectClient} from
     * @return A new {@link IProjectsClient}
     */
    public IProjectsClient getProjectsClient(URI uri);

    /**
     * Get {@link IBranchClient} from uri
     * @param uri URI to get {@link IBranchClient} from
     * @return A new {@link IBranchClient}
     */
    public IBranchClient getBranchClient(URI uri);

    /**
     * Get {@link IBranchClient} from uri
     * @param uri URI to get {@link IBranchClient} from
     * @return A new {@link IBranchClient}
     */
    public IUsersClient getUsersClient(URI uri);

}