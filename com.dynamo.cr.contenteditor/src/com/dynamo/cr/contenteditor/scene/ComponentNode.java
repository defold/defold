package com.dynamo.cr.contenteditor.scene;

import com.dynamo.cr.contenteditor.editors.DrawContext;

public class ComponentNode extends Node {

    private String resource;

    public ComponentNode(Scene scene, String resource) {
        super(scene, FLAG_CAN_HAVE_CHILDREN);
        this.resource = resource;
    }

    @Override
    public String getName() {
        return resource;
    }

    @Override
    public void draw(DrawContext context) {
        // TODO Auto-generated method stub

    }

    public String getResource() {
        return resource;
    }

    @Override
    protected boolean verifyChild(Node child) {
        return false;
    }
}
