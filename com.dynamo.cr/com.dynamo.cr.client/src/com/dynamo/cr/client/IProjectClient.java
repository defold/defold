package com.dynamo.cr.client;

import java.io.InputStream;

import com.dynamo.cr.protocol.proto.Protocol.BranchList;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;
import com.dynamo.cr.protocol.proto.Protocol.ProjectInfo;

public interface IProjectClient extends IClient {

    public long getProjectId();

    public void deleteBranch(String branch)
            throws RepositoryException;

    public BranchList getBranchList()
            throws RepositoryException;

    public BranchStatus getBranchStatus(String branch, boolean fetch)
            throws RepositoryException;

    public void createBranch(String branch)
            throws RepositoryException;

    public ProjectInfo getProjectInfo() throws RepositoryException;

    public void setProjectInfo(ProjectInfo projectInfo) throws RepositoryException;

    public void uploadEngine(String platform, InputStream stream) throws RepositoryException;

    public byte[] downloadEngine(String platform, String key) throws RepositoryException;

    public String downloadEngineManifest(String platform, String key) throws RepositoryException;

}
