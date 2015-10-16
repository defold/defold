package com.dynamo.cr.guied.ui;

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
import com.dynamo.cr.guied.core.ClippingNode;
import com.dynamo.cr.guied.core.ClippingNode.ClippingState;
import com.dynamo.cr.guied.core.TextNode;
import com.dynamo.cr.guied.util.Clipping;
import com.dynamo.cr.guied.util.TextUtil;
import com.dynamo.cr.guied.util.TextUtil.TextMetric;
import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.FontRendererHandle;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.cr.sceneed.ui.util.Shader;
import com.dynamo.gui.proto.Gui.NodeDesc.Pivot;
import com.dynamo.render.proto.Font.FontMap;
import com.dynamo.render.proto.Font.FontTextureFormat;
import com.jogamp.opengl.util.texture.Texture;

public class TextNodeRenderer implements INodeRenderer<TextNode> {

    private static final EnumSet<Pass> passes = EnumSet.of(Pass.OUTLINE, Pass.TRANSPARENT, Pass.SELECTION);

    private Graphics2D graphics;

    private Shader shaderPlain;
    private Shader shaderDF;
    private Shader shaderBMFont;

    public static class TextLine {
        public TextLine(String text, double width) {
            this.text = text;
            this.width = width;
        }

        public String text;
        public double width;
    }

    public TextNodeRenderer() {
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
            shader.load(gl, FrameworkUtil.getBundle(TextNodeRenderer.class), path);
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
    public void setup(RenderContext renderContext, TextNode node) {
        GL2 gl = renderContext.getGL();

        if (this.shaderPlain == null) {
            this.shaderPlain = loadShader(gl, "/content/font_plain");
        }

        if (this.shaderDF == null) {
            this.shaderDF = loadShader(gl, "/content/font_df");
        }

        if (this.shaderBMFont == null) {
            this.shaderBMFont = loadShader(gl, "/content/font_bmfont");
        }

        if (passes.contains(renderContext.getPass())) {
            ClippingState state = null;
            ClippingNode clipper = node.getClosestParentClippingNode();
            if (clipper != null) {
                state = clipper.getChildClippingState();
            }
            RenderData<TextNode> data = renderContext.add(this, node, new Point3d(), state);
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

        boolean clipping = renderData.getUserData() != null;
        if (clipping && renderData.getPass() == Pass.TRANSPARENT) {
            Clipping.beginClipping(gl);
            ClippingState state = (ClippingState)renderData.getUserData();
            Clipping.setupClipping(gl, state);
        }

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

        Texture textTexture = textRenderHandle.getTexture(gl);
        FontMap fontMap = textRenderHandle.getFontMap();

        List<TextLine> lines = TextUtil.layout(new TextMetric(textRenderHandle), actualText, node.getSize().x,
                node.isLineBreak());
        double width = 0;
        for (TextLine l : lines) {
            width = Math.max(width, l.width);
        }

        float  ascent = fontMap.getMaxAscent();
        float descent = fontMap.getMaxDescent();

        double x0 = -pivotOffsetX(node, width);
        double y0 = -pivotOffsetY(node, ascent, descent, lines.size());

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

                shader.setUniforms(gl, "uni_outline_color", calcNormRGBA(node.getOutline(), (float)node.getOutlineAlpha() * color[3]));

            } else if (textRenderHandle.getInputFormat() == InputFontFormat.FORMAT_TRUETYPE &&
                    fontMap.getImageFormat() == FontTextureFormat.TYPE_DISTANCE_FIELD) {

                shader = this.shaderDF;
                shader.enable(gl);

                float sdf_world_scale = (float) Math.sqrt(node.getScale().getX() * node.getScale().getX() + node.getScale().getY() * node.getScale().getY());
                float [] sdfParams = new float[] { sdf_world_scale * fontMap.getSdfScale(), sdf_world_scale * fontMap.getSdfOffset(), sdf_world_scale * fontMap.getSdfOutline(), 1.0f  };
                shader.setUniforms(gl, "uni_sdf_params", sdfParams);
                shader.setUniforms(gl, "uni_outline_color", calcNormRGBA(node.getOutline(), (float)node.getOutlineAlpha() * color[3]));

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

                            double u_min = sU * (g.getX() + g.getLeftBearing());
                            double v_min = sV * (g.getY() - g.getAscent());
                            double u_max = u_min + sU * g.getWidth();
                            double v_max = v_min + sV * (g.getAscent() + g.getDescent());

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

                        x += g.getAdvance();
                    }
                }
                y -= ascent + descent;
            }

            gl.glEnd();

            if (shader != null) {
                shader.disable(gl);
            }

            if (textTexture != null) {
                textTexture.disable(gl);
            }
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

        if (clipping && renderData.getPass() == Pass.TRANSPARENT) {
            Clipping.endClipping(gl);
        }
    }

}
