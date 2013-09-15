package com.dynamo.cr.guieditor.render;

import java.awt.Font;
import java.awt.FontMetrics;
import java.awt.Graphics2D;
import java.awt.Rectangle;
import java.awt.RenderingHints;
import java.awt.font.FontRenderContext;
import java.awt.font.GlyphVector;
import java.awt.geom.Rectangle2D;
import java.awt.image.BufferedImage;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.IntBuffer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import javax.media.opengl.GL;
import javax.media.opengl.GL2;
import javax.media.opengl.glu.GLU;
import javax.vecmath.Matrix4d;

import org.eclipse.ui.services.IDisposable;

import com.dynamo.cr.guieditor.util.TextUtil;
import com.dynamo.cr.guieditor.util.TextUtil.ITextMetric;
import com.dynamo.gui.proto.Gui.NodeDesc.BlendMode;
import com.jogamp.opengl.util.awt.TextRenderer;
import com.jogamp.opengl.util.texture.Texture;

public class GuiRenderer implements IDisposable, IGuiRenderer {
    private ArrayList<RenderCommmand> renderCommands = new ArrayList<GuiRenderer.RenderCommmand>();
    private GL2 gl;
    private int currentName;

    // Picking
    private static final int MAX_NODES = 4096;
    private static IntBuffer selectBuffer = ByteBuffer.allocateDirect(4 * MAX_NODES).order(ByteOrder.nativeOrder()).asIntBuffer();
    private TextRenderer debugTextRenderer;
    private Graphics2D graphics;

    public GuiRenderer() {
        BufferedImage image = new BufferedImage(4, 4, BufferedImage.TYPE_INT_ARGB);
        graphics = (Graphics2D) image.getGraphics();
        graphics.setRenderingHint(RenderingHints.KEY_TEXT_ANTIALIASING, RenderingHints.VALUE_TEXT_ANTIALIAS_ON);
        graphics.setRenderingHint(RenderingHints.KEY_FRACTIONALMETRICS, RenderingHints.VALUE_FRACTIONALMETRICS_ON);
    }

    private abstract class RenderCommmand {

        private BlendMode blendMode;
        private Texture texture;
        private Matrix4d transform;

        public RenderCommmand(double r, double g, double b, double a, BlendMode blendMode, Texture texture, Matrix4d transform) {
            this.r = r;
            this.g = g;
            this.b = b;
            this.a = a;
            this.blendMode = blendMode;
            this.texture = texture;
            this.transform = transform;
        }
        double r, g, b, a;
        int name = -1;

