package com.dynamo.cr.tileeditor;

import java.awt.Color;
import java.awt.image.BufferedImage;
import java.nio.FloatBuffer;
import java.nio.IntBuffer;
import java.util.List;

import javax.media.opengl.GL;
import javax.media.opengl.GL2;
import javax.media.opengl.GLContext;
import javax.media.opengl.GLDrawableFactory;
import javax.media.opengl.GLProfile;
import javax.media.opengl.glu.GLU;
import javax.vecmath.Vector2f;

import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.KeyEvent;
import org.eclipse.swt.events.KeyListener;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseListener;
import org.eclipse.swt.events.MouseMoveListener;
import org.eclipse.swt.events.MouseWheelListener;
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

import com.dynamo.bob.tile.ConvexHull;
import com.dynamo.bob.tile.TileSetUtil;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.TextureHandle;
import com.dynamo.cr.sceneed.core.util.CameraUtil;
import com.dynamo.cr.sceneed.core.util.CameraUtil.Movement;
import com.dynamo.cr.tileeditor.scene.AnimationNode;
import com.dynamo.cr.tileeditor.scene.CollisionGroupNode;
import com.dynamo.cr.tileeditor.scene.TileSetNode;
import com.dynamo.cr.tileeditor.scene.TileSetNodePresenter;
import com.jogamp.common.nio.Buffers;
import com.jogamp.opengl.util.texture.Texture;
import com.jogamp.opengl.util.texture.awt.AWTTextureIO;

