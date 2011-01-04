package com.dynamo.cr.client;

import java.net.URI;

import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;
import com.dynamo.cr.protocol.proto.Protocol.BuildDesc;
import com.dynamo.cr.protocol.proto.Protocol.BuildLog;
import com.dynamo.cr.protocol.proto.Protocol.ResourceInfo;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.ClientHandlerException;
import com.sun.jersey.api.client.ClientResponse;
import com.sun.jersey.api.client.WebResource;

public class BranchClient extends BaseClient implements IBranchClient {

    private ClientFactory factory;

    // NOTE: Only public for package
    BranchClient(ClientFactory factory, URI uri, Client client) {
        this.factory = factory;
        this.resource = client.resource(uri);
    }

    @Override
    public BranchStatus getBranchStatus() throws RepositoryException {
        return wrapGet("", BranchStatus.class);
    }

    @Override
    public ResourceInfo getResourceInfo(String path) throws RepositoryException {
        try {
            WebResource sub_resource = resource.path("/resources/info").queryParam("path", path);
            ResourceInfo cached = factory.getCachedResourceInfo(sub_resource.getURI());

            if (cached != null)
                return cached;

            ClientResponse resp = sub_resource.get(ClientResponse.class);
            if (resp.getStatus() != 200) {
                throwRespositoryException(resp);
            }
            ResourceInfo ret = resp.getEntity(ResourceInfo.class);
            factory.cacheResourceInfo(sub_resource.getURI(), ret);
            return ret;
        }
        catch (ClientHandlerException e) {
            throwRespositoryException(e);
            return null; // Never reached
        }
    }

    void discardResourceInfo(String path) {
        WebResource sub_resource = resource.path("/resources/info").queryParam("path", path);
        factory.discardResourceInfo(sub_resource.getURI());
    }

    @Override
    public byte[] getResourceData(String path) throws RepositoryException {
        try {
            ClientResponse resp = resource.path("/resources/data").queryParam("path", path).get(ClientResponse.class);
            if (resp.getStatus() != 200) {
                throwRespositoryException(resp);
            }
            return resp.getEntity(byte[].class);
        }
        catch (ClientHandlerException e) {
            throwRespositoryException(e);
            return null; // Never reached
        }
    }

    @Override
    public void putResourceData(String path, byte[] data) throws RepositoryException {
        try {
            discardResourceInfo(path);
            ClientResponse resp = resource.path("/resources/data").queryParam("path", path).put(ClientResponse.class, data);
            if (resp.getStatus() != 200 && resp.getStatus() != 204) {
                throwRespositoryException(resp);
            }
        }
        catch (ClientHandlerException e) {
            throwRespositoryException(e);
        }
    }

    @Override
    public void mkdir(String path) throws RepositoryException {
        try {
            ClientResponse resp = resource.path("/resources/data").queryParam("path", path).queryParam("directory", "true").put(ClientResponse.class);
            if (resp.getStatus() != 200 && resp.getStatus() != 204) {
                throwRespositoryException(resp);
            }
        }
        catch (ClientHandlerException e) {
            throwRespositoryException(e);
        }
    }

    @Override
    public void deleteResource(String path) throws RepositoryException {
        try {
            discardResourceInfo(path);
            ClientResponse resp = resource.path("/resources/info").queryParam("path", path).delete(ClientResponse.class);
            if (resp.getStatus() != 200 && resp.getStatus() != 204) {
                throwRespositoryException(resp);
            }
        }
        catch (ClientHandlerException e) {
            throwRespositoryException(e);
        }
    }

    @Override
    public void renameResource(String source, String destination) throws RepositoryException {
        try {
            // NOTE: Currently we flush all resources.
            // It is not sufficient to flush source and destination. We be also
            // flush the parent directory as it contains the sub-resources
            factory.flushResourceInfoCache();

            ClientResponse resp = resource.path("/resources/rename").queryParam("source", source).queryParam("destination", destination).post(ClientResponse.class);
            if (resp.getStatus() != 200 && resp.getStatus() != 204) {
                throwRespositoryException(resp);
            }
        }
        catch (ClientHandlerException e) {
            throwRespositoryException(e);
        }
    }

