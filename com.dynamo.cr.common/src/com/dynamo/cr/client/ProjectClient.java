package com.dynamo.cr.client;


import java.io.InputStream;
import java.net.URI;

import javax.ws.rs.core.UriBuilder;

import com.dynamo.cr.proto.Config.ProjectConfiguration;
import com.dynamo.cr.protocol.proto.Protocol.ApplicationInfo;
import com.dynamo.cr.protocol.proto.Protocol.BranchList;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.ClientHandlerException;
import com.sun.jersey.api.client.ClientResponse;

public class ProjectClient extends BaseClient implements IProjectClient {

    private ClientFactory factory;
    private URI uri;

    // NOTE: Only public for package
    ProjectClient(ClientFactory factory, URI uri, Client client) {
        this.factory = factory;
        this.uri = uri;
        this.client = client;
        resource = client.resource(uri);
    }

    @Override
    public void createBranch(String user, String branch) throws RepositoryException {
        wrapPut(String.format("/%s/%s", user, branch));
    }

    @Override
    public BranchStatus getBranchStatus(String user, String branch) throws RepositoryException {
        return wrapGet(String.format("/%s/%s", user, branch), BranchStatus.class);
    }

    @Override
    public BranchList getBranchList(String user) throws RepositoryException {
        return wrapGet(String.format("/%s", user), BranchList.class);
    }

    @Override
    public void deleteBranch(String user, String branch) throws RepositoryException {
        wrapDelete(String.format("/%s/%s", user, branch));
    }


    @Override
    public BranchClient getBranchClient(String user, String branch) {
        return new BranchClient(factory, UriBuilder.fromUri(uri).path(user + "/" + branch).build(), client);
    }

    @Override
    public ProjectConfiguration getProjectConfiguration() throws RepositoryException {
        return wrapGet("", ProjectConfiguration.class);
    }

    @Override
    public ApplicationInfo getApplicationInfo(String platform)
            throws RepositoryException {

        try {
            ClientResponse resp = resource.path("/application_info").queryParam("platform", platform).get(ClientResponse.class);
            if (resp.getStatus() != 200) {
                throwRespositoryException(resp);
            }
            return resp.getEntity(ApplicationInfo.class);
        }
        catch (ClientHandlerException e) {
            throwRespositoryException(e);
            return null; // Never reached
        }
    }

    @Override
    public InputStream getApplicationData(String platform)
            throws RepositoryException {
        try {
            ClientResponse resp = resource.path("/application_data").queryParam("platform", platform).get(ClientResponse.class);
            if (resp.getStatus() != 200) {
                throwRespositoryException(resp);
            }
            return resp.getEntityInputStream();
        }
        catch (ClientHandlerException e) {
            throwRespositoryException(e);
            return null; // Never reached
        }
    }

}
