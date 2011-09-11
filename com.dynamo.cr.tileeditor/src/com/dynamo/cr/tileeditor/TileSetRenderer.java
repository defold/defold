package com.dynamo.cr.tileeditor;

import java.awt.Color;
import java.awt.image.BufferedImage;

import javax.media.opengl.GL;
import javax.media.opengl.GLContext;
import javax.media.opengl.GLDrawableFactory;
import javax.media.opengl.glu.GLU;
import javax.vecmath.Vector3f;

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
import com.sun.opengl.util.texture.Texture;
import com.sun.opengl.util.texture.TextureIO;

public class TileSetRenderer implements
IDisposable,
MouseListener,
MouseMoveListener,
Listener,
KeyListener {

    private final TileSetPresenter presenter;
    private final GLCanvas canvas;
    private final GLContext context;
    private final int[] viewPort = new int[4];
    private Texture texture;
    private boolean paintRequested = false;
    // Render data
    private BufferedImage image;
    private float[] vertices;
    private int[] hullIndices;
    private int[] hullCounts;
    private Color[] hullColors;
    private Vector3f hullScale;
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
        gl.glClearDepth(1.0f);
        gl.glEnable(GL.GL_DEPTH_TEST);
        gl.glDepthFunc(GL.GL_LEQUAL);
        gl.glHint(GL.GL_PERSPECTIVE_CORRECTION_HINT, GL.GL_NICEST);
        gl.setSwapInterval(1);

        BufferedImage backgroundImage = new BufferedImage(2, 2, BufferedImage.TYPE_INT_ARGB);
        backgroundImage.setRGB(0, 0, 0xff999999);
        backgroundImage.setRGB(1, 0, 0xff666666);
        backgroundImage.setRGB(0, 1, 0xff666666);
        backgroundImage.setRGB(1, 1, 0xff999999);
        background = TextureIO.newTexture(backgroundImage, false);

        this.context.release();

        this.canvas.addListener(SWT.Resize, this);
        this.canvas.addListener(SWT.Paint, this);
        this.canvas.addMouseListener(this);
        this.canvas.addMouseMoveListener(this);
        this.canvas.addKeyListener(this);
    }

    @Override
    public void dispose() {
        this.context.makeCurrent();
        this.background.dispose();
        this.context.release();
        canvas.dispose();
    }

    public void setTiles(BufferedImage image, float[] v, int[] hullIndices,
            int[] hullCounts, Color[] hullColors, Vector3f hullScale) {
        this.image = image;
        this.vertices = v;
        this.hullIndices = hullIndices;
        this.hullCounts = hullCounts;
        this.hullColors = hullColors;
        this.hullScale = hullScale;
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
        // TODO Auto-generated method stub

    }

    // MouseListener

    @Override
    public void mouseDoubleClick(MouseEvent e) {
        // TODO Auto-generated method stub

    }

    @Override
    public void mouseDown(MouseEvent e) {
        // TODO Auto-generated method stub

    }

    @Override
    public void mouseUp(MouseEvent e) {
        // TODO Auto-generated method stub

    }

    private void requestPaint() {
        if (this.paintRequested)
            return;
        this.paintRequested = true;

        if (!canvas.isDisposed()) {
            canvas.setCurrent();
            context.makeCurrent();
            GL gl = context.getGL();
            GLU glu = new GLU();
            try {
                gl.glDisable(GL.GL_LIGHTING);
                gl.glPolygonMode(GL.GL_FRONT_AND_BACK, GL.GL_FILL);
                gl.glDisable(GL.GL_DEPTH_TEST);

                gl.glMatrixMode(GL.GL_PROJECTION);
                gl.glLoadIdentity();
                glu.gluOrtho2D(0, viewPort[2], 0, viewPort[3]);
                gl.glViewport(viewPort[0], viewPort[1], viewPort[2], viewPort[3]);

                gl.glMatrixMode(GL.GL_MODELVIEW);
                gl.glLoadIdentity();
                gl.glTranslatef(0.5f, 0.5f, 0.5f);
                gl.glClearColor(1.0f, 0.0f, 0.0f, 1);
                gl.glClear(GL.GL_COLOR_BUFFER_BIT | GL.GL_DEPTH_BUFFER_BIT);

                if (image != null) {
                    float width = image.getWidth();
                    float height = image.getHeight();
                    gl.glBegin(GL.GL_TRIANGLES);
                    gl.glTexCoord2f(0.0f, 1.0f);
                    gl.glVertex2f(0.0f, 0.0f);
                    gl.glTexCoord2f(0.0f, 0.0f);
                    gl.glVertex2f(0.0f, height);
                    gl.glTexCoord2f(1.0f, 1.0f);
                    gl.glVertex2f(width, 0.0f);
                    gl.glTexCoord2f(0.0f, 0.0f);
                    gl.glVertex2f(0.0f, height);
                    gl.glTexCoord2f(1.0f, 0.0f);
                    gl.glVertex2f(width, height);
                    gl.glTexCoord2f(1.0f, 1.0f);
                    gl.glVertex2f(width, 0.0f);
                    gl.glEnd();
                }

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
}