public class TileSetRenderer2 implements
IDisposable,
MouseListener,
MouseMoveListener,
MouseWheelListener,
Listener,
KeyListener {

    private static final float BORDER_SIZE = 3.0f;

    private TileSetNodePresenter presenter;
    private IPresenterContext presenterContext;
    private TileSetNode tileSet;
    private GLCanvas canvas;
    private GLContext context;
    private final IntBuffer viewPort = Buffers.newDirectIntBuffer(4);
    private boolean paintRequested = false;
    private float scale = 1.0f;
    private boolean enabled = true;
    private boolean resetView = true;
    private String brushCollisionGroup = "";
    private Color brushColor = Color.white;
    private int activeTile = -1;
    private AnimationNode selectedAnimation;

    // Camera data
    private Movement cameraMovement = Movement.IDLE;
    private int lastX;
    private int lastY;
    private final float[] offset = new float[2];

    // Render data
    private FloatBuffer tileVertexBuffer;
    private FloatBuffer hullVertexBuffer;
    private FloatBuffer hullFrameVertexBuffer;
    private BufferedImage tileSetImage;
    private TextureHandle tileSetTexture;
    private Texture backgroundTexture;
    private Texture transparentTexture;

    public TileSetRenderer2() {
        this.tileSetTexture = new TextureHandle();
    }

    public void setPresenter(TileSetNodePresenter presenter, IPresenterContext presenterContext) {
        this.presenter = presenter;
        this.presenterContext = presenterContext;
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

        // See comment in section "OpenGL and jogl" in README.md about
        // the order below any why the factory must be created before setCurrent
        GLDrawableFactory factory = GLDrawableFactory.getFactory(GLProfile.getGL2GL3());
        this.canvas.setCurrent();
		this.context = factory.createExternalGLContext();

        int result = this.context.makeCurrent();
        if (result != GLContext.CONTEXT_NOT_CURRENT) {
            GL2 gl = this.context.getGL().getGL2();
            gl.glPolygonMode(GL2.GL_FRONT, GL2.GL_FILL);

            BufferedImage backgroundImage = new BufferedImage(2, 2, BufferedImage.TYPE_INT_ARGB);
            backgroundImage.setRGB(0, 0, 0xff999999);
            backgroundImage.setRGB(1, 0, 0xff666666);
            backgroundImage.setRGB(0, 1, 0xff666666);
            backgroundImage.setRGB(1, 1, 0xff999999);
            this.backgroundTexture = AWTTextureIO.newTexture(GLProfile.getGL2GL3(), backgroundImage, false);
            BufferedImage transparentImage = new BufferedImage(1, 1, BufferedImage.TYPE_INT_ARGB);
            transparentImage.setRGB(0, 0, 0x00000000);
            this.transparentTexture = AWTTextureIO.newTexture(GLProfile.getGL2GL3(), transparentImage, false);

            // if the image is already set, set the corresponding texture
            this.resetView = true;
            if (this.tileSet != null && this.tileSet.getStatus().isOK()) {
                setupRenderData();
            }

            this.context.release();
        }

        this.canvas.addListener(SWT.Resize, this);
        this.canvas.addListener(SWT.Paint, this);
        this.canvas.addMouseListener(this);
        this.canvas.addMouseMoveListener(this);
        this.canvas.addMouseWheelListener(this);
        this.canvas.addKeyListener(this);
    }

    public GLContext getContext() {
		return context;
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

    private void setupRenderData() {
        BufferedImage loadedImage = this.tileSet.getLoadedImage();
        if (loadedImage != null && !loadedImage.equals(this.tileSetImage)) {
            this.tileSetImage = loadedImage;
            this.tileSetTexture.setImage(loadedImage);
        }

        float[] hullVertices = this.tileSet.getConvexHullPoints();

        this.hullVertexBuffer = Buffers.newDirectFloatBuffer(hullVertices.length);

        int hullCount = tileSet.getConvexHulls().size();
        this.hullFrameVertexBuffer = Buffers.newDirectFloatBuffer(hullCount * 6 * 4);

        requestPaint();
    }

    public void setInput(TileSetNode tileSet) {
        if (this.tileSet != tileSet) {
            this.tileSet = tileSet;
            if (this.context != null) {
                this.context.makeCurrent();
                this.resetView = true;
                setupRenderData();
                this.context.release();
            }
        }
    }

    public void refresh() {
        if (this.tileSet != null && !this.tileSet.getStatus().isOK()) {
            setEnabled(false);
            return;
        }
        setEnabled(true);
        if (this.context != null) {
            this.context.makeCurrent();
            setupRenderData();
            this.context.release();
        }
    }

    public void setSelection(IStructuredSelection selection) {
        Object[] selectedObjects = selection.toArray();
        if (selectedObjects.length == 1 && selectedObjects[0] instanceof CollisionGroupNode) {
            CollisionGroupNode collisionGroup = (CollisionGroupNode)selectedObjects[0];
            setBrushCollisionGroup(collisionGroup.getId());
        } else {
            setBrushCollisionGroup("");
        }
        if (selectedObjects.length == 1 && selectedObjects[0] instanceof AnimationNode) {
            this.selectedAnimation = (AnimationNode)selectedObjects[0];
        } else {
            this.selectedAnimation = null;
        }
        requestPaint();
    }

    public void setFocus() {
        this.canvas.setFocus();
    }

    public Control getControl() {
        return this.canvas;
    }

    public String getBrushCollisionGroup() {
        return this.brushCollisionGroup;
    }

    public void setBrushCollisionGroup(String brushCollisionGroup) {
        this.brushCollisionGroup = brushCollisionGroup;
        this.brushColor = getCollisionGroupColor(brushCollisionGroup);
        requestPaint();
    }

    private void centerCamera(float width, float height) {
        this.offset[0] = -width * 0.5f;
        this.offset[1] = -height * 0.5f;
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

    private void dolly(double amount) {
        this.scale += amount * this.scale;
        this.scale = Math.max(0.1f, this.scale);
        activeTile = -1;
    }

    // MouseMoveListener

    @Override
    public void mouseMove(MouseEvent e) {
        int dx = e.x - this.lastX;
        int dy = e.y - this.lastY;
        int activeTile = pickTile(e.x, e.y);
        switch (this.cameraMovement) {
        case TRACK:
            float recipScale = 1.0f / this.scale;
            this.offset[0] += dx * recipScale;
            this.offset[1] -= dy * recipScale;
            activeTile = -1;
            requestPaint();
            break;
        case DOLLY:
            float ds = -dy * 0.005f;
            dolly(ds);
            requestPaint();
            break;
        case IDLE:
            if ((e.stateMask & SWT.BUTTON1) == SWT.BUTTON1) {
                paintTile();
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

        this.cameraMovement = CameraUtil.getMovement(event);
        if (this.cameraMovement == Movement.IDLE) {
            if (event.button == 1) {
                beginPainting();
                paintTile();
            }
        } else {
            this.activeTile = -1;
            requestPaint();
        }
    }

    @Override
    public void mouseUp(MouseEvent e) {
        if (this.cameraMovement != Movement.IDLE) {
            this.cameraMovement = Movement.IDLE;
            int activeTile = pickTile(e.x, e.y);
            if (activeTile != this.activeTile) {
                this.activeTile = activeTile;
                requestPaint();
            }
        } else if (e.button == 1) {
            endPainting();
        }
    }

    @Override
    public void mouseScrolled(MouseEvent e) {
        dolly(e.count * 0.02);
        requestPaint();
    }

    private void beginPainting() {
        this.presenter.onBeginPaintTile(this.presenterContext);
    }

    private void endPainting() {
        this.presenter.onEndPaintTile(this.presenterContext);
    }

    private void paintTile() {
        List<ConvexHull> hulls = this.tileSet.getConvexHulls();
        int hullCount = hulls.size();
        if (activeTile >= 0 && activeTile < hullCount && hulls.get(activeTile).getCount() > 0) {
            this.presenter.onPaintTile(this.presenterContext, activeTile);
        }
    }

    public boolean isEnabled() {
        return this.enabled;
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
            GL2 gl = this.context.getGL().getGL2();
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

    private void setupViewProj(GL2 gl, GLU glu) {
        gl.glMatrixMode(GL2.GL_PROJECTION);
        gl.glLoadIdentity();
        float recipScale = 1.0f / this.scale;
        float x = 0.5f * this.viewPort.get(2) * recipScale;
        float y = 0.5f * this.viewPort.get(3) * recipScale;
        glu.gluOrtho2D(-x, x, -y, y);

        gl.glMatrixMode(GL2.GL_MODELVIEW);
        gl.glLoadIdentity();
        gl.glTranslatef(this.offset[0], this.offset[1], 0.0f);
    }

    private void drawTileSet(GL2 gl) {
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

    private void drawBackground(GL2 gl, float width, float height) {
        this.backgroundTexture.bind(gl);
        this.backgroundTexture.setTexParameteri(gl, GL.GL_TEXTURE_MIN_FILTER, GL.GL_NEAREST);
        this.backgroundTexture.setTexParameteri(gl, GL.GL_TEXTURE_MAG_FILTER, GL.GL_NEAREST);
        this.backgroundTexture.setTexParameteri(gl, GL.GL_TEXTURE_WRAP_S, GL.GL_REPEAT);
        this.backgroundTexture.setTexParameteri(gl, GL.GL_TEXTURE_WRAP_T, GL.GL_REPEAT);
        this.backgroundTexture.enable(gl);
        final float recipTileSize = 0.0625f;
        float recipTexelWidth = width * recipTileSize;
        float recipTexelHeight = height * recipTileSize;
        float u0 = (0.5f / width + recipTileSize * this.offset[0]) * this.scale;
        float u1 = u0 + recipTexelWidth * this.scale;
        float v0 = (0.5f / height + recipTileSize * this.offset[1]) * this.scale;
        float v1 = v0 + recipTexelHeight * this.scale;
        gl.glBegin(GL2.GL_QUADS);
        gl.glTexCoord2f(u0, v0);
        gl.glVertex2f(0.0f, 0.0f);
        gl.glTexCoord2f(u0, v1);
        gl.glVertex2f(0.0f, height);
        gl.glTexCoord2f(u1, v1);
        gl.glVertex2f(width, height);
        gl.glTexCoord2f(u1, v0);
        gl.glVertex2f(width, 0.0f);
        gl.glEnd();
        this.backgroundTexture.disable(gl);
    }

    private void drawTiles(GL2 gl, float borderSize, int tilesPerRow, int tilesPerColumn, float width, float height, int imageWidth, int imageHeight) {
        int tileCount = tilesPerRow * tilesPerColumn;

        // Construct vertex data

        int vertexCount = 4 * tileCount;
        // tile vertex data is 2 uv + 3 pos
        int componentCount = 5 * vertexCount;
        if (this.tileVertexBuffer == null || this.tileVertexBuffer.capacity() != componentCount) {
            this.tileVertexBuffer = Buffers.newDirectFloatBuffer(componentCount);
        }
        FloatBuffer v = this.tileVertexBuffer;
        FloatBuffer hv = this.hullFrameVertexBuffer;
        float recipImageWidth = 1.0f / imageWidth;
        float recipImageHeight = 1.0f / imageHeight;
        float recipScale = 1.0f / this.scale;
        float border = borderSize * recipScale;
        float tileZ = 0.5f;
        float animZ = 0.7f;
        float hz = 0.3f;
        float activeX0 = 0.0f;
        float activeX1 = 0.0f;
        float activeY0 = 0.0f;
        float activeY1 = 0.0f;
        float halfBorder = border / 3.0f;
        float[] hc = new float[3];
        int tileWidth = this.tileSet.getTileWidth();
        int tileHeight = this.tileSet.getTileHeight();
        List<ConvexHull> hulls = this.tileSet.getConvexHulls();
        List<CollisionGroupNode> tileCollisionGroups = this.tileSet.getTileCollisionGroups();
        float[] hullVertices = this.tileSet.getConvexHullPoints();
        Vector2f uvMin = new Vector2f();
        Vector2f uvMax = new Vector2f();
        for (int row = 0; row < tilesPerColumn; ++row) {
            for (int column = 0; column < tilesPerRow; ++column) {
                float x0 = column * (tileWidth + border) + border;
                float x1 = x0 + tileWidth;
                float y0 = (tilesPerColumn - row - 1) * (tileHeight + border) + border;
                float y1 = y0 + tileHeight;
                int spriteRow = row;
                int spriteColumn = column;
                int index = row * tilesPerRow + column;
                float z = tileZ;
                boolean flipHorizontal = false;
                boolean flipVertical = false;
                if (this.selectedAnimation != null && (index + 1) == this.selectedAnimation.getStartTile()) {
                    int spriteIndex = this.selectedAnimation.getCurrentTile()-1;
                    spriteRow = spriteIndex / tilesPerRow;
                    spriteColumn = spriteIndex % tilesPerRow;
                    z = animZ;
                    flipHorizontal = this.selectedAnimation.isFlipHorizontally();
                    flipVertical = this.selectedAnimation.isFlipVertically();
                }
                calculateUVs(spriteRow, spriteColumn, recipImageWidth, recipImageHeight, this.tileSet, uvMin, uvMax);
                float u0 = uvMin.getX();
                float u1 = uvMax.getX();
                if (flipHorizontal) {
                    u0 = uvMax.getX();
                    u1 = uvMin.getX();
                }
                float v1 = uvMax.getY();
                float v0 = uvMin.getY();
                if (flipVertical) {
                    v0 = uvMax.getY();
                    v1 = uvMin.getY();
                }
                v.put(u0); v.put(v0); v.put(x0); v.put(y0); v.put(z);
                v.put(u0); v.put(v1); v.put(x0); v.put(y1); v.put(z);
                v.put(u1); v.put(v1); v.put(x1); v.put(y1); v.put(z);
                v.put(u1); v.put(v0); v.put(x1); v.put(y0); v.put(z);
                if (!hulls.isEmpty()) {
                    ConvexHull hull = hulls.get(index);
                    int hullCount = hull.getCount();
                    if (hullCount > 0) {
                        int hullIndex = hull.getIndex();
                        for (int i = 0; i < hullCount; ++i) {
                            int hi = 2 * (hullIndex + i);
                            this.hullVertexBuffer.put(x0 + 0.5f + hullVertices[hi+0]);
                            this.hullVertexBuffer.put(y0 + 0.5f + hullVertices[hi+1]);
                        }
                        String groupId = null;
                        CollisionGroupNode group = tileCollisionGroups.get(index);
                        if (group != null) {
                            groupId = group.getId();
                        }
                        Color color = getCollisionGroupColor(groupId);
                        color.getColorComponents(hc);
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

        gl.glEnableClientState(GL2.GL_TEXTURE_COORD_ARRAY);
        gl.glEnableClientState(GL2.GL_VERTEX_ARRAY);

        gl.glEnable(GL.GL_DEPTH_TEST);
        gl.glDepthMask(true);
        gl.glEnable(GL.GL_BLEND);
        gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);
        Texture texture = this.tileSetTexture.getTexture(gl);
        if (texture != null) {
            texture.bind(gl);
            texture.setTexParameteri(gl, GL.GL_TEXTURE_MIN_FILTER, GL.GL_NEAREST);
            texture.setTexParameteri(gl, GL.GL_TEXTURE_MAG_FILTER, GL.GL_NEAREST);
            texture.setTexParameteri(gl, GL.GL_TEXTURE_WRAP_S, GL2.GL_CLAMP);
            texture.setTexParameteri(gl, GL.GL_TEXTURE_WRAP_T, GL2.GL_CLAMP);
            texture.enable(gl);
        } else {
            this.transparentTexture.bind(gl);
            this.transparentTexture.enable(gl);
        }

        gl.glInterleavedArrays(GL2.GL_T2F_V3F, 0, v);

        gl.glDrawArrays(GL2.GL_QUADS, 0, vertexCount);

        if (texture != null) {
            texture.disable(gl);
        } else {
            this.transparentTexture.disable(gl);
        }

        gl.glDisableClientState(GL2.GL_TEXTURE_COORD_ARRAY);

        // Active tile

        float z = tileZ - 0.1f;

        if (this.activeTile >= 0) {
            float f = 1.0f / 255.0f;
            gl.glBegin(GL2.GL_QUADS);
            gl.glColor4f(brushColor.getRed() * f, brushColor.getGreen() * f, brushColor.getBlue() * f, brushColor.getAlpha() * f);
            gl.glVertex3f(activeX0, activeY0, z);
            gl.glVertex3f(activeX0, activeY1, z);
            gl.glVertex3f(activeX1, activeY1, z);
            gl.glVertex3f(activeX1, activeY0, z);
            gl.glEnd();
        }

        // Hull Frames

        if (!hulls.isEmpty()) {
            hv.flip();

            gl.glEnableClientState(GL2.GL_COLOR_ARRAY);

            gl.glInterleavedArrays(GL2.GL_C3F_V3F, 0, hv);

            gl.glDrawArrays(GL2.GL_QUADS, 0, hv.limit() / 6);

            gl.glDisableClientState(GL2.GL_COLOR_ARRAY);
        }

        gl.glDepthMask(false);

        // Overlay

        z = 0.0f;
        gl.glBegin(GL2.GL_QUADS);
        gl.glColor4f(0.2f, 0.2f, 0.2f, 0.7f);
        gl.glVertex3f(0.0f, 0.0f, z);
        gl.glVertex3f(0.0f, height, z);
        gl.glVertex3f(width, height, z);
        gl.glVertex3f(width, 0.0f, z);
        gl.glEnd();

        gl.glDisable(GL.GL_DEPTH_TEST);

        // Hulls

        if (!hulls.isEmpty()) {
            this.hullVertexBuffer.flip();
            gl.glVertexPointer(2, GL.GL_FLOAT, 0, this.hullVertexBuffer);

            Color c = null;
            float f = 1.0f / 255.0f;
            int hullCount = hulls.size();
            for (int i = 0; i < hullCount; ++i) {
                ConvexHull hull = hulls.get(i);
                String groupId = null;
                CollisionGroupNode group = tileCollisionGroups.get(i);
                if (group != null) {
                    groupId = group.getId();
                }
                c = getCollisionGroupColor(groupId);
                gl.glColor4f(c.getRed() * f, c.getGreen() * f, c.getBlue() *
                f, c.getAlpha() * f);
                gl.glDrawArrays(GL.GL_LINE_LOOP, hull.getIndex(), hull.getCount());
            }
        }

        // Anim overlay
        if (this.selectedAnimation != null) {
            z = 0.6f;
            gl.glEnable(GL.GL_DEPTH_TEST);
            gl.glBegin(GL2.GL_QUADS);
            gl.glColor4f(0.0f, 0.0f, 0.0f, 0.6f);
            gl.glVertex3f(0.0f, 0.0f, z);
            gl.glVertex3f(0.0f, height, z);
            gl.glVertex3f(width, height, z);
            gl.glVertex3f(width, 0.0f, z);
            gl.glEnd();
            gl.glDisable(GL.GL_DEPTH_TEST);
        }

        // Clean up
        gl.glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        gl.glDisableClientState(GL2.GL_VERTEX_ARRAY);
        gl.glDisable(GL.GL_BLEND);
    }

    private Color getCollisionGroupColor(String name) {
        Color color = CollisionGroupNode.getCollisionGroupColor(name);
        if (color == null) {
            color = new Color(1.0f, 1.0f, 1.0f, 1.0f);
        }
        return color;
    }

    private TileSetUtil.Metrics calculateMetrics() {
        TileSetNode ts = this.tileSet;
        return TileSetUtil.calculateMetrics(ts.getLoadedImage(),
                ts.getTileWidth(),
                ts.getTileHeight(),
                ts.getTileMargin(),
                ts.getTileSpacing(),
                ts.getLoadedCollision(),
                this.scale,
                BORDER_SIZE);
    }

    private void calculateUVs(int row, int column, float recipImageWidth, float recipImageHeight, TileSetNode node, Vector2f outUVMin, Vector2f outUVMax) {
        outUVMin.setX((column * (node.getTileSpacing() + 2*node.getTileMargin() + node.getTileWidth()) + node.getTileMargin()) * recipImageWidth);
        outUVMax.setX(outUVMin.getX() + node.getTileWidth() * recipImageWidth);
        outUVMax.setY((row * (node.getTileSpacing() + 2*node.getTileMargin() + node.getTileHeight()) + node.getTileMargin()) * recipImageHeight);
        outUVMin.setY(outUVMax.getY() + node.getTileHeight() * recipImageHeight);
    }
}
