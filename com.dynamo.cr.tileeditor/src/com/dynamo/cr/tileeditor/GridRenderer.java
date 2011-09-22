package com.dynamo.cr.tileeditor;

import java.awt.image.BufferedImage;
import java.nio.FloatBuffer;
import java.nio.IntBuffer;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.inject.Inject;
import javax.media.opengl.GL;
import javax.media.opengl.GLContext;
import javax.media.opengl.GLDrawableFactory;
import javax.media.opengl.glu.GLU;
import javax.vecmath.Point2f;
import javax.vecmath.Point2i;
import javax.vecmath.Vector2f;

import org.eclipse.swt.SWT;
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

import com.dynamo.cr.tileeditor.core.IGridView;
import com.dynamo.cr.tileeditor.core.ILogger;
import com.dynamo.cr.tileeditor.core.Layer;
import com.sun.opengl.util.BufferUtil;
import com.sun.opengl.util.texture.Texture;
import com.sun.opengl.util.texture.TextureIO;

public class GridRenderer implements
IDisposable,
MouseListener,
MouseMoveListener,
Listener {

    private final static int CAMERA_MODE_NONE = 0;
    private final static int CAMERA_MODE_PAN = 1;
    private final static int CAMERA_MODE_ZOOM = 2;

    private final IGridView.Presenter presenter;
    private final ILogger logger;

    private GLCanvas canvas;
    private GLContext context;
    private final IntBuffer viewPort = BufferUtil.newIntBuffer(4);
    private boolean paintRequested = false;
    private final boolean mac = System.getProperty("os.name").equals("Mac OS X");
    private boolean enabled = true;
    private Point2i activeCell = null;

    // Model replication
    private List<Layer> layers;
    private float cellWidth;
    private float cellHeight;
    private BufferedImage tileSetImage;
    private int tileWidth;
    private int tileHeight;
    private int tileMargin;
    private int tileSpacing;
    private int brushTile = -1; // -1 is clear tile

    // Camera data
    private int cameraMode = CAMERA_MODE_NONE;
    private int lastX;
    private int lastY;
    private final Point2f position = new Point2f(0.0f, 0.0f);
    private float scale = 1.0f;

    // Render data
    private Texture tileSetTexture;
    private FloatBuffer cellVertexBuffer;
    private Texture backgroundTexture;

    @Inject
    public GridRenderer(IGridView.Presenter presenter, ILogger logger) {
        this.presenter = presenter;
        this.logger = logger;
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

        // if the image is already set, set the corresponding texture
        if (this.tileSetImage != null) {
            this.tileSetTexture = TextureIO.newTexture(tileSetImage, false);
        }

        this.context.release();

        this.canvas.addListener(SWT.Resize, this);
        this.canvas.addListener(SWT.Paint, this);
        this.canvas.addMouseListener(this);
        this.canvas.addMouseMoveListener(this);
    }

    @Override
    public void dispose() {
        if (this.context != null) {
            this.context.makeCurrent();
            this.backgroundTexture.dispose();
            if (this.tileSetTexture != null) {
                this.tileSetTexture.dispose();
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

    public Control getControl() {
        return this.canvas;
    }

    public void setLayers(List<Layer> layers) {
        this.layers = new ArrayList<Layer>(layers.size());
        for (Layer layer : layers) {
            this.layers.add(new Layer(layer));
        }
        requestPaint();
    }

    public void setCells(int layerIndex, Map<Long, Layer.Cell> cells) {
        this.layers.get(layerIndex).setCells(new HashMap<Long, Layer.Cell>(cells));
        requestPaint();
    }

    public void setCell(int layerIndex, long cellIndex, Layer.Cell cell) {
        Layer layer = this.layers.get(layerIndex);
        Layer.Cell oldCell = layer.getCell(cellIndex);
        if ((oldCell == null && cell != null) || (oldCell != null && !oldCell.equals(cell))) {
            layer.setCell(cellIndex, cell);
            requestPaint();
        }
    }

    public void setCellWidth(float cellWidth) {
        if (this.cellWidth != cellWidth) {
            this.cellWidth = cellWidth;
            requestPaint();
        }
    }

    public void setCellHeight(float cellHeight) {
        if (this.cellHeight != cellHeight) {
            this.cellHeight = cellHeight;
            requestPaint();
        }
    }

    public void setTileSet(BufferedImage tileSetImage, int tileWidth, int tileHeight, int tileMargin, int tileSpacing) {
        boolean repaint = false;
        if (this.tileSetImage != tileSetImage) {
            if (this.context != null) {
                this.context.makeCurrent();
                if (this.tileSetTexture != null) {
                    if (tileSetImage != null) {
                        this.tileSetTexture.updateImage(TextureIO.newTextureData(tileSetImage, false));
                    } else {
                        this.tileSetTexture.dispose();
                        this.tileSetTexture = null;
                    }
                } else {
                    this.tileSetTexture = TextureIO.newTexture(tileSetImage, false);
                }
                this.context.release();
            }
            this.tileSetImage = tileSetImage;
            repaint = true;
        }
        if (this.tileWidth != tileWidth) {
            this.tileWidth = tileWidth;
            repaint = true;
        }
        if (this.tileHeight != tileHeight) {
            this.tileHeight = tileHeight;
            repaint = true;
        }
        if (this.tileMargin != tileMargin) {
            this.tileMargin = tileMargin;
            repaint = true;
        }
        if (this.tileSpacing != tileSpacing) {
            this.tileSpacing = tileSpacing;
            repaint = true;
        }
        if (repaint) {
            requestPaint();
        }
    }

    public int getBrushTile() {
        return this.brushTile;
    }

    public void setBrushTile(int brushTile) {
        if (this.brushTile != brushTile) {
            this.brushTile = brushTile;
            requestPaint();
        }
    }

    public Rectangle getViewRect() {
        return new Rectangle(0, 0, this.viewPort.get(2), this.viewPort.get(3));
    }

    public void setCamera(Point2f position, float zoom) {
        boolean repaint = false;
        if (!this.position.equals(position)) {
            this.position.set(position);
            repaint = true;
        }
        if (this.scale != zoom) {
            this.scale = zoom;
            repaint = true;
        }
        if (repaint) {
            requestPaint();
        }
    }

    // Listener

    @Override
    public void handleEvent(Event event) {
        if (event.type == SWT.Resize) {
            Rectangle client = this.canvas.getClientArea();
            this.viewPort.put(0);
            this.viewPort.put(0);
            this.viewPort.put(client.width);
            this.viewPort.put(client.height);
            this.viewPort.flip();
        } else if (event.type == SWT.Paint) {
            requestPaint();
        }
    }

    // MouseMoveListener

    @Override
    public void mouseMove(MouseEvent e) {
        int dx = e.x - this.lastX;
        int dy = e.y - this.lastY;
        Point2i activeCell = pickCell(e.x, e.y);
        switch (this.cameraMode) {
        case CAMERA_MODE_PAN:
            activeCell = null;
            this.presenter.onPreviewPan(dx, dy);
            break;
        case CAMERA_MODE_ZOOM:
            activeCell = null;
            this.presenter.onPreviewZoom(dy);
            break;
        case CAMERA_MODE_NONE:
            if ((e.stateMask & SWT.BUTTON1) == SWT.BUTTON1) {
                if (activeCell != null) {
                    this.presenter.onPaint(activeCell.getX(), activeCell.getY());
                }
            }
        }
        this.lastX = e.x;
        this.lastY = e.y;
        if ((this.activeCell == null && activeCell != null) || (this.activeCell != null && !this.activeCell.equals(activeCell))) {
            this.activeCell = activeCell;
            requestPaint();
        }
    }

    private Point2i pickCell(int x, int y) {
        if (isEnabled()) {
            int viewPortWidth = this.viewPort.get(2);
            int viewPortHeight = this.viewPort.get(3);
            float cellX = (x - 0.5f * viewPortWidth) / this.scale - this.position.getX();
            cellX /= this.cellWidth;
            float cellY = (y - 0.5f * viewPortHeight) / this.scale + this.position.getY();
            cellY /= this.cellHeight;
            cellY += 1.0f;
            return new Point2i((int)Math.floor(cellX), (int)Math.floor(cellY));
        }
        return null;
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
            this.cameraMode = CAMERA_MODE_PAN;
            this.activeCell = null;
        } else if ((this.mac && event.stateMask == (SWT.CTRL))
                || (!this.mac && event.button == 3 && event.stateMask == SWT.ALT)) {
            this.cameraMode = CAMERA_MODE_ZOOM;
            this.activeCell = null;
        } else {
            this.cameraMode = CAMERA_MODE_NONE;
            if (event.button == 1) {
                this.presenter.onPaintBegin();
                if (this.activeCell != null) {
                    this.presenter.onPaint(this.activeCell.getX(), this.activeCell.getY());
                }
            }
        }
    }

    @Override
    public void mouseUp(MouseEvent e) {
        if (this.cameraMode != CAMERA_MODE_NONE) {
            this.cameraMode = CAMERA_MODE_NONE;
            Point2i activeCell = pickCell(e.x, e.y);
            if ((this.activeCell == null && activeCell != null) || (this.activeCell != null && !this.activeCell.equals(activeCell))) {
                this.activeCell = activeCell;
                requestPaint();
            }
        } else if (e.button == 1) {
            this.presenter.onPaintEnd();
        }
    }

    public boolean isEnabled() {
        //return this.enabled && this.tileSetImage != null && this.cellWidth != 0.0f && this.cellHeight != 0.0f;
        return true;
    }

    public void setEnabled(boolean enabled) {
        this.enabled = enabled;
        requestPaint();
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

                gl.glViewport(this.viewPort.get(0), this.viewPort.get(1), this.viewPort.get(2), this.viewPort.get(3));

                setupViewProj(gl, glu);

                render(gl, glu);

            } catch (Throwable e) {
                logger.logException(e);
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
        gl.glTranslatef(this.position.getX(), this.position.getY(), 0.0f);
    }

    private void render(GL gl, GLU glu) {
        if (!isEnabled()) {
            return;
        }

        // tiles
        renderCells(gl);

        // grid (cell-dividing lines)
        renderGrid(gl);

        renderTileSet(gl, glu);
    }

    private void renderCells(GL gl) {
        /*        int tileCount = tilesPerRow * tilesPerColumn;

        // Construct vertex data

        int vertexCount = 4 * tileCount;
        // tile vertex data is 2 uv + 3 pos
        int componentCount = 5 * vertexCount;
        if (this.cellVertexBuffer == null || this.cellVertexBuffer.capacity() != componentCount) {
            this.cellVertexBuffer = BufferUtil.newFloatBuffer(componentCount);
        }
        FloatBuffer v = this.cellVertexBuffer;
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
                    if (index == this.activeCell) {
                        activeX0 = x0 - halfBorder;
                        activeX1 = x1 + halfBorder;
                        activeY0 = y0 - halfBorder;
                        activeY1 = y1 + halfBorder;
                    }
                }
            }
        }
        v.flip();
        hv.flip();

        // Tiles

        gl.glEnableClientState(GL.GL_TEXTURE_COORD_ARRAY);
        gl.glEnableClientState(GL.GL_VERTEX_ARRAY);

        gl.glEnable(GL.GL_DEPTH_TEST);
        gl.glDepthMask(true);
        gl.glEnable(GL.GL_BLEND);
        gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);
        if (this.tileSetTexture != null) {
            this.tileSetTexture.bind();
            this.tileSetTexture.setTexParameteri(GL.GL_TEXTURE_MIN_FILTER, GL.GL_NEAREST);
            this.tileSetTexture.setTexParameteri(GL.GL_TEXTURE_MAG_FILTER, GL.GL_NEAREST);
            this.tileSetTexture.setTexParameteri(GL.GL_TEXTURE_WRAP_S, GL.GL_CLAMP);
            this.tileSetTexture.setTexParameteri(GL.GL_TEXTURE_WRAP_T, GL.GL_CLAMP);
            this.tileSetTexture.enable();
        } else {
            this.transparentTexture.bind();
            this.transparentTexture.enable();
        }

        gl.glInterleavedArrays(GL.GL_T2F_V3F, 0, v);

        gl.glDrawArrays(GL.GL_QUADS, 0, vertexCount);

        if (this.tileSetTexture != null) {
            this.tileSetTexture.disable();
        } else {
            this.transparentTexture.disable();
        }

        gl.glDisableClientState(GL.GL_TEXTURE_COORD_ARRAY);

        // Active tile

        z -= 0.1f;

        if (this.activeCell >= 0) {
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

        gl.glEnableClientState(GL.GL_COLOR_ARRAY);

        gl.glInterleavedArrays(GL.GL_C3F_V3F, 0, hv);

        gl.glDrawArrays(GL.GL_QUADS, 0, hv.limit() / 6);

        gl.glDisableClientState(GL.GL_COLOR_ARRAY);

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
        gl.glDisable(GL.GL_BLEND);*/
    }

    private void renderGrid(GL gl) {
        if (this.cellWidth <= 0.0f || this.cellHeight <= 0.0f) {
            return;
        }

        Vector2f offset = new Vector2f(this.position);
        offset.negate();
        Vector2f extent = new Vector2f(this.viewPort.get(2), this.viewPort.get(3));
        extent.scale(0.5f / this.scale);
        Point2f min = new Point2f(offset);
        min.sub(extent);
        Point2f minNorm = new Point2f(min);
        minNorm.scale(1.0f / this.cellWidth);
        Point2f max = new Point2f(offset);
        max.add(extent);
        Point2f maxNorm = new Point2f(max);
        maxNorm.scale(1.0f / this.cellHeight);

        float byteRecip = 1.0f / 255.0f;
        float[] redColor = new float[] {1.0f, 0.0f, 0.0f};
        float[] greenColor = new float[] {0.0f, 1.0f, 0.0f};
        float[] gridColor = new float[] {114.0f * byteRecip, 123.0f * byteRecip, 130.0f * byteRecip};

        int xMin = (int)Math.ceil(minNorm.getX());
        int xMax = (int)Math.ceil(maxNorm.getX());
        int yMin = (int)Math.ceil(minNorm.getY());
        int yMax = (int)Math.ceil(maxNorm.getY());

        final int vertexCount = 2 * ((xMax - xMin) + (yMax - yMin));
        if (vertexCount == 0) {
            return;
        }
        final int componentCount = 6;
        FloatBuffer v = BufferUtil.newFloatBuffer(vertexCount * componentCount);
        for (int i = xMin; i < xMax; ++i) {
            float[] color = gridColor;
            if (i == 0) {
                color = greenColor;
            }
            float x = i * this.cellWidth;
            v.put(color); v.put(x); v.put(min.getY()); v.put(0.0f);
            v.put(color); v.put(x); v.put(max.getY()); v.put(0.0f);
        }

        for (int i = yMin; i < yMax; ++i) {
            float[] color = gridColor;
            if (i == 0) {
                color = redColor;
            }
            float y = i * this.cellHeight;
            v.put(color); v.put(min.getX()); v.put(y); v.put(0.0f);
            v.put(color); v.put(max.getX()); v.put(y); v.put(0.0f);
        }
        v.flip();

        gl.glEnableClientState(GL.GL_VERTEX_ARRAY);
        gl.glEnableClientState(GL.GL_COLOR_ARRAY);

        gl.glInterleavedArrays(GL.GL_C3F_V3F, 0, v);

        gl.glDrawArrays(GL.GL_LINES, 0, vertexCount);

        gl.glDisableClientState(GL.GL_VERTEX_ARRAY);
        gl.glDisableClientState(GL.GL_COLOR_ARRAY);
    }

    private void renderTileSet(GL gl, GLU glu) {
        if (this.tileSetTexture == null) {
            return;
        }

        setupTileSetViewProj(gl, glu);

        gl.glDisable(GL.GL_DEPTH);

        float screenRatio = 0.28f;
        float extent = Math.max(this.viewPort.get(2) * screenRatio, this.viewPort.get(3) * screenRatio);

        this.tileSetTexture.bind();
        this.tileSetTexture.enable();

        gl.glColor3f(1.0f, 1.0f, 1.0f);
        gl.glBegin(GL.GL_QUADS);
        gl.glTexCoord2f(0.0f, 0.0f);
        gl.glVertex2f(0.0f, 0.0f);
        gl.glTexCoord2f(1.0f, 0.0f);
        gl.glVertex2f(extent, 0.0f);
        gl.glTexCoord2f(1.0f, 1.0f);
        gl.glVertex2f(extent, extent);
        gl.glTexCoord2f(0.0f, 1.0f);
        gl.glVertex2f(0.0f, extent);
        gl.glEnd();

        this.tileSetTexture.disable();
    }

    private void setupTileSetViewProj(GL gl, GLU glu) {
        gl.glMatrixMode(GL.GL_PROJECTION);
        gl.glLoadIdentity();
        glu.gluOrtho2D(0, this.viewPort.get(2), this.viewPort.get(3), 0);

        gl.glMatrixMode(GL.GL_MODELVIEW);
        gl.glLoadIdentity();
    }

}
