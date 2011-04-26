package com.dynamo.cr.scene.graph;

import com.dynamo.cr.scene.resource.Resource;


public abstract class ComponentNode<T extends Resource> extends Node {
    T resource;

    public ComponentNode(String resourceName, T resource, Scene scene) {
        super(resourceName, scene, FLAG_CAN_HAVE_CHILDREN);
        this.resource = resource;
    }

    @Override
    public void draw(DrawContext context) {
    }

    public T getResource() {
        return resource;
    }

    public String getResourceIdentifier() {
        return getIdentifier();
    }

    @Override
    protected boolean verifyChild(Node child) {
        return false;
    }
}
