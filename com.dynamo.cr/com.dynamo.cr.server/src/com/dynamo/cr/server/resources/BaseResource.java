package com.dynamo.cr.server.resources;

import javax.inject.Inject;
import javax.persistence.EntityManager;
import javax.ws.rs.WebApplicationException;
import javax.ws.rs.core.Context;
import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.Response;
import javax.ws.rs.core.Response.Status;
import javax.ws.rs.core.SecurityContext;

import com.dynamo.cr.branchrepo.BranchRepository;
import com.dynamo.cr.server.Server;

public class BaseResource {
    @Inject
    protected Server server;

    @Inject
    protected BranchRepository branchRepository;

    @Context
    protected SecurityContext securityContext;

    @Inject
    protected EntityManager em;

    protected static void throwWebApplicationException(Status status, String msg) {
        ResourceUtil.throwWebApplicationException(status, msg);
    }

    protected Response okResponse(String fmt, Object...args) {
        return Response
                .status(Status.OK)
                .type(MediaType.TEXT_PLAIN)
                .entity(String.format(fmt, args))
                .build();
    }

}
