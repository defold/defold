package com.dynamo.bob.pipeline;

import java.util.ArrayList;
import java.util.List;

public class ResourceNode {

    public final String relativeFilepath;
    public final String absoluteFilepath;
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
        this.children.add(childNode);
    }

    public List<ResourceNode> getChildren() {
        return this.children;
    }

    public void print(int indent) {
        String padding = new String(new char[indent]).replace('\0', ' ');
        System.out.println(padding + this.relativeFilepath);
        for (ResourceNode child : this.children) {
            child.print(indent + 4);
        }
    }

}