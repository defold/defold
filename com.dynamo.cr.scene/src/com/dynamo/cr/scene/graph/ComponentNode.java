package com.dynamo.cr.scene.graph;


public class ComponentNode extends Node {
    public ComponentNode(String resource, Scene scene) {
        super(resource, scene, FLAG_CAN_HAVE_CHILDREN);
    }

    @Override
    public void draw(DrawContext context) {
        // TODO Auto-generated method stub

    }

    public String getResource() {
        return getIdentifier();
    }

    @Override
    protected boolean verifyChild(Node child) {
        return false;
    }
}
