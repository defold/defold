package com.dynamo.cr.client;

import java.io.IOException;
import java.net.URI;
import java.util.regex.Pattern;

import javax.ws.rs.core.Response.Status;
import javax.ws.rs.core.UriBuilder;

import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.Path;

import com.dynamo.cr.branchrepo.BranchRepositoryException;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;
import com.dynamo.cr.protocol.proto.Protocol.CommitDesc;
import com.dynamo.cr.protocol.proto.Protocol.Log;
import com.dynamo.cr.protocol.proto.Protocol.ProjectInfo;
import com.dynamo.cr.protocol.proto.Protocol.ResolveStage;
import com.dynamo.cr.protocol.proto.Protocol.ResourceInfo;
import com.dynamo.cr.protocol.proto.Protocol.UserInfo;

public class LocalBranchClient implements IBranchClient {

    private IClientFactory factory;
    private URI uri;
    private LocalBranchRepository branchRepository;
    private long projectId;
    private String project;
    private String user;
    private String branch;
    private String branchRoot;

    /* package*/ LocalBranchClient(IClientFactory factory, URI uri, UserInfo userInfo, ProjectInfo projectInfo, String branchRoot, String email, String password) {
        this.factory = factory;
        this.uri = uri;
        this.projectId = projectInfo.getId();
        this.project = Long.toString(projectId);
        this.user = Long.toString(userInfo.getId());
        this.branchRoot = branchRoot;

        branch = new Path(uri.getPath()).lastSegment();

        IPath repositoryRootPath = new Path(UriBuilder.fromUri(projectInfo.getRepositoryUrl()).build().getPath());
        repositoryRootPath = repositoryRootPath.removeLastSegments(1);
        URI repositoryRootUrl = UriBuilder.fromUri(projectInfo.getRepositoryUrl()).replacePath(repositoryRootPath.toPortableString()).build();

        Pattern[] patterns = new Pattern[] {
                Pattern.compile(".*?\\.git"),
                Pattern.compile("/content/build"),
                Pattern.compile("/build"),
        };

        branchRepository = new LocalBranchRepository(branchRoot,
                                                     repositoryRootUrl.toString(),
                                                     /* TODO */ null,
                                                     patterns,
                                                     userInfo,
                                                     password,
                                                     repositoryRootUrl.getHost());
    }

