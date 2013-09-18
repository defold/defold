package com.dynamo.cr.guieditor.render;

import java.awt.Font;
import java.awt.FontMetrics;
import java.awt.geom.Rectangle2D;
import java.util.List;

import javax.media.opengl.GL2;
import javax.vecmath.Matrix4d;

import com.dynamo.gui.proto.Gui.NodeDesc.BlendMode;
import com.jogamp.opengl.util.awt.TextRenderer;
import com.jogamp.opengl.util.texture.Texture;

public interface IGuiRenderer {

    public void begin(GL2 gl);

    public void end();

    public void drawQuad(double x0, double y0, double x1, double y1, double r,
            double g, double b, double a, BlendMode blendMode, Texture texture, Matrix4d transform);

    public void drawTextLines(TextRenderer textRenderer, List<TextLine> lines, double[] xOffsets, double x0, double y0,
                              double r,
                              double g, double b, double a, BlendMode blendMode, Texture texture, Matrix4d transform);

    public void drawTextLinesBounds(TextRenderer textRenderer, List<TextLine> lines, double x0, double y0, double r,
                                    double g, double b, double a, Matrix4d transform);

    public Rectangle2D getStringBounds(TextRenderer textRenderer, String text);

    public void setName(int name);

    public void clearName();

    public FontMetrics getFontMetrics(Font font);

    /**
     * Start GL select
     * @note x and y are specify center of selection
     * @param gl GL context
     * @param x center x coordinate
     * @param y center y coordinate
     * @param w selection width
     * @param h selection height
     * @param viewPort view-port
     */
    public void beginSelect(GL2 gl, int x, int y, int w, int h, int viewPort[]);

    public SelectResult endSelect();

    public TextRenderer getDebugTextRenderer();

    public static class TextLine {
        public TextLine(String text, double width) {
            this.text = text;
            this.width = width;
        }

        public String text;
        public double width;
    }

    List<TextLine> layout(TextRenderer textRenderer, String text, double width,
            boolean lineBreak);

}