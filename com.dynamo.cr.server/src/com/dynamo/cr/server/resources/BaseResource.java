package com.dynamo.cr.server.resources;

import javax.ws.rs.WebApplicationException;
import javax.ws.rs.core.Context;
import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.Response;
import javax.ws.rs.core.Response.Status;
import javax.ws.rs.core.SecurityContext;

import com.dynamo.cr.branchrepo.BranchRepository;
import com.dynamo.cr.server.Server;

public class BaseResource {
    @Context
    protected Server server;

    @Context
    protected BranchRepository branchRepository;

    @Context
    protected SecurityContext securityContext;

    protected void throwWebApplicationException(Status status, String msg) {
        Response response = Response
                .status(status)
                .type(MediaType.TEXT_PLAIN)
                .entity(msg)
                .build();
        throw new WebApplicationException(response);
    }

    protected Response okResponse(String fmt, Object...args) {
        return Response
                .status(Status.OK)
                .type(MediaType.TEXT_PLAIN)
                .entity(String.format(fmt, args))
                .build();
    }

}
