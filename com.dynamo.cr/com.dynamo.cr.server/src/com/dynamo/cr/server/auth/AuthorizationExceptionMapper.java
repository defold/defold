package com.dynamo.cr.server.auth;

import javax.ws.rs.core.Response;
import javax.ws.rs.core.Response.Status;
import javax.ws.rs.ext.ExceptionMapper;
import javax.ws.rs.ext.Provider;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * <p>Map an authorization exception to an HTTP 403 response.</p>
 */
@Provider
public class AuthorizationExceptionMapper implements ExceptionMapper<AuthorizationException> {

    protected static Logger logger = LoggerFactory.getLogger(AuthorizationExceptionMapper.class);

    public Response toResponse(AuthorizationException e) {
        logger.error(e.getMessage(), e);
        return Response.
                status(Status.FORBIDDEN).
                type("text/plain").
                entity(e.getMessage()).
                build();
    }

}
