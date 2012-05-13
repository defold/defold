package com.dynamo.cr.client;

import java.net.URI;

import javax.ws.rs.core.UriBuilder;

import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.Path;

import com.dynamo.cr.common.providers.ProtobufProviders;
import com.dynamo.cr.protocol.proto.Protocol.ProjectInfo;
import com.dynamo.cr.protocol.proto.Protocol.UserInfo;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.ClientHandlerException;
import com.sun.jersey.api.client.ClientResponse;
import com.sun.jersey.api.client.WebResource;

public class ClientFactory implements IClientFactory {
    private Client client;
    private String branchRoot;
    private BranchLocation branchLocation;
    private String email;
    private String password;

    /**
     * ClientFactory constructor
     * @note branchRoot, email and password is only used for local branches
     * @param client
     * @param branchLocation
     * @param branchRoot
     * @param email
     * @param password
     */
    public ClientFactory(Client client, BranchLocation branchLocation, String branchRoot, String email, String password) {
        this.client = client;
        this.branchRoot = branchRoot;
        this.branchLocation = branchLocation;
        this.email = email;
        this.password = password;
    }

    protected <T> T wrapGet(WebResource resource, String path, Class<T> klass) throws RepositoryException {
        try {
            ClientResponse resp = resource.path(path).accept(ProtobufProviders.APPLICATION_XPROTOBUF).get(ClientResponse.class);
            if (resp.getStatus() != 200) {
                ClientUtils.throwRespositoryException(resp);
            }
            return resp.getEntity(klass);
        }
        catch (ClientHandlerException e) {
            ClientUtils.throwRespositoryException(e);
            return null; // Never reached
        }
    }

    @Override
    public IProjectClient getProjectClient(URI uri) throws RepositoryException {
        IPath basePath = new Path(uri.getPath()).removeLastSegments(3);
        URI baseURI = UriBuilder.fromUri(uri).replacePath(basePath.toPortableString()).build();
        WebResource baseResource = client.resource(baseURI);
        UserInfo userInfo = wrapGet(baseResource, "/users/" + email, UserInfo.class);

        WebResource resource = client.resource(uri);
        ProjectInfo projectInfo = wrapGet(resource, "/project_info", ProjectInfo.class);

        IProjectClient pc;
        if (branchLocation == BranchLocation.REMOTE)  {
            pc = new ProjectClient(this, uri, client);
        } else {
            pc = new LocalProjectClient(this, uri, client, userInfo, projectInfo, branchRoot, email, password);
        }
        return pc;
    }

    @Override
    public IProjectsClient getProjectsClient(URI uri) {
        ProjectsClient pc = new ProjectsClient(this, uri, client);
        return pc;
    }

    @Override
    public IBranchClient getBranchClient(URI uri) throws RepositoryException {
        IPath basePath = new Path(uri.getPath()).removeLastSegments(5);
        URI baseURI = UriBuilder.fromUri(uri).replacePath(basePath.toPortableString()).build();
        WebResource baseResource = client.resource(baseURI);
        UserInfo userInfo = wrapGet(baseResource, "/users/" + email, UserInfo.class);

        IPath projectPath = new Path(uri.getPath()).removeLastSegments(2);
        URI projectURI = UriBuilder.fromUri(uri).replacePath(projectPath.toPortableString()).build();
        WebResource resource = client.resource(projectURI);
        ProjectInfo projectInfo = wrapGet(resource, "/project_info", ProjectInfo.class);

        IBranchClient bc;
        if (branchLocation == BranchLocation.REMOTE)  {
            bc = new BranchClient(this, uri, client);
        } else {
            bc = new LocalBranchClient(this, uri, userInfo, projectInfo, branchRoot, email, password);
        }
        return bc;
    }

    @Override
    public IUsersClient getUsersClient(URI uri) {
        return new UsersClient(this, uri, client);
    }

}
