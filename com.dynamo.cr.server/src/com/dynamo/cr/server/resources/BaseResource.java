package com.dynamo.cr.server.resources;

import javax.ws.rs.core.Context;
import javax.ws.rs.core.SecurityContext;

import com.dynamo.cr.branchrepo.BranchRepository;
import com.dynamo.cr.server.Server;

public class BaseResource {
    @Context
    protected Server server;

    @Context
    protected BranchRepository branchRepository;

    @Context
    protected SecurityContext securityContext;

}
