package com.dynamo.cr.tileeditor;

import java.awt.Color;
import java.awt.image.BufferedImage;
import java.nio.FloatBuffer;

import javax.media.opengl.GL;
import javax.media.opengl.GLContext;
import javax.media.opengl.GLDrawableFactory;
import javax.media.opengl.glu.GLU;

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
    private final GLCanvas canvas;
    private final GLContext context;
    private final int[] viewPort = new int[4];
    private boolean paintRequested = false;
    private boolean mac = false;
    private final float scale = 1.0f;
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
    private float[] hullVertices;
    private int[] hullIndices;
    private int[] hullCounts;
    private Color[] hullColors;
    private final Texture background;

    public TileSetRenderer(TileSetPresenter presenter, Composite parent) {
        this.presenter = presenter;

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
        if (!System.getProperty("os.name").equals("Mac OS X")) {
            gl.setSwapInterval(1);
        } else {
            this.mac = true;
        }

        BufferedImage backgroundImage = new BufferedImage(2, 2, BufferedImage.TYPE_INT_ARGB);
        backgroundImage.setRGB(0, 0, 0xff999999);
        backgroundImage.setRGB(1, 0, 0xff666666);
        backgroundImage.setRGB(0, 1, 0xff666666);
        backgroundImage.setRGB(1, 1, 0xff999999);
        background = TextureIO.newTexture(backgroundImage, false);
        background.setTexParameteri(GL.GL_TEXTURE_MIN_FILTER, GL.GL_NEAREST);
        background.setTexParameteri(GL.GL_TEXTURE_MAG_FILTER, GL.GL_NEAREST);
        background.setTexParameteri(GL.GL_TEXTURE_WRAP_S, GL.GL_REPEAT);
        background.setTexParameteri(GL.GL_TEXTURE_WRAP_T, GL.GL_REPEAT);

        this.context.release();

        this.canvas.addListener(SWT.Resize, this);
        this.canvas.addListener(SWT.Paint, this);
        this.canvas.addMouseListener(this);
        this.canvas.addMouseMoveListener(this);
        this.canvas.addKeyListener(this);
    }

    public GLCanvas getCanvas() {
        return canvas;
    }

    @Override
    public void dispose() {
        this.context.makeCurrent();
        this.background.dispose();
        if (this.texture != null) {
            this.texture.dispose();
        }
        this.context.release();
        canvas.dispose();
    }

    void setTileData(BufferedImage image, BufferedImage collision,
            int tileWidth, int tileHeight, int tileMargin, int tileSpacing,
            float[] hullVertices, int[] hullIndices, int[] hullCounts, Color[] hullColors) {
        if (image != this.image) {
            this.context.makeCurrent();
            if (this.texture != null) {
                this.texture.dispose();
            }
            this.texture = TextureIO.newTexture(image, false);
            this.texture.setTexParameteri(GL.GL_TEXTURE_MIN_FILTER, GL.GL_NEAREST);
            this.texture.setTexParameteri(GL.GL_TEXTURE_MAG_FILTER, GL.GL_NEAREST);
            this.texture.setTexParameteri(GL.GL_TEXTURE_WRAP_S, GL.GL_CLAMP);
            this.texture.setTexParameteri(GL.GL_TEXTURE_WRAP_T, GL.GL_CLAMP);
            this.image = image;
            this.context.release();
        }
        this.collision = collision;
        this.tileWidth = tileWidth;
        this.tileHeight = tileHeight;
        this.tileMargin = tileMargin;
        this.tileSpacing = tileSpacing;
        this.hullVertices = hullVertices;
        this.hullIndices = hullIndices;
        this.hullCounts = hullCounts;
        this.hullColors = hullColors;
        requestPaint();
    }


    public void clearTiles() {
    }

    public void setTileHullColor(int tileIndex, Color color) {
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
            offset[0] += dx;
            offset[1] -= dy;
            requestPaint();
            break;
        case CAMERA_MODE_DOLLY:
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
        if (this.paintRequested)
            return;
        this.paintRequested = true;

        if (!this.canvas.isDisposed()) {
            this.canvas.setCurrent();
            this.context.makeCurrent();
            GL gl = this.context.getGL();
            GLU glu = new GLU();
            try {
                gl.glViewport(this.viewPort[0], this.viewPort[1], this.viewPort[2], this.viewPort[3]);

                gl.glMatrixMode(GL.GL_PROJECTION);
                gl.glLoadIdentity();
                glu.gluOrtho2D(0, this.viewPort[2], 0, this.viewPort[3]);

                gl.glMatrixMode(GL.GL_MODELVIEW);
                gl.glLoadIdentity();
                gl.glTranslatef(0.5f, 0.5f, 0.5f);
                gl.glClearColor(0.0f, 0.0f, 0.0f, 1);
                gl.glClear(GL.GL_COLOR_BUFFER_BIT);

                drawTileSet(gl);

                //                doDraw(gl);
            } catch (Throwable e) {
                // Don't show dialog or similar in paint-handle
                e.printStackTrace();
            } finally {
                canvas.swapBuffers();
                context.release();
            }
        }
        paintRequested = false;
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

        // 1 pixel spacing between tiles
        int totalWidth = (this.tileWidth + 1) * tilesPerRow + 1;
        int totalHeight = (this.tileHeight + 1) * tilesPerColumn + 1;

        // temp center, round for pixel perfection
        gl.glTranslatef(Math.round(offset[0]), Math.round(offset[1]), 0.0f);

        // background
        drawBackground(gl, totalWidth, totalHeight);

        // tiles
        drawTiles(gl, tilesPerRow, tilesPerColumn);

        // grid
        drawBorders(gl, totalWidth, totalHeight, tilesPerRow, tilesPerColumn);
    }

    private void drawBackground(GL gl, int width, int height) {
        this.background.bind();
        this.background.enable();
        float recipTexelWidth = width * 0.0625f;
        float recipTexelHeight = height * 0.0625f;
        float halfTexelWidth = 0.5f / width;
        float halfTexelHeight = 0.5f / height;
        float u0 = halfTexelWidth;
        float u1 = recipTexelWidth + halfTexelWidth;
        float v0 = -halfTexelHeight;
        float v1 = recipTexelHeight - halfTexelHeight;
        gl.glBegin(GL.GL_QUADS);
        gl.glTexCoord2f(u0, v1);
        gl.glVertex2f(0.0f, 0.0f);
        gl.glTexCoord2f(u0, v0);
        gl.glVertex2f(0.0f, height);
        gl.glTexCoord2f(u1, v0);
        gl.glVertex2f(width, height);
        gl.glTexCoord2f(u1, v1);
        gl.glVertex2f(width, 0.0f);
        gl.glEnd();
        this.background.disable();
    }

    private void drawTiles(GL gl, int tilesPerRow, int tilesPerColumn) {
        if (this.image == null) {
            return;
        }

        int tileCount = tilesPerRow * tilesPerColumn;

        // vertex data is 3 components for position and 2 for uv
        int vertexCount = 4 * tileCount;
        FloatBuffer v = BufferUtil.newFloatBuffer(vertexCount * 2);
        FloatBuffer t = BufferUtil.newFloatBuffer(vertexCount * 2);
        // render space is scaled pixel space, into which the tiles are rendered with one pixel non-scaled spacing
        float recipImageWidth = 1.0f / image.getWidth();
        float recipImageHeight = 1.0f / image.getHeight();
        for (int row = 0; row < tilesPerColumn; ++row) {
            for (int column = 0; column < tilesPerRow; ++column) {
                float x0 = column * (tileWidth + 1.0f) + 1.0f;
                float x1 = x0 + tileWidth;
                float y0 = row * (tileHeight + 1.0f) + 1.0f;
                float y1 = y0 + tileHeight;
                float u0 = (column * (tileSpacing + 2*tileMargin + tileWidth) + tileMargin + 0.5f) * recipImageWidth;
                float u1 = u0 + tileWidth * recipImageWidth;
                float v1 = ((tilesPerColumn - row - 1) * (tileSpacing + 2*tileMargin + tileHeight) + tileMargin - 0.5f) * recipImageHeight;
                float v0 = v1 + tileHeight * recipImageHeight;
                v.put(x0); v.put(y0); t.put(u0); t.put(v0);
                v.put(x0); v.put(y1); t.put(u0); t.put(v1);
                v.put(x1); v.put(y1); t.put(u1); t.put(v1);
                v.put(x1); v.put(y0); t.put(u1); t.put(v0);
            }
        }
        v.flip();
        t.flip();

        gl.glEnable(GL.GL_BLEND);
        gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);
        gl.glEnableClientState(GL.GL_TEXTURE_COORD_ARRAY);
        gl.glEnableClientState(GL.GL_VERTEX_ARRAY);
        this.texture.bind();
        this.texture.enable();

        gl.glTexCoordPointer(2, GL.GL_FLOAT, 0, t);
        gl.glVertexPointer(2, GL.GL_FLOAT, 0, v);

        gl.glDrawArrays(GL.GL_QUADS, 0, vertexCount);

        this.texture.disable();
        gl.glDisableClientState(GL.GL_TEXTURE_COORD_ARRAY);
        gl.glDisableClientState(GL.GL_VERTEX_ARRAY);
        gl.glDisable(GL.GL_BLEND);
    }

    private void drawBorders(GL gl, int width, int height, int tilesPerRow, int tilesPerColumn) {
        int dx = (width - 1)/tilesPerRow;
        int dy = (height - 1)/tilesPerColumn;
        int vertexCount = ((tilesPerRow + 1) + (tilesPerColumn + 1)) * 2;
        FloatBuffer v = BufferUtil.newFloatBuffer(vertexCount * 2);
        for (int column = 0; column <= tilesPerRow; ++column) {
            float x0 = column * dx;
            v.put(x0); v.put(0.0f);
            v.put(x0); v.put(height);
        }
        for (int row = 0; row <= tilesPerColumn; ++row) {
            float y0 = row * dy;
            v.put(0.0f); v.put(y0);
            v.put(width); v.put(y0);
        }
        v.flip();

        gl.glColor4f(0.1f, 0.1f, 0.1f, 0.6f);

        gl.glEnable(GL.GL_BLEND);
        gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);
        gl.glEnableClientState(GL.GL_VERTEX_ARRAY);

        gl.glVertexPointer(2, GL.GL_FLOAT, 0, v);
        gl.glDrawArrays(GL.GL_LINES, 0, vertexCount);

        gl.glDisableClientState(GL.GL_VERTEX_ARRAY);
        gl.glColor3f(1.0f, 1.0f, 1.0f);
        gl.glDisable(GL.GL_BLEND);
    }
}
