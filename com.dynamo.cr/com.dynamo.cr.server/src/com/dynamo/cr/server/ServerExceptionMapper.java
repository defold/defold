package com.dynamo.cr.server;

import javax.ws.rs.core.Response;
import javax.ws.rs.ext.ExceptionMapper;
import javax.ws.rs.ext.Provider;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;


/**
 * <p>Map an server exception to an custom HTTP response.</p>
 */
@Provider
public class ServerExceptionMapper implements ExceptionMapper<ServerException> {

    protected static Logger logger = LoggerFactory.getLogger(ServerExceptionMapper.class);

    public Response toResponse(ServerException e) {
        logger.error(e.getMessage(), e);
        return Response.
                status(e.getStatus()).
                type("text/plain").
                entity(e.getMessage()).
                build();
    }

}
