package com.dynamo.cr.guieditor.scene;

import java.awt.geom.Rectangle2D;

import com.dynamo.cr.guieditor.DrawContext;
import com.dynamo.cr.guieditor.render.IGuiRenderer;
import com.dynamo.gui.proto.Gui.NodeDesc;
import com.dynamo.gui.proto.Gui.NodeDesc.Builder;

public class BoxGuiNode extends GuiNode {
    public BoxGuiNode(GuiScene scene, NodeDesc nodeDesc) {
        super(scene, nodeDesc);
    }

    public void draw(DrawContext context) {
        IGuiRenderer renderer = context.getRenderer();
        double x0 = position.x - size.x / 2;
        double y0 = position.y - size.y / 2;
        double x1 = x0 + size.x;
        double y1 = y0 + size.y;

        renderer.drawQuad(x0, y0, x1, y1, color.x, color.y, color.z, color.w, getBlendMode(), context.getRenderResourceCollection().getTexture(getTexture()));
    }

    public void drawSelect(DrawContext context) {
        IGuiRenderer renderer = context.getRenderer();
        double x0 = position.x - size.x / 2;
        double y0 = position.y - size.y / 2;
        double x1 = x0 + size.x;
        double y1 = y0 + size.y;
        renderer.drawQuad(x0, y0, x1, y1, color.x, color.y, color.z, color.w, null, null);
    }

    public Rectangle2D getBounds() {
        double x0 = position.x - size.x / 2;
        double y0 = position.y - size.y / 2;
        return new Rectangle2D.Double(x0, y0, size.x, size.y);
    }

    @Override
    public void doBuildNodeDesc(Builder builder) {
    }
}
