package com.dynamo.cr.guieditor;

import java.util.List;

import com.dynamo.cr.guieditor.render.GuiRenderer;
import com.dynamo.cr.guieditor.scene.GuiNode;

public class DrawContext {
    private GuiRenderer renderer;
    private List<GuiNode> selectedNodes;

    public DrawContext(GuiRenderer renderer, List<GuiNode> list) {
        this.renderer = renderer;
        this.selectedNodes = list;
    }

    public GuiRenderer getRenderer() {
        return renderer;
    }

    public boolean isSelected(GuiNode guiNode) {
        for (GuiNode node : selectedNodes) {
            if (node == guiNode)
                return true;
        }
        return false;
    }
}
