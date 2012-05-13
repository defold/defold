package com.dynamo.cr.client;

import java.net.URI;

import javax.ws.rs.core.UriBuilder;

import com.sun.jersey.api.client.ClientHandlerException;
import com.sun.jersey.api.client.ClientResponse;

public class ClientUtils {

    public static URI getProjectUri(IProjectsClient projectsClient, long id) {
        return UriBuilder.fromUri(projectsClient.getURI()).path(Long.toString(id)).build();
    }

    public static URI getBranchUri(IProjectClient projectClient, String branch) {
        return UriBuilder.fromUri(projectClient.getURI()).path("branches").path(branch).build();
    }

    public static void throwRespositoryException(ClientResponse resp) throws RepositoryException {
        throw new RepositoryException(resp.toString() + "\n" + resp.getEntity(String.class), resp.getClientResponseStatus().getStatusCode());
    }

    public static void throwRespositoryException(ClientHandlerException e) throws RepositoryException {
        throw new RepositoryException(e.getMessage(), -1);
    }
}
