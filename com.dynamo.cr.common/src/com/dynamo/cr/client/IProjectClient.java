package com.dynamo.cr.client;

import java.io.InputStream;

import com.dynamo.cr.proto.Config.ProjectConfiguration;
import com.dynamo.cr.protocol.proto.Protocol.ApplicationInfo;
import com.dynamo.cr.protocol.proto.Protocol.BranchList;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;

public interface IProjectClient {

    public IBranchClient getBranchClient(String user, String branch);

    public void deleteBranch(String user, String branch)
            throws RepositoryException;

    public BranchList getBranchList(String user)
            throws RepositoryException;

    public BranchStatus getBranchStatus(String user, String branch)
            throws RepositoryException;

    public void createBranch(String user, String branch)
            throws RepositoryException;

    public ProjectConfiguration getProjectConfiguration() throws RepositoryException;

    public ApplicationInfo getApplicationInfo(String platform) throws RepositoryException;

    public InputStream getApplicationData(String platform) throws RepositoryException;
}
