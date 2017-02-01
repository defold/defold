package com.dynamo.cr.server.resources;

import com.dynamo.cr.server.Server;
import com.dynamo.cr.server.auth.UserPrincipal;
import com.dynamo.cr.server.model.User;

import javax.inject.Inject;
import javax.persistence.EntityManager;
import javax.ws.rs.core.Context;
import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.Response;
import javax.ws.rs.core.Response.Status;
import javax.ws.rs.core.SecurityContext;

public class BaseResource {
    @Inject
    protected Server server;

    @Context
    protected SecurityContext securityContext;

    @Inject
    protected EntityManager em;

    static void throwWebApplicationException(Status status, String msg) {
        ResourceUtil.throwWebApplicationException(status, msg);
    }

    public User getUser() {
        UserPrincipal p = (UserPrincipal) securityContext.getUserPrincipal();

        // NOTE: We must re-fetch the user here and probably related to
        // different EntityManagers
        return em.find(User.class, p.getUser().getId());
    }

    Response okResponse(String fmt, Object... args) {
        return Response
                .status(Status.OK)
                .type(MediaType.TEXT_PLAIN)
                .entity(String.format(fmt, args))
                .build();
    }
}
