package com.dynamo.cr.tileeditor;

import java.awt.Color;
import java.awt.image.BufferedImage;
import java.nio.FloatBuffer;

import javax.media.opengl.GL;
import javax.media.opengl.GLContext;
import javax.media.opengl.GLDrawableFactory;
import javax.media.opengl.glu.GLU;
import javax.vecmath.Vector2f;

import org.eclipse.swt.SWT;
import org.eclipse.swt.events.KeyEvent;
import org.eclipse.swt.events.KeyListener;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseListener;
import org.eclipse.swt.events.MouseMoveListener;
import org.eclipse.swt.graphics.Rectangle;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.opengl.GLCanvas;
import org.eclipse.swt.opengl.GLData;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Listener;
import org.eclipse.ui.services.IDisposable;

import com.dynamo.cr.tileeditor.core.TileSetPresenter;
import com.dynamo.cr.tileeditor.core.TileSetUtil;
import com.sun.opengl.util.BufferUtil;
import com.sun.opengl.util.texture.Texture;
import com.sun.opengl.util.texture.TextureIO;

public class TileSetRenderer implements
IDisposable,
MouseListener,
MouseMoveListener,
Listener,
KeyListener {

    private final static int CAMERA_MODE_NONE = 0;
    private final static int CAMERA_MODE_TRACK = 1;
    private final static int CAMERA_MODE_DOLLY = 2;

    private final TileSetPresenter presenter;
    private GLCanvas canvas;
    private GLContext context;
    private final int[] viewPort = new int[4];
    private boolean paintRequested = false;
    private boolean mac = false;
    private float scale = 1.0f;
    // Camera data
    private int cameraMode = CAMERA_MODE_NONE;
    private int lastX;
    private int lastY;
    private final float[] offset = new float[2];
    // Render data
    private BufferedImage image;
    private Texture texture;
    private BufferedImage collision;
    private int tileWidth;
    private int tileHeight;
    private int tileMargin;
    private int tileSpacing;
    private FloatBuffer hullVertices;
    private int[] hullIndices;
    private int[] hullCounts;
    private Color[] hullColors;
    private Texture background;

    public TileSetRenderer(TileSetPresenter presenter) {
        this.presenter = presenter;

        this.mac = System.getProperty("os.name").equals("Mac OS X");
    }

    public void createControls(Composite parent) {
        GLData data = new GLData();
        data.doubleBuffer = true;
        data.depthSize = 24;

        this.canvas = new GLCanvas(parent, SWT.NO_REDRAW_RESIZE | SWT.NO_BACKGROUND, data);
        GridData gd = new GridData(SWT.FILL, SWT.FILL, true, true);
        gd.widthHint = SWT.DEFAULT;
        gd.heightHint = SWT.DEFAULT;
        this.canvas.setLayoutData(gd);

        this.canvas.setCurrent();

        this.context = GLDrawableFactory.getFactory().createExternalGLContext();

        this.context.makeCurrent();
        GL gl = this.context.getGL();
        gl.glPolygonMode(GL.GL_FRONT, GL.GL_FILL);

        BufferedImage backgroundImage = new BufferedImage(2, 2, BufferedImage.TYPE_INT_ARGB);
        backgroundImage.setRGB(0, 0, 0xff999999);
        backgroundImage.setRGB(1, 0, 0xff666666);
        backgroundImage.setRGB(0, 1, 0xff666666);
        backgroundImage.setRGB(1, 1, 0xff999999);
        background = TextureIO.newTexture(backgroundImage, false);

        // if the image is already set, set the corresponding texture
        if (this.image != null) {
            this.texture = TextureIO.newTexture(image, false);
        }

        this.context.release();

        this.canvas.addListener(SWT.Resize, this);
        this.canvas.addListener(SWT.Paint, this);
        this.canvas.addMouseListener(this);
        this.canvas.addMouseMoveListener(this);
        this.canvas.addKeyListener(this);
    }

    @Override
    public void dispose() {
        if (this.context != null) {
            this.context.makeCurrent();
            this.background.dispose();
            if (this.texture != null) {
                this.texture.dispose();
            }
            this.context.release();
        }
        if (this.canvas != null) {
            canvas.dispose();
        }
    }

    public void setFocus() {
        this.canvas.setFocus();
    }

    void setImage(BufferedImage image) {
        if (this.image != image) {
            if (this.context != null) {
                this.context.makeCurrent();
                if (this.texture != null) {
                    this.texture.updateImage(TextureIO.newTextureData(image, false));
                } else {
                    this.texture = TextureIO.newTexture(image, false);
                }
                this.context.release();
            }
            this.image = image;
            requestPaint();
        }
    }

    void setTileWidth(int tileWidth) {
        if (this.tileWidth != tileWidth) {
            this.tileWidth = tileWidth;
            requestPaint();
        }
    }

    void setTileHeight(int tileHeight) {
        if (this.tileHeight != tileHeight) {
            this.tileHeight = tileHeight;
            requestPaint();
        }
    }

    void setTileMargin(int tileMargin) {
        if (this.tileMargin != tileMargin) {
            this.tileMargin = tileMargin;
            requestPaint();
        }
    }

    void setTileSpacing(int tileSpacing) {
        if (this.tileSpacing != tileSpacing) {
            this.tileSpacing = tileSpacing;
            requestPaint();
        }
    }

    void setCollision(BufferedImage collision) {
        if (this.collision != collision) {
            this.collision = collision;
            requestPaint();
        }
    }

    void setHulls(float[] hullVertices, int[] hullIndices, int[] hullCounts, Color[] hullColors) {
        // Assume it is different
        this.hullVertices = BufferUtil.newFloatBuffer(hullVertices.length);
        this.hullVertices.put(hullVertices);
        this.hullVertices.flip();
        this.hullIndices = hullIndices;
        this.hullCounts = hullCounts;
        this.hullColors = hullColors;
        requestPaint();
    }

    public void setHullColor(int tileIndex, Color color) {
        if (this.hullColors[tileIndex] != color) {
            this.hullColors[tileIndex] = color;
            requestPaint();
        }
    }

    // Listener

    @Override
    public void handleEvent(Event event) {
        if (event.type == SWT.Resize) {
            Rectangle client = canvas.getClientArea();
            viewPort[0] = 0;
            viewPort[1] = 0;
            viewPort[2] = client.width;
            viewPort[3] = client.height;
        } else if (event.type == SWT.Paint) {
            requestPaint();
        }
    }

    // KeyListener

    @Override
    public void keyPressed(KeyEvent e) {
        // TODO Auto-generated method stub

    }

    @Override
    public void keyReleased(KeyEvent e) {
        // TODO Auto-generated method stub

    }

    // MouseMoveListener

    @Override
    public void mouseMove(MouseEvent e) {
        int dx = e.x - this.lastX;
        int dy = e.y - this.lastY;
        switch (this.cameraMode) {
        case CAMERA_MODE_TRACK:
            float recipScale = 1.0f / this.scale;
            this.offset[0] += dx * recipScale;
            this.offset[1] -= dy * recipScale;
            requestPaint();
            break;
        case CAMERA_MODE_DOLLY:
            float ds = -dy * 0.005f;
            this.scale += (this.scale > 1.0f) ? ds * this.scale : ds;
            this.scale = Math.max(0.1f, this.scale);
            requestPaint();
            break;
        }
        this.lastX = e.x;
        this.lastY = e.y;
    }

    // MouseListener

    @Override
    public void mouseDoubleClick(MouseEvent e) {
        // TODO Auto-generated method stub

    }

    @Override
    public void mouseDown(MouseEvent event) {
        this.lastX = event.x;
        this.lastY = event.y;

        if ((this.mac && event.stateMask == (SWT.ALT | SWT.CTRL))
                || (!this.mac && event.button == 2 && event.stateMask == SWT.ALT))
        {
            this.cameraMode = CAMERA_MODE_TRACK;
        }
        else if ((this.mac && event.stateMask == (SWT.CTRL))
                || (!this.mac && event.button == 3 && event.stateMask == SWT.ALT))
        {
            this.cameraMode = CAMERA_MODE_DOLLY;
        }
        else
        {
            this.cameraMode = CAMERA_MODE_NONE;
        }
    }

    @Override
    public void mouseUp(MouseEvent e) {
        this.cameraMode = CAMERA_MODE_NONE;
    }

    private void requestPaint() {
        if (this.paintRequested || this.canvas == null)
            return;
        this.paintRequested = true;

        Display.getDefault().timerExec(10, new Runnable() {

            @Override
            public void run() {
                paintRequested = false;
                paint();
            }
        });
    }

    private void paint() {
        if (!this.canvas.isDisposed()) {
            this.canvas.setCurrent();
            this.context.makeCurrent();
            GL gl = this.context.getGL();
            GLU glu = new GLU();
            try {
                gl.glDepthMask(true);
                gl.glEnable(GL.GL_DEPTH_TEST);
                gl.glClearColor(0.0f, 0.0f, 0.0f, 1);
                gl.glClearDepth(1.0);
                gl.glClear(GL.GL_COLOR_BUFFER_BIT | GL.GL_DEPTH_BUFFER_BIT);
                gl.glDisable(GL.GL_DEPTH_TEST);
                gl.glDepthMask(false);

                gl.glViewport(this.viewPort[0], this.viewPort[1], this.viewPort[2], this.viewPort[3]);

                gl.glMatrixMode(GL.GL_PROJECTION);
                gl.glLoadIdentity();
                float recipScale = 1.0f / this.scale;
                float x = 0.5f * this.viewPort[2] * recipScale;
                float y = 0.5f * this.viewPort[3] * recipScale;
                glu.gluOrtho2D(-x, x, -y, y);

                gl.glMatrixMode(GL.GL_MODELVIEW);
                gl.glLoadIdentity();
                gl.glTranslatef(this.offset[0], this.offset[1], 0.0f);

                drawTileSet(gl);

            } catch (Throwable e) {
                // Don't show dialog or similar in paint-handle
                e.printStackTrace();
            } finally {
                canvas.swapBuffers();
                context.release();
            }
        }
    }

    private void drawTileSet(GL gl) {
        if (this.image == null && this.collision == null) {
            return;
        }

        int width = 0;
        int height = 0;
        if (this.image != null) {
            width = this.image.getWidth();
            height = this.image.getHeight();
        } else {
            width = this.collision.getWidth();
            height = this.collision.getHeight();
        }
        int tilesPerRow = TileSetUtil.calculateTileCount(this.tileWidth, width, this.tileMargin, this.tileSpacing);
        int tilesPerColumn = TileSetUtil.calculateTileCount(this.tileHeight, height, this.tileMargin, this.tileSpacing);

        final float borderSize = 2.0f;
        float pixelBorderSize = borderSize / this.scale;
        float totalWidth = (this.tileWidth + pixelBorderSize) * tilesPerRow + pixelBorderSize;
        float totalHeight = (this.tileHeight + pixelBorderSize) * tilesPerColumn + pixelBorderSize;

        // background
        drawBackground(gl, totalWidth, totalHeight);

        // tiles
        drawTiles(gl, borderSize, tilesPerRow, tilesPerColumn, totalWidth, totalHeight);
    }

    private void drawBackground(GL gl, float width, float height) {
        this.background.bind();
        this.background.setTexParameteri(GL.GL_TEXTURE_MIN_FILTER, GL.GL_NEAREST);
        this.background.setTexParameteri(GL.GL_TEXTURE_MAG_FILTER, GL.GL_NEAREST);
        this.background.setTexParameteri(GL.GL_TEXTURE_WRAP_S, GL.GL_REPEAT);
        this.background.setTexParameteri(GL.GL_TEXTURE_WRAP_T, GL.GL_REPEAT);
        this.background.enable();
        final float recipTileSize = 0.0625f;
        float recipTexelWidth = width * recipTileSize;
        float recipTexelHeight = height * recipTileSize;
        float u0 = (0.5f / width + recipTileSize * this.offset[0]) * this.scale;
        float u1 = u0 + recipTexelWidth * this.scale;
        float v0 = (0.5f / height + recipTileSize * this.offset[1]) * this.scale;
        float v1 = v0 + recipTexelHeight * this.scale;
        gl.glBegin(GL.GL_QUADS);
        gl.glTexCoord2f(u0, v0);
        gl.glVertex2f(0.0f, 0.0f);
        gl.glTexCoord2f(u0, v1);
        gl.glVertex2f(0.0f, height);
        gl.glTexCoord2f(u1, v1);
        gl.glVertex2f(width, height);
        gl.glTexCoord2f(u1, v0);
        gl.glVertex2f(width, 0.0f);
        gl.glEnd();
        this.background.disable();
    }

    private void drawTiles(GL gl, float borderSize, int tilesPerRow, int tilesPerColumn, float width, float height) {
        if (this.image == null) {
            return;
        }

        int tileCount = tilesPerRow * tilesPerColumn;
        // vertex data is 3 components for position and 2 for uv
        Vector2f[] hullOffsets = null;
        if (this.hullVertices != null) {
            hullOffsets = new Vector2f[2 * tileCount];
        }
        int vertexCount = 4 * tileCount;
        FloatBuffer v = BufferUtil.newFloatBuffer(vertexCount * 3);
        FloatBuffer t = BufferUtil.newFloatBuffer(vertexCount * 2);
        float recipImageWidth = 1.0f / image.getWidth();
        float recipImageHeight = 1.0f / image.getHeight();
        float recipScale = 1.0f / this.scale;
        float border = borderSize * recipScale;
        for (int row = 0; row < tilesPerColumn; ++row) {
            for (int column = 0; column < tilesPerRow; ++column) {
                float x0 = column * (tileWidth + border) + border;
                float x1 = x0 + tileWidth;
                float y0 = row * (tileHeight + border) + border;
                float y1 = y0 + tileHeight;
                float u0 = (column * (tileSpacing + 2*tileMargin + tileWidth) + tileMargin) * recipImageWidth;
                float u1 = u0 + tileWidth * recipImageWidth;
                float v1 = ((tilesPerColumn - row - 1) * (tileSpacing + 2*tileMargin + tileHeight) + tileMargin) * recipImageHeight;
                float v0 = v1 + tileHeight * recipImageHeight;
                v.put(x0); v.put(y0); v.put(0.0f); t.put(u0); t.put(v0);
                v.put(x0); v.put(y1); v.put(0.0f); t.put(u0); t.put(v1);
                v.put(x1); v.put(y1); v.put(0.0f); t.put(u1); t.put(v1);
                v.put(x1); v.put(y0); v.put(0.0f); t.put(u1); t.put(v0);
                if (hullOffsets != null) {
                    int index = column + (tilesPerColumn - row - 1) * tilesPerRow;
                    hullOffsets[index] = new Vector2f(x0, y0);
                }
            }
        }
        v.flip();
        t.flip();

        gl.glEnable(GL.GL_DEPTH_TEST);
        gl.glDepthMask(true);
        gl.glEnable(GL.GL_BLEND);
        gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);
        gl.glEnableClientState(GL.GL_TEXTURE_COORD_ARRAY);
        gl.glEnableClientState(GL.GL_VERTEX_ARRAY);
        this.texture.bind();
        this.texture.setTexParameteri(GL.GL_TEXTURE_MIN_FILTER, GL.GL_NEAREST);
        this.texture.setTexParameteri(GL.GL_TEXTURE_MAG_FILTER, GL.GL_NEAREST);
        this.texture.setTexParameteri(GL.GL_TEXTURE_WRAP_S, GL.GL_CLAMP);
        this.texture.setTexParameteri(GL.GL_TEXTURE_WRAP_T, GL.GL_CLAMP);
        this.texture.enable();

        gl.glTexCoordPointer(2, GL.GL_FLOAT, 0, t);
        gl.glVertexPointer(3, GL.GL_FLOAT, 0, v);

        gl.glDrawArrays(GL.GL_QUADS, 0, vertexCount);

        this.texture.disable();
        gl.glDepthMask(false);
        gl.glDisableClientState(GL.GL_TEXTURE_COORD_ARRAY);

        // Overlay

        gl.glColor4f(0.2f, 0.2f, 0.2f, 0.7f);
        gl.glBegin(GL.GL_QUADS);
        float z = -0.5f;
        gl.glVertex3f(0.0f, 0.0f, z);
        gl.glVertex3f(0.0f, height, z);
        gl.glVertex3f(width, height, z);
        gl.glVertex3f(width, 0.0f, z);
        gl.glEnd();

        gl.glDisable(GL.GL_DEPTH_TEST);

        float[] color = new float[4];
        if (this.hullVertices != null) {
            gl.glVertexPointer(2, GL.GL_FLOAT, 0, this.hullVertices);
            for (int i = 0; i < this.hullCounts.length; ++i) {
                color = hullColors[i].getComponents(color);
                gl.glPushMatrix();
                gl.glTranslatef(hullOffsets[i].x + 0.5f, hullOffsets[i].y + 0.5f, 0.0f);
                gl.glColor4f(color[0], color[1], color[2], color[3]);
                gl.glDrawArrays(GL.GL_LINE_LOOP, this.hullIndices[i], this.hullCounts[i]);
                gl.glPopMatrix();
            }
        }
        gl.glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        gl.glDisableClientState(GL.GL_VERTEX_ARRAY);
        gl.glDisable(GL.GL_BLEND);
    }

}
