package com.dynamo.cr.sceneed.ui;

import java.awt.Font;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.IntBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import javax.inject.Inject;
import javax.media.opengl.GL;
import javax.media.opengl.GLContext;
import javax.media.opengl.GLDrawableFactory;
import javax.media.opengl.glu.GLU;
import javax.vecmath.Matrix4d;
import javax.vecmath.Point2i;
import javax.vecmath.Point3d;
import javax.vecmath.Vector4d;

import org.eclipse.jface.dialogs.Dialog;
import org.eclipse.jface.dialogs.InputDialog;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.StructuredSelection;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.KeyEvent;
import org.eclipse.swt.events.KeyListener;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseListener;
import org.eclipse.swt.events.MouseMoveListener;
import org.eclipse.swt.events.MouseWheelListener;
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
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.sceneed.core.Camera;
import com.dynamo.cr.sceneed.core.CameraController;
import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.INodeType;
import com.dynamo.cr.sceneed.core.INodeTypeRegistry;
import com.dynamo.cr.sceneed.core.IRenderView;
import com.dynamo.cr.sceneed.core.IRenderViewController;
import com.dynamo.cr.sceneed.core.IRenderViewController.FocusType;
import com.dynamo.cr.sceneed.core.IRenderViewProvider;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.cr.sceneed.core.SceneGrid;
import com.dynamo.cr.sceneed.screenrecord.ScreenRecorder;
import com.dynamo.cr.sceneed.ui.RenderView.SelectResult.Pair;
import com.sun.opengl.util.j2d.TextRenderer;

