package com.dynamo.cr.client;

import java.io.InputStream;
import java.net.URI;
import java.util.regex.Pattern;

import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.UriBuilder;

import org.apache.commons.io.IOUtils;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.Path;

import com.dynamo.cr.branchrepo.BranchRepositoryException;
import com.dynamo.cr.common.providers.ProtobufProviders;
import com.dynamo.cr.protocol.proto.Protocol.BranchList;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;
import com.dynamo.cr.protocol.proto.Protocol.ProjectInfo;
import com.dynamo.cr.protocol.proto.Protocol.UserInfo;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.ClientHandlerException;
import com.sun.jersey.api.client.ClientResponse;
import com.sun.jersey.api.client.ClientResponse.Status;
import com.sun.jersey.api.client.WebResource;

public class LocalProjectClient implements IProjectClient {

    private IClientFactory factory;
    private URI uri;
    private long projectId;
    private LocalBranchRepository branchRepository;
    private String project;
    private String user;
    private Client client;

    /* package*/ LocalProjectClient(IClientFactory factory, URI uri, Client client, UserInfo userInfo, ProjectInfo projectInfo, String branchRoot, String email, String password) {
        this.factory = factory;
        this.uri = uri;
        this.client = client;
        this.projectId = projectInfo.getId();
        this.project = Long.toString(projectId);
        this.user = Long.toString(userInfo.getId());

        IPath repositoryRootPath = new Path(UriBuilder.fromUri(projectInfo.getRepositoryUrl()).build().getPath());
        repositoryRootPath = repositoryRootPath.removeLastSegments(1);
        URI repositoryRootUrl = UriBuilder.fromUri(projectInfo.getRepositoryUrl()).replacePath(repositoryRootPath.toPortableString()).build();

        branchRepository = new LocalBranchRepository(branchRoot,
                                                     repositoryRootUrl.toString(),
                                                     /* TODO */ null,
                                                     /* TODO */ new Pattern[0],
                                                     userInfo,
                                                     password,
                                                     repositoryRootUrl.getHost());
    }

    @Override
    public IClientFactory getClientFactory() {
        return factory;
    }

    @Override
    public URI getURI() {
        return uri;
    }

    @Override
    public long getProjectId() {
        return projectId;
    }

    @Override
    public void deleteBranch(String branch) throws RepositoryException {
        try {
            branchRepository.deleteBranch(project, user, branch);
        } catch (BranchRepositoryException e) {
            throw new RepositoryException("Unable to delete branch", e);
        }
    }

    @Override
    public BranchList getBranchList() throws RepositoryException {
        return branchRepository.getBranchList(project, user);
    }

    @Override
    public BranchStatus getBranchStatus(String branch, boolean fetch)
            throws RepositoryException {
        try {
            return branchRepository.getBranchStatus(project, user, branch, fetch);
        } catch (Exception e) {
            throw new RepositoryException("Unable to fetch branch status", e);
        }
    }

    @Override
    public void createBranch(String branch) throws RepositoryException {
        try {
            branchRepository.createBranch(project, user, branch);
        } catch (Exception e) {
            throw new RepositoryException("Unable to create branch", e);
        }
    }

    protected <T> T wrapGet(String path, Class<T> klass) throws RepositoryException {
        return wrapGet(path, klass, ProtobufProviders.APPLICATION_XPROTOBUF);
    }

    protected <T> T wrapGet(String path, Class<T> klass, String... accepts) throws RepositoryException {
        try {
            WebResource resource = client.resource(uri);

            ClientResponse resp = resource.path(path).accept(accepts).get(ClientResponse.class);
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
    public ProjectInfo getProjectInfo() throws RepositoryException {
        return wrapGet("/project_info", ProjectInfo.class);
    }

    @Override
    public void setProjectInfo(ProjectInfo projectInfo)
            throws RepositoryException {
        try {
            WebResource resource = client.resource(uri);
            ClientResponse resp = resource.path("/project_info").accept(ProtobufProviders.APPLICATION_XPROTOBUF).put(ClientResponse.class, projectInfo);

            if (resp.getClientResponseStatus() != Status.NO_CONTENT) {
                ClientUtils.throwRespositoryException(resp);
            }
        }
        catch (ClientHandlerException e) {
            ClientUtils.throwRespositoryException(e);
        }
    }

    public void uploadEngine(String platform, InputStream stream) throws RepositoryException {
        try {
            WebResource resource = client.resource(uri);
            ClientResponse resp = resource
                    .path("/engine").path(platform)
                    .type(MediaType.APPLICATION_OCTET_STREAM_TYPE)
                    .post(ClientResponse.class, stream);

            if (resp.getStatus() != 200) {
                ClientUtils.throwRespositoryException(resp);
            }
        } catch (ClientHandlerException e) {
            ClientUtils.throwRespositoryException(e);
        } finally {
            if (stream != null) {
                IOUtils.closeQuietly(stream);
            }
        }
    }

    @Override
    public byte[] downloadEngine(String platform, String key) throws RepositoryException {
        return wrapGet("/engine/" + platform + "/" + key, byte[].class, MediaType.APPLICATION_OCTET_STREAM);
    }

    @Override
    public String downloadEngineManifest(String platform, String key) throws RepositoryException {
        return wrapGet("/engine_manifest/" + platform + "/" + key, String.class, "text/xml");
    }

}
