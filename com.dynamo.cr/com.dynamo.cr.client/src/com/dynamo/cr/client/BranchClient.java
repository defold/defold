package com.dynamo.cr.client;

import java.net.URI;

import com.dynamo.cr.common.providers.ProtobufProviders;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;
import com.dynamo.cr.protocol.proto.Protocol.CommitDesc;
import com.dynamo.cr.protocol.proto.Protocol.Log;
import com.dynamo.cr.protocol.proto.Protocol.ResourceInfo;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.ClientHandlerException;
import com.sun.jersey.api.client.ClientResponse;
import com.sun.jersey.api.client.WebResource;

public class BranchClient extends BaseClient implements IBranchClient {

    // NOTE: Only public for package
    BranchClient(IClientFactory factory, URI uri, Client client) {
        super(factory, uri);
        this.resource = client.resource(uri);
    }

    @Override
    public String getNativeLocation() {
        return null;
    }

    @Override
    public BranchStatus getBranchStatus(boolean fetch) throws RepositoryException {
        return wrapGet("", BranchStatus.class);
    }

    @Override
    public void autoStage() {
        // Only support in local client
    }

    @Override
    public ResourceInfo getResourceInfo(String path) throws RepositoryException {
        try {
            WebResource sub_resource = resource.path("/resources/info").queryParam("path", path);

            ClientResponse resp = sub_resource.accept(ProtobufProviders.APPLICATION_XPROTOBUF).get(ClientResponse.class);
            if (resp.getStatus() != 200) {
                ClientUtils.throwRespositoryException(resp);
            }
            ResourceInfo ret = resp.getEntity(ResourceInfo.class);
            return ret;
        }
        catch (ClientHandlerException e) {
            ClientUtils.throwRespositoryException(e);
            return null; // Never reached
        }
    }

    @Override
    public byte[] getResourceData(String path, String revision) throws RepositoryException {
        try {
            ClientResponse resp = resource.path("/resources/data").queryParam("path", path).queryParam("revision", revision).get(ClientResponse.class);
            if (resp.getStatus() != 200) {
                ClientUtils.throwRespositoryException(resp);
            }
            return resp.getEntity(byte[].class);
        }
        catch (ClientHandlerException e) {
            ClientUtils.throwRespositoryException(e);
            return null; // Never reached
        }
    }

    @Override
    public void putResourceData(String path, byte[] data) throws RepositoryException {
        try {
            ClientResponse resp = resource.path("/resources/data").queryParam("path", path).put(ClientResponse.class, data);
            if (resp.getStatus() != 200 && resp.getStatus() != 204) {
                ClientUtils.throwRespositoryException(resp);
            }
        }
        catch (ClientHandlerException e) {
            ClientUtils.throwRespositoryException(e);
        }
    }

    @Override
    public void mkdir(String path) throws RepositoryException {
        try {
            ClientResponse resp = resource.path("/resources/data").queryParam("path", path).queryParam("directory", "true").put(ClientResponse.class);
            if (resp.getStatus() != 200 && resp.getStatus() != 204) {
                ClientUtils.throwRespositoryException(resp);
            }
        }
        catch (ClientHandlerException e) {
            ClientUtils.throwRespositoryException(e);
        }
    }

    @Override
    public void deleteResource(String path) throws RepositoryException {
        try {
            ClientResponse resp = resource.path("/resources/info").queryParam("path", path).delete(ClientResponse.class);
            if (resp.getStatus() != 200 && resp.getStatus() != 204) {
                ClientUtils.throwRespositoryException(resp);
            }
        }
        catch (ClientHandlerException e) {
            ClientUtils.throwRespositoryException(e);
        }
    }

    @Override
    public void renameResource(String source, String destination) throws RepositoryException {
        try {
            ClientResponse resp = resource.path("/resources/rename").queryParam("source", source).queryParam("destination", destination).post(ClientResponse.class);
            if (resp.getStatus() != 200 && resp.getStatus() != 204) {
                ClientUtils.throwRespositoryException(resp);
            }
        }
        catch (ClientHandlerException e) {
            ClientUtils.throwRespositoryException(e);
        }
    }

	@Override
	public void revertResource(String path) throws RepositoryException {
        try {
            ClientResponse resp = resource.path("/resources/revert").queryParam("path", path).put(ClientResponse.class);
            if (resp.getStatus() != 200 && resp.getStatus() != 204) {
                ClientUtils.throwRespositoryException(resp);
            }
        }
        catch (ClientHandlerException e) {
            ClientUtils.throwRespositoryException(e);
        }
	}

    @Override
    public BranchStatus update() throws RepositoryException {
        return wrapPost("update", BranchStatus.class);
    }

    @Override
    public CommitDesc commit(String message) throws RepositoryException {
        try {
            ClientResponse resp = resource.path("commit")
            .queryParam("all", "true")
            .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
            .post(ClientResponse.class, message);
            if (resp.getStatus() != 200 && resp.getStatus() != 204) {
                ClientUtils.throwRespositoryException(resp);
                return null;
            }
            return resp.getEntity(CommitDesc.class);
        }
        catch (ClientHandlerException e) {
            ClientUtils.throwRespositoryException(e);
            return null;
        }
    }

    @Override
    public CommitDesc commitMerge(String message) throws RepositoryException {
        try {
            ClientResponse resp = resource.path("commit")
            .queryParam("all", "false")
            .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
            .post(ClientResponse.class, message);
            if (resp.getStatus() != 200 && resp.getStatus() != 204) {
                ClientUtils.throwRespositoryException(resp);
                return null;
            }
            return resp.getEntity(CommitDesc.class);
        }
        catch (ClientHandlerException e) {
            ClientUtils.throwRespositoryException(e);
            return null;
        }
    }

    @Override
    public void resolve(String path, String stage) throws RepositoryException {
        try {
            ClientResponse resp = resource.path("resolve")
                .queryParam("path", path)
                .queryParam("stage", stage)
                .post(ClientResponse.class);
            if (resp.getStatus() != 200 && resp.getStatus() != 204) {
                ClientUtils.throwRespositoryException(resp);
            }
        }
        catch (ClientHandlerException e) {
            ClientUtils.throwRespositoryException(e);
        }
    }

    @Override
    public void publish() throws RepositoryException {
        wrapPost("publish");
    }

    @Override
    public Log log(int maxCount) throws RepositoryException {
        try {
            ClientResponse resp = resource.path("log")
                .queryParam("max_count", new Integer(maxCount).toString())
                .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
                .get(ClientResponse.class);
            if (resp.getStatus() != 200 && resp.getStatus() != 204) {
                ClientUtils.throwRespositoryException(resp);
            }
            return resp.getEntity(Log.class);
        }
        catch (ClientHandlerException e) {
            ClientUtils.throwRespositoryException(e);
            return null; // Never reached
        }
    }

    @Override
    public void reset(String mode, String target) throws RepositoryException {
        try {
            ClientResponse resp = resource.path("reset")
                    .queryParam("mode", mode)
                    .queryParam("target", target)
                    .post(ClientResponse.class);
            if (resp.getStatus() != 200 && resp.getStatus() != 204) {
                ClientUtils.throwRespositoryException(resp);
            }
        }
        catch (ClientHandlerException e) {
            ClientUtils.throwRespositoryException(e);
        }
    }

}
