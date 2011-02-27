package com.dynamo.cr.contenteditor.scene;

import com.dynamo.cr.contenteditor.editors.DrawContext;

public class PrototypeNode extends Node {

    private String name;

    public PrototypeNode(Scene scene, String name) {
        super(scene, FLAG_CAN_HAVE_CHILDREN);
        this.name = name;
    }

    @Override
    public void nodeAdded(Node node) {
        assert (node instanceof ComponentNode);
    }

    @Override
    public String getName() {
        return name;
    }

    @Override
    public void draw(DrawContext context) {
    }

    @Override
    protected boolean verifyChild(Node child) {
        return (child instanceof ComponentNode) || (child instanceof BrokenNode);
    }
}
