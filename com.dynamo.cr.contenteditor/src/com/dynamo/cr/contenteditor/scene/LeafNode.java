package com.dynamo.cr.contenteditor.scene;


public abstract class LeafNode extends Node
{
    public LeafNode(String identifier, Scene scene) {
        super(identifier, scene, 0);
    }

    @Override
    protected final boolean verifyChild(Node child) {
        return false;
    }
}
