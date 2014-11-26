package com.dynamo.cr.guied.ui;

import java.awt.Font;
import java.awt.FontMetrics;
import java.awt.Graphics2D;
import java.awt.RenderingHints;
import java.awt.image.BufferedImage;
import java.util.EnumSet;
import java.util.List;

import javax.media.opengl.GL;
import javax.media.opengl.GL2;
import javax.vecmath.Point3d;

import com.dynamo.cr.guied.core.TextNode;
import com.dynamo.cr.guied.util.Clipping;
import com.dynamo.cr.guied.util.TextUtil;
import com.dynamo.cr.guied.util.TextUtil.TextMetric;
import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.gui.proto.Gui.NodeDesc.Pivot;
import com.jogamp.opengl.util.awt.TextRenderer;

public class TextNodeRenderer implements INodeRenderer<TextNode> {

    private static final EnumSet<Pass> passes = EnumSet.of(Pass.OUTLINE, Pass.TRANSPARENT, Pass.SELECTION);

    private TextRenderer debugTextRenderer;
    private Graphics2D graphics;

    public static class TextLine {
        public TextLine(String text, double width) {
            this.text = text;
            this.width = width;
        }

        public String text;
        public double width;
    }

    public TextNodeRenderer() {
        String fontName = Font.SANS_SERIF;
        Font debugFont = new Font(fontName, Font.BOLD, 28);
        debugTextRenderer = new TextRenderer(debugFont, true, true);
        BufferedImage image = new BufferedImage(4, 4, BufferedImage.TYPE_INT_ARGB);
        graphics = (Graphics2D) image.getGraphics();
        graphics.setRenderingHint(RenderingHints.KEY_TEXT_ANTIALIASING, RenderingHints.VALUE_TEXT_ANTIALIAS_ON);
        graphics.setRenderingHint(RenderingHints.KEY_FRACTIONALMETRICS, RenderingHints.VALUE_FRACTIONALMETRICS_ON);
    }

    @Override
    public void dispose(GL2 gl) {
        debugTextRenderer.dispose();
        graphics.dispose();
    }

    @Override
    public void setup(RenderContext renderContext, TextNode node) {
        if (passes.contains(renderContext.getPass())) {
            RenderData<TextNode> data = renderContext.add(this, node, new Point3d(), null);
            data.setIndex(node.getRenderKey());
        }
    }

    private double pivotOffsetX(TextNode node, double width) {
        Pivot p = node.getPivot();

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

    private double[] lineOffsets(TextNode node, List<TextLine> lines, double width) {
        double[] result = new double[lines.size()];
        for (int i = 0; i < result.length; ++i) {
            result[i] = pivotOffsetX(node, width - lines.get(i).width);
        }

        return result;
    }

    private double pivotOffsetY(TextNode node, double ascent, double descent, int lineCount) {
        Pivot p = node.getPivot();

        switch (p) {
        case PIVOT_CENTER:
        case PIVOT_E:
        case PIVOT_W:
            return -(ascent + descent) * (lineCount) * 0.5 + ascent;

        case PIVOT_N:
        case PIVOT_NE:
        case PIVOT_NW:
            return ascent;

        case PIVOT_S:
        case PIVOT_SW:
        case PIVOT_SE:
            return -(ascent + descent) * (lineCount - 1) - descent;
        }

        assert false;
        return Double.MAX_VALUE;
    }

    @Override
    public void render(RenderContext renderContext, TextNode node, RenderData<TextNode> renderData) {
        GL2 gl = renderContext.getGL();

        Clipping.setState(renderContext, node);

        boolean transparent = renderData.getPass() == Pass.TRANSPARENT;
        if (transparent) {
            switch (node.getBlendMode()) {
            case BLEND_MODE_ALPHA:
                gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);
                break;
            case BLEND_MODE_ADD:
            case BLEND_MODE_ADD_ALPHA:
                gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE);
                break;
            case BLEND_MODE_MULT:
                gl.glBlendFunc(GL.GL_ZERO, GL.GL_SRC_COLOR);
                break;
            }
        }

        String actualText = node.getText();

        TextRenderer textRenderer = node.getTextRendererHandle().getTextRenderer();
        if (textRenderer == null) {
            textRenderer = this.debugTextRenderer;
            actualText = String.format("Error: Font '%s' not found", node.getFont());
        }

        List<TextLine> lines = TextUtil.layout(new TextMetric(textRenderer), actualText, node.getSize().x,
                node.isLineBreak());
        double width = 0;
        for (TextLine l : lines) {
            width = Math.max(width, l.width);
        }

        Font font = textRenderer.getFont();
        FontMetrics metrics = graphics.getFontMetrics(font);
        int ascent = metrics.getMaxAscent();
        int descent = metrics.getMaxDescent();

        double x0 = -pivotOffsetX(node, width);
        double y0 = -pivotOffsetY(node, ascent, descent, lines.size());

        float[] color = node.calcNormRGBA();

        if (transparent) {
            double[] xOffsets = lineOffsets(node, lines, width);
            textRenderer.begin3DRendering();

            gl.glColor4fv(renderContext.selectColor(node, color), 0);
            double x = x0;
            double y = y0;
            for (int i = 0; i < xOffsets.length; ++i) {
                TextLine l = lines.get(i);
                textRenderer.draw3D(l.text, (float) (x + xOffsets[i]), (float) y, 0.0f, 1);
                y -= ascent + descent;
            }

            textRenderer.end3DRendering();
        } else {
            gl.glColor4fv(renderContext.selectColor(node, color), 0);
            double h = 0;
            double w = 0;
            for (TextLine t : lines) {
                h += ascent + descent;
                w = Math.max(w, t.width);
            }

            y0 += ascent;
            double x1 = x0 + w;
            double y1 = y0 - h;

            gl.glBegin(GL2.GL_QUADS);

            gl.glTexCoord2d(0, 0);
            gl.glVertex2d(x0, y0);

            gl.glTexCoord2d(1, 0);
            gl.glVertex2d(x1, y0);

            gl.glTexCoord2d(1, 1);
            gl.glVertex2d(x1, y1);

            gl.glTexCoord2d(0, 1);
            gl.glVertex2d(x0, y1);
            gl.glEnd();

            // Update AABB
            AABB aabb = new AABB();
            aabb.union(x0, y0, 0.0);
            aabb.union(x1, y1, 0.0);
            node.setAABB(aabb);
        }

        if (transparent) {
            gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);
        }
    }

}