public class RenderView implements
MouseListener,
MouseMoveListener,
MouseWheelListener,
KeyListener,
Listener,
IRenderView {

    private static Logger logger = LoggerFactory.getLogger(RenderView.class);
    private final INodeTypeRegistry nodeTypeRegistry;
    private Node input;
    private IStructuredSelection selection;

    private GLCanvas canvas;
    private GLContext context;
    private final int[] viewPort = new int[4];
    private boolean enabled = true;
    private boolean pendingFrameSelection = false;

    private List<IRenderViewProvider> providers = new ArrayList<IRenderViewProvider>();
    private List<IRenderViewController> controllers = new ArrayList<IRenderViewController>();
    private IRenderViewController focusController = null;
    private CameraController cameraController;

    private Camera camera;

    // Selection
    private static final int PICK_BUFFER_SIZE = 4096;
    private static IntBuffer selectBuffer = ByteBuffer.allocateDirect(4 * PICK_BUFFER_SIZE).order(ByteOrder.nativeOrder()).asIntBuffer();
    private static final int MIN_SELECTION_BOX = 16;

    private boolean paintRequested = false;

    private Set<INodeType> hiddenNodeTypes = new HashSet<INodeType>();
    private TextureRegistry textureRegistry;
    private Map<INodeType, INodeRenderer<Node>> renderers = new HashMap<INodeType, INodeRenderer<Node>>();

    private SceneGrid grid;
    private boolean simulating = false;
    private TextRenderer smallTextRenderer;

    private ScreenRecorder screenRecorder = null;

    private boolean gridShown = true;
    private boolean outlineShown = true;

    @Inject
    public RenderView(INodeTypeRegistry manager) {
        this.nodeTypeRegistry = manager;
        this.selection = new StructuredSelection();
        this.textureRegistry = new TextureRegistry();

        grid = new SceneGrid();
    }

    @Override
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

        String fontName = Font.SANS_SERIF;
        Font debugFont = new Font(fontName, Font.BOLD, 12);
        this.smallTextRenderer = new TextRenderer(debugFont, true, true);

        this.context.release();

        this.canvas.addListener(SWT.Resize, this);
        this.canvas.addListener(SWT.Paint, this);
        this.canvas.addListener(SWT.MouseExit, this);
        this.canvas.addMouseListener(this);
        this.canvas.addMouseMoveListener(this);
        this.canvas.addMouseWheelListener(this);
        this.canvas.addKeyListener(this);
    }

    @Override
    public void dispose() {
        if (this.context != null) {
            this.context.makeCurrent();
            GL gl = this.context.getGL();

            for (INodeRenderer<Node> r : this.renderers.values()) {
                if (r != null) {
                    r.dispose(gl);
                }
            }

            if (this.smallTextRenderer != null) {
                this.smallTextRenderer.dispose();
            }

            this.context.release();
            this.context.destroy();
        }
        if (this.canvas != null) {
            canvas.dispose();
        }
    }

    @Override
    public void setFocus() {
        this.canvas.setFocus();
    }

    public Control getControl() {
        return this.canvas;
    }

    @Override
    public void refresh() {
        this.grid.updateGrids(getViewTransform(), getProjectionTransform());
        for (IRenderViewController controller : this.controllers) {
            controller.refresh();
        }
        requestPaint();
    }

    @Override
    public void setSimulating(boolean simulating) {
        this.simulating  = simulating;
    }

    @Override
    public Node getInput() {
        return this.input;
    }

    @Override
    public void setInput(Node input) {
        this.input = input;
    }

    @Override
    public void setSelection(IStructuredSelection selection) {
        this.selection = selection;
        for (IRenderViewController controller : this.controllers) {
            controller.setSelection(selection);
        }
    }

    public Rectangle getViewRect() {
        return new Rectangle(0, 0, this.viewPort[2], this.viewPort[3]);
    }

    private static double dot(Vector4d v0, Vector4d v1)
    {
        return v0.x * v1.x + v0.y * v1.y + v0.z * v1.z;
    }

    @Override
    public void viewToWorld(int x, int y, Vector4d worldPoint, Vector4d worldVector) {
        Vector4d clickPos = camera.unProject(x, y, 0);
        Vector4d clickDir = new Vector4d();

        if (camera.getType() == Camera.Type.ORTHOGRAPHIC)
        {
            /*
             * NOTES:
             * We cancel out the z-component in world_point below.
             * The convention is that the unproject z-value for orthographic projection should
             * be 0.0.
             * Pity that the orthographic case is an exception. Solution?
             */
            Matrix4d view = new Matrix4d();
            camera.getViewMatrix(view);

            Vector4d view_axis = new Vector4d();
            view.getColumn(2, view_axis);
            clickDir.set(view_axis);

            double projectedLength = dot(clickPos, view_axis);
            view_axis.scale(projectedLength);
            clickPos.sub(view_axis);
        }
        else
        {
            clickDir.sub(clickPos, camera.getPosition());
            clickDir.normalize();
        }

        worldPoint.set(clickPos);
        worldVector.set(clickDir);
    }

    @Override
    public double[] worldToView(Point3d point) {
        Point3d ret = camera.project(point.x, point.y, point.z);
        return new double[] { ret.x, ret.y };
    }

    @Override
    public Matrix4d getViewTransform() {
        Matrix4d ret = new Matrix4d();
        camera.getViewMatrix(ret);
        return ret;
    }

    @Override
    public Matrix4d getProjectionTransform() {
        Matrix4d ret = new Matrix4d();
        camera.getProjectionMatrix(ret);
        return ret;
    }

    @Override
    public Vector4d getCameraFocusPoint() {
        if (this.cameraController != null) {
            return this.cameraController.getFocusPoint();
        }
        throw new IllegalStateException("No camera controller available");
    }

    @Override
    public Camera getCamera() {
        return this.camera;
    }

    @Override
    public void setCamera(Camera camera) {
        this.camera = camera;
    }

    private void doFrameSelection() {
        if (this.cameraController != null) {
            this.cameraController.frameSelection();
        }
    }

    @Override
    public void frameSelection() {
        if (this.context != null) {
            doFrameSelection();
            requestPaint();
        } else {
            this.pendingFrameSelection = true;
        }
    }

    // Listener

    @Override
    public void handleEvent(Event event) {
        if (event.type == SWT.Resize) {
            Rectangle client = this.canvas.getClientArea();
            this.viewPort[0] = 0;
            this.viewPort[1] = 0;
            this.viewPort[2] = client.width;
            this.viewPort[3] = client.height;
            this.camera.setViewport(viewPort[0],
                                    viewPort[1],
                                    viewPort[2],
                                    viewPort[3]);

            Point size = canvas.getSize();
            double aspect = ((double) size.x) / size.y;
            camera.setOrthographic(camera.getFov(), aspect, -100000, 100000);
            if (this.pendingFrameSelection) {
                this.pendingFrameSelection = false;
                doFrameSelection();
            }
        } else if (event.type == SWT.Paint) {
            requestPaint();
        }
    }

    // MouseMoveListener

    @Override
    public void mouseMove(MouseEvent e) {
        if (this.focusController != null) {
            this.focusController.mouseMove(e);
            requestPaint();
        }
    }

    // MouseListener

    @Override
    public void mouseDoubleClick(MouseEvent e) {
        if (this.focusController != null) {
            this.focusController.mouseDoubleClick(e);
            requestPaint();
        }
    }

    private void initFocusController(MouseEvent e) {
        finalFocusController();

        List<Node> nodes = findNodesBySelection(new Point2i(e.x, e.y));
        FocusType topFocus = FocusType.NONE;
        for (IRenderViewController controller : this.controllers) {
            FocusType focus = controller.getFocusType(nodes, e);
            if (focus.ordinal() > topFocus.ordinal()) {
                topFocus = focus;
                this.focusController = controller;
            }
        }
        if (this.focusController != null) {
            this.focusController.initControl(nodes);
            requestPaint();
        }
    }

    private void finalFocusController() {
        if (this.focusController != null) {
            this.focusController.finalControl();
            this.focusController = null;
            requestPaint();
        }
    }

    @Override
    public void mouseDown(MouseEvent e) {
        initFocusController(e);
        if (this.focusController != null) {
            this.focusController.mouseDown(e);
            requestPaint();
        }
    }

    @Override
    public void mouseUp(MouseEvent e) {
        if (this.focusController != null) {
            this.focusController.mouseUp(e);
        }
        finalFocusController();
    }

    @Override
    public void mouseScrolled(MouseEvent e) {
        initFocusController(e);
        if (this.focusController != null) {
            this.focusController.mouseScrolled(e);
        }
        finalFocusController();
    }

    public List<Node> findNodesBySelection(Point2i p) {
        return findNodesBySelection(p, p);
    }

    @Override
    public List<Node> findNodesBySelection(Point2i start, Point2i end) {
        Point2i dims = new Point2i(end.x, end.y);
        dims.sub(start);
        Point2i center = new Point2i((int) Math.round(dims.x * 0.5), (int) Math.round(dims.y * 0.5));
        center.add(start);
        dims.absolute();
        int x = center.x;
        int y = center.y;
        int width = Math.max(MIN_SELECTION_BOX, dims.x);
        int height = Math.max(MIN_SELECTION_BOX, dims.y);
        return findNodesBySelection(x, y, width, height);
    }

    /**
     * NOTE: x and y must be center coordinates
     * @param x center coordinate
     * @param y center coordinate
     * @param width width to select for
     * @param height height to select for
     * @return list of selected nodes
     */
    public List<Node> findNodesBySelection(int x, int y, int width, int height) {
        ArrayList<Node> nodes = new ArrayList<Node>(32);

        if (width > 0 && height > 0) {
            context.makeCurrent();
            GL gl = context.getGL();
            GLU glu = new GLU();

            for (Pass pass : Pass.getSelectionPasses() ) {

                beginSelect(gl, pass, x, y, width, height);

                List<Pass> passes = Arrays.asList(pass);
                RenderContext renderContext = renderNodes(gl, glu, passes, true);

                SelectResult result = endSelect(gl);

                List<RenderData<? extends Node>> renderDataList = renderContext.getRenderData();

                // The selection result is sorted according to z
                // We want to use the draw-order instead that is a function of
                // pass, z among other such that eg manipulators get higher priority than regular nodes
                List<RenderData<? extends Node>> drawOrderSorted = new ArrayList<RenderData<? extends Node>>();
                for (Pair pair : result.selected) {
                    RenderData<? extends Node> renderData = renderDataList.get(pair.index);
                    drawOrderSorted.add(renderData);
                }
                Collections.sort(drawOrderSorted);
                Collections.reverse(drawOrderSorted);

                for (RenderData<? extends Node> renderData : drawOrderSorted) {
                    nodes.add(renderData.getNode());
                }
            }
        }

        return nodes;
    }

    @Override
    public void keyPressed(KeyEvent e) {
        if (this.focusController != null) {
            this.focusController.keyPressed(e);
            requestPaint();
        }
    }

    @Override
    public void keyReleased(KeyEvent e) {
        if (this.focusController != null) {
            this.focusController.keyReleased(e);
            requestPaint();
        }
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

    private void beginSelect(GL gl, Pass pass, int x, int y, int w, int h) {
        gl.glSelectBuffer(PICK_BUFFER_SIZE, selectBuffer);
        gl.glRenderMode(GL.GL_SELECT);

        GLU glu = new GLU();
        gl.glMatrixMode(GL.GL_PROJECTION);
        gl.glLoadIdentity();

        if (pass == Pass.SELECTION) {
            glu.gluPickMatrix(x, viewPort[3] - y, w, h, viewPort, 0);
        } else {
            glu.gluOrtho2D(x - w / 2, x + w /2, y + h / 2, y - h / 2);
        }

        if (pass == Pass.SELECTION) {
            Matrix4d projection = new Matrix4d();
            camera.getProjectionMatrix(projection);
            if (pass.transformModel()) {
                RenderUtil.multMatrix(gl, projection);
            }
        }

        gl.glMatrixMode(GL.GL_MODELVIEW);
        if (pass == Pass.SELECTION) {
            Matrix4d view = new Matrix4d();
            camera.getViewMatrix(view);
            RenderUtil.loadMatrix(gl, view);
        } else {
            gl.glLoadIdentity();
        }

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

    public void requestPaint() {
        if (this.paintRequested || this.canvas == null)
            return;

        this.paintRequested = true;

        Display.getCurrent().timerExec(15, new Runnable() {

            @Override
            public void run() {
                paintRequested = false;
                if (simulating || screenRecorder != null) {
                    requestPaint();
                }
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

                gl.glViewport(this.viewPort[0], this.viewPort[1], this.viewPort[2], this.viewPort[3]);

                render(gl, glu);

            } catch (Throwable e) {
                logger.error("Error occurred while rendering", e);
            } finally {
                canvas.swapBuffers();
                context.release();

                if (screenRecorder != null) {
                    screenRecorder.captureScreen();
                }
            }
        }
    }

    private void render(GL gl, GLU glu) {
        if (!isEnabled()) {
            return;
        }

        List<Pass> passes = Arrays.asList(Pass.BACKGROUND, Pass.OUTLINE, Pass.ICON_OUTLINE, Pass.TRANSPARENT, Pass.ICON, Pass.MANIPULATOR, Pass.OVERLAY);
        renderNodes(gl, glu, passes, false);
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

    private RenderContext renderNodes(GL gl, GLU glu, List<Pass> passes, boolean pick) {
        double dt = 0;
        if (simulating) {
            dt = 1.0 / 60.0;
        }

        RenderContext renderContext = new RenderContext(this, dt, gl, glu, textureRegistry, this.selection, this.smallTextRenderer);

        for (IRenderViewProvider provider : providers) {
            for (Pass pass : passes) {
                renderContext.setPass(pass);
                provider.setup(renderContext);
            }
        }

        renderContext.sort();

        int nextName = 0;
        Pass currentPass = null;
        List<RenderData<? extends Node>> renderDataList = renderContext.getRenderData();
        Matrix4d transform = new Matrix4d();
        for (RenderData<? extends Node> renderData : renderDataList) {
            Pass pass = renderData.getPass();

            if (currentPass != pass) {
                setupPass(renderContext.getGL(), renderContext.getGLU(), pass);
                currentPass = pass;
            }
            renderContext.setPass(currentPass);
            if (pick) {
                gl.glPushName(nextName++);
            }
            Node node = renderData.getNode();
            node.getWorldTransform(transform);
            gl.glPushMatrix();
            if (pass.transformModel()) {
                RenderUtil.multMatrix(gl, transform);
            }
            doRender(renderContext, renderData);
            gl.glPopMatrix();
            if (pick) {
                gl.glPopName();
            }
        }

        return renderContext;
    }

    private void setupPass(GL gl, GLU glu, Pass pass) {

        // Default projection
        // TODO: Temp camera

        if (!pass.isSelectionPass()) {
            gl.glMatrixMode(GL.GL_PROJECTION);
            Matrix4d projection = new Matrix4d();
            camera.getProjectionMatrix(projection );
            RenderUtil.loadMatrix(gl, projection);
        }

        gl.glMatrixMode(GL.GL_MODELVIEW);
        Matrix4d view = new Matrix4d();
        camera.getViewMatrix(view);
        RenderUtil.loadMatrix(gl, view);

        switch (pass) {
        case BACKGROUND:
            gl.glMatrixMode(GL.GL_PROJECTION);
            gl.glLoadIdentity();
            glu.gluOrtho2D(-1, 1, -1, 1);

            gl.glMatrixMode(GL.GL_MODELVIEW);
            gl.glLoadIdentity();

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

        case MANIPULATOR:
            gl.glPolygonMode(GL.GL_FRONT_AND_BACK, GL.GL_FILL);
            gl.glDisable(GL.GL_BLEND);
            gl.glDisable(GL.GL_DEPTH_TEST);
            gl.glDepthMask(false);
            break;

        case OVERLAY:
            gl.glMatrixMode(GL.GL_PROJECTION);
            gl.glLoadIdentity();
            glu.gluOrtho2D(this.viewPort[0], this.viewPort[2], this.viewPort[3], this.viewPort[1]);

            gl.glMatrixMode(GL.GL_MODELVIEW);
            gl.glLoadIdentity();

            gl.glPolygonMode(GL.GL_FRONT_AND_BACK, GL.GL_FILL);
            gl.glEnable(GL.GL_BLEND);
            gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);
            gl.glDisable(GL.GL_DEPTH_TEST);
            gl.glDepthMask(false);
            break;

        case ICON:
            gl.glMatrixMode(GL.GL_PROJECTION);
            gl.glLoadIdentity();
            glu.gluOrtho2D(this.viewPort[0], this.viewPort[2], this.viewPort[3], this.viewPort[1]);

            gl.glMatrixMode(GL.GL_MODELVIEW);
            gl.glLoadIdentity();

            gl.glPolygonMode(GL.GL_FRONT_AND_BACK, GL.GL_FILL);
            gl.glEnable(GL.GL_BLEND);
            gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);
            gl.glDisable(GL.GL_DEPTH_TEST);
            gl.glDepthMask(false);
            break;

        case ICON_OUTLINE:
            gl.glMatrixMode(GL.GL_PROJECTION);
            gl.glLoadIdentity();
            glu.gluOrtho2D(this.viewPort[0], this.viewPort[2], this.viewPort[3], this.viewPort[1]);

            gl.glMatrixMode(GL.GL_MODELVIEW);
            gl.glLoadIdentity();

            gl.glPolygonMode(GL.GL_FRONT_AND_BACK, GL.GL_LINE);
            gl.glDisable(GL.GL_BLEND);
            gl.glDisable(GL.GL_DEPTH_TEST);
            gl.glDepthMask(false);
            break;

        case ICON_SELECTION:
            break;

        case SELECTION:
            break;

        default:
            throw new RuntimeException(String.format("Pass %s not implemented", pass.toString()));
        }

    }

    @Override
    public void setupNode(RenderContext renderContext, Node node) {
        if (!node.isVisible()) {
            return;
        }

        Class<? extends Node> nodeClass = node.getClass();
        INodeType nodeType = this.nodeTypeRegistry.getNodeTypeClass(nodeClass);
        boolean abort = false;
        // outlines are only render either when the node is selected or outlines are set to be shown
        boolean outlinePass = (renderContext.getPass() == Pass.OUTLINE || renderContext.getPass() == Pass.ICON_OUTLINE);
        boolean render = true;
        if (outlinePass && !renderContext.isSelected(node) && !isOutlineShown()) {
            render = false;
        }
        if (render && nodeType != null) {
            if (!this.hiddenNodeTypes.contains(nodeType)) {

                if (!renderers.containsKey(nodeType)) {
                    renderers.put(nodeType, nodeType.createRenderer());
                }

                INodeRenderer<Node> renderer = renderers.get(nodeType);
                if (renderer != null)
                    renderer.setup(renderContext, node);
            } else {
                abort = true;
            }
        }

        if (!abort) {
            for (Node child : node.getChildren()) {
                setupNode(renderContext, child);
            }
        }
    }

    // IRenderView

    @Override
    public void addRenderProvider(IRenderViewProvider provider) {
        assert !providers.contains(provider);
        providers.add(provider);
    }

    @Override
    public void removeRenderProvider(IRenderViewProvider provider) {
        assert providers.contains(provider);
        providers.remove(provider);
    }

    @Override
    public void addRenderController(IRenderViewController controller) {
        this.controllers.add(controller);
        if (controller instanceof CameraController) {
            this.cameraController = (CameraController)controller;
        }
    }

    @Override
    public void removeRenderController(IRenderViewController controller) {
        this.controllers.remove(controller);
        if (this.cameraController == controller) {
            this.cameraController = null;
        }
    }

    @Override
    public void activateGLContext() {
        this.canvas.setCurrent();
        this.context.makeCurrent();
    }

    @Override
    public void releaseGLContext() {
        this.context.release();
    }

    @Override
    public void setNodeTypeVisible(INodeType nodeType, boolean visible) {
        if (visible) {
            this.hiddenNodeTypes.remove(nodeType);
        } else {
            this.hiddenNodeTypes.add(nodeType);
        }
    }

    @Override
    public SceneGrid getGrid() {
        return this.grid;
    }

    @Override
    public void toggleRecord() {
        if (screenRecorder == null) {

            InputDialog dialog = new InputDialog(null, "Screen Capture Path", "Specifiy capture path", "/tmp/c", null);
            if (dialog.open() == Dialog.OK) {
                screenRecorder = new ScreenRecorder(dialog.getValue());
                screenRecorder.start();
            }

        } else {
            screenRecorder.stop();
            screenRecorder = null;
        }
    }

    @Override
    public boolean isGridShown() {
        return this.gridShown;
    }

    @Override
    public void setGridShown(boolean gridShown) {
        this.gridShown = gridShown;
    }

    @Override
    public boolean isOutlineShown() {
        return this.outlineShown;
    }

    @Override
    public void setOutlineShown(boolean outlineShown) {
        this.outlineShown = outlineShown;
    }

}
