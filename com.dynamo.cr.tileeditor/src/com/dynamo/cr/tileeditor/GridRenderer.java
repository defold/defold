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
import javax.vecmath.Vector3f;
import javax.vecmath.Vector4f;

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
import com.dynamo.cr.tileeditor.core.Layer.Cell;
import com.dynamo.cr.tileeditor.core.TileSetUtil;
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
    private final static float BORDER_SIZE = 1.0f;

    private final IGridView.Presenter presenter;
    private final ILogger logger;

    private GLCanvas canvas;
    private GLContext context;
    private final IntBuffer viewPort = BufferUtil.newIntBuffer(4);
    private boolean paintRequested = false;
    private final boolean mac = System.getProperty("os.name").equals("Mac OS X");
    private boolean enabled = true;
    private Point2i activeCell = null;
    private int activeTile = -1;
    private boolean showPalette;

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
    private boolean hFlip = false;
    private boolean vFlip = false;

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

    public void showPalette(boolean show) {
        this.showPalette = show;
        requestPaint();
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

    public void setBrush(int brushTile, boolean hFlip, boolean vFlip) {
        boolean repaint = false;
        if (this.brushTile != brushTile) {
            this.brushTile = brushTile;
            repaint = true;
        }
        if (this.hFlip != hFlip) {
            this.hFlip = hFlip;
            repaint = true;
        }
        if (this.vFlip != vFlip) {
            this.vFlip = vFlip;
            repaint = true;
        }
        if (repaint) {
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
        if (this.showPalette) {
            int activeTile = pickTile(e.x, e.y);
            if (this.activeTile != activeTile) {
                this.activeTile = activeTile;
                requestPaint();
            }
        } else {
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
            if ((this.activeCell != activeCell) && (this.activeCell == null || activeCell == null || !this.activeCell.equals(activeCell))) {
                this.activeCell = activeCell;
                requestPaint();
            }
        }
    }

    private int pickTile(int x, int y) {
        if (isEnabled()) {
            TileSetUtil.Metrics metrics = calculateMetrics();
            if (metrics != null) {
                int viewPortWidth = this.viewPort.get(2);
                int viewPortHeight = this.viewPort.get(3);
                int tileX = (int)Math.floor((x - 0.5f * (viewPortWidth - metrics.visualWidth) - BORDER_SIZE)/(this.tileWidth + BORDER_SIZE));
                int tileY = (int)Math.floor((y - 0.5f * (viewPortHeight - metrics.visualHeight) - BORDER_SIZE)/(this.tileHeight + BORDER_SIZE));
                if (tileX >= 0
                        && tileX < metrics.tilesPerRow
                        && tileY >= 0
                        && tileY < metrics.tilesPerColumn) {
                    return tileX + tileY * metrics.tilesPerRow;
                }
            }
        }
        return -1;
    }

    private Point2i pickCell(int x, int y) {
        if (isEnabled()) {
            int viewPortWidth = this.viewPort.get(2);
            int viewPortHeight = this.viewPort.get(3);
            float cellX = (x - 0.5f * viewPortWidth) / this.scale - this.position.getX();
            cellX /= this.cellWidth;
            float cellY = (0.5f * viewPortHeight - y) / this.scale - this.position.getY();
            cellY /= this.cellHeight;
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

        if (this.showPalette) {
            if (event.button == 1 && this.activeTile != -1) {
                this.presenter.onSelectTile(this.activeTile, false, false);
            }
        } else {
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
        return this.enabled && this.cellWidth > 0.0f && this.cellHeight > 0.0f;
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

        // tile set palette
        renderTileSet(gl, glu);
    }

    private void renderCells(GL gl) {
        TileSetUtil.Metrics metrics = calculateMetrics();
        if (metrics == null) {
            return;
        }

        float recipImageWidth = 1.0f / metrics.tileSetWidth;
        float recipImageHeight = 1.0f / metrics.tileSetHeight;

        this.tileSetTexture.bind();
        this.tileSetTexture.setTexParameteri(GL.GL_TEXTURE_MIN_FILTER, GL.GL_NEAREST);
        this.tileSetTexture.setTexParameteri(GL.GL_TEXTURE_MAG_FILTER, GL.GL_NEAREST);
        this.tileSetTexture.setTexParameteri(GL.GL_TEXTURE_WRAP_S, GL.GL_CLAMP);
        this.tileSetTexture.setTexParameteri(GL.GL_TEXTURE_WRAP_T, GL.GL_CLAMP);
        this.tileSetTexture.enable();

        gl.glColor3f(1.0f, 1.0f, 1.0f);

        if (this.activeCell != null && this.brushTile != -1) {
            int x = this.activeCell.x;
            int y = this.activeCell.y;
            float x0 = x * cellWidth;
            float x1 = x0 + cellWidth;
            float y0 = y * cellHeight;
            float y1 = y0 + cellHeight;
            x = this.brushTile % metrics.tilesPerRow;
            y = this.brushTile / metrics.tilesPerRow;
            float u0 = (x * (tileSpacing + 2*tileMargin + tileWidth) + tileMargin) * recipImageWidth;
            float u1 = u0 + tileWidth * recipImageWidth;
            float v0 = (y * (tileSpacing + 2*tileMargin + tileHeight) + tileMargin) * recipImageHeight;
            float v1 = v0 + tileHeight * recipImageHeight;

            gl.glBegin(GL.GL_QUADS);
            gl.glTexCoord2f(u0, v1);
            gl.glVertex2f(x0, y0);
            gl.glTexCoord2f(u0, v0);
            gl.glVertex2f(x0, y1);
            gl.glTexCoord2f(u1, v0);
            gl.glVertex2f(x1, y1);
            gl.glTexCoord2f(u1, v1);
            gl.glVertex2f(x1, y0);
            gl.glEnd();
        }

        Point2f min = new Point2f();
        Point2f max = new Point2f();
        Point2i cellMin = new Point2i();
        Point2i cellMax = new Point2i();
        calculateCellBounds(min, max, cellMin, cellMax);

        float z = 0.0f;

        Map<Long, Cell> cells = new HashMap<Long, Cell>();
        for (Layer layer : this.layers) {
            cells.clear();
            for (Map.Entry<Long, Cell> entry : layer.getCells().entrySet()) {
                if (entry.getValue() != null) {
                    int x = Layer.toCellX(entry.getKey());
                    int y = Layer.toCellY(entry.getKey());
                    if ((x + 1) >= cellMin.getX() && x <= cellMax.getX() && (y + 1) >= cellMin.getY() && y <= cellMax.getY()) {
                        cells.put(entry.getKey(), entry.getValue());
                    }
                }
            }
            int n = cells.size();

            if (n > 0) {
                int vertexCount = n * 4;
                int componentCount = 5;
                FloatBuffer v = BufferUtil.newFloatBuffer(vertexCount * componentCount);
                for (Map.Entry<Long, Cell> entry : cells.entrySet()) {
                    int x = Layer.toCellX(entry.getKey());
                    int y = Layer.toCellY(entry.getKey());
                    float x0 = x * this.cellWidth;
                    float x1 = x0 + this.cellWidth;
                    float y0 = y * this.cellHeight;
                    float y1 = y0 + this.cellHeight;
                    int tile = entry.getValue().getTile();
                    x = tile % metrics.tilesPerRow;
                    y = tile / metrics.tilesPerRow;
                    float u0 = (x * (tileSpacing + 2*tileMargin + tileWidth) + tileMargin) * recipImageWidth;
                    float u1 = u0 + tileWidth * recipImageWidth;
                    float v0 = (y * (tileSpacing + 2*tileMargin + tileHeight) + tileMargin) * recipImageHeight;
                    float v1 = v0 + tileHeight * recipImageHeight;

                    v.put(u0); v.put(v1); v.put(x0); v.put(y0); v.put(z);
                    v.put(u0); v.put(v0); v.put(x0); v.put(y1); v.put(z);
                    v.put(u1); v.put(v0); v.put(x1); v.put(y1); v.put(z);
                    v.put(u1); v.put(v1); v.put(x1); v.put(y0); v.put(z);
                }
                v.flip();

                gl.glEnableClientState(GL.GL_VERTEX_ARRAY);
                gl.glEnableClientState(GL.GL_TEXTURE_COORD_ARRAY);

                gl.glInterleavedArrays(GL.GL_T2F_V3F, 0, v);

                gl.glDrawArrays(GL.GL_QUADS, 0, vertexCount);

                gl.glDisableClientState(GL.GL_VERTEX_ARRAY);
                gl.glDisableClientState(GL.GL_TEXTURE_COORD_ARRAY);
            }

        }

        this.tileSetTexture.disable();
    }

    private void renderGrid(GL gl) {
        Point2f min = new Point2f();
        Point2f max = new Point2f();
        Point2i cellMin = new Point2i();
        Point2i cellMax = new Point2i();
        calculateCellBounds(min, max, cellMin, cellMax);

        float byteRecip = 1.0f / 255.0f;
        float[] redColor = new float[] {1.0f, 0.0f, 0.0f};
        float[] greenColor = new float[] {0.0f, 1.0f, 0.0f};
        float[] gridColor = new float[] {114.0f * byteRecip, 123.0f * byteRecip, 130.0f * byteRecip};

        final int vertexCount = 2 * ((cellMax.getX() - cellMin.getX()) + (cellMax.getY() - cellMin.getY()));
        if (vertexCount == 0) {
            return;
        }
        final float z = 0.0f;
        final int componentCount = 6;
        FloatBuffer v = BufferUtil.newFloatBuffer(vertexCount * componentCount);
        for (int i = cellMin.getX(); i < cellMax.getX(); ++i) {
            float[] color = gridColor;
            if (i == 0) {
                color = greenColor;
            }
            float x = i * this.cellWidth;
            v.put(color); v.put(x); v.put(min.getY()); v.put(z);
            v.put(color); v.put(x); v.put(max.getY()); v.put(z);
        }

        for (int i = cellMin.getY(); i < cellMax.getY(); ++i) {
            float[] color = gridColor;
            if (i == 0) {
                color = redColor;
            }
            float y = i * this.cellHeight;
            v.put(color); v.put(min.getX()); v.put(y); v.put(z);
            v.put(color); v.put(max.getX()); v.put(y); v.put(z);
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
        if (this.tileSetTexture == null || !this.showPalette) {
            return;
        }

        TileSetUtil.Metrics metrics = calculateMetrics();

        if (metrics == null) {
            return;
        }

        float viewPortWidth = this.viewPort.get(2);
        float viewPortHeight = this.viewPort.get(3);

        gl.glMatrixMode(GL.GL_PROJECTION);
        gl.glLoadIdentity();
        glu.gluOrtho2D(0, viewPortWidth, viewPortHeight, 0);

        gl.glMatrixMode(GL.GL_MODELVIEW);
        gl.glLoadIdentity();

        // Render dim

        gl.glEnable(GL.GL_BLEND);
        gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);

        gl.glColor4f(0.0f, 0.0f, 0.0f, 0.7f);

        gl.glBegin(GL.GL_QUADS);
        gl.glVertex2f(0.0f, 0.0f);
        gl.glVertex2f(viewPortWidth, 0.0f);
        gl.glVertex2f(viewPortWidth, viewPortHeight);
        gl.glVertex2f(0.0f, viewPortHeight);
        gl.glEnd();

        gl.glDisable(GL.GL_BLEND);

        // Build tile set data

        Vector3f offset = new Vector3f((float)Math.floor(0.5f * (viewPortWidth - metrics.visualWidth)), (float)Math.floor(0.5f * (viewPortHeight - metrics.visualHeight)), 0.0f);
        gl.glTranslatef(offset.x, offset.y, offset.z);

        final int vertexCount = metrics.tilesPerRow * metrics.tilesPerColumn * 4;
        final int componentCount = 5;
        FloatBuffer v = BufferUtil.newFloatBuffer(vertexCount * componentCount);

        float z = 0.9f;

        float recipImageWidth = 1.0f / metrics.tileSetWidth;
        float recipImageHeight = 1.0f / metrics.tileSetHeight;

        Vector4f[] overlays = new Vector4f[3];
        float[] overlayColors = new float[3];
        overlays[0] = new Vector4f(0.0f, 0.0f, metrics.visualWidth, metrics.visualHeight);
        overlayColors[0] = 0.4f;
        overlayColors[1] = 1.0f;
        overlayColors[2] = 1.0f;

        for (int y = 0; y < metrics.tilesPerColumn; ++y) {
            for (int x = 0; x < metrics.tilesPerRow; ++x) {
                float x0 = x * (tileWidth + BORDER_SIZE) + BORDER_SIZE;
                float x1 = x0 + tileWidth;
                float y0 = y * (tileHeight + BORDER_SIZE) + BORDER_SIZE;
                float y1 = y0 + tileHeight;
                float u0 = (x * (tileSpacing + 2*tileMargin + tileWidth) + tileMargin) * recipImageWidth;
                float u1 = u0 + tileWidth * recipImageWidth;
                float v0 = (y * (tileSpacing + 2*tileMargin + tileHeight) + tileMargin) * recipImageHeight;
                float v1 = v0 + tileHeight * recipImageHeight;
                v.put(u0); v.put(v0); v.put(x0); v.put(y0); v.put(z);
                v.put(u0); v.put(v1); v.put(x0); v.put(y1); v.put(z);
                v.put(u1); v.put(v1); v.put(x1); v.put(y1); v.put(z);
                v.put(u1); v.put(v0); v.put(x1); v.put(y0); v.put(z);
                int tileIndex = x + y * metrics.tilesPerRow;
                if (this.activeTile == tileIndex) {
                    overlays[1] = new Vector4f(x0 - BORDER_SIZE, y0 - BORDER_SIZE, x1 + BORDER_SIZE, y1 + BORDER_SIZE);
                }
                if (this.brushTile == tileIndex) {
                    overlays[2] = new Vector4f(x0 - BORDER_SIZE, y0 - BORDER_SIZE, x1 + BORDER_SIZE, y1 + BORDER_SIZE);
                }
            }
        }
        v.flip();

        this.tileSetTexture.bind();
        this.tileSetTexture.setTexParameteri(GL.GL_TEXTURE_MIN_FILTER, GL.GL_NEAREST);
        this.tileSetTexture.setTexParameteri(GL.GL_TEXTURE_MAG_FILTER, GL.GL_NEAREST);
        this.tileSetTexture.setTexParameteri(GL.GL_TEXTURE_WRAP_S, GL.GL_CLAMP);
        this.tileSetTexture.setTexParameteri(GL.GL_TEXTURE_WRAP_T, GL.GL_CLAMP);
        this.tileSetTexture.enable();

        gl.glEnable(GL.GL_DEPTH_TEST);
        gl.glDepthMask(true);
        gl.glClear(GL.GL_DEPTH_BUFFER_BIT);

        gl.glEnableClientState(GL.GL_TEXTURE_COORD_ARRAY);
        gl.glEnableClientState(GL.GL_VERTEX_ARRAY);
        gl.glColor3f(1.0f, 1.0f, 1.0f);

        gl.glInterleavedArrays(GL.GL_T2F_V3F, 0, v);

        gl.glDrawArrays(GL.GL_QUADS, 0, vertexCount);

        gl.glDisableClientState(GL.GL_TEXTURE_COORD_ARRAY);
        gl.glDisableClientState(GL.GL_VERTEX_ARRAY);

        this.tileSetTexture.disable();

        // tile set overlays

        gl.glDepthMask(false);

        z = 0.0f;

        gl.glBegin(GL.GL_QUADS);

        for (int i = 0; i < overlays.length; ++i) {
            Vector4f o = overlays[i];
            if (o != null) {
                gl.glColor3f(overlayColors[i], overlayColors[i], overlayColors[i]);
                float x0 = o.getX();
                float y0 = o.getY();
                float x1 = o.getZ();
                float y1 = o.getW();
                gl.glVertex3f(x0, y0, z);
                gl.glVertex3f(x1, y0, z);
                gl.glVertex3f(x1, y1, z);
                gl.glVertex3f(x0, y1, z);
            }
        }

        gl.glEnd();

        gl.glColor3f(1.0f, 1.0f, 1.0f);
        gl.glDisable(GL.GL_DEPTH_TEST);

    }

    private TileSetUtil.Metrics calculateMetrics() {
        return TileSetUtil.calculateMetrics(this.tileSetImage, this.tileWidth, this.tileHeight, this.tileMargin, this.tileSpacing, null, 1.0f, 1.0f);
    }

    private void calculateCellBounds(Point2f outMin, Point2f outMax, Point2i outCellMin, Point2i outCellMax) {
        Vector2f offset = new Vector2f(this.position);
        offset.negate();
        Vector2f extent = new Vector2f(this.viewPort.get(2), this.viewPort.get(3));
        extent.scale(0.5f / this.scale);
        outMin.set(offset);
        outMin.sub(extent);
        Point2f minNorm = new Point2f(outMin);
        minNorm.scale(1.0f / this.cellWidth);
        outMax.set(offset);
        outMax.add(extent);
        Point2f maxNorm = new Point2f(outMax);
        maxNorm.scale(1.0f / this.cellHeight);

        outCellMin.set((int)Math.ceil(minNorm.getX()), (int)Math.ceil(minNorm.getY()));
        outCellMax.set((int)Math.ceil(maxNorm.getX()), (int)Math.ceil(maxNorm.getY()));

    }
}
