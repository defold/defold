package com.dynamo.cr.server;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.slf4j.MDC;

import com.sun.jersey.spi.container.ContainerRequest;
import com.sun.jersey.spi.container.ContainerResponse;
import com.sun.jersey.spi.container.ContainerResponseFilter;

public class PostLoggingResponseFilter implements ContainerResponseFilter {

    protected static Logger logger = LoggerFactory.getLogger(PostLoggingResponseFilter.class);

    @Override
    public ContainerResponse filter(ContainerRequest request,
            ContainerResponse response) {
        // NOTE: Do *NOT* remove this statement. It's used for access logging.
        logger.info("");
        MDC.remove("remoteAddr");
        MDC.remove("userId");
        MDC.remove("method");
        return response;
    }

}
