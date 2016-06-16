package com.dynamo.cr.server.resources;

import com.dynamo.cr.server.auth.AccessTokenAuthenticator;

import javax.annotation.security.RolesAllowed;
import javax.inject.Inject;
import javax.servlet.http.HttpServletRequest;
import javax.ws.rs.POST;
import javax.ws.rs.Path;
import javax.ws.rs.core.Context;
import javax.ws.rs.core.Response;

@Path("access_token")
@RolesAllowed(value = {"user"})
public class AccessTokenResource extends BaseResource {

    private final AccessTokenAuthenticator accessTokenAuthenticator;

    @Inject
    public AccessTokenResource(AccessTokenAuthenticator accessTokenAuthenticator) {
        super();
        this.accessTokenAuthenticator = accessTokenAuthenticator;
    }

    @POST
    public Response generateNewToken(@Context HttpServletRequest httpServletRequest) {
        return okResponse(accessTokenAuthenticator.createLifetimeToken(getUser(), httpServletRequest.getRemoteAddr()));
    }
}
