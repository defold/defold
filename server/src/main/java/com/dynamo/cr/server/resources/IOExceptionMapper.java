package com.dynamo.cr.server.resources;

import java.io.IOException;

import javax.ws.rs.core.Response;
import javax.ws.rs.core.Response.Status;
import javax.ws.rs.ext.ExceptionMapper;
import javax.ws.rs.ext.Provider;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * <p>Maps IOException to an HTTP 500 Internal Server Error.</p>
 */
@Provider
public class IOExceptionMapper implements ExceptionMapper<IOException> {

    protected static Logger logger = LoggerFactory.getLogger(IOExceptionMapper.class);

    public Response toResponse(IOException e) {
        logger.error(e.getMessage(), e);
        return Response.
                status(Status.INTERNAL_SERVER_ERROR).
                type("text/plain").
                entity(e.getMessage()).
                build();
    }

}
