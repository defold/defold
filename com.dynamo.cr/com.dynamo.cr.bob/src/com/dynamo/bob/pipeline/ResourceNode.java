package com.dynamo.bob.pipeline;

import java.util.ArrayList;
import java.util.List;

public class ResourceNode {

    public final String relativeFilepath;
    public final String absoluteFilepath;
    private ResourceNode parent = null;
    private final List<ResourceNode> children = new ArrayList<ResourceNode>();

    public ResourceNode(final String relativeFilepath, final String absoluteFilepath) {
        if (relativeFilepath.startsWith("/")) {
            this.relativeFilepath = relativeFilepath;
        } else {
            this.relativeFilepath = "/" + relativeFilepath;
        }
        this.absoluteFilepath = absoluteFilepath;
    }

    public void addChild(ResourceNode childNode) {
        childNode.parent = this;
        this.children.add(childNode);
    }

    public List<ResourceNode> getChildren() {
        return this.children;
    }

    public ResourceNode getParent() {
        return this.parent;
    }

}