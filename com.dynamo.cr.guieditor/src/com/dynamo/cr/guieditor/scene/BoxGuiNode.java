package com.dynamo.cr.guieditor.scene;

import java.awt.geom.Rectangle2D;

import com.dynamo.cr.guieditor.DrawContext;
import com.dynamo.cr.guieditor.render.IGuiRenderer;
import com.dynamo.gui.proto.Gui.NodeDesc;
import com.dynamo.gui.proto.Gui.NodeDesc.Builder;
import com.dynamo.gui.proto.Gui.NodeDesc.Pivot;

public class BoxGuiNode extends GuiNode {
    public BoxGuiNode(GuiScene scene, NodeDesc nodeDesc) {
        super(scene, nodeDesc);
    }

    private double pivotOffsetX(double width) {
        Pivot p = getPivot();

        switch (p) {
        case PIVOT_CENTER:
        case PIVOT_S:
        case PIVOT_N:
            return width * 0.5;

        case PIVOT_NE:
        case PIVOT_E:
        case PIVOT_SE:
            return width;

        case PIVOT_SW:
        case PIVOT_W:
        case PIVOT_NW:
            return 0;
        }

        assert false;
        return Double.MAX_VALUE;
    }

    private double pivotOffsetY(double height) {
        Pivot p = getPivot();

        switch (p) {
        case PIVOT_CENTER:
        case PIVOT_E:
        case PIVOT_W:
            return height * 0.5;

        case PIVOT_N:
        case PIVOT_NE:
        case PIVOT_NW:
            return height;

        case PIVOT_S:
        case PIVOT_SW:
        case PIVOT_SE:
            return 0;
        }

        assert false;
        return Double.MAX_VALUE;
    }

    public void draw(DrawContext context) {
        IGuiRenderer renderer = context.getRenderer();
        double x0 = position.x; // - size.x / 2;
        double y0 = position.y; //- size.y / 2;

        x0 -= pivotOffsetX(size.x);
        y0 -= pivotOffsetY(size.y);

        double x1 = x0 + size.x;
        double y1 = y0 + size.y;

        renderer.drawQuad(x0, y0, x1, y1, color.red / 255.0, color.green / 255.0, color.blue / 255.0, getAlpha(), getBlendMode(), context.getRenderResourceCollection().getTexture(getTexture()));
    }

    public void drawSelect(DrawContext context) {
        IGuiRenderer renderer = context.getRenderer();
        double x0 = position.x; // - size.x / 2;
        double y0 = position.y; //- size.y / 2;

        x0 -= pivotOffsetX(size.x);
        y0 -= pivotOffsetY(size.y);

        double x1 = x0 + size.x;
        double y1 = y0 + size.y;
        renderer.drawQuad(x0, y0, x1, y1, color.red / 255.0, color.green / 255.0, color.blue / 255.0, getAlpha(), null, null);
    }

    public Rectangle2D getVisualBounds() {
        double x0 = position.x; // - size.x / 2;
        double y0 = position.y; //- size.y / 2;

        x0 -= pivotOffsetX(size.x);
        y0 -= pivotOffsetY(size.y);
        return new Rectangle2D.Double(x0, y0, size.x, size.y);
    }

    @Override
    public void doBuildNodeDesc(Builder builder) {
    }
}
