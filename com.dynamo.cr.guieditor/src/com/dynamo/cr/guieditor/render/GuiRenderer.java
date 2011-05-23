package com.dynamo.cr.guieditor.render;

import java.awt.Font;
import java.awt.FontFormatException;
import java.awt.geom.Rectangle2D;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.IntBuffer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.media.opengl.GL;
import javax.media.opengl.GLException;
import javax.media.opengl.glu.GLU;

import org.apache.commons.io.IOUtils;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.ui.services.IDisposable;

import com.dynamo.gui.proto.Gui.NodeDesc.BlendMode;
import com.sun.opengl.util.j2d.TextRenderer;
import com.sun.opengl.util.texture.Texture;
import com.sun.opengl.util.texture.TextureIO;

public class GuiRenderer implements IDisposable {

    private Map<String, FontInfo> fonts = new HashMap<String, FontInfo>();
    private Map<String, TextureInfo> textures = new HashMap<String, TextureInfo>();
    private FontInfo currentFont;
    private ArrayList<RenderCommmand> renderCommands = new ArrayList<GuiRenderer.RenderCommmand>();
    private GL gl;
    private int currentName;

    // Picking
    private static final int MAX_NODES = 4096;
    private static IntBuffer selectBuffer = ByteBuffer.allocateDirect(4 * MAX_NODES).order(ByteOrder.nativeOrder()).asIntBuffer();

    public GuiRenderer() {
        String fontName = Font.SANS_SERIF;
        Font debugFont = new Font(fontName, Font.BOLD, 28);
        fonts.put(getDebugFontName(), new FontInfo(getDebugFontName(), null, debugFont, 24));
    }

    private static class FontInfo {
        public FontInfo(String name, IFile file, Font font, int size) {
            this.name = name;
            this.file = file;
            this.size = size;
            this.textRenderer = new TextRenderer(font, true, true);
        }
        String name;
        IFile file;
        int size;
        TextRenderer textRenderer;
    }

    private static class TextureInfo {
        public TextureInfo(String name, IFile file, byte[] textureData) {
            this.name = name;
            this.file = file;
            this.textureData = textureData;
        }
        String name;
        IFile file;
        byte[] textureData;
        Texture texture;
    }

    private abstract class RenderCommmand {

        private BlendMode blendMode;
        private String textureName;
        public RenderCommmand(double r, double g, double b, double a, BlendMode blendMode, String textureName) {
            this.r = r;
            this.g = g;
            this.b = b;
            this.a = a;
            this.blendMode = blendMode;
            this.textureName = textureName;
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
                    gl.glBlendFunc(GL.GL_ONE, GL.GL_ONE);
                break;

                case BLEND_MODE_ADD_ALPHA:
                    gl.glBlendFunc(GL.GL_ONE, GL.GL_SRC_ALPHA);
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
            Texture texture = null;
            if (textureName != null && textureName.length() > 0) {
                TextureInfo textureInfo = textures.get(textureName);
                if (textureInfo != null && textureInfo.texture != null) {
                    texture = textureInfo.texture;
                }
            }

            if (texture != null) {
                gl.glEnable(GL.GL_TEXTURE_2D);
                gl.glTexEnvf(GL.GL_TEXTURE_ENV, GL.GL_TEXTURE_ENV_MODE, GL.GL_MODULATE);
                texture.bind();
            }
            else {
                gl.glDisable(GL.GL_TEXTURE_2D);
            }
        }

        public abstract void draw(GL gl);
    }

    private class TextRenderCommmand extends RenderCommmand {
        private String text;
        private double x0, y0;
        private String font;

        public TextRenderCommmand(String font, String text, double x0, double y0, double r, double g, double b, double a, BlendMode blendMode, String texture) {
            super(r, g, b, a, blendMode, texture);
            this.font = font;
            this.text = text;
            this.x0 = x0;
            this.y0 = y0;
        }

        @Override
        public void draw(GL gl) {

            if (currentFont != null && currentFont.name.equals(name))  {
                // Use current font
            }
            else {
                if (currentFont != null) {
                    // End current rendering for font
                    currentFont.textRenderer.end3DRendering();
                }
                currentFont = fonts.get(font);
                if (currentFont != null) {
                    currentFont.textRenderer.begin3DRendering();
                }

            }

            if (currentFont != null) {
                gl.glColor4d(r, g, b, a);
                setupBlendMode();
                currentFont.textRenderer.draw3D(text, (float) x0, (float) y0, 0, 1);
            }
        }
    }

    private class QuadRenderCommand extends RenderCommmand {
        private double x0, y0, x1, y1;

        public QuadRenderCommand(double x0, double y0, double x1, double y1, double r, double g, double b, double a, BlendMode blendMode, String texture) {
            super(r, g, b, a, blendMode, texture);
            this.x0 = x0;
            this.y0 = y0;
            this.x1 = x1;
            this.y1 = y1;
        }

        @Override
        public void draw(GL gl) {
            if (currentFont != null) {
                // End current font rendering
                currentFont.textRenderer.end3DRendering();
                currentFont = null;
            }

            if (name != -1)
                gl.glPushName(name);

            setupBlendMode();
            setupTexture();

            gl.glBegin(GL.GL_QUADS);
            gl.glColor4d(r, g, b, a);

            gl.glTexCoord2d(0, 1);
            gl.glVertex2d(x0, y0);

            gl.glTexCoord2d(1, 1);
            gl.glVertex2d(x1, y0);

            gl.glTexCoord2d(1, 0);
            gl.glVertex2d(x1, y1);

            gl.glTexCoord2d(0, 0);
            gl.glVertex2d(x0, y1);
            gl.glEnd();

            if (name != -1)
                gl.glPopName();

        }
    }

