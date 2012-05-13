package com.dynamo.cr.server;

import javax.servlet.http.HttpServletRequest;
import javax.ws.rs.core.Context;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.slf4j.MDC;

import com.sun.jersey.spi.container.ContainerRequest;
import com.sun.jersey.spi.container.ContainerRequestFilter;

public class PreLoggingRequestFilter implements ContainerRequestFilter {

    protected static Logger logger = LoggerFactory.getLogger(PreLoggingRequestFilter.class);

    @Context
    HttpServletRequest servletRequest;

    @Override
    public ContainerRequest filter(ContainerRequest request) {
        String remoteAddr = servletRequest.getRemoteAddr();
        MDC.put("method", servletRequest.getMethod());
        MDC.put("uri", servletRequest.getRequestURI());
        MDC.put("remoteAddr", remoteAddr);

        return request;
    }

}