	@Override
	public void revertResource(String path) throws RepositoryException {
        try {
            // NOTE: Currently we flush all resources.
            // It is not sufficient to flush path. We be also
            // flush the parent directory as it contains the sub-resources
            factory.flushResourceInfoCache();

            ClientResponse resp = resource.path("/resources/revert").queryParam("path", path).put(ClientResponse.class);
            if (resp.getStatus() != 200 && resp.getStatus() != 204) {
                throwRespositoryException(resp);
            }
        }
        catch (ClientHandlerException e) {
            throwRespositoryException(e);
        }
	}

    @Override
    public BranchStatus update() throws RepositoryException {
        factory.flushResourceInfoCache();
        return wrapPost("update", BranchStatus.class);
    }

    @Override
    public void commit(String message) throws RepositoryException {
        try {
            ClientResponse resp = resource.path("commit").queryParam("all", "true").post(ClientResponse.class, message);
            if (resp.getStatus() != 200 && resp.getStatus() != 204) {
                throwRespositoryException(resp);
            }
        }
        catch (ClientHandlerException e) {
            throwRespositoryException(e);
        }
    }

    @Override
    public void commitMerge(String message) throws RepositoryException {
        try {
            ClientResponse resp = resource.path("commit").queryParam("all", "false").post(ClientResponse.class, message);
            if (resp.getStatus() != 200 && resp.getStatus() != 204) {
                throwRespositoryException(resp);
            }
        }
        catch (ClientHandlerException e) {
            throwRespositoryException(e);
        }
    }

    @Override
    public void resolve(String path, String stage) throws RepositoryException {
        try {
            discardResourceInfo(path);

            ClientResponse resp = resource.path("resolve")
                .queryParam("path", path)
                .queryParam("stage", stage)
                .post(ClientResponse.class);
            if (resp.getStatus() != 200 && resp.getStatus() != 204) {
                throwRespositoryException(resp);
            }
        }
        catch (ClientHandlerException e) {
            throwRespositoryException(e);
        }
    }

    @Override
    public void publish() throws RepositoryException {
        wrapPost("publish");
    }

    @Override
    public URI getURI() {
        return resource.getURI();
    }

    @Override
    public BuildDesc build(boolean rebuild) throws RepositoryException {
        try {
            ClientResponse resp = resource.path("builds").queryParam("rebuild", rebuild ? "true" : "false").post(ClientResponse.class);
            if (resp.getStatus() != 200 && resp.getStatus() != 204) {
                throwRespositoryException(resp);
            }
            return resp.getEntity(BuildDesc.class);
        }
        catch (ClientHandlerException e) {
            throwRespositoryException(e);
            return null; // Never reached
        }
    }

    @Override
    public BuildDesc getBuildStatus(int id) throws RepositoryException {
        try {
            ClientResponse resp = resource.path("builds").queryParam("id", Integer.toString(id)).get(ClientResponse.class);
            if (resp.getStatus() != 200 && resp.getStatus() != 204) {
                throwRespositoryException(resp);
            }
            return resp.getEntity(BuildDesc.class);
        }
        catch (ClientHandlerException e) {
            throwRespositoryException(e);
            return null; // Never reached
        }
    }

    @Override
    public BuildLog getBuildLogs(int id) throws RepositoryException {
        try {
            ClientResponse resp = resource.path("builds/log").queryParam("id", Integer.toString(id)).get(ClientResponse.class);
            if (resp.getStatus() != 200 && resp.getStatus() != 204) {
                throwRespositoryException(resp);
            }
            return resp.getEntity(BuildLog.class);
        }
        catch (ClientHandlerException e) {
            throwRespositoryException(e);
            return null; // Never reached
        }

    }

}
