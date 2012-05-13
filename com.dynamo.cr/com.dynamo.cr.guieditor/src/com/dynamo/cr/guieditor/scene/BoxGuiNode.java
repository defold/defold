package com.dynamo.cr.guieditor.scene;

import java.awt.geom.Rectangle2D;

import javax.vecmath.Matrix4d;
import javax.vecmath.Vector4d;

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

    @Override
    public void draw(DrawContext context) {
        IGuiRenderer renderer = context.getRenderer();

        double x0 = -pivotOffsetX(size.x);
        double y0 = -pivotOffsetY(size.y);

        double x1 = x0 + size.x;
        double y1 = y0 + size.y;

        Matrix4d transform = new Matrix4d();
        calculateWorldTransform(transform);
        renderer.drawQuad(x0, y0, x1, y1, color.red / 255.0, color.green / 255.0, color.blue / 255.0, getAlpha(), getBlendMode(), context.getRenderResourceCollection().getTexture(getTexture()), transform);
    }

    @Override
    public void drawSelect(DrawContext context) {
        IGuiRenderer renderer = context.getRenderer();

        double x0 = -pivotOffsetX(size.x);
        double y0 = -pivotOffsetY(size.y);

        double x1 = x0 + size.x;
        double y1 = y0 + size.y;
        Matrix4d transform = new Matrix4d();
        calculateWorldTransform(transform);
        renderer.drawQuad(x0, y0, x1, y1, color.red / 255.0, color.green / 255.0, color.blue / 255.0, getAlpha(), null, null, transform);
    }

    @Override
    public Rectangle2D getVisualBounds() {
        Matrix4d transform = new Matrix4d();
        calculateWorldTransform(transform);

        double x0 = -pivotOffsetX(size.x);
        double y0 = -pivotOffsetY(size.y);
        double x1 = x0 + size.x;
        double y1 = y0 + size.y;
        Vector4d points[] = new Vector4d[] {
                new Vector4d(x0, y0, 0, 1),
                new Vector4d(x1, y0, 0, 1),
                new Vector4d(x0, y1, 0, 1),
                new Vector4d(x1, y1, 0, 1),
        };
        double xMin = Double.POSITIVE_INFINITY, yMin = Double.POSITIVE_INFINITY;
        double xMax = Double.NEGATIVE_INFINITY, yMax = Double.NEGATIVE_INFINITY;
        for (int i = 0; i < 4; ++i) {
            transform.transform(points[i]);
            xMin = Math.min(xMin, points[i].getX());
            xMax = Math.max(xMax, points[i].getX());
            yMin = Math.min(yMin, points[i].getY());
            yMax = Math.max(yMax, points[i].getY());
        }
        return new Rectangle2D.Double(xMin, yMin, xMax - xMin, yMax - yMin);
    }

    @Override
    public void doBuildNodeDesc(Builder builder) {
    }
}
