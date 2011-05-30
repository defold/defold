package com.dynamo.cr.guieditor.scene;

import java.awt.geom.Rectangle2D;

import com.dynamo.cr.guieditor.DrawContext;
import com.dynamo.cr.guieditor.property.Property;
import com.dynamo.cr.guieditor.render.IGuiRenderer;
import com.dynamo.gui.proto.Gui.NodeDesc;
import com.dynamo.gui.proto.Gui.NodeDesc.Builder;
import com.sun.opengl.util.j2d.TextRenderer;

public class TextGuiNode extends GuiNode {

    @Property(commandFactory = UndoableCommandFactory.class)
    private String text;
    @Property(commandFactory = UndoableCommandFactory.class)
    private String font;

    private Rectangle2D textBounds;

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
        this.textBounds = null;
    }

    public String getFont() {
        return font;
    }

    public void setFont(String font) {
        this.font = font;
        this.textBounds = null;
    }

    private String getErrorText() {
        return String.format("Error: Font '%s' not found", font);
    }

    public void draw(DrawContext context) {
        IGuiRenderer renderer = context.getRenderer();
        double x0 = position.x;
        double y0 = position.y;

        TextRenderer textRenderer = context.getRenderResourceCollection().getTextRenderer(font);
        if (textRenderer != null) {
            if (textBounds == null)
                textBounds = renderer.getStringBounds(textRenderer, text);
            renderer.drawString(textRenderer, text, x0, y0, color.x, color.y, color.z, color.w, getBlendMode(), context.getRenderResourceCollection().getTexture(getTexture()));
        }
        else {
            String errorText = getErrorText();
            if (textBounds == null)
                textBounds = renderer.getStringBounds(null, errorText);
            renderer.drawString(null, errorText, x0, y0, 1, 0, 0, 1, null, null);
        }
    }

    public void drawSelect(DrawContext context) {
        IGuiRenderer renderer = context.getRenderer();
        double x0 = position.x;
        double y0 = position.y;

        TextRenderer textRenderer = context.getRenderResourceCollection().getTextRenderer(font);

        if (textRenderer != null) {
            renderer.drawStringBounds(textRenderer, text, x0, y0, color.x, color.y, color.z, color.w);
        }
        else {
            String errorText = getErrorText();
            renderer.drawStringBounds(null, errorText, x0, y0, 1, 0, 0, 1);
        }
    }

    public Rectangle2D getBounds() {
        if (textBounds != null) {
            // Return cached text bounds
            double x = position.x + textBounds.getX();
            double y = position.y - (textBounds.getHeight() + textBounds.getY());
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
