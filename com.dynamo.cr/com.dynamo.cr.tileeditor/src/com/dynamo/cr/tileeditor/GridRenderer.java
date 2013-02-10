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
import javax.vecmath.Matrix4d;
import javax.vecmath.Point2f;
import javax.vecmath.Point2i;
import javax.vecmath.Vector2f;

import org.eclipse.swt.SWT;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseListener;
import org.eclipse.swt.events.MouseMoveListener;
import org.eclipse.swt.events.MouseTrackAdapter;
import org.eclipse.swt.events.MouseWheelListener;
import org.eclipse.swt.graphics.Cursor;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.graphics.Rectangle;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.opengl.GLCanvas;
import org.eclipse.swt.opengl.GLData;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Listener;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.handlers.IHandlerService;
import org.eclipse.ui.services.IDisposable;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.bob.tile.TileSetUtil;
import com.dynamo.cr.sceneed.core.SceneUtil;
import com.dynamo.cr.sceneed.core.SceneUtil.MouseType;
import com.dynamo.cr.sceneed.core.util.CameraUtil;
import com.dynamo.cr.sceneed.core.util.CameraUtil.Movement;
import com.dynamo.cr.sceneed.ui.RenderUtil;
import com.dynamo.cr.tileeditor.core.IGridView;
import com.dynamo.cr.tileeditor.core.Layer;
import com.dynamo.cr.tileeditor.core.Layer.Cell;
import com.dynamo.cr.tileeditor.core.MapBrush;
import com.sun.opengl.util.BufferUtil;
import com.sun.opengl.util.texture.Texture;
import com.sun.opengl.util.texture.TextureIO;

