package com.dynamo.cr.server.resources;

import javax.ws.rs.GET;
import javax.ws.rs.Path;

@Path("/status")
public class StatusResource {

    @GET
    public String getServerStatus() {
        // Currently only used to check if the server is running
        // We could extend status-status later with more info
        // using protocol buffers
        return "OK";
    }
}

