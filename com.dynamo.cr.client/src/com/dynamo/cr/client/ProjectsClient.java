package com.dynamo.cr.client;

import java.net.URI;

import javax.ws.rs.core.UriBuilder;

import com.dynamo.cr.protocol.proto.Protocol.ProjectInfoList;
import com.sun.jersey.api.client.Client;

public class ProjectsClient extends BaseClient implements IProjectsClient {

    private URI uri;

    // NOTE: Only public for package
    ProjectsClient(URI uri, Client client) {
        this.uri = uri;
        this.client = client;
        resource = client.resource(uri);
    }

    @Override
    public ProjectInfoList getProjects() throws RepositoryException {
        return wrapGet("/", ProjectInfoList.class);
    }

    @Override
    public IProjectClient getProjectClient(long projectId) {
        URI newUri = UriBuilder.fromUri(uri).path("/" + projectId).build();
        return new ProjectClient(newUri, client);
    }

}
