package com.dynamo.cr.contenteditor.scene;


public abstract class LeafNode extends Node
{
    public LeafNode(Scene scene) {
        super(scene, 0);
    }

    @Override
    protected final boolean verifyChild(Node child) {
        return false;
    }
}
