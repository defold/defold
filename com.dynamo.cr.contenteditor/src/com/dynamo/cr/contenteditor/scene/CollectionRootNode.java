package com.dynamo.cr.contenteditor.scene;

import com.dynamo.cr.contenteditor.editors.DrawContext;

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
