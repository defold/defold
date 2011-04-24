package com.dynamo.cr.client;

import com.dynamo.cr.protocol.proto.Protocol.ProjectInfoList;

public interface IProjectsClient extends IClient {
    ProjectInfoList getProjects() throws RepositoryException;
}
