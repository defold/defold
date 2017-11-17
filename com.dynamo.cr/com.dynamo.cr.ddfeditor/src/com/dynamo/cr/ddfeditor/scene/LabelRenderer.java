package com.dynamo.cr.ddfeditor.scene;

import java.awt.Graphics2D;
import java.awt.RenderingHints;
import java.awt.image.BufferedImage;
import java.io.IOException;
import java.util.EnumSet;
import java.util.List;

import javax.media.opengl.GL;
import javax.media.opengl.GL2;
import javax.vecmath.Point3d;

import org.eclipse.swt.graphics.RGB;
import org.osgi.framework.FrameworkUtil;

import com.dynamo.bob.font.Fontc.InputFontFormat;
import com.dynamo.cr.sceneed.core.TextUtil;
import com.dynamo.cr.sceneed.core.TextUtil.TextLine;
import com.dynamo.cr.sceneed.core.TextUtil.TextMetric;

import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.FontRendererHandle;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.cr.sceneed.ui.util.Shader;
import com.dynamo.label.proto.Label.LabelDesc.Pivot;
import com.dynamo.render.proto.Font.FontMap;
import com.dynamo.render.proto.Font.FontTextureFormat;
import com.jogamp.opengl.util.texture.Texture;

public class LabelRenderer implements INodeRenderer<LabelNode> {

    private static final EnumSet<Pass> passes = EnumSet.of(Pass.OUTLINE, Pass.TRANSPARENT, Pass.SELECTION);

    private Graphics2D graphics;

    private Shader shaderPlain;
    private Shader shaderDF;
    private Shader shaderBMFont;

    public LabelRenderer() {
        BufferedImage image = new BufferedImage(4, 4, BufferedImage.TYPE_INT_ARGB);
        graphics = (Graphics2D) image.getGraphics();
        graphics.setRenderingHint(RenderingHints.KEY_TEXT_ANTIALIASING, RenderingHints.VALUE_TEXT_ANTIALIAS_ON);
        graphics.setRenderingHint(RenderingHints.KEY_FRACTIONALMETRICS, RenderingHints.VALUE_FRACTIONALMETRICS_ON);
    }

    @Override
    public void dispose(GL2 gl) {
        if (this.shaderPlain != null) {
            this.shaderPlain.disable(gl);
        }
        if (this.shaderDF != null) {
            this.shaderDF.disable(gl);
        }
        if (this.shaderBMFont != null) {
            this.shaderBMFont.disable(gl);
        }
        graphics.dispose();
    }

    private static Shader loadShader(GL2 gl, String path) {
        Shader shader = new Shader(gl);
        try {
            shader.load(gl, FrameworkUtil.getBundle(LabelRenderer.class), path);
        } catch (IOException e) {
            shader.dispose(gl);
            throw new IllegalStateException(e);
        }
        return shader;
    }

    private float[] calcNormRGBA(RGB rgb, float alpha) {
        float[] rgba = new float[] {1.0f, 1.0f, 1.0f, 1.0f};
        float inv = 1.0f / 255.0f;
        rgba[0] *= rgb.red * inv;
        rgba[1] *= rgb.green * inv;
        rgba[2] *= rgb.blue * inv;
        rgba[3] *= alpha;
        return rgba;
    }

    @Override
    public void setup(RenderContext renderContext, LabelNode node) {
        GL2 gl = renderContext.getGL();

        if (this.shaderPlain == null) {
            this.shaderPlain = loadShader(gl, "/content/label_plain");
        }

        if (this.shaderDF == null) {
            this.shaderDF = loadShader(gl, "/content/label_df");
        }

        if (this.shaderBMFont == null) {
            this.shaderBMFont = loadShader(gl, "/content/label_bmfont");
        }

        if (passes.contains(renderContext.getPass())) {
            renderContext.add(this, node, new Point3d(), null);
        }
    }

