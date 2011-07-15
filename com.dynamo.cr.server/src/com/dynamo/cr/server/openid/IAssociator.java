package com.dynamo.cr.server.openid;

import java.io.IOException;

/**
 * Associator interface for creating assocations between relying part and provider
 * @author chmu
 *
 */
public interface IAssociator {
    /**
     * Associate with provider, ie exchange shared secret, association handle, etc
     * @param endPoint {@link EndPoint} to associate with
     * @return an association.
     * @throws IOException
     * @throws OpenIDException
     */
    public Association associate(EndPoint endPoint) throws IOException, OpenIDException;
}
