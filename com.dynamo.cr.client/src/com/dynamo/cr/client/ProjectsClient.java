package com.dynamo.cr.client;

import java.net.URI;

import com.dynamo.cr.protocol.proto.Protocol.ProjectInfoList;
import com.sun.jersey.api.client.Client;

public class ProjectsClient extends BaseClient implements IProjectsClient {

    // NOTE: Only public for package
    ProjectsClient(IClientFactory factory, URI uri, Client client) {
        super(factory, uri);
        this.client = client;
        resource = client.resource(uri);
    }

    @Override
    public ProjectInfoList getProjects() throws RepositoryException {
        return wrapGet("/", ProjectInfoList.class);
    }
}
