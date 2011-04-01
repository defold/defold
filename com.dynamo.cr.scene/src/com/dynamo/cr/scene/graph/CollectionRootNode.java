package com.dynamo.cr.scene.graph;


public class CollectionRootNode extends Node {

    public CollectionRootNode(Scene scene) {
        super("root", scene, FLAG_CAN_HAVE_CHILDREN);
    }

    @Override
    protected boolean verifyChild(Node child) {
        return child instanceof CollectionNode;
    }

    @Override
    public void draw(DrawContext context) {
    }
}
