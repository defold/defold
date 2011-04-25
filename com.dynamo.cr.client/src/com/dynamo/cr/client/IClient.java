package com.dynamo.cr.client;

import java.net.URI;

public interface IClient {
    /**
     * Get client factory associated with this client
     * @return
     */
    IClientFactory getClientFactory();

    /**
     * Get full URI for this client.
     * @return uri for this client
     */
    URI getURI();
}
