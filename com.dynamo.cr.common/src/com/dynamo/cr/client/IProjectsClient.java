package com.dynamo.cr.client;

import com.dynamo.cr.protocol.proto.Protocol.ProjectInfoList;

public interface IProjectsClient {
    ProjectInfoList getProjects() throws RepositoryException;
    IProjectClient getProjectClient(long projectId);
}
