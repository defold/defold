package com.dynamo.cr.editor.fs.internal;

import java.util.ArrayList;
import java.util.List;

import com.dynamo.cr.client.IBranchClient;
import com.dynamo.cr.client.RepositoryException;
import com.dynamo.cr.protocol.proto.Protocol.ResourceInfo;
import com.dynamo.cr.protocol.proto.Protocol.ResourceType;

public class ResourceInfoCache {

    static class Node
    {
        /// Flags set to 1 if the meta-data for this node is known
        final static int STATE_METADATA_KNOWN = (1 << 0);
        /// Flags set to 1 if children for this node is known
        final static int STATE_METADATA_CHILDREN_KNOWN = (1 << 1);

        String path;
        String name;
        ResourceType type;
        long lastModified;
        int size;

        int state = 0;

        List<ResourceNode> children = new ArrayList<ResourceNode>();
    }

    private Node root = null;
    private IBranchClient branchClient;

    public ResourceInfoCache(IBranchClient branchClient) {
        this.branchClient = branchClient;
    }

    public ResourceInfo getResourceInfo(String path) throws RepositoryException
    {
        return this.branchClient.getResourceInfo(path);
    }

    public void addResourceInfo(ResourceInfo resourceInfo)
    {
    }

    public ResourceNode getNode(String path)
    {
        return null;
    }
}
