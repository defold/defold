package com.dynamo.cr.server.openid;

import java.io.IOException;

/**
 * OpenID endpoint resolver
 * @author chmu
 *
 */
public interface IEndPointResolver {
    /**
     * Resolve endpoint, ie load XML-RDS document from provider and create an {@link EndPoint}
     * @param provider
     * @return new {@link EndPoint}
     * @throws IOException
     * @throws OpenIDException
     */
    public EndPoint resolve(String provider) throws IOException, OpenIDException;
}
