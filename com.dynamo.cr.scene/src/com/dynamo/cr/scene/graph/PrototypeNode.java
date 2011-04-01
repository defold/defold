package com.dynamo.cr.scene.graph;


public class PrototypeNode extends Node {

    public PrototypeNode(String identifier, Scene scene) {
        super(identifier, scene, FLAG_CAN_HAVE_CHILDREN);
    }

    @Override
    public void nodeAdded(Node node) {
        assert (node instanceof ComponentNode);
    }

    @Override
    public void draw(DrawContext context) {
    }

    @Override
    protected boolean verifyChild(Node child) {
        return child instanceof ComponentNode;
    }
}
