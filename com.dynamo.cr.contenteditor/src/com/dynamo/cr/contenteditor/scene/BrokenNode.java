package com.dynamo.cr.contenteditor.scene;

import com.dynamo.cr.contenteditor.editors.DrawContext;

public class BrokenNode extends LeafNode {

    private String errorMessage;

    public BrokenNode(String identifier, Scene scene, String errorMessage) {
        super(identifier, scene);
        this.errorMessage = errorMessage;
    }

    @Override
    public void draw(DrawContext context) {
    }

    @Override
    public String getToolTip() {
        return errorMessage;
    }
}
