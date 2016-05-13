package com.dynamo.cr.server;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import javax.ws.rs.core.Response;
import javax.ws.rs.ext.ExceptionMapper;
import javax.ws.rs.ext.Provider;

/**
 * Map an server exception to an custom HTTP response.
 */
@Provider
public class ServerExceptionMapper implements ExceptionMapper<ServerException> {

    private static final Logger LOGGER = LoggerFactory.getLogger(ServerExceptionMapper.class);

    public Response toResponse(ServerException e) {
        LOGGER.error(e.getMessage(), e);
        return Response.
                status(e.getStatus()).
                type("text/plain").
                entity(e.getMessage()).
                build();
    }
}
