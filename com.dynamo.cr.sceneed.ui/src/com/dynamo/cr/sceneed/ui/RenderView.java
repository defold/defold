package com.dynamo.cr.sceneed.ui;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.IntBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
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
import org.eclipse.ui.services.IDisposable;

import com.dynamo.cr.editor.core.ILogger;
import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.INodeType;
import com.dynamo.cr.sceneed.core.INodeTypeRegistry;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.cr.sceneed.ui.RenderView.SelectResult.Pair;
import com.dynamo.cr.sceneed.ui.preferences.PreferenceConstants;
import com.sun.opengl.util.BufferUtil;

public class RenderView implements
IDisposable,
MouseListener,
MouseMoveListener,
Listener,
ISelectionProvider {

    private final INodeTypeRegistry nodeTypeRegistry;
    private final ILogger logger;

    private GLCanvas canvas;
    private GLContext context;
    private final IntBuffer viewPort = BufferUtil.newIntBuffer(4);
    private boolean paintRequested = false;
    private boolean enabled = true;

    // TODO: Temp "camera"
    private double[] orthoCamera = new double[6];

    // Picking
    private static final int PICK_BUFFER_SIZE = 4096;
    private static IntBuffer selectBuffer = ByteBuffer.allocateDirect(4 * PICK_BUFFER_SIZE).order(ByteOrder.nativeOrder()).asIntBuffer();

    private Node root;

    // SelectionProvider

    private final List<ISelectionChangedListener> selectionListeners = new ArrayList<ISelectionChangedListener>();
    private IStructuredSelection selection = new StructuredSelection();

    @Inject
    public RenderView(INodeTypeRegistry manager, ILogger logger) {
        this.nodeTypeRegistry = manager;
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
            this.context.release();
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

            // TODO: Temp "camera"
            Point size = canvas.getSize();
            double aspect = ((double) size.x) / size.y;
            double left = -300.0 / 2;
            double right = 300.0 / 2;
            double bottom = left / aspect;
            double top = right / aspect;
            orthoCamera[0] = left;
            orthoCamera[1] = right;
            orthoCamera[2] = bottom;
            orthoCamera[3] = top;
            orthoCamera[4] = -100000;
            orthoCamera[5] = 100000;

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
        List<Node> sel = rectangleSelect(event.x, event.y, 16, 16);
        StructuredSelection newSelection = new StructuredSelection(sel);
        setSelection(newSelection);
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
        requestPaint();
    }

    public boolean isEnabled() {
        return this.enabled;
    }

    public void setEnabled(boolean enabled) {
        this.enabled = enabled;
        requestPaint();
    }

    public static class SelectResult
    {
        public static class Pair implements Comparable<Pair> {
            Pair(long z, int index) {
                this.z = z;
                this.index = index;
            }
            public long z;
            public int index;

            @Override
            public int compareTo(Pair o) {
                return (z<o.z ? -1 : (z==o.z ? 0 : 1));
            }

            @Override
            public String toString() {
                return String.format("%d (%d)", index, z);
            }
        }
        public SelectResult(List<Pair> selected, long minz)
        {
            this.selected = selected;
            minZ = minz;

        }
        public List<Pair> selected;
        public long minZ = Long.MAX_VALUE;
    }

    private void beginSelect(GL gl, int x, int y, int w, int h) {
        gl.glSelectBuffer(PICK_BUFFER_SIZE, selectBuffer);
        gl.glRenderMode(GL.GL_SELECT);

        GLU glu = new GLU();
        gl.glMatrixMode(GL.GL_PROJECTION);
        gl.glLoadIdentity();
        glu.gluPickMatrix(x, y, w, h, viewPort);
        gl.glOrtho(orthoCamera[0], orthoCamera[1], orthoCamera[2], orthoCamera[3], orthoCamera[4], orthoCamera[5]);

        gl.glMatrixMode(GL.GL_MODELVIEW);
        gl.glLoadIdentity();

        gl.glInitNames();
    }

    private static long toUnsignedInt(int i)
    {
        long tmp = i;
        return ( tmp << 32 ) >>> 32;
    }

    public SelectResult endSelect(GL gl)
    {
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

    private List<Node> rectangleSelect(int x, int y, int width, int height) {
        ArrayList<Node> toSelect = new ArrayList<Node>(32);

        if (width > 0 && height > 0) {
            context.makeCurrent();
            GL gl = context.getGL();
            GLU glu = new GLU();

            // x and y are center coordinates
            beginSelect(gl, x + width / 2, y + height / 2, width, height);

            List<Pass> passes = Arrays.asList(Pass.SELECTION);
            RenderContext renderContext = renderNodes(gl, glu, passes);

            SelectResult result = endSelect(gl);

            List<RenderData<? extends Node>> renderDataList = renderContext.getRenderData();
            for (Pair pair : result.selected) {
                Node node = renderDataList.get(pair.index).getNode();
                toSelect.add(node);
            }
        }

        return toSelect;
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
        glu.gluOrtho2D(-1, 1, -1, 1);

        gl.glMatrixMode(GL.GL_MODELVIEW);
        gl.glLoadIdentity();

        // background
        setupPass(gl, Pass.BACKGROUND);
        renderBackground(gl, viewPortWidth, viewPortHeight);

        // TODO: Temp "camera"
        gl.glMatrixMode(GL.GL_PROJECTION);
        gl.glLoadIdentity();
        gl.glOrtho(orthoCamera[0], orthoCamera[1], orthoCamera[2], orthoCamera[3], orthoCamera[4], orthoCamera[5]);
        gl.glMatrixMode(GL.GL_MODELVIEW);

        List<Pass> passes = Arrays.asList(Pass.OUTLINE, Pass.TRANSPARENT);
        renderNodes(gl, glu, passes);
    }

    /*
     * This function exists solely to let the type inference engine in java resolve
     * the types without class-cast warnings or cast-hacks..
     */
    private <T extends Node> void doRender(RenderContext renderContext, RenderData<T> renderData) {
        INodeRenderer<T> renderer = renderData.getNodeRenderer();
        T node = renderData.getNode();
        renderer.render(renderContext, node, renderData);
    }

    private RenderContext renderNodes(GL gl, GLU glu, List<Pass> passes) {
        RenderContext renderContext = new RenderContext(gl, glu, this.selection);

        if (this.root != null) {
            for (Pass pass : passes) {
                renderContext.setPass(pass);
                setupNode(renderContext, this.root);
            }
        }

        renderContext.sort();

        Pass currentPass = null;
        List<RenderData<? extends Node>> renderDataList = renderContext.getRenderData();
        for (RenderData<? extends Node> renderData : renderDataList) {
            Pass pass = renderData.getPass();

            if (currentPass != pass) {
                setupPass(renderContext.getGL(), pass);
                currentPass = pass;
            }
            renderContext.setPass(currentPass);
            doRender(renderContext, renderData);
        }

        return renderContext;
    }

    private void setupPass(GL gl, Pass pass) {
        switch (pass) {
        case BACKGROUND:
            gl.glPolygonMode(GL.GL_FRONT_AND_BACK, GL.GL_FILL);
            gl.glDisable(GL.GL_BLEND);
            gl.glDisable(GL.GL_DEPTH_TEST);
            gl.glDepthMask(false);
            break;

        case OPAQUE:
            gl.glPolygonMode(GL.GL_FRONT_AND_BACK, GL.GL_FILL);
            gl.glDisable(GL.GL_BLEND);
            gl.glEnable(GL.GL_DEPTH_TEST);
            gl.glDepthMask(true);
            break;

        case OUTLINE:
            gl.glPolygonMode(GL.GL_FRONT_AND_BACK, GL.GL_LINE);
            gl.glDisable(GL.GL_BLEND);
            gl.glDisable(GL.GL_DEPTH_TEST);
            gl.glDepthMask(false);
            break;

        case TRANSPARENT:
            gl.glPolygonMode(GL.GL_FRONT_AND_BACK, GL.GL_FILL);
            gl.glEnable(GL.GL_BLEND);
            gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);
            gl.glEnable(GL.GL_DEPTH_TEST);
            gl.glDepthMask(false);
            break;

        case SELECTION:
            break;

        default:
            throw new RuntimeException(String.format("Pass %s not implemented", pass.toString()));
        }

    }

    private void setupNode(RenderContext renderContext, Node node) {
        INodeType nodeType = this.nodeTypeRegistry.getNodeType(node.getClass());
        if (nodeType != null) {
            INodeRenderer<Node> renderer = nodeType.getRenderer();
            renderer.setup(renderContext, node);
        }

        for (Node child : node.getChildren()) {
            setupNode(renderContext, child);
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
