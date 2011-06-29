package com.dynamo.cr.server;

import javax.ws.rs.core.MediaType;

import com.sun.jersey.spi.container.ContainerRequest;
import com.sun.jersey.spi.container.ContainerRequestFilter;

public class JsonpRequestFilter implements ContainerRequestFilter {

    @Override
    public ContainerRequest filter(ContainerRequest request) {
        if (request.getQueryParameters().get("callback") != null) {
            request.getAcceptableMediaTypes().add(new MediaType("text", "javascript"));
        }
        return request;
    }

}
