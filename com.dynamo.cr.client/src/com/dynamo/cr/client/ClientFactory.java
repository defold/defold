package com.dynamo.cr.client;

import java.net.URI;

import com.sun.jersey.api.client.Client;

public class ClientFactory implements IClientFactory {
    private Client client;

    public ClientFactory(Client client) {
        this.client = client;
    }

    @Override
    public IProjectClient getProjectClient(URI uri) {
        ProjectClient pc = new ProjectClient(this, uri, client);
        return pc;
    }

    @Override
    public IProjectsClient getProjectsClient(URI uri) {
        ProjectsClient pc = new ProjectsClient(this, uri, client);
        return pc;
    }

    @Override
    public IBranchClient getBranchClient(URI uri) {
        return new BranchClient(this, uri, client);
    }

    @Override
    public IUsersClient getUsersClient(URI uri) {
        return new UsersClient(this, uri, client);
    }

}
