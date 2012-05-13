package com.dynamo.cr.editor.fs.test;

import java.net.URI;

import com.dynamo.cr.client.IBranchClient;
import com.dynamo.cr.client.IClientFactory;
import com.dynamo.cr.client.IProjectClient;
import com.dynamo.cr.client.IProjectsClient;
import com.dynamo.cr.client.IUsersClient;

public class MockClientFactory implements IClientFactory {

    private MockBranchClient branchClient;

    public MockClientFactory(MockBranchClient branchClient) {
        this.branchClient = branchClient;
    }

    @Override
    public IProjectClient getProjectClient(URI uri) {
        throw new RuntimeException("Not impl.");
    }

    @Override
    public IProjectsClient getProjectsClient(URI uri) {
        throw new RuntimeException("Not impl.");
    }

    @Override
    public IBranchClient getBranchClient(URI uri) {
        return this.branchClient;
    }

    @Override
    public IUsersClient getUsersClient(URI uri) {
        throw new RuntimeException("Not impl.");
    }

}
