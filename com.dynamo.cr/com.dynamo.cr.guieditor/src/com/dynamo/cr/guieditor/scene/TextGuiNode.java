package com.dynamo.cr.guieditor.scene;

import java.awt.FontMetrics;
import java.awt.geom.Rectangle2D;
import java.util.ArrayList;
import java.util.List;

import javax.vecmath.Matrix4d;
import javax.vecmath.Vector4d;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.swt.graphics.RGB;

import com.dynamo.cr.guieditor.Activator;
import com.dynamo.cr.guieditor.DrawContext;
import com.dynamo.cr.guieditor.render.IGuiRenderer;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.gui.proto.Gui.NodeDesc;
import com.dynamo.gui.proto.Gui.NodeDesc.BlendMode;
import com.dynamo.gui.proto.Gui.NodeDesc.Builder;
import com.dynamo.gui.proto.Gui.NodeDesc.Pivot;
import com.dynamo.proto.DdfMath.Vector4;
import com.jogamp.opengl.util.awt.TextRenderer;
import com.jogamp.opengl.util.texture.Texture;

public class TextGuiNode extends GuiNode {

    @Property
    private String text;
    @Property(editorType = EditorType.DROP_DOWN)
    private String font;

    @Property()
    protected RGB outline;
    @Property
    private double outlineAlpha;

    @Property()
    protected RGB shadow;
    @Property
    private double shadowAlpha;

    private Rectangle2D textBounds = new Rectangle2D.Double(0, 0, 1, 1);
    private double pivotOffsetX;
    private double pivotOffsetY;

    public TextGuiNode(GuiScene scene, NodeDesc nodeDesc) {
        super(scene, nodeDesc);
        this.font = nodeDesc.getFont();
        this.text = nodeDesc.getText();
        this.outline = new RGB((int) (nodeDesc.getOutline().getX() * 255),
                (int) (nodeDesc.getOutline().getY() * 255),
                (int) (nodeDesc.getOutline().getZ() * 255));
        this.outlineAlpha = nodeDesc.getOutline().getW();
        this.shadow = new RGB((int) (nodeDesc.getShadow().getX() * 255),
                (int) (nodeDesc.getShadow().getY() * 255),
                (int) (nodeDesc.getShadow().getZ() * 255));
        this.shadowAlpha = nodeDesc.getShadow().getW();
    }

    @Override
    protected void verify() {
        super.verify();
        if (outlineAlpha < 0 || outlineAlpha > 1.0) {
            IStatus status = new Status(IStatus.ERROR, Activator.PLUGIN_ID, "outline alpha value must between 0 and 1");
            statusMap.put("outlineAlpha", status);
        }
        if (shadowAlpha < 0 || shadowAlpha > 1.0) {
            IStatus status = new Status(IStatus.ERROR, Activator.PLUGIN_ID, "shadow alpha value must between 0 and 1");
            statusMap.put("shadowAlpha", status);
        }
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

    public Object[] getFontOptions() {
        List<String> fonts = new ArrayList<String>();
        for (EditorFontDesc f : this.scene.getFonts()) {
            fonts.add(f.getName());
        }
        return fonts.toArray();
    }

    public RGB getOutline() {
        return new RGB(outline.red, outline.green, outline.blue);
    }

    public void setOutline(RGB outline) {
        this.outline.red = outline.red;
        this.outline.green = outline.green;
        this.outline.blue = outline.blue;
    }

    public double getOutlineAlpha() {
        return outlineAlpha;
    }

    public void setOutlineAlpha(double outlineAlpha) {
        this.outlineAlpha = outlineAlpha;
        verify();
    }

    public RGB getShadow() {
        return new RGB(shadow.red, shadow.green, shadow.blue);
    }

    public void setShadow(RGB shadow) {
        this.shadow.red = shadow.red;
        this.shadow.green = shadow.green;
        this.shadow.blue = shadow.blue;
    }

    public double getShadowAlpha() {
        return shadowAlpha;
    }

    public void setShadowAlpha(double shadowAlpha) {
        this.shadowAlpha = shadowAlpha;
        verify();
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

    @Override
    public void draw(DrawContext context) {
        IGuiRenderer renderer = context.getRenderer();

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

        double x0 = -pivotOffsetX;
        double y0 = -pivotOffsetY;

        Matrix4d transform = new Matrix4d();
        calculateWorldTransform(transform);
        renderer.drawString(textRenderer, actualText, x0, y0, r, g, b, alpha, blendMode, texture, transform);
    }

    @Override
    public void drawSelect(DrawContext context) {
        IGuiRenderer renderer = context.getRenderer();

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

        double x0 = -pivotOffsetX;
        double y0 = -pivotOffsetY;

        Matrix4d transform = new Matrix4d();
        calculateWorldTransform(transform);
        renderer.drawStringBounds(textRenderer, actualText, x0, y0, 1, 1, 1, 1, transform);
    }

    @Override
    public Rectangle2D getVisualBounds() {
        if (textBounds != null) {
            // Return cached text bounds
            double x0 = textBounds.getX();
            double y0 = -(textBounds.getHeight() + textBounds.getY());

            x0 -= pivotOffsetX;
            y0 -= pivotOffsetY;
            double x1 = x0 + textBounds.getWidth();
            double y1 = y0 + textBounds.getHeight();

            Vector4d[] points = new Vector4d[] {
                    new Vector4d(x0, y0, 0, 1),
                    new Vector4d(x1, y0, 0, 1),
                    new Vector4d(x1, y1, 0, 1),
                    new Vector4d(x0, y1, 0, 1),
            };
            double xMin = Double.POSITIVE_INFINITY, yMin = Double.POSITIVE_INFINITY;
            double xMax = Double.NEGATIVE_INFINITY, yMax = Double.NEGATIVE_INFINITY;

            Matrix4d transform = new Matrix4d();
            calculateWorldTransform(transform);
            for (int i = 0; i < 4; ++i) {
                transform.transform(points[i]);
                xMin = Math.min(xMin, points[i].getX());
                yMin = Math.min(yMin, points[i].getY());
                xMax = Math.max(xMax, points[i].getX());
                yMax = Math.max(yMax, points[i].getY());
            }

            Rectangle2D ret = new Rectangle2D.Double(xMin, yMin, xMax - xMin, yMax - yMin);
            return ret;
        }

        // else return something temp (predraw)
        return new Rectangle2D.Double(0, 0, 1, 1);
    }

    @Override
    public void doBuildNodeDesc(Builder builder) {
        Vector4 outline4 = Vector4.newBuilder()
                .setX(outline.red / 255.0f )
                .setY(outline.green / 255.0f )
                .setZ(outline.blue / 255.0f)
                .setW((float) outlineAlpha).build();
        Vector4 shadow4 = Vector4.newBuilder()
                .setX(shadow.red / 255.0f )
                .setY(shadow.green / 255.0f )
                .setZ(shadow.blue / 255.0f)
                .setW((float) shadowAlpha).build();
        builder.setText(this.text)
               .setFont(this.font)
               .setOutline(outline4)
               .setShadow(shadow4);
    }

}
