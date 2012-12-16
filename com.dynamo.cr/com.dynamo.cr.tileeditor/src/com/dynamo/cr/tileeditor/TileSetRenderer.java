package com.dynamo.cr.tileeditor;

import java.awt.Color;
import java.awt.image.BufferedImage;
import java.nio.FloatBuffer;
import java.nio.IntBuffer;

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
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Listener;
import org.eclipse.ui.services.IDisposable;

import com.dynamo.bob.tile.TileSetUtil;
import com.dynamo.cr.sceneed.core.TextureHandle;
import com.dynamo.cr.tileeditor.core.TileSetPresenter;
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
    private static final float BORDER_SIZE = 3.0f;

    private final TileSetPresenter presenter;
    private GLCanvas canvas;
    private GLContext context;
    private final IntBuffer viewPort = BufferUtil.newIntBuffer(4);
    private boolean paintRequested = false;
    private boolean mac = false;
    private float scale = 1.0f;
    private boolean enabled = true;
    private boolean resetView = true;
    private String brushCollisionGroup = "";
    private Color brushColor = Color.white;
    private int activeTile = -1;

    // Camera data
    private int cameraMode = CAMERA_MODE_NONE;
    private int lastX;
    private int lastY;
    private final float[] offset = new float[2];

    // Render data
    private BufferedImage image;
    private TextureHandle texture;
    private BufferedImage collision;
    private int tileWidth;
    private int tileHeight;
    private int tileMargin;
    private int tileSpacing;
    private FloatBuffer tileVertexBuffer;
    private float[] hullVertices;
    private int[] hullIndices;
    private int[] hullCounts;
    private Color[] hullColors;
    private FloatBuffer hullVertexBuffer;
    private FloatBuffer hullFrameVertexBuffer;
    private Texture backgroundTexture;
    private Texture transparentTexture;

    public TileSetRenderer(TileSetPresenter presenter) {
        this.presenter = presenter;
        this.texture = new TextureHandle();

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
        this.backgroundTexture = TextureIO.newTexture(backgroundImage, false);
        BufferedImage transparentImage = new BufferedImage(1, 1, BufferedImage.TYPE_INT_ARGB);
        transparentImage.setRGB(0, 0, 0x00000000);
        this.transparentTexture = TextureIO.newTexture(transparentImage, false);

        // if the image is already set, set the corresponding texture
        if (this.image != null) {
            this.texture.setImage(this.image);
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
            this.context.destroy();
        }
        if (this.canvas != null) {
            canvas.dispose();
        }
    }

    public void setFocus() {
        this.canvas.setFocus();
    }

    public Control getControl() {
        return this.canvas;
    }

    public void setImage(BufferedImage image) {
        if (this.image != image) {
            this.texture.setImage(image);
            if (this.image == null) {
                this.resetView = true;
            }
            this.image = image;
            requestPaint();
        }
    }

    public void setTileWidth(int tileWidth) {
        if (this.tileWidth != tileWidth) {
            this.tileWidth = tileWidth;
            requestPaint();
        }
    }

    public void setTileHeight(int tileHeight) {
        if (this.tileHeight != tileHeight) {
            this.tileHeight = tileHeight;
            requestPaint();
        }
    }

    public void setTileMargin(int tileMargin) {
        if (this.tileMargin != tileMargin) {
            this.tileMargin = tileMargin;
            requestPaint();
        }
    }

    public void setTileSpacing(int tileSpacing) {
        if (this.tileSpacing != tileSpacing) {
            this.tileSpacing = tileSpacing;
            requestPaint();
        }
    }

    public void setCollision(BufferedImage collision) {
        if (this.collision != collision) {
            if (this.collision == null && this.image == null) {
                this.resetView = true;
            }
            this.collision = collision;
            requestPaint();
        }
    }

    public void setHulls(float[] hullVertices, int[] hullIndices, int[] hullCounts, Color[] hullColors) {
        if (this.hullVertices == null || this.hullVertices.length != hullVertices.length || this.hullVertexBuffer == null) {
            this.hullVertexBuffer = BufferUtil.newFloatBuffer(hullVertices.length);
        }
        this.hullVertices = hullVertices;
        if (hullIndices != null) {
            if (this.hullIndices == null || this.hullIndices.length != hullIndices.length || this.hullFrameVertexBuffer == null) {
                this.hullFrameVertexBuffer = BufferUtil.newFloatBuffer(hullIndices.length * 6 * 4);
            } else {
                this.hullFrameVertexBuffer.limit(hullIndices.length * 6 * 4);
            }
        } else {
            this.hullFrameVertexBuffer = null;
        }
        this.hullIndices = hullIndices;
        this.hullCounts = hullCounts;
        this.hullColors = hullColors;
        requestPaint();
    }

    public String getBrushCollisionGroup() {
        return this.brushCollisionGroup;
    }

    public void setBrushCollisionGroup(String brushCollisionGroup, Color brushColor) {
        this.brushCollisionGroup = brushCollisionGroup;
        this.brushColor = brushColor;
        requestPaint();
    }

    private void centerCamera(float width, float height) {
        if (this.image != null || this.collision != null) {
            this.offset[0] = -width * 0.5f;
            this.offset[1] = -height * 0.5f;
        }
    }

    public void frameTileSet() {
        if (this.canvas != null && isEnabled()) {
            TileSetUtil.Metrics metrics = calculateMetrics();
            if (metrics != null) {
                Rectangle clientArea = this.canvas.getClientArea();
                // the metrics are based on the scale.. poor solution but works surprisingly well
                for (int i = 0; i < 5; ++i) {
                    this.scale = Math.min(clientArea.width / (metrics.visualWidth * 1.1f), clientArea.height / (metrics.visualHeight * 1.1f));
                    metrics = calculateMetrics();
                }
                centerCamera(metrics.visualWidth, metrics.visualHeight);
                requestPaint();
            }
        }
    }

    public void resetZoom() {
        if (this.canvas != null && isEnabled()) {
            this.scale = 1.0f;
            TileSetUtil.Metrics metrics = calculateMetrics();
            if (metrics != null) {
                centerCamera(metrics.visualWidth, metrics.visualHeight);
                requestPaint();
            }
        }
    }

    // Listener

    @Override
    public void handleEvent(Event event) {
        if (event.type == SWT.Resize) {
            Rectangle client = canvas.getClientArea();
            viewPort.put(0);
            viewPort.put(0);
            viewPort.put(client.width);
            viewPort.put(client.height);
            viewPort.flip();
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
        int activeTile = pickTile(e.x, e.y);
        switch (this.cameraMode) {
        case CAMERA_MODE_TRACK:
            float recipScale = 1.0f / this.scale;
            this.offset[0] += dx * recipScale;
            this.offset[1] -= dy * recipScale;
            activeTile = -1;
            requestPaint();
            break;
        case CAMERA_MODE_DOLLY:
            float ds = -dy * 0.005f;
            this.scale += (this.scale > 1.0f) ? ds * this.scale : ds;
            this.scale = Math.max(0.1f, this.scale);
            activeTile = -1;
            requestPaint();
            break;
        case CAMERA_MODE_NONE:
            if ((e.stateMask & SWT.BUTTON1) == SWT.BUTTON1 && this.hullCounts != null) {
                if (activeTile >= 0 && activeTile < this.hullCounts.length && this.hullCounts[activeTile] > 0) {
                    this.presenter.setConvexHullCollisionGroup(activeTile);
                }
            }
        }
        this.lastX = e.x;
        this.lastY = e.y;
        if (activeTile != this.activeTile) {
            this.activeTile = activeTile;
            requestPaint();
        }
    }

    private int pickTile(int x, int y) {
        if (isEnabled()) {
            TileSetUtil.Metrics metrics = calculateMetrics();
            if (metrics == null) {
                return -1;
            }
            int viewPortWidth = this.viewPort.get(2);
            int viewPortHeight = this.viewPort.get(3);
            float tileX = (x - 0.5f * viewPortWidth) / this.scale - this.offset[0];
            tileX /= metrics.visualWidth;
            float tileY = (y - 0.5f * viewPortHeight) / this.scale + this.offset[1];
            tileY /= metrics.visualHeight;
            tileY += 1.0f;
            if (tileX >= 0.0f && tileX < 1.0f && tileY >= 0.0f && tileY < 1.0f) {
                int column = (int)Math.floor(tileX * metrics.tilesPerRow);
                int row = (int)Math.floor(tileY * metrics.tilesPerColumn);
                return row * metrics.tilesPerRow + column;
            }
        }
        return -1;
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
                || (!this.mac && event.button == 2 && event.stateMask == SWT.ALT)) {
            this.cameraMode = CAMERA_MODE_TRACK;
            this.activeTile = -1;
            requestPaint();
        } else if ((this.mac && event.stateMask == (SWT.CTRL))
                || (!this.mac && event.button == 3 && event.stateMask == SWT.ALT)) {
            this.cameraMode = CAMERA_MODE_DOLLY;
            this.activeTile = -1;
            requestPaint();
        } else {
            this.cameraMode = CAMERA_MODE_NONE;
            if (event.button == 1 && this.hullCounts != null) {
                this.presenter.beginSetConvexHullCollisionGroup(this.brushCollisionGroup);
                if (this.activeTile >= 0 && this.activeTile < this.hullCounts.length && this.hullCounts[this.activeTile] > 0) {
                    this.presenter.setConvexHullCollisionGroup(this.activeTile);
                }
            }
        }
    }

    @Override
    public void mouseUp(MouseEvent e) {
        if (this.cameraMode != CAMERA_MODE_NONE) {
            this.cameraMode = CAMERA_MODE_NONE;
            int activeTile = pickTile(e.x, e.y);
            if (activeTile != this.activeTile) {
                this.activeTile = activeTile;
                requestPaint();
            }
        } else if (e.button == 1 && this.hullCounts != null) {
            this.presenter.endSetConvexHullCollisionGroup();
        }
    }

    public boolean isEnabled() {
        return this.enabled && (this.image != null || this.collision != null);
    }

    public void setEnabled(boolean enabled) {
        if (this.enabled != enabled) {
            this.enabled = enabled;
            if (enabled) {
                resetZoom();
            } else {
                requestPaint();
            }
        }
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
                if (this.resetView) {
                    this.resetView = false;
                    resetZoom();
                }

                gl.glDepthMask(true);
                gl.glEnable(GL.GL_DEPTH_TEST);
                gl.glClearColor(0.0f, 0.0f, 0.0f, 1);
                gl.glClearDepth(1.0);
                gl.glClear(GL.GL_COLOR_BUFFER_BIT | GL.GL_DEPTH_BUFFER_BIT);
                gl.glDisable(GL.GL_DEPTH_TEST);
                gl.glDepthMask(false);

                gl.glViewport(this.viewPort.get(0), this.viewPort.get(1), this.viewPort.get(2), this.viewPort.get(3));

                setupViewProj(gl, glu);

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

    private void setupViewProj(GL gl, GLU glu) {
        gl.glMatrixMode(GL.GL_PROJECTION);
        gl.glLoadIdentity();
        float recipScale = 1.0f / this.scale;
        float x = 0.5f * this.viewPort.get(2) * recipScale;
        float y = 0.5f * this.viewPort.get(3) * recipScale;
        glu.gluOrtho2D(-x, x, -y, y);

        gl.glMatrixMode(GL.GL_MODELVIEW);
        gl.glLoadIdentity();
        gl.glTranslatef(this.offset[0], this.offset[1], 0.0f);
    }

    private void drawTileSet(GL gl) {
        if (!isEnabled()) {
            return;
        }

        TileSetUtil.Metrics metrics = calculateMetrics();

        if (metrics == null) {
            return;
        }

        // background
        drawBackground(gl, metrics.visualWidth, metrics.visualHeight);

        // tiles
        drawTiles(gl, BORDER_SIZE, metrics.tilesPerRow, metrics.tilesPerColumn, metrics.visualWidth, metrics.visualHeight, metrics.tileSetWidth, metrics.tileSetHeight);
    }

    private void drawBackground(GL gl, float width, float height) {
        this.backgroundTexture.bind();
        this.backgroundTexture.setTexParameteri(GL.GL_TEXTURE_MIN_FILTER, GL.GL_NEAREST);
        this.backgroundTexture.setTexParameteri(GL.GL_TEXTURE_MAG_FILTER, GL.GL_NEAREST);
        this.backgroundTexture.setTexParameteri(GL.GL_TEXTURE_WRAP_S, GL.GL_REPEAT);
        this.backgroundTexture.setTexParameteri(GL.GL_TEXTURE_WRAP_T, GL.GL_REPEAT);
        this.backgroundTexture.enable();
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
        this.backgroundTexture.disable();
    }

    private void drawTiles(GL gl, float borderSize, int tilesPerRow, int tilesPerColumn, float width, float height, int imageWidth, int imageHeight) {
        int tileCount = tilesPerRow * tilesPerColumn;

        // Construct vertex data

        int vertexCount = 4 * tileCount;
        // tile vertex data is 2 uv + 3 pos
        int componentCount = 5 * vertexCount;
        if (this.tileVertexBuffer == null || this.tileVertexBuffer.capacity() != componentCount) {
            this.tileVertexBuffer = BufferUtil.newFloatBuffer(componentCount);
        }
        FloatBuffer v = this.tileVertexBuffer;
        FloatBuffer hv = this.hullFrameVertexBuffer;
        float recipImageWidth = 1.0f / imageWidth;
        float recipImageHeight = 1.0f / imageHeight;
        float recipScale = 1.0f / this.scale;
        float border = borderSize * recipScale;
        float z = 0.5f;
        float hz = 0.3f;
        float activeX0 = 0.0f;
        float activeX1 = 0.0f;
        float activeY0 = 0.0f;
        float activeY1 = 0.0f;
        float halfBorder = border / 3.0f;
        float[] hc = new float[3];
        for (int row = 0; row < tilesPerColumn; ++row) {
            for (int column = 0; column < tilesPerRow; ++column) {
                float x0 = column * (tileWidth + border) + border;
                float x1 = x0 + tileWidth;
                float y0 = (tilesPerColumn - row - 1) * (tileHeight + border) + border;
                float y1 = y0 + tileHeight;
                float u0 = (column * (tileSpacing + 2*tileMargin + tileWidth) + tileMargin) * recipImageWidth;
                float u1 = u0 + tileWidth * recipImageWidth;
                float v1 = (row * (tileSpacing + 2*tileMargin + tileHeight) + tileMargin) * recipImageHeight;
                float v0 = v1 + tileHeight * recipImageHeight;
                v.put(u0); v.put(v0); v.put(x0); v.put(y0); v.put(z);
                v.put(u0); v.put(v1); v.put(x0); v.put(y1); v.put(z);
                v.put(u1); v.put(v1); v.put(x1); v.put(y1); v.put(z);
                v.put(u1); v.put(v0); v.put(x1); v.put(y0); v.put(z);
                if (this.collision != null) {
                    int index = column + row * tilesPerRow;
                    int hullCount = this.hullCounts[index];
                    if (hullCount > 0) {
                        int hullIndex = this.hullIndices[index];
                        for (int i = 0; i < hullCount; ++i) {
                            int hi = 2 * (hullIndex + i);
                            this.hullVertexBuffer.put(x0 + 0.5f + this.hullVertices[hi+0]);
                            this.hullVertexBuffer.put(y0 + 0.5f + this.hullVertices[hi+1]);
                        }
                        this.hullColors[index].getColorComponents(hc);
                        float hx0 = x0 - halfBorder;
                        float hx1 = x1 + halfBorder;
                        float hy0 = y0 - halfBorder;
                        float hy1 = y1 + halfBorder;
                        hv.put(hc); hv.put(hx0); hv.put(hy0); hv.put(hz);
                        hv.put(hc); hv.put(hx0); hv.put(hy1); hv.put(hz);
                        hv.put(hc); hv.put(hx1); hv.put(hy1); hv.put(hz);
                        hv.put(hc); hv.put(hx1); hv.put(hy0); hv.put(hz);
                    }
                    if (index == this.activeTile) {
                        activeX0 = x0 - halfBorder;
                        activeX1 = x1 + halfBorder;
                        activeY0 = y0 - halfBorder;
                        activeY1 = y1 + halfBorder;
                    }
                }
            }
        }
        v.flip();

        // Tiles

        gl.glEnableClientState(GL.GL_TEXTURE_COORD_ARRAY);
        gl.glEnableClientState(GL.GL_VERTEX_ARRAY);

        gl.glEnable(GL.GL_DEPTH_TEST);
        gl.glDepthMask(true);
        gl.glEnable(GL.GL_BLEND);
        gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);
        Texture texture = this.texture.getTexture();
        if (texture != null) {
            texture.bind();
            texture.setTexParameteri(GL.GL_TEXTURE_MIN_FILTER, GL.GL_NEAREST);
            texture.setTexParameteri(GL.GL_TEXTURE_MAG_FILTER, GL.GL_NEAREST);
            texture.setTexParameteri(GL.GL_TEXTURE_WRAP_S, GL.GL_CLAMP);
            texture.setTexParameteri(GL.GL_TEXTURE_WRAP_T, GL.GL_CLAMP);
            texture.enable();
        } else {
            this.transparentTexture.bind();
            this.transparentTexture.enable();
        }

        gl.glInterleavedArrays(GL.GL_T2F_V3F, 0, v);

        gl.glDrawArrays(GL.GL_QUADS, 0, vertexCount);

        if (texture != null) {
            texture.disable();
        } else {
            this.transparentTexture.disable();
        }

        gl.glDisableClientState(GL.GL_TEXTURE_COORD_ARRAY);

        // Active tile

        z -= 0.1f;

        if (this.activeTile >= 0) {
            float f = 1.0f / 255.0f;
            gl.glBegin(GL.GL_QUADS);
            gl.glColor4f(brushColor.getRed() * f, brushColor.getGreen() * f, brushColor.getBlue() * f, brushColor.getAlpha() * f);
            gl.glVertex3f(activeX0, activeY0, z);
            gl.glVertex3f(activeX0, activeY1, z);
            gl.glVertex3f(activeX1, activeY1, z);
            gl.glVertex3f(activeX1, activeY0, z);
            gl.glEnd();
        }

        // Hull Frames

        if (this.collision != null) {
            hv.flip();

            gl.glEnableClientState(GL.GL_COLOR_ARRAY);

            gl.glInterleavedArrays(GL.GL_C3F_V3F, 0, hv);

            gl.glDrawArrays(GL.GL_QUADS, 0, hv.limit() / 6);

            gl.glDisableClientState(GL.GL_COLOR_ARRAY);
        }

        gl.glDepthMask(false);

        // Overlay

        z = 0.0f;
        gl.glBegin(GL.GL_QUADS);
        gl.glColor4f(0.2f, 0.2f, 0.2f, 0.7f);
        gl.glVertex3f(0.0f, 0.0f, z);
        gl.glVertex3f(0.0f, height, z);
        gl.glVertex3f(width, height, z);
        gl.glVertex3f(width, 0.0f, z);
        gl.glEnd();

        gl.glDisable(GL.GL_DEPTH_TEST);

        // Hulls

        if (this.collision != null) {
            this.hullVertexBuffer.flip();
            gl.glVertexPointer(2, GL.GL_FLOAT, 0, this.hullVertexBuffer);

            Color c = null;
            float f = 1.0f / 255.0f;
            for (int i = 0; i < this.hullCounts.length; ++i) {
                c = hullColors[i];
                gl.glColor4f(c.getRed() * f, c.getGreen() * f, c.getBlue() * f, c.getAlpha() * f);
                gl.glDrawArrays(GL.GL_LINE_LOOP, this.hullIndices[i], this.hullCounts[i]);
            }
        }

        // Clean up
        gl.glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        gl.glDisableClientState(GL.GL_VERTEX_ARRAY);
        gl.glDisable(GL.GL_BLEND);
    }

    private TileSetUtil.Metrics calculateMetrics() {
        return TileSetUtil.calculateMetrics(this.image, this.tileWidth, this.tileHeight, this.tileMargin, this.tileSpacing, this.collision, this.scale, BORDER_SIZE);
    }
}
