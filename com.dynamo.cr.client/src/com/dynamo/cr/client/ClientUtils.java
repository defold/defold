package com.dynamo.cr.client;

import java.net.URI;

import javax.ws.rs.core.UriBuilder;

public class ClientUtils {

    public static URI getProjectUri(IProjectsClient projectsClient, long id) {
        return UriBuilder.fromUri(projectsClient.getURI()).path(Long.toString(id)).build();
    }

    public static URI getBranchUri(IProjectClient projectClient, String branch) {
        return UriBuilder.fromUri(projectClient.getURI()).path("branches").path(branch).build();
    }
}