        public void setupBlendMode() {
            if (blendMode != null) {
                gl.glEnable(GL.GL_BLEND);
                switch (blendMode) {

                case BLEND_MODE_ALPHA:
                    gl.glBlendFunc (GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);
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
            else {
                gl.glDisable(GL.GL_BLEND);
            }
        }

        public void setupTexture() {
            if (texture != null) {
                gl.glEnable(GL.GL_TEXTURE_2D);
                gl.glTexEnvf(GL2.GL_TEXTURE_ENV, GL2.GL_TEXTURE_ENV_MODE, GL2.GL_MODULATE);
                texture.bind(gl);
            }
            else {
                gl.glDisable(GL.GL_TEXTURE_2D);
            }
        }

        public void pushTransform() {
            gl.glPushMatrix();
            double[] a = new double[16];
            int i = 0;
            a[i++] = transform.m00;
            a[i++] = transform.m10;
            a[i++] = transform.m20;
            a[i++] = transform.m30;

            a[i++] = transform.m01;
            a[i++] = transform.m11;
            a[i++] = transform.m21;
            a[i++] = transform.m31;

            a[i++] = transform.m02;
            a[i++] = transform.m12;
            a[i++] = transform.m22;
            a[i++] = transform.m32;

            a[i++] = transform.m03;
            a[i++] = transform.m13;
            a[i++] = transform.m23;
            a[i++] = transform.m33;

            gl.glMultMatrixd(a, 0);
        }

        public void popTransform() {
            gl.glPopMatrix();
        }

        public abstract void draw(GL2 gl);
    }

    private class TextRenderCommmand extends RenderCommmand {
        private List<TextLine> lines;
        private double[] xOffsets;
        private double x0, y0;
        private TextRenderer textRenderer;

        public TextRenderCommmand(TextRenderer textRenderer, List<TextLine> lines, double[] xOffsets, double x0,
                double y0, double r, double g, double b, double a, BlendMode blendMode, Texture texture,
                Matrix4d transform) {
            super(r, g, b, a, blendMode, texture, transform);
            this.textRenderer = textRenderer;
            this.lines = lines;
            this.xOffsets = xOffsets;
            this.x0 = x0;
            this.y0 = y0;
        }

        @Override
        public void draw(GL2 gl) {
            Font font = textRenderer.getFont();
            FontMetrics metrics = getFontMetrics(font);
            int ascent = metrics.getMaxAscent();
            int descent = metrics.getMaxDescent();

            textRenderer.begin3DRendering();
            gl.glColor4d(r, g, b, a);
            pushTransform();
            setupBlendMode();

            double x = x0;
            double y = y0;
            for (int i = 0; i < this.xOffsets.length; ++i) {
                TextLine l = lines.get(i);
                textRenderer.draw3D(l.text, (float) (x + xOffsets[i]), (float) y, 0, 1);
                y -= ascent + descent;
            }

            textRenderer.end3DRendering();
            popTransform();
        }
    }

    private class QuadRenderCommand extends RenderCommmand {
        private double x0, y0, x1, y1;

        public QuadRenderCommand(double x0, double y0, double x1, double y1, double r, double g, double b, double a, BlendMode blendMode, Texture texture, Matrix4d transform) {
            super(r, g, b, a, blendMode, texture, transform);
            this.x0 = x0;
            this.y0 = y0;
            this.x1 = x1;
            this.y1 = y1;
        }

        @Override
        public void draw(GL2 gl) {
            if (name != -1)
                gl.glPushName(name);

            setupBlendMode();
            setupTexture();
            pushTransform();

            gl.glBegin(GL2.GL_QUADS);
            gl.glColor4d(r, g, b, a);

            gl.glTexCoord2d(0, 0);
            gl.glVertex2d(x0, y0);

            gl.glTexCoord2d(1, 0);
            gl.glVertex2d(x1, y0);

            gl.glTexCoord2d(1, 1);
            gl.glVertex2d(x1, y1);

            gl.glTexCoord2d(0, 1);
            gl.glVertex2d(x0, y1);
            gl.glEnd();

            if (name != -1)
                gl.glPopName();

            popTransform();
        }
    }

    @Override
    public void dispose() {
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.guieditor.render.IGuiRenderer#begin(javax.media.opengl.GL)
     */
    @Override
    public void begin(GL2 gl) {
        this.renderCommands.clear();
        this.renderCommands.ensureCapacity(1024);
        this.gl = gl;
        this.currentName = -1;

        if (debugTextRenderer == null) {
            String fontName = Font.SANS_SERIF;
            Font debugFont = new Font(fontName, Font.BOLD, 28);
            debugTextRenderer = new TextRenderer(debugFont, true, true);
        }
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.guieditor.render.IGuiRenderer#end()
     */
    @Override
    public void end() {
        for (RenderCommmand command : renderCommands) {
            command.draw(gl);
        }
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.guieditor.render.IGuiRenderer#drawQuad(double, double, double, double, double, double, double, double, com.dynamo.gui.proto.Gui.NodeDesc.BlendMode, java.lang.String)
     */
    @Override
    public void drawQuad(double x0, double y0, double x1, double y1, double r, double g, double b, double a, BlendMode blendMode, Texture texture, Matrix4d transform) {
        QuadRenderCommand command = new QuadRenderCommand(x0, y0, x1, y1, r, g, b, a, blendMode, texture, transform);
        if (currentName != -1) {
            command.name = currentName;
        }
        this.renderCommands.add(command);
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.guieditor.render.IGuiRenderer#drawString(java.lang.String, java.lang.String, double, double, double, double, double, double, com.dynamo.gui.proto.Gui.NodeDesc.BlendMode, java.lang.String)
     */
    @Override
    public void drawTextLines(TextRenderer textRenderer, List<TextLine> lines, double[] xOffsets, double x0, double y0,
                              double r, double g, double b, double a, BlendMode blendMode, Texture texture,
            Matrix4d transform) {
        if (textRenderer == null)
            textRenderer = debugTextRenderer;

        TextRenderCommmand command = new TextRenderCommmand(textRenderer, lines, xOffsets, x0, y0, r, g, b, a,
                blendMode, texture, transform);
        if (currentName != -1) {
            command.name = currentName;
        }
        this.renderCommands.add(command);
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.guieditor.render.IGuiRenderer#drawStringBounds(java.lang.String, java.lang.String, double, double, double, double, double, double)
     */
    @Override
    public void drawTextLinesBounds(TextRenderer textRenderer, List<TextLine> lines, double x0, double y0, double r,
                                    double g, double b, double a, Matrix4d transform) {
        if (textRenderer == null)
            textRenderer = debugTextRenderer;

        FontMetrics metrics = getFontMetrics(textRenderer.getFont());
        int ascent = metrics.getMaxAscent();
        int descent = metrics.getMaxDescent();

        double h = 0;
        double w = 0;
        for (TextLine t : lines) {
            h += ascent + descent;
            w = Math.max(w, t.width);
        }

        double x = x0;
        double y = y0 + ascent;
        QuadRenderCommand command = new QuadRenderCommand(x, y, x + w, y - h, r, g, b, a, null, null,
                transform);
        if (currentName != -1) {
            command.name = currentName;
        }
        this.renderCommands.add(command);
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.guieditor.render.IGuiRenderer#getStringBounds(java.lang.String, java.lang.String)
     */
    @Override
    public Rectangle2D getStringBounds(TextRenderer textRenderer, String text) {
        if (textRenderer == null)
            textRenderer = debugTextRenderer;

        return textRenderer.getBounds(text);
    }

    private static class TextMetric implements ITextMetric {
        public TextMetric(TextRenderer textRenderer) {
            this.font = textRenderer.getFont();
            this.fontRenderContext = textRenderer.getFontRenderContext();
        }

        @Override
        public Rectangle getVisualBounds(String text) {
            GlyphVector glyphVector = font.createGlyphVector(this.fontRenderContext, text);
            return glyphVector.getOutline().getBounds();
        }

        @Override
        public float getLSB(char c) {
            GlyphVector glyphVector = font.createGlyphVector(this.fontRenderContext, "" + c);
            return glyphVector.getGlyphMetrics(0).getLSB();
        }

        private Font font;
        private FontRenderContext fontRenderContext;
    }

    @Override
    public List<TextLine> layout(TextRenderer textRenderer, String text, double width, boolean lineBreak) {
        return TextUtil.layout(new TextMetric(textRenderer), text, width, lineBreak);
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.guieditor.render.IGuiRenderer#setName(int)
     */
    @Override
    public void setName(int name) {
        this.currentName = name;
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.guieditor.render.IGuiRenderer#clearName()
     */
    @Override
    public void clearName() {
        this.currentName = -1;
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.guieditor.render.IGuiRenderer#beginSelect(javax.media.opengl.GL, int, int, int, int, int[])
     */
    @Override
    public void beginSelect(GL2 gl, int x, int y, int w, int h, int viewPort[]) {
        begin(gl);
        gl.glSelectBuffer(MAX_NODES, selectBuffer);
        gl.glRenderMode(GL2.GL_SELECT);

        GLU glu = new GLU();

        gl.glMatrixMode(GL2.GL_PROJECTION);
        gl.glLoadIdentity();
        glu.gluPickMatrix(x, y, w, h, viewPort, 0);
        glu.gluOrtho2D(0, viewPort[2], 0, viewPort[3]);

        gl.glMatrixMode(GL2.GL_MODELVIEW);
        gl.glLoadIdentity();

        gl.glInitNames();
    }

    private static long toUnsignedInt(int i)
    {
        long tmp = i;
        return ( tmp << 32 ) >>> 32;
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.guieditor.render.IGuiRenderer#endSelect()
     */
    @Override
    public SelectResult endSelect()
    {
        end();

        long minz;
        minz = Long.MAX_VALUE;

        gl.glFlush();
        int hits = gl.glRenderMode(GL2.GL_RENDER);

        List<SelectResult.Pair> selected = new ArrayList<SelectResult.Pair>();

        int names, ptr, ptrNames = 0, numberOfNames = 0;
        ptr = 0;
        for (int i = 0; i < hits; i++)
        {
           names = selectBuffer.get(ptr);
           ptr++;
           {
               numberOfNames = names;
               minz = toUnsignedInt(selectBuffer.get(ptr));
               ptrNames = ptr+2;
               selected.add(new SelectResult.Pair(minz, selectBuffer.get(ptrNames)));
           }

           ptr += names+2;
        }
        ptr = ptrNames;

        Collections.sort(selected);

        if (numberOfNames > 0)
        {
            return new SelectResult(selected, minz);
        }
        else
        {
            return new SelectResult(selected, minz);
        }
    }

    @Override
    public FontMetrics getFontMetrics(Font font) {
        return graphics.getFontMetrics(font);
    }

    @Override
    public TextRenderer getDebugTextRenderer() {
        return debugTextRenderer;
    }

}
