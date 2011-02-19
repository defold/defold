package com.dynamo.cr.contenteditor.scene;

import com.dynamo.cr.contenteditor.editors.DrawContext;

public class BrokenNode extends LeafNode {

    private String name;
    private String errorMessage;

    public BrokenNode(Scene scene, String name, String errorMessage) {
        super(scene);
        this.name = name;
        this.errorMessage = errorMessage;
    }

    @Override
    public String getName() {
        return name;
    }

    @Override
    public void draw(DrawContext context) {
    }

    public String getErrorMessage() {
        return errorMessage;
    }

}
