package com.dynamo.bob.pipeline;

import java.util.ArrayList;
import java.util.List;

public class ResourceNode {

    public final String relativeFilepath;
    public final String absoluteFilepath;
    private final List<ResourceNode> children = new ArrayList<ResourceNode>();

    public ResourceNode(final String relativeFilepath, final String absoluteFilepath) {
        this.relativeFilepath = relativeFilepath;
        this.absoluteFilepath = absoluteFilepath;
        System.out.println("Creating ResourceNode: " + absoluteFilepath);
    }

    public void addChild(ResourceNode childNode) {
        System.out.println(this.relativeFilepath + " -> " + childNode.relativeFilepath);
        this.children.add(childNode);
    }

    public void print(int indent) {
        String padding = new String(new char[indent]).replace('\0', ' ');
        System.out.println(padding + this.relativeFilepath);
        for (ResourceNode child : this.children) {
            child.print(indent + 4);
        }
    }

}