    private double pivotOffsetX(LabelNode node, double width) {
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

    private double[] lineOffsets(LabelNode node, List<TextLine> lines, double width) {
        double[] result = new double[lines.size()];
        for (int i = 0; i < result.length; ++i) {
            result[i] = pivotOffsetX(node, width - lines.get(i).width);
        }

        return result;
    }

    // This function will calculate the vertical offset of the pivot for the
    // north/top position of the actual text within the node. If the pivot is
    // placed north, the offset will be the ascent of the text (The recommended
    // distance above the baseline for singled spaced text).
    private double pivotTextOffsetY(LabelNode node, double ascent, double descent, double leading, int lineCount) {
        Pivot p = node.getPivot();

        double lineHeight = ascent + descent;
        switch (p) {
        case PIVOT_CENTER:
        case PIVOT_E:
        case PIVOT_W:
            return -(lineCount * lineHeight * leading - lineHeight * (leading - 1.0f)) * 0.5 + ascent;

        case PIVOT_N:
        case PIVOT_NE:
        case PIVOT_NW:
            return ascent;

        case PIVOT_S:
        case PIVOT_SW:
        case PIVOT_SE:
            return -(lineHeight * leading * (lineCount - 1)) - descent;
        }

        assert false;
        return Double.MAX_VALUE;
    }

    // This function will calculate the vertical offset of the pivot for the
    // south/bottom position of the LabelNode. If the pivot is placed south, the
    // offset will be 0.
    private double pivotNodeOffsetY(LabelNode node, double height) {
        Pivot p = node.getPivot();

        switch(p) {
            case PIVOT_N:
            case PIVOT_NE:
            case PIVOT_NW:
                return height;

            case PIVOT_CENTER:
            case PIVOT_E:
            case PIVOT_W:
                return height * 0.5;

            case PIVOT_S:
            case PIVOT_SE:
            case PIVOT_SW:
                return 0;
        }

        assert false;
        return Double.MAX_VALUE;
    }

    @Override
    public void render(RenderContext renderContext, LabelNode node, RenderData<LabelNode> renderData) {
        GL2 gl = renderContext.getGL();
        String actualText = node.getText();

        FontRendererHandle textRenderHandle = null;
        textRenderHandle = node.getFontRendererHandle(gl);

        if (textRenderHandle == null || !textRenderHandle.isValid()) {
            textRenderHandle = node.getDefaultFontRendererHandle();
            actualText = String.format("Error: Font '%s' not found", node.getFont());
        }

        if (textRenderHandle == null || !textRenderHandle.isValid()) {
            // Failed to load default font renderer
            return;
        }

        boolean transparent = renderData.getPass() == Pass.TRANSPARENT;
        if (transparent) {
            switch (node.getBlendMode()) {
            case BLEND_MODE_ALPHA:
                gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);
                break;
            case BLEND_MODE_ADD:
                gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE);
                break;
            case BLEND_MODE_MULT:
                gl.glBlendFunc(GL.GL_ZERO, GL.GL_SRC_COLOR);
                break;
            }
        }

        Texture textTexture = textRenderHandle.getTexture(gl);
        FontMap fontMap = textRenderHandle.getFontMap();

        float  ascent = fontMap.getMaxAscent();
        float descent = fontMap.getMaxDescent();
        float lineHeight = ascent + descent;
        float tracking = lineHeight * (float)node.getTracking();

        List<TextLine> lines = TextUtil.layout(new TextMetric(textRenderHandle, tracking), actualText, node.getSize().x, node.getLineBreak());
        double width = 0;
        for (TextLine l : lines) {
            width = Math.max(width, l.width);
        }

        double x0 = -pivotOffsetX(node, width);
        double y0 = -pivotTextOffsetY(node, ascent, descent, node.getLeading(), lines.size());

        float[] color = node.calcNormRGBA();

        if (transparent) {
            double[] xOffsets = lineOffsets(node, lines, width);
            float sU = 0;
            float sV = 0;

            if (textTexture != null) {
                textTexture.bind(gl);
                textTexture.setTexParameteri(gl, GL.GL_TEXTURE_MIN_FILTER, GL.GL_LINEAR);
                textTexture.setTexParameteri(gl, GL.GL_TEXTURE_MAG_FILTER, GL.GL_LINEAR);

                textTexture.setTexParameteri(gl, GL.GL_TEXTURE_WRAP_S, GL2.GL_REPEAT);
                textTexture.setTexParameteri(gl, GL.GL_TEXTURE_WRAP_T, GL2.GL_REPEAT);
                textTexture.enable(gl);

                sU = 1.0f / (float)textTexture.getImageWidth();
                sV = 1.0f / (float)textTexture.getImageHeight();
            }

            Shader shader = null;

            if (textRenderHandle.getInputFormat() == InputFontFormat.FORMAT_TRUETYPE &&
                fontMap.getImageFormat() == FontTextureFormat.TYPE_BITMAP) {

                shader = this.shaderPlain;
                shader.enable(gl);

                shader.setUniforms(gl, "uni_outline_color", calcNormRGBA(node.getOutline(), (float)fontMap.getOutlineAlpha() * (float)node.getOutlineAlpha() * color[3]));

            } else if (textRenderHandle.getInputFormat() == InputFontFormat.FORMAT_TRUETYPE &&
                    fontMap.getImageFormat() == FontTextureFormat.TYPE_DISTANCE_FIELD) {

                shader = this.shaderDF;
                shader.enable(gl);

                float sdf_edge_value = 0.75f;
                float sdf_world_scale = (float) Math.sqrt(node.getScale().getX() * node.getScale().getX() + node.getScale().getY() * node.getScale().getY());
                float smoothing = 0.25f / (sdf_world_scale * fontMap.getSdfSpread());
                float [] sdfParams = new float[] { sdf_edge_value, fontMap.getSdfOutline(), smoothing, fontMap.getSdfSpread() };
                shader.setUniforms(gl, "uni_sdf_params", sdfParams);
                shader.setUniforms(gl, "uni_outline_color", calcNormRGBA(node.getOutline(), (float)fontMap.getOutlineAlpha() * (float)node.getOutlineAlpha() * color[3]));
                color[3] *= (float) fontMap.getAlpha();
            } else {
                shader = this.shaderBMFont;
                shader.enable(gl);

            }

            shader.setUniforms(gl, "uni_face_color", renderContext.selectColor(node, color));

            gl.glBegin(GL2.GL_QUADS);

            double y = y0;
            for (int i = 0; i < xOffsets.length; ++i) {
                TextLine l = lines.get(i);
                double x = x0 + xOffsets[i];

                for (int j = 0; j < l.text.length(); j++) {
                    char c = l.text.charAt(j);
                    FontMap.Glyph g = textRenderHandle.getGlyph(c);
                    if (g != null) {
                        if (g.getWidth() > 0) {

                            double u_min = sU * (g.getX() + fontMap.getGlyphPadding());
                            double v_min = sV * (g.getY() + fontMap.getGlyphPadding());
                            double u_max = sU * (g.getX() + fontMap.getGlyphPadding() + g.getWidth());
                            double v_max = sV * (g.getY() + fontMap.getGlyphPadding() + g.getAscent() + g.getDescent());

                            double x_min = x + g.getLeftBearing();
                            double x_max = x_min + g.getWidth();
                            double y_min = y - g.getDescent();
                            double y_max = y + g.getAscent();

                            gl.glTexCoord2d(u_min, v_max);
                            gl.glVertex2d(x_min, y_min);

                            gl.glTexCoord2d(u_max, v_max);
                            gl.glVertex2d(x_max, y_min);

                            gl.glTexCoord2d(u_max, v_min);
                            gl.glVertex2d(x_max, y_max);

                            gl.glTexCoord2d(u_min, v_min);
                            gl.glVertex2d(x_min, y_max);

                        }

                        x += g.getAdvance() + (float)tracking;
                    }
                }
                y -= (float)(lineHeight * node.getLeading());
            }

            gl.glEnd();

            if (shader != null) {
                shader.disable(gl);
            }

            if (textTexture != null) {
                textTexture.disable(gl);
            }
        } else {
            // Render the outline of the actual LabelNode.
            float[] nodeDefaultColor = renderContext.selectColor(node, color);
            float[] nodeOutlineColor = {
                    Math.min(1.0f, nodeDefaultColor[0] * 1.3f),
                    Math.min(1.0f, nodeDefaultColor[1] * 1.3f),
                    Math.min(1.0f, nodeDefaultColor[2] * 1.3f)
            };

            double nodeX0 = -pivotOffsetX(node, node.getSize().x);
            double nodeX1 = nodeX0 + node.getSize().x;
            double nodeY0 = -pivotNodeOffsetY(node, node.getSize().y);
            double nodeY1 = nodeY0 + node.getSize().y;

            gl.glColor4fv(nodeOutlineColor, 0);
            gl.glBegin(GL2.GL_QUADS);

            gl.glTexCoord2d(0, 0);
            gl.glVertex2d(nodeX0, nodeY0);

            gl.glTexCoord2d(1, 0);
            gl.glVertex2d(nodeX1, nodeY0);

            gl.glTexCoord2d(1, 1);
            gl.glVertex2d(nodeX1, nodeY1);

            gl.glTexCoord2d(0, 1);
            gl.glVertex2d(nodeX0, nodeY1);

            gl.glEnd();

            // Render the outline of the actual text contained within the
            // LabelNode. This outline can overflow the outline of the LabelNode.
            double h = 0;
            double w = 0;
            for (TextLine t : lines) {
                h += lineHeight * node.getLeading();
                w = Math.max(w, t.width);
            }

            h -= (node.getLeading() - 1.0) * lineHeight;

            y0 += ascent;
            double x1 = x0 + w;
            double y1 = y0 - h;

            gl.glColor4fv(renderContext.selectColor(node, color), 0);
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
