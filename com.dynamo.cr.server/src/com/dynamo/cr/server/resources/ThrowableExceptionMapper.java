package com.dynamo.cr.server.resources;

import javax.ws.rs.core.Response;
import javax.ws.rs.core.Response.Status;
import javax.ws.rs.ext.ExceptionMapper;

/**
 * <p>Map an Throwable exception to an HTTP 500 Internal Server Error.</p>
 */
//@Provider TODO: Currently disabled. This catches NotFoundException too...!
public class ThrowableExceptionMapper implements ExceptionMapper<Throwable> {

    public Response toResponse(Throwable e) {
        return Response.
                status(Status.INTERNAL_SERVER_ERROR).
                type("text/plain").
                entity(e.getMessage()).
                build();
    }

}
