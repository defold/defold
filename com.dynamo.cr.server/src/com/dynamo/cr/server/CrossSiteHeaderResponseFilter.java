package com.dynamo.cr.server;

import java.net.MalformedURLException;
import java.net.URL;
import java.util.List;

import javax.ws.rs.core.MultivaluedMap;

import com.sun.jersey.spi.container.ContainerRequest;
import com.sun.jersey.spi.container.ContainerResponse;
import com.sun.jersey.spi.container.ContainerResponseFilter;

public class CrossSiteHeaderResponseFilter implements ContainerResponseFilter {

    @Override
    public ContainerResponse filter(ContainerRequest request,
            ContainerResponse response) {
        /*
         * TODO: Access-Control-Allow-Origin must be restricted to x.appspot.com
         * or similar!
         */

        MultivaluedMap<String, Object> httpHeaders = response.getHttpHeaders();
        httpHeaders.remove("Access-Control-Allow-Methods");
        httpHeaders.add("Access-Control-Allow-Methods", "GET, OPTIONS, HEAD, DELETE, POST");

        httpHeaders.add("Access-Control-Allow-Headers",
                "X-Email, X-Password, X-Auth, Content-Type");

        List<String> origins = request.getRequestHeaders().get("origin");
        if (origins != null && origins.size() > 0) {
            httpHeaders.add("Access-Control-Allow-Origin", origins.get(0));
        } else {
            // Safari does not include "origin" in GET-requests (only for
            // cross-site?)
            // However, we try to infer origin from referer for the
            // Access-Control-Allow-Origin header
            // TODO: Correct? Is safari buggy? Neither Chrome nor FF requires
            // this
            List<String> referers = request.getRequestHeaders().get("referer");
            if (referers != null && referers.size() > 0) {
                try {
                    URL url = new URL(referers.get(0));
                    httpHeaders.add("Access-Control-Allow-Origin", new URL(
                            "http", url.getHost(), url.getPort(), ""));

                } catch (MalformedURLException e) {
                    e.printStackTrace();
                }
            }
        }

        httpHeaders.add("Access-Control-Allow-Credentials", "true");
        return response;
    }

}
