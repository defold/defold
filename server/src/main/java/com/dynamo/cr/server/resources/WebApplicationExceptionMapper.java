package com.dynamo.cr.server.resources;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.slf4j.MDC;

import javax.ws.rs.WebApplicationException;
import javax.ws.rs.core.Response;
import javax.ws.rs.ext.ExceptionMapper;
import javax.ws.rs.ext.Provider;

/**
 * Map a web application exception to a message or the error code when no message exists.
 */
@Provider
public class WebApplicationExceptionMapper implements ExceptionMapper<WebApplicationException> {

    private static final Logger LOGGER = LoggerFactory.getLogger(WebApplicationExceptionMapper.class);

    @Override
    public Response toResponse(WebApplicationException e) {

        try {
            Response exceptionResponse = e.getResponse();

            if (exceptionResponse != null) {
                // Do logging
                Response.Status status = Response.Status.fromStatusCode(exceptionResponse.getStatus());
                LOGGER.warn("{} - {}, {}", status.getStatusCode(), status.getReasonPhrase(), e.getMessage());

                MDC.put("status", Integer.toString(exceptionResponse.getStatus()));

                if (exceptionResponse.getEntity() == null) {
                    String message = String.format("Error %d", exceptionResponse.getStatus());
                    if (e.getCause() != null) {
                        message += ": " + e.getCause().getMessage();
                    }
                    return Response.fromResponse(exceptionResponse).entity(message).build();
                }
            }
            return exceptionResponse;
        } finally {
            MDC.remove("status");
        }
    }
}
