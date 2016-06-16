package com.dynamo.cr.server.resources;

import com.dynamo.cr.server.auth.AccessTokenAuthenticator;

import javax.inject.Inject;
import javax.servlet.http.HttpServletRequest;
import javax.ws.rs.POST;
import javax.ws.rs.Path;
import javax.ws.rs.core.Context;

@Path("access_token")
public class AccessTokenResource extends BaseResource {

    private final AccessTokenAuthenticator accessTokenAuthenticator;

    @Inject
    public AccessTokenResource(AccessTokenAuthenticator accessTokenAuthenticator) {
        super();
        this.accessTokenAuthenticator = accessTokenAuthenticator;
    }

    @POST
    public String generateNewToken(@Context HttpServletRequest httpServletRequest) {
        return accessTokenAuthenticator.createLifetimeToken(getUser(), httpServletRequest.getRemoteAddr());
    }
}