public class GridRenderer implements
IDisposable,
MouseListener,
MouseMoveListener,
MouseWheelListener,
Listener {

    private final static float BORDER_SIZE = 1.0f;

    private static Logger logger = LoggerFactory.getLogger(GridRenderer.class);
    private final IGridView.Presenter presenter;
    private final GridEditor gridEditor;

    private GLCanvas canvas;
    private GLContext context;
    private final IntBuffer viewPort = BufferUtil.newIntBuffer(4);
    private boolean paintRequested = false;
    private boolean enabled = true;
    private Point2i activeCell = null;
    private Point2i startActiveCell = null;
    private int activeTile = -1;
    private boolean showPalette;
    private boolean visibleGrid = false;
    private Cursor defaultCursor;

    // Model replication
    private List<Layer> layers;
    private Layer selectedLayer;
    private BufferedImage tileSetImage;
    private int tileWidth;
    private int tileHeight;
    private int tileMargin;
    private int tileSpacing;
    private MapBrush brush = new MapBrush();

    // Camera data
    private Movement cameraMovement = Movement.IDLE;
    private int lastX;
    private int lastY;
    private final Point2f position = new Point2f(0.0f, 0.0f);
    private float scale = 1.0f;

    // Render data
    private Texture tileSetTexture;
    private Texture backgroundTexture;

    @Inject
    public GridRenderer(IGridView.Presenter presenter, GridEditor gridEditor) {
        this.presenter = presenter;
        this.gridEditor = gridEditor;
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
        this.canvas.addListener(SWT.MouseExit, this);
        this.canvas.addListener(SWT.KeyDown, this);
        this.canvas.addListener(SWT.KeyUp, this);
        this.canvas.addMouseListener(this);
        this.canvas.addMouseMoveListener(this);
        this.canvas.addMouseWheelListener(this);
        this.canvas.addMouseTrackListener(new MouseTrackAdapter() {
            @Override
            public void mouseEnter(MouseEvent e) {
                activeCell = pickCell(e.x, e.y);
                lastX = e.x;
                lastY = e.y;
            }
        });

        this.defaultCursor = this.canvas.getCursor();
        updateCursor();
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

    public boolean isShowingPalette() {
        return this.showPalette;
    }

    public void showPalette(boolean show) {
        if (this.showPalette != show) {
            this.showPalette = show;
            Display display = this.canvas.getDisplay();
            Control cursor = display.getCursorControl();
            if (cursor != null) {
                Point p = this.canvas.getDisplay().getCursorLocation();
                p = display.map(null, this.canvas, p);
                if (show) {
                    this.activeCell = null;
                    this.activeTile = pickTile(p.x, p.y);
                } else {
                    this.activeTile = -1;
                    this.activeCell = pickCell(p.x, p.y);
                }
            }
            requestPaint();
            updateCursor();
        }
    }

    public void setLayers(List<Layer> layers) {
        this.layers = new ArrayList<Layer>(layers.size());
        for (Layer layer : layers) {
            this.layers.add(new Layer(layer));
        }
        requestPaint();
    }

    public void setSelectedLayer(Layer layer) {
        this.selectedLayer = layer;
        requestPaint();
        updateCursor();
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

    public void setTileSet(BufferedImage tileSetImage, int tileWidth, int tileHeight, int tileMargin, int tileSpacing) {
        boolean repaint = false;
        if (this.tileSetImage != tileSetImage) {
            if (this.context != null && this.canvas != null) {
                this.canvas.setCurrent();
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

    public void setBrush(MapBrush brush) {
        if (!this.brush.equals(brush)) {
            this.brush = brush;
            requestPaint();
            updateCursor();
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
            if (this.activeCell != null) {
                activeCell = pickCell(this.lastX, this.lastY);
            }
            requestPaint();
        }
    }

    public void setVisibleGrid(boolean visibleGrid) {
        this.visibleGrid = visibleGrid;
        requestPaint();
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
        } else if (event.type == SWT.MouseExit) {
            if (this.activeCell != null) {
                this.activeCell = null;
                requestPaint();
            }
        } else {
            // Update cursor when shift is pressed and mouse type is one-button
            if (SceneUtil.getMouseType() == MouseType.ONE_BUTTON && event.keyCode == SWT.SHIFT) {
                if (event.type == SWT.KeyDown) {
                    Cursor cursor = this.gridEditor.getCursor(GridEditor.CURSOR_TYPE_CROSS);
                    this.canvas.setCursor(cursor);
                } else if (event.type == SWT.KeyUp) {
                    updateCursor();
                }
            }
        }
    }

    private void dolly(double amount) {
        this.presenter.onPreviewZoom(amount);
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
            switch (this.cameraMovement) {
            case TRACK:
                activeCell = null;
                this.presenter.onPreviewPan(dx, dy);
                break;
            case DOLLY:
                activeCell = null;
                dolly(-dy * 0.005);
                break;
            case IDLE:
                if ((e.stateMask & SWT.BUTTON1) == SWT.BUTTON1) {
                    if (activeCell != null && this.startActiveCell == null) {
                        this.presenter.onPaint(activeCell.getX(), activeCell.getY());
                    }
                }
            }
            this.lastX = e.x;
            this.lastY = e.y;
            if (this.activeCell != activeCell && (this.activeCell == null || activeCell == null || !this.activeCell.equals(activeCell))) {
                this.activeCell = activeCell;
                requestPaint();
            }
        }
    }

    private float calculateTileSourceZoom(TileSetUtil.Metrics metrics, int viewPortWidth, int viewPortHeight) {
        float ratio = Math.max(metrics.visualWidth / viewPortWidth, metrics.visualHeight / viewPortHeight);
        float zoom = 0.2f + ratio;
        if (zoom < 1.0f) {
            zoom = 1.0f;
        }
        return zoom;
    }

    private int pickTile(int x, int y) {
        if (isEnabled()) {
            TileSetUtil.Metrics metrics = calculateMetrics();
            if (metrics != null) {
                int viewPortWidth = this.viewPort.get(2);
                int viewPortHeight = this.viewPort.get(3);
                float zoom = calculateTileSourceZoom(metrics, viewPortWidth, viewPortHeight);
                float xp = (x - 0.5f * viewPortWidth) * zoom + 0.5f * metrics.visualWidth;
                float yp = (y - 0.5f * viewPortHeight) * zoom + 0.5f * metrics.visualHeight;
                int tileX = (int)Math.floor((xp - BORDER_SIZE) / (this.tileWidth + BORDER_SIZE));
                int tileY = (int)Math.floor((yp - BORDER_SIZE) / (this.tileHeight + BORDER_SIZE));
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
        if (this.selectedLayer != null && isEnabled()) {
            int viewPortWidth = this.viewPort.get(2);
            int viewPortHeight = this.viewPort.get(3);
            float cellX = (x - 0.5f * viewPortWidth) / this.scale + this.position.getX();
            cellX /= this.tileWidth;
            float cellY = (0.5f * viewPortHeight - y) / this.scale + this.position.getY();
            cellY /= this.tileHeight;
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

                IHandlerService service = (IHandlerService) PlatformUI.getWorkbench().getService(IHandlerService.class);
                try {
                    service.executeCommand("com.dynamo.cr.tileeditor.commands.ShowPalette", null);
                } catch (Exception e) {
                    logger.error("Error occurred while executing command", e);
                }
            }
        } else {
            this.cameraMovement = CameraUtil.getMovement(event);
            if (this.cameraMovement == Movement.IDLE && this.activeCell != null) {
                // Block brush selection
                MouseType mouseType = SceneUtil.getMouseType();
                if ((mouseType == MouseType.ONE_BUTTON && event.button == 1 && event.stateMask == SWT.SHIFT)
                        || (mouseType == MouseType.THREE_BUTTON && event.button == 3))
                {
                    this.startActiveCell = new Point2i(this.activeCell);
                    requestPaint();
                } else if (event.button == 1) {
                    this.presenter.onPaintBegin();
                    this.presenter.onPaint(this.activeCell.getX(), this.activeCell.getY());
                }
            }
        }
        updateCursor();
    }

    @Override
    public void mouseUp(MouseEvent e) {
        if (this.cameraMovement != Movement.IDLE) {
            this.cameraMovement = Movement.IDLE;
            Point2i activeCell = pickCell(e.x, e.y);
            if ((this.activeCell == null && activeCell != null) || (this.activeCell != null && !this.activeCell.equals(activeCell))) {
                this.activeCell = activeCell;
                requestPaint();
            }
        } else if (this.activeCell != null) {
            if (this.startActiveCell != null) {
                this.presenter.onSelectCells(this.selectedLayer, this.startActiveCell.x, this.startActiveCell.y, this.activeCell.x, this.activeCell.y);
                this.startActiveCell = null;
                requestPaint();
            } else {
                this.presenter.onPaintEnd();
            }
        }
        updateCursor();
    }

    @Override
    public void mouseScrolled(MouseEvent e) {
        dolly(e.count * 0.02);
    }

    public boolean isEnabled() {
        return this.enabled;
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

                render(gl, glu);

            } catch (Throwable e) {
                logger.error("Error occurred while rendering", e);
            } finally {
                canvas.swapBuffers();
                context.release();
            }
        }
    }

    private void render(GL gl, GLU glu) {
        if (!isEnabled()) {
            return;
        }

        int viewPortWidth = this.viewPort.get(2);
        int viewPortHeight = this.viewPort.get(3);

        gl.glMatrixMode(GL.GL_PROJECTION);
        gl.glLoadIdentity();
        glu.gluOrtho2D(-1.0f, 1.0f, -1.0f, 1.0f);

        gl.glMatrixMode(GL.GL_MODELVIEW);
        gl.glLoadIdentity();

        // background
        renderBackground(gl, viewPortWidth, viewPortHeight);

        float recipScale = 1.0f / this.scale;
        float x = 0.5f * viewPortWidth * recipScale;
        float y = 0.5f * viewPortHeight * recipScale;
        gl.glMatrixMode(GL.GL_PROJECTION);
        gl.glLoadIdentity();
        glu.gluOrtho2D(-x, x, -y, y);

        gl.glMatrixMode(GL.GL_MODELVIEW);
        gl.glLoadIdentity();
        gl.glTranslatef(-this.position.getX(), -this.position.getY(), 0.0f);

        // tiles
        renderCells(gl);

        // grid (cell-dividing lines)
        if (visibleGrid) {
            renderGrid(gl);
        }

        // box for right-click selection
        renderSelectionBox(gl);

        // tile set palette
        renderTileSet(gl, glu);
    }

    private void renderBackground(GL gl, int width, int height) {
        this.backgroundTexture.bind();
        this.backgroundTexture.setTexParameteri(GL.GL_TEXTURE_MIN_FILTER, GL.GL_NEAREST);
        this.backgroundTexture.setTexParameteri(GL.GL_TEXTURE_MAG_FILTER, GL.GL_NEAREST);
        this.backgroundTexture.setTexParameteri(GL.GL_TEXTURE_WRAP_S, GL.GL_REPEAT);
        this.backgroundTexture.setTexParameteri(GL.GL_TEXTURE_WRAP_T, GL.GL_REPEAT);
        this.backgroundTexture.enable();
        final float recipTileSize = 0.0625f;
        float x0 = -1.0f;
        float x1 = 1.0f;
        float y0 = -1.0f;
        float y1 = 1.0f;
        float u0 = 0.0f;
        float u1 = width * recipTileSize;
        float v0 = 0.0f;
        float v1 = height * recipTileSize;
        float l = 0.4f;
        gl.glColor3f(l, l, l);
        gl.glBegin(GL.GL_QUADS);
        gl.glTexCoord2f(u0, v0);
        gl.glVertex2f(x0, y0);
        gl.glTexCoord2f(u0, v1);
        gl.glVertex2f(x0, y1);
        gl.glTexCoord2f(u1, v1);
        gl.glVertex2f(x1, y1);
        gl.glTexCoord2f(u1, v0);
        gl.glVertex2f(x1, y0);
        gl.glEnd();
        this.backgroundTexture.disable();
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

        gl.glEnable(GL.GL_BLEND);
        gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);

        Point2f min = new Point2f();
        Point2f max = new Point2f();
        Point2i cellMin = new Point2i();
        Point2i cellMax = new Point2i();
        calculateCellBounds(min, max, cellMin, cellMax);

        float z = 0.0f;

        Point2i brushMin = new Point2i();
        Point2i brushMax = new Point2i();
        if (this.brush != null && this.activeCell != null) {
            brushMin.set(this.activeCell);
            brushMax.set(this.brush.getWidth(), this.brush.getHeight());
            brushMax.add(this.activeCell);
        }

        Map<Long, Cell> cells = new HashMap<Long, Cell>();
        for (Layer layer : this.layers) {
            cells.clear();
            for (Map.Entry<Long, Cell> entry : layer.getCells().entrySet()) {
                if (entry.getValue() != null) {
                    int x = Layer.toCellX(entry.getKey());
                    int y = Layer.toCellY(entry.getKey());
                    if ((x + 1) >= cellMin.getX() && x <= cellMax.getX() && (y + 1) >= cellMin.getY() && y <= cellMax.getY()) {
                        // Do not draw over active cell
                        boolean currentCell = false;
                        if (this.startActiveCell == null && layer.equals(this.selectedLayer) && this.activeCell != null) {
                            currentCell = brushMin.x <= x && x < brushMax.x
                                    && brushMin.y <= y && y < brushMax.y;
                        }
                        if (!currentCell) {
                            cells.put(entry.getKey(), entry.getValue());
                        }
                    }
                }
            }
            int n = cells.size();

            if (n > 0) {
                z = layer.getZ();
                int vertexCount = n * 4;
                int componentCount = 5;
                FloatBuffer v = BufferUtil.newFloatBuffer(vertexCount * componentCount);
                for (Map.Entry<Long, Cell> entry : cells.entrySet()) {
                    int x = Layer.toCellX(entry.getKey());
                    int y = Layer.toCellY(entry.getKey());
                    float x0 = x * this.tileWidth;
                    float x1 = x0 + this.tileWidth;
                    float y0 = y * this.tileHeight;
                    float y1 = y0 + this.tileHeight;
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

                if (this.selectedLayer != null && !layer.equals(this.selectedLayer)) {
                    gl.glColor4f(1.0f, 1.0f, 1.0f, 0.5f);
                }
                gl.glEnableClientState(GL.GL_VERTEX_ARRAY);
                gl.glEnableClientState(GL.GL_TEXTURE_COORD_ARRAY);

                gl.glInterleavedArrays(GL.GL_T2F_V3F, 0, v);

                gl.glDrawArrays(GL.GL_QUADS, 0, vertexCount);

                gl.glDisableClientState(GL.GL_VERTEX_ARRAY);
                gl.glDisableClientState(GL.GL_TEXTURE_COORD_ARRAY);
                gl.glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            }

            if (this.selectedLayer != null && layer.equals(this.selectedLayer)) {
                if (this.activeCell != null && this.startActiveCell == null) {
                    if (this.brush != null) {
                        int width = this.brush.getWidth();
                        int height = this.brush.getHeight();
                        gl.glBegin(GL.GL_QUADS);
                        for (int dx = 0; dx < width; ++dx) {
                            for (int dy = 0; dy < height; ++dy) {
                                int brushTile = this.brush.getCell(dx, dy).getTile();
                                if (brushTile > -1) {
                                    int x = this.activeCell.x + dx;
                                    int y = this.activeCell.y + dy;
                                    float x0 = x * this.tileWidth;
                                    float x1 = x0 + this.tileWidth;
                                    float y0 = y * this.tileHeight;
                                    float y1 = y0 + this.tileHeight;
                                    x = brushTile % metrics.tilesPerRow;
                                    y = brushTile / metrics.tilesPerRow;
                                    float u0 = (x * (tileSpacing + 2*tileMargin + tileWidth) + tileMargin) * recipImageWidth;
                                    float u1 = u0 + tileWidth * recipImageWidth;
                                    float v0 = (y * (tileSpacing + 2*tileMargin + tileHeight) + tileMargin) * recipImageHeight;
                                    float v1 = v0 + tileHeight * recipImageHeight;

                                    gl.glTexCoord2f(u0, v1);
                                    gl.glVertex2f(x0, y0);
                                    gl.glTexCoord2f(u0, v0);
                                    gl.glVertex2f(x0, y1);
                                    gl.glTexCoord2f(u1, v0);
                                    gl.glVertex2f(x1, y1);
                                    gl.glTexCoord2f(u1, v1);
                                    gl.glVertex2f(x1, y0);
                                }
                            }
                        }
                        gl.glEnd();
                    }
                }
            }

        }

        gl.glDisable(GL.GL_BLEND);

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
            float x = i * this.tileWidth;
            v.put(color); v.put(x); v.put(min.getY()); v.put(z);
            v.put(color); v.put(x); v.put(max.getY()); v.put(z);
        }

        for (int i = cellMin.getY(); i < cellMax.getY(); ++i) {
            float[] color = gridColor;
            if (i == 0) {
                color = redColor;
            }
            float y = i * this.tileHeight;
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

    private void renderSelectionBox(GL gl) {
        if (this.selectedLayer != null && this.activeCell != null) {
            float minX = 0.0f;
            float maxX = 0.0f;
            float minY = 0.0f;
            float maxY = 0.0f;
            if (this.startActiveCell != null) {
                minX = this.tileWidth * Math.min(this.startActiveCell.x, this.activeCell.x);
                maxX = this.tileWidth * (Math.max(this.startActiveCell.x, this.activeCell.x) + 1);
                minY = this.tileHeight * Math.min(this.startActiveCell.y, this.activeCell.y);
                maxY = this.tileHeight * (Math.max(this.startActiveCell.y, this.activeCell.y) + 1);
            } else {
                minX = this.tileWidth * this.activeCell.x;
                minY = this.tileHeight * this.activeCell.y;
                maxX = minX + this.tileWidth * this.brush.getWidth();
                maxY = minY + this.tileHeight * this.brush.getHeight();
            }
            gl.glDepthMask(false);
            gl.glDisable(GL.GL_DEPTH_TEST);
            gl.glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            gl.glBegin(GL.GL_LINE_LOOP);
            gl.glVertex2f(minX, minY);
            gl.glVertex2f(maxX, minY);
            gl.glVertex2f(maxX, maxY);
            gl.glVertex2f(minX, maxY);
            gl.glEnd();
            gl.glDepthMask(true);
            gl.glEnable(GL.GL_DEPTH_TEST);
        }
    }

    private void renderTileSet(GL gl, GLU glu) {
        if (this.tileSetTexture == null || !this.showPalette) {
            return;
        }

        TileSetUtil.Metrics metrics = calculateMetrics();

        if (metrics == null) {
            return;
        }

        int viewPortWidth = this.viewPort.get(2);
        int viewPortHeight = this.viewPort.get(3);

        gl.glMatrixMode(GL.GL_PROJECTION);
        gl.glLoadIdentity();
        glu.gluOrtho2D(0, viewPortWidth, viewPortHeight, 0);

        gl.glMatrixMode(GL.GL_MODELVIEW);
        gl.glLoadIdentity();
        gl.glPushMatrix();

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

        // Adjust zoom if tile source is too large
        float zoom = calculateTileSourceZoom(metrics, viewPortWidth, viewPortHeight);

        gl.glTranslatef(viewPortWidth * 0.5f, viewPortHeight * 0.5f, 0.0f);
        Matrix4d m = new Matrix4d();
        m.setIdentity();
        m.setScale(1.0f / zoom);
        RenderUtil.multMatrix(gl, m);
        gl.glTranslatef(-metrics.visualWidth * 0.5f, -metrics.visualHeight * 0.5f, 0.0f);

        // Build tile set data

        int vertexCount = metrics.tilesPerRow * metrics.tilesPerColumn * 4;
        int componentCount = 5;
        FloatBuffer v = BufferUtil.newFloatBuffer(vertexCount * componentCount);

        float z = 0.9f;

        float recipImageWidth = 1.0f / metrics.tileSetWidth;
        float recipImageHeight = 1.0f / metrics.tileSetHeight;

        int brushTile = -1;
        if (this.brush.getWidth() == 1 && this.brush.getHeight() == 1) {
            brushTile = this.brush.getCell(0, 0).getTile();
        }
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

        // Grid
        vertexCount = ((metrics.tilesPerColumn + 1) * 2 + (metrics.tilesPerRow + 1) * 2);
        componentCount = 2;
        FloatBuffer vg = BufferUtil.newFloatBuffer(vertexCount * componentCount);
        for (int y = 0; y <= metrics.tilesPerColumn; ++y) {
            float y0 = y * (tileHeight + BORDER_SIZE);
            vg.put(0.0f); vg.put(y0);
            vg.put(metrics.visualWidth); vg.put(y0);
        }
        for (int x = 0; x <= metrics.tilesPerRow; ++x) {
            float x0 = x * (tileWidth + BORDER_SIZE);
            vg.put(x0); vg.put(0.0f);
            vg.put(x0); vg.put(metrics.visualHeight);
        }
        vg.flip();

        gl.glDisable(GL.GL_DEPTH_TEST);
        gl.glEnableClientState(GL.GL_VERTEX_ARRAY);
        gl.glColor3f(0.3f, 0.3f, 0.3f);
        gl.glInterleavedArrays(GL.GL_V2F, 0, vg);
        gl.glDrawArrays(GL.GL_LINES, 0, vertexCount);
        gl.glDisableClientState(GL.GL_VERTEX_ARRAY);

        // Grid highlights
        if (this.activeTile >= 0 || brushTile >= 0) {
            gl.glPolygonMode(GL.GL_FRONT_AND_BACK, GL.GL_LINE);

            if (brushTile >= 0) {
                float w = tileWidth + BORDER_SIZE;
                float x0 = (brushTile % metrics.tilesPerRow) * w;
                float x1 = x0 + w;
                float h = tileHeight + BORDER_SIZE;
                float y0 = (brushTile / metrics.tilesPerRow) * h;
                float y1 = y0 + h;
                gl.glBegin(GL.GL_QUADS);
                gl.glColor3f(0.8f, 0.8f, 0.8f);
                gl.glVertex2f(x0, y0);
                gl.glVertex2f(x1, y0);
                gl.glVertex2f(x1, y1);
                gl.glVertex2f(x0, y1);
                gl.glEnd();
            }
            if (this.activeTile >= 0) {
                float w = tileWidth + BORDER_SIZE;
                float x0 = (this.activeTile % metrics.tilesPerRow) * w;
                float x1 = x0 + w;
                float h = tileHeight + BORDER_SIZE;
                float y0 = (this.activeTile / metrics.tilesPerRow) * h;
                float y1 = y0 + h;
                gl.glBegin(GL.GL_QUADS);
                gl.glColor3f(1.0f, 1.0f, 1.0f);
                gl.glVertex2f(x0, y0);
                gl.glVertex2f(x1, y0);
                gl.glVertex2f(x1, y1);
                gl.glVertex2f(x0, y1);
                gl.glEnd();
            }

            gl.glPolygonMode(GL.GL_FRONT_AND_BACK, GL.GL_FILL);
        }

        gl.glPopMatrix();

        gl.glDisable(GL.GL_BLEND);
        gl.glDisable(GL.GL_DEPTH_TEST);

    }

    private TileSetUtil.Metrics calculateMetrics() {
        return TileSetUtil.calculateMetrics(this.tileSetImage, this.tileWidth, this.tileHeight, this.tileMargin, this.tileSpacing, null, 1.0f, 1.0f);
    }

    private void calculateCellBounds(Point2f outMin, Point2f outMax, Point2i outCellMin, Point2i outCellMax) {
        if (this.scale == 0.0f || this.tileWidth == 0.0f || this.tileHeight == 0.0f) {
            return;
        }
        Vector2f offset = new Vector2f(this.position);
        Vector2f extent = new Vector2f(this.viewPort.get(2), this.viewPort.get(3));
        extent.scale(0.5f / this.scale);
        outMin.set(offset);
        outMin.sub(extent);
        Point2f minNorm = new Point2f(outMin);
        minNorm.x /= this.tileWidth;
        minNorm.y /= this.tileHeight;
        outMax.set(offset);
        outMax.add(extent);
        Point2f maxNorm = new Point2f(outMax);
        maxNorm.x /= this.tileWidth;
        maxNorm.y /= this.tileHeight;

        outCellMin.set((int)Math.ceil(minNorm.getX()), (int)Math.ceil(minNorm.getY()));
        outCellMax.set((int)Math.ceil(maxNorm.getX()), (int)Math.ceil(maxNorm.getY()));

    }

    private void updateCursor() {
        if (this.canvas != null) {
            int cursorType = -1;
            if (!this.showPalette) {
                if (this.selectedLayer != null) {
                    // Cross when box selecting
                    if (this.startActiveCell != null) {
                        cursorType = GridEditor.CURSOR_TYPE_CROSS;
                    } else {
                        boolean eraser = true;
                        int width = this.brush.getWidth();
                        int height = this.brush.getHeight();
                        for (int x = 0; x < width && eraser; ++x) {
                            for (int y = 0; y < height && eraser; ++y) {
                                if (this.brush.getCell(x, y).getTile() > -1) {
                                    eraser = false;
                                }
                            }
                        }
                        if (eraser) {
                            cursorType = GridEditor.CURSOR_TYPE_ERASER;
                        } else {
                            cursorType = GridEditor.CURSOR_TYPE_PENCIL;
                        }
                    }
                } else {
                    cursorType = GridEditor.CURSOR_TYPE_UNAVAILABLE;
                }
            }
            Cursor cursor = this.defaultCursor;
            if (cursorType > -1) {
                cursor = this.gridEditor.getCursor(cursorType);
            }
            this.canvas.setCursor(cursor);
        }
    }

}