    @Override
    public void dispose() {
    }

    public void begin(GL gl) {
        this.currentFont = null;
        this.renderCommands.clear();
        this.renderCommands.ensureCapacity(1024);
        this.gl = gl;
        this.currentName = -1;
        loadDeferredTextures();
    }

    private void loadDeferredTextures() {
        for (TextureInfo textureInfo : textures.values()) {
            if (textureInfo.textureData != null) {
                try {
                    Texture texture = TextureIO.newTexture(new ByteArrayInputStream(textureInfo.textureData), true, textureInfo.file.getFileExtension());
                    textureInfo.texture = texture;
                } catch (Throwable e) {
                    e.printStackTrace();
                } finally {
                    textureInfo.textureData = null;
                }
            }
        }
    }

    public void end() {
        for (RenderCommmand command : renderCommands) {
            command.draw(gl);
        }
        if (currentFont != null) {
            // End current font rendering
            currentFont.textRenderer.end3DRendering();
            currentFont = null;
        }
    }

    public void drawQuad(double x0, double y0, double x1, double y1, double r, double g, double b, double a, BlendMode blendMode, String texture) {
        QuadRenderCommand command = new QuadRenderCommand(x0, y0, x1, y1, r, g, b, a, blendMode, texture);
        if (currentName != -1) {
            command.name = currentName;
        }
        this.renderCommands.add(command);
    }

    public void drawString(String font, String text, double x0, double y0, double r, double g, double b, double a, BlendMode blendMode, String texture) {
        TextRenderCommmand command = new TextRenderCommmand(font, text, x0, y0, r, g, b, a, blendMode, texture);
        if (currentName != -1) {
            command.name = currentName;
        }
        this.renderCommands.add(command);
    }

    public String getDebugFontName() {
        return "__debug__";
    }

    public void drawStringBounds(String font, String text, double x0, double y0, double r, double g, double b, double a) {
        FontInfo fontInfo = fonts.get(font);
        if (fontInfo != null) {
            Rectangle2D bounds = fontInfo.textRenderer.getBounds(text);
            double x = x0 + bounds.getX();
            double y = y0 - (bounds.getHeight() + bounds.getY());
            double w = bounds.getWidth();
            double h = bounds.getHeight();
            QuadRenderCommand command = new QuadRenderCommand(x, y, x + w, y + h, r, g, b, a, null, null);
            if (currentName != -1) {
                command.name = currentName;
            }
            this.renderCommands.add(command);
        }
    }

    public Rectangle2D getStringBounds(String font, String text) {
        FontInfo fontInfo = fonts.get(font);
        if (fontInfo != null) {
            return fontInfo.textRenderer.getBounds(text);
        }
        return null;
    }


    public boolean isFontLoaded(String alias, IFile fontFile, int size) {
        FontInfo fontInfo = fonts.get(alias);
        if (fontInfo != null) {
            return fontInfo.file.equals(fontFile) && fontInfo.size == size;
        }
        return false;
    }

    public boolean isTextureLoaded(String alias, IFile textureFile) {
        TextureInfo textureInfo = textures.get(alias);
        if (textureInfo != null) {
            return textureInfo.file.equals(textureFile);
        }
        return false;
    }

    public void loadFont(String alias, IFile fontFile, int size) throws CoreException, IOException, FontFormatException {
        if (isFontLoaded(alias, fontFile, size))
            return;

        InputStream fontStream = fontFile.getContents();
        try {
            ByteArrayOutputStream fontOut = new ByteArrayOutputStream();
            IOUtils.copy(fontStream, fontOut);
            byte[] fontData = fontOut.toByteArray();
            Font font = Font.createFont(Font.TRUETYPE_FONT, new ByteArrayInputStream(fontData));

            font = font.deriveFont(Font.PLAIN, size);
            fonts.put(alias, new FontInfo(alias, fontFile, font, size));
        }
        finally {
            fontStream.close();
        }
    }

    public void loadTexture(String alias, IFile textureFile) throws GLException, IOException, CoreException {
        if (isTextureLoaded(alias, textureFile))
            return;

        ByteArrayOutputStream output = new ByteArrayOutputStream(1024 * 128);
        IOUtils.copy(textureFile.getContents(), output);
        TextureInfo textureInfo = new TextureInfo(alias, textureFile, output.toByteArray());
        textures.put(alias, textureInfo);
    }

    public void setName(int name) {
        this.currentName = name;
    }

    public void clearName() {
        this.currentName = -1;
    }

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
    public void beginSelect(GL gl, int x, int y, int w, int h, int viewPort[]) {
        begin(gl);
        gl.glSelectBuffer(MAX_NODES, selectBuffer);
        gl.glRenderMode(GL.GL_SELECT);

        GLU glu = new GLU();

        gl.glMatrixMode(GL.GL_PROJECTION);
        gl.glLoadIdentity();
        glu.gluPickMatrix(x, y, w, h, viewPort, 0);
        glu.gluOrtho2D(0, viewPort[2], 0, viewPort[3]);

        gl.glMatrixMode(GL.GL_MODELVIEW);
        gl.glLoadIdentity();

        gl.glInitNames();
    }

    private static long toUnsignedInt(int i)
    {
        long tmp = i;
        return ( tmp << 32 ) >>> 32;
    }

    public SelectResult endSelect()
    {
        end();

        long minz;
        minz = Long.MAX_VALUE;

        gl.glFlush();
        int hits = gl.glRenderMode(GL.GL_RENDER);

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

    public boolean hasFont(String font) {
        return fonts.containsKey(font);
    }

}
