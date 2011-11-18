package com.dynamo.cr.sceneed.ui;

import java.nio.IntBuffer;
import java.util.ArrayList;
import java.util.List;

import javax.inject.Inject;
import javax.media.opengl.GL;
import javax.media.opengl.GLContext;
import javax.media.opengl.GLDrawableFactory;
import javax.media.opengl.glu.GLU;

import org.eclipse.jface.preference.IPreferenceStore;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.ISelectionChangedListener;
import org.eclipse.jface.viewers.ISelectionProvider;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.SelectionChangedEvent;
import org.eclipse.jface.viewers.StructuredSelection;
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

import com.dynamo.cr.sceneed.core.ILogger;
import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.INodeTypeRegistry;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.ui.preferences.PreferenceConstants;
import com.sun.opengl.util.BufferUtil;

public class RenderView implements
IDisposable,
MouseListener,
MouseMoveListener,
Listener,
ISelectionProvider {

    private final INodeTypeRegistry manager;
    private final ILogger logger;

    private GLCanvas canvas;
    private GLContext context;
    private final IntBuffer viewPort = BufferUtil.newIntBuffer(4);
    private boolean paintRequested = false;
    private boolean enabled = true;

    private Node root;

    // SelectionProvider

    private final List<ISelectionChangedListener> selectionListeners = new ArrayList<ISelectionChangedListener>();
    private IStructuredSelection selection = new StructuredSelection();

    @Inject
    public RenderView(INodeTypeRegistry manager, ILogger logger) {
        this.manager = manager;
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

        this.context.release();

        this.canvas.addListener(SWT.Resize, this);
        this.canvas.addListener(SWT.Paint, this);
        this.canvas.addListener(SWT.MouseExit, this);
        this.canvas.addMouseListener(this);
        this.canvas.addMouseMoveListener(this);
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

    public Node getRoot() {
        return this.root;
    }

    public void setRoot(Node root) {
        this.root = root;
        requestPaint();
    }

    public void refresh() {
        requestPaint();
    }

    public Rectangle getViewRect() {
        return new Rectangle(0, 0, this.viewPort.get(2), this.viewPort.get(3));
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
        // To be used for tools
    }

    // MouseListener

    @Override
    public void mouseDoubleClick(MouseEvent e) {
        // TODO Auto-generated method stub

    }

    @Override
    public void mouseDown(MouseEvent event) {
        // To be used for tools
    }

    @Override
    public void mouseUp(MouseEvent e) {
        // To be used for tools
    }

    // SelectionProvider

    @Override
    public void addSelectionChangedListener(ISelectionChangedListener listener) {
        this.selectionListeners.add(listener);
    }

    @Override
    public ISelection getSelection() {
        return this.selection;
    }

    @Override
    public void removeSelectionChangedListener(
            ISelectionChangedListener listener) {
        this.selectionListeners.remove(listener);
    }

    @Override
    public void setSelection(ISelection selection) {
        if (selection instanceof IStructuredSelection) {
            this.selection = (IStructuredSelection)selection;
            SelectionChangedEvent event = new SelectionChangedEvent(this, this.selection);
            for (ISelectionChangedListener listener : this.selectionListeners) {
                listener.selectionChanged(event);
            }
        }
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
                logger.logException(e);
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

        if (this.root != null) {
            renderNode(this.root, gl, glu);
        }
    }

    private void renderNode(Node node, GL gl, GLU glu) {
        INodeRenderer renderer = this.manager.getRenderer(node.getClass());
        if (renderer != null) {
            renderer.render(node, gl, glu);
        }

        for (Node child : node.getChildren()) {
            renderNode(child, gl, glu);
        }
    }

    private float[] parseColor(String preferenceName) {
        IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        String color = store.getString(preferenceName);
        String[] components = color.split(",");
        float[] c = new float[3];
        float recip = 1.0f / 255.0f;
        if (components.length == 3) {
            c[0] = Integer.parseInt(components[0]) * recip;
            c[1] = Integer.parseInt(components[1]) * recip;
            c[2] = Integer.parseInt(components[2]) * recip;
        } else {
            c[0] = 0.0f; c[1] = 0.0f; c[2] = 0.0f;
        }
        return c;
    }

    private void renderBackground(GL gl, int width, int height) {
        // Fetch colors
        float[] topColor = parseColor(PreferenceConstants.P_TOP_BKGD_COLOR);
        float[] bottomColor = parseColor(PreferenceConstants.P_BOTTOM_BKGD_COLOR);

        float x0 = -1.0f;
        float x1 = 1.0f;
        float y0 = -1.0f;
        float y1 = 1.0f;
        gl.glBegin(GL.GL_QUADS);
        gl.glColor3fv(topColor, 0);
        gl.glVertex2f(x0, y1);
        gl.glVertex2f(x1, y1);
        gl.glColor3fv(bottomColor, 0);
        gl.glVertex2f(x1, y0);
        gl.glVertex2f(x0, y0);
        gl.glEnd();
    }

}