    @Override
    public String getNativeLocation() {
        return new Path(branchRoot).append(project).append(user).append(branch).toOSString();
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
    public byte[] getResourceData(String path, String revision)
            throws RepositoryException {
        try {
            return branchRepository.getResourceData(project, user, branch, path, revision);
        } catch (BranchRepositoryException e) {
            throw new RepositoryException(String.format("Unable to get resource data '%s'", path), e.getStatus().getStatusCode());
        } catch (IOException e) {
            throw new RepositoryException(String.format("Unable to get resource data '%s'", path), e);
        }
    }

    @Override
    public ResourceInfo getResourceInfo(String path) throws RepositoryException {
        try {
            return branchRepository.getResourceInfo(project, user, branch, path);
        } catch (BranchRepositoryException e) {
            throw new RepositoryException(String.format("Unable to get resource info '%s'", path), e.getStatus().getStatusCode());
        } catch (IOException e) {
            throw new RepositoryException(String.format("Unable to get resource info '%s'", path), e);
        }
    }

    @Override
    public BranchStatus getBranchStatus(boolean fetch) throws RepositoryException {
        try {
            return branchRepository.getBranchStatus(project, user, branch, fetch);
        } catch (Exception e) {
            throw new RepositoryException("Unable to get branch status", e);
        }
    }

    @Override
    public void autoStage() throws RepositoryException {
        try {
            branchRepository.autoStage(project, user, branch);
        } catch (Exception e) {
            throw new RepositoryException("Unable to auto-stage files", e);
        }
    }

    @Override
    public void putResourceData(String path, byte[] bytes)
            throws RepositoryException {
        try {
            branchRepository.putResourceData(project, user, branch, path, bytes);
        } catch (Exception e) {
            throw new RepositoryException(String.format("Unable to put resource data '%s'", path), e);
        }
    }

    @Override
    public void mkdir(String path) throws RepositoryException {
        try {
            branchRepository.mkdir(project, user, branch, path);
        } catch (BranchRepositoryException e) {
            throw new RepositoryException(String.format("Unable to make directory '%s'", path), e);
        }
    }

    @Override
    public void deleteResource(String path) throws RepositoryException {
        try {
            branchRepository.deleteResource(project, user, branch, path);
        } catch (Exception e) {
            throw new RepositoryException(String.format("Unable to get delete resource '%s'", path), e);
        }

    }

    @Override
    public void renameResource(String source, String destination)
            throws RepositoryException {
        try {
            branchRepository.renameResource(project, user, branch, source, destination);
        } catch (Exception e) {
            throw new RepositoryException(String.format("Unable to rename resource from '%s' to '%s'", source, destination), e);
        }
    }

    @Override
    public void revertResource(String path) throws RepositoryException {
        try {
            branchRepository.revertResource(project, user, branch, path);
        } catch (Exception e) {
            throw new RepositoryException(String.format("Unable to revert resource '%s'", path), e);
        }
    }

    @Override
    public BranchStatus update() throws RepositoryException {
        try {
            branchRepository.updateBranch(project, user, branch);
            return branchRepository.getBranchStatus(project, user, branch, true);
        } catch (Exception e) {
            throw new RepositoryException(String.format("Unable to update branch '%s'", branch), e);
        }
    }

    @Override
    public CommitDesc commit(String message) throws RepositoryException {
        try {
            return branchRepository.commitBranch(project, user, branch, message);
        } catch (Exception e) {
            throw new RepositoryException(String.format("Unable to commit branch '%s'", branch), e);
        }
    }

    @Override
    public CommitDesc commitMerge(String message) throws RepositoryException {
        try {
            return branchRepository.commitMergeBranch(project, user, branch, message);
        } catch (Exception e) {
            throw new RepositoryException(String.format("Unable to commit branch '%s'", branch), e);
        }
    }

    @Override
    public void resolve(String path, String stage) throws RepositoryException {
        ResolveStage s;
        if (stage.equals("base"))
            s = ResolveStage.BASE;
        else if (stage.equals("yours"))
            s = ResolveStage.YOURS;
        else if (stage.equals("theirs"))
            s = ResolveStage.THEIRS;
        else
            throw new RepositoryException(String.format("Unknown stage: %s", stage), 500);

        try {
            branchRepository.resolveResource(project, user, branch, path, s);
        } catch (BranchRepositoryException e) {
            throw new RepositoryException(String.format("Unable to resolve '%s'", path), e.getStatus().getStatusCode());
        } catch (IOException e) {
            throw new RepositoryException(String.format("Unable to resolve '%s'", path), e);
        }
    }

    @Override
    public void publish() throws RepositoryException {
        try {
            branchRepository.publishBranch(project, user, branch);
        } catch (BranchRepositoryException e) {
            throw new RepositoryException("Unable to publish branch", e.getStatus().getStatusCode());
        } catch (IOException e) {
            throw new RepositoryException("Unable to publish branch", Status.BAD_REQUEST.getStatusCode());
        }
    }

    @Override
    public Log log(int maxCount) throws RepositoryException {
        try {
            return branchRepository.logBranch(project, user, branch, maxCount);
        } catch (Exception e) {
            throw new RepositoryException(String.format("Unable to get log for branch '%s'", branch), e);
        }
    }

    @Override
    public void reset(String mode, String target) throws RepositoryException {
        try {
            branchRepository.reset(project, user, branch, mode, target);
        } catch (Exception e) {
            throw new RepositoryException(String.format("Unable to reset resource '%s'", target), e);
        }
    }

}
