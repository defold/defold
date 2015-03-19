package com.dynamo.cr.client;

import java.io.InputStream;
import java.net.URI;

import com.dynamo.cr.common.providers.ProtobufProviders;
import com.dynamo.cr.protocol.proto.Protocol.BranchList;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;
import com.dynamo.cr.protocol.proto.Protocol.ProjectInfo;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.ClientHandlerException;
import com.sun.jersey.api.client.ClientResponse;
import com.sun.jersey.api.client.ClientResponse.Status;

public class ProjectClient extends BaseClient implements IProjectClient {

    private long projectId;

    /* package*/ ProjectClient(IClientFactory factory, URI uri, Client client) {
        super(factory, uri);
        this.client = client;
        String[] tmp = uri.getPath().split("/");
        // TODO: We should probably not use "full" uri to constructor, ie uri without project id...
        // This parsing is *budget*!
        this.projectId = Long.parseLong(tmp[tmp.length-1]);
        resource = client.resource(uri);
    }

    @Override
    public long getProjectId() {
        return this.projectId;
    }

    @Override
    public void createBranch(String branch) throws RepositoryException {
        wrapPut(String.format("/branches/%s", branch));
    }

    @Override
    public BranchStatus getBranchStatus(String branch, boolean fetch) throws RepositoryException {
        return wrapGet(String.format("/branches/%s", branch), BranchStatus.class);
    }

    @Override
    public BranchList getBranchList() throws RepositoryException {
        return wrapGet(String.format("/branches"), BranchList.class);
    }

    @Override
    public void deleteBranch(String branch) throws RepositoryException {
        wrapDelete(String.format("/branches/%s", branch));
    }

    @Override
    public ProjectInfo getProjectInfo() throws RepositoryException {
        return wrapGet("/project_info", ProjectInfo.class);
    }

    @Override
    public void setProjectInfo(ProjectInfo projectInfo)
            throws RepositoryException {
        try {
            ClientResponse resp = resource.path("/project_info").accept(ProtobufProviders.APPLICATION_XPROTOBUF).put(ClientResponse.class, projectInfo);

            if (resp.getClientResponseStatus() != Status.NO_CONTENT) {
                ClientUtils.throwRespositoryException(resp);
            }
        }
        catch (ClientHandlerException e) {
            ClientUtils.throwRespositoryException(e);
        }
    }

    @Override
    public void uploadEngine(String platform, InputStream stream) throws RepositoryException {
        throw new RuntimeException("Not supported");
    }

    @Override
    public byte[] downloadEngine(String platform, String key) throws RepositoryException {
        throw new RuntimeException("Not supported");
    }

    @Override
    public String downloadEngineManifest(String platform, String key)
            throws RepositoryException {
        throw new RuntimeException("Not supported");
    }
}
