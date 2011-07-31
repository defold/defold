package com.dynamo.cr.guieditor.scene;

import java.awt.FontMetrics;
import java.awt.geom.Rectangle2D;

import com.dynamo.cr.guieditor.DrawContext;
import com.dynamo.cr.guieditor.render.IGuiRenderer;
import com.dynamo.cr.properties.Property;
import com.dynamo.gui.proto.Gui.NodeDesc;
import com.dynamo.gui.proto.Gui.NodeDesc.BlendMode;
import com.dynamo.gui.proto.Gui.NodeDesc.Builder;
import com.dynamo.gui.proto.Gui.NodeDesc.Pivot;
import com.sun.opengl.util.j2d.TextRenderer;
import com.sun.opengl.util.texture.Texture;

public class TextGuiNode extends GuiNode {

    @Property(commandFactory = UndoableCommandFactory.class)
    private String text;
    @Property(commandFactory = UndoableCommandFactory.class)
    private String font;

    private Rectangle2D textBounds = new Rectangle2D.Double(0, 0, 1, 1);
    private double pivotOffsetX;
    private double pivotOffsetY;

    public TextGuiNode(GuiScene scene, NodeDesc nodeDesc) {
        super(scene, nodeDesc);
        this.font = nodeDesc.getFont();
        this.text = nodeDesc.getText();
    }

    public String getText() {
        return text;
    }

    public void setText(String text) {
        this.text = text;
    }

    public String getFont() {
        return font;
    }

    public void setFont(String font) {
        this.font = font;
    }

    private String getErrorText() {
        return String.format("Error: Font '%s' not found", font);
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

    private double pivotOffsetY(double ascent, double descent) {
        Pivot p = getPivot();

        switch (p) {
        case PIVOT_CENTER:
        case PIVOT_E:
        case PIVOT_W:
            return -descent * 0.5 + ascent * 0.5;

        case PIVOT_N:
        case PIVOT_NE:
        case PIVOT_NW:
            return ascent;

        case PIVOT_S:
        case PIVOT_SW:
        case PIVOT_SE:
            return -descent;
        }

        assert false;
        return Double.MAX_VALUE;
    }

    public void draw(DrawContext context) {
        IGuiRenderer renderer = context.getRenderer();
        double x0 = position.x;
        double y0 = position.y;

        TextRenderer textRenderer = context.getRenderResourceCollection().getTextRenderer(font);

        String actualText = text;

        double r = color.red / 255.0;
        double g = color.green / 255.0;
        double b = color.blue / 255.0;
        double alpha = getAlpha();
        BlendMode blendMode = getBlendMode();
        Texture texture = context.getRenderResourceCollection().getTexture(getTexture());

        if (textRenderer == null) {
            textRenderer = renderer.getDebugTextRenderer();
            actualText = getErrorText();
            r = alpha = 1;
            g = b = 0;
            blendMode = null;
            texture = null;
        }

        textBounds = renderer.getStringBounds(textRenderer, actualText);

        FontMetrics metrics = renderer.getFontMetrics(textRenderer.getFont());
        int ascent = metrics.getMaxAscent();
        int descent = metrics.getMaxDescent();

        int width = metrics.stringWidth(actualText);
        pivotOffsetX = pivotOffsetX(width);
        pivotOffsetY = pivotOffsetY(ascent, descent);

        x0 -= pivotOffsetX;
        y0 -= pivotOffsetY;

        renderer.drawString(textRenderer, actualText, x0, y0, r, g, b, alpha, blendMode, texture);
    }

    public void drawSelect(DrawContext context) {
        IGuiRenderer renderer = context.getRenderer();
        double x0 = position.x;
        double y0 = position.y;

        TextRenderer textRenderer = context.getRenderResourceCollection().getTextRenderer(font);

        String actualText = text;

        if (textRenderer == null) {
            textRenderer = renderer.getDebugTextRenderer();
            actualText = getErrorText();
        }

        FontMetrics metrics = renderer.getFontMetrics(textRenderer.getFont());
        int ascent = metrics.getMaxAscent();
        int descent = metrics.getMaxDescent();

        int width = metrics.stringWidth(actualText);
        pivotOffsetX = pivotOffsetX(width);
        pivotOffsetY = pivotOffsetY(ascent, descent);

        x0 -= pivotOffsetX;
        y0 -= pivotOffsetY;

        renderer.drawStringBounds(textRenderer, actualText, x0, y0, 1, 1, 1, 1);
    }

    public Rectangle2D getVisualBounds() {
        if (textBounds != null) {
            // Return cached text bounds
            double x = position.x + textBounds.getX();
            double y = position.y - (textBounds.getHeight() + textBounds.getY());

            x -= pivotOffsetX;
            y -= pivotOffsetY;

            Rectangle2D ret = new Rectangle2D.Double(x, y, textBounds.getWidth(), textBounds.getHeight());
            return ret;
        }

        // else return something temp (predraw)
        return new Rectangle2D.Double(0, 0, 1, 1);
    }

    @Override
    public void doBuildNodeDesc(Builder builder) {
        builder.setText(this.text)
               .setFont(this.font);
    }

}
