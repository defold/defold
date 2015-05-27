package com.dynamo.cr.server;

import javax.ws.rs.core.MultivaluedMap;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.sun.jersey.spi.container.ContainerRequest;
import com.sun.jersey.spi.container.ContainerResponse;
import com.sun.jersey.spi.container.ContainerResponseFilter;

public class NoCacheHeaderResponseFilter implements ContainerResponseFilter {
    protected static Logger logger = LoggerFactory.getLogger(NoCacheHeaderResponseFilter.class);

    @Override
    public ContainerResponse filter(ContainerRequest request,
            ContainerResponse response) {

        MultivaluedMap<String, Object> httpHeaders = response.getHttpHeaders();
        httpHeaders.remove("Cache-Control");
        httpHeaders.add("Cache-Control", "no-cache, must-revalidate");
        return response;
    }

}
