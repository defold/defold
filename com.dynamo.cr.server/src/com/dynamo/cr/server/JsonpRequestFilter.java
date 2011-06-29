package com.dynamo.cr.server;

import javax.ws.rs.core.MediaType;

import com.sun.jersey.spi.container.ContainerRequest;
import com.sun.jersey.spi.container.ContainerRequestFilter;

public class JsonpRequestFilter implements ContainerRequestFilter {

    @Override
    public ContainerRequest filter(ContainerRequest request) {
        if (request.getQueryParameters().get("callback") != null) {
            // Force application/json for jsonp requests
            request.getAcceptableMediaTypes().clear();
            request.getAcceptableMediaTypes().add(MediaType.APPLICATION_JSON_TYPE);
        }
        return request;
    }

}
