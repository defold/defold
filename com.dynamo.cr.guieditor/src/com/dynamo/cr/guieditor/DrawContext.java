package com.dynamo.cr.guieditor;

import java.util.List;

import com.dynamo.cr.guieditor.render.IGuiRenderer;
import com.dynamo.cr.guieditor.scene.GuiNode;
import com.dynamo.cr.guieditor.scene.RenderResourceCollection;

public class DrawContext {
    private IGuiRenderer renderer;
    private RenderResourceCollection renderResourceCollection;
    private List<GuiNode> selectedNodes;

    public DrawContext(IGuiRenderer renderer, RenderResourceCollection renderResourceCollection, List<GuiNode> list) {
        this.renderer = renderer;
        this.renderResourceCollection = renderResourceCollection;
        this.selectedNodes = list;
    }

    public IGuiRenderer getRenderer() {
        return renderer;
    }

    public boolean isSelected(GuiNode guiNode) {
        for (GuiNode node : selectedNodes) {
            if (node == guiNode)
                return true;
        }
        return false;
    }

    public RenderResourceCollection getRenderResourceCollection() {
        return renderResourceCollection;
    }
}
