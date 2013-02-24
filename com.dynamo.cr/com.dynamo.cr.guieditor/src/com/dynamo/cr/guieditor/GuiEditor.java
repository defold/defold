package com.dynamo.cr.guieditor;

import java.awt.geom.Rectangle2D;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;

import javax.media.opengl.GL;
import javax.media.opengl.GL2;
import javax.media.opengl.GLContext;
import javax.media.opengl.GLDrawableFactory;
import javax.media.opengl.GLProfile;
import javax.media.opengl.glu.GLU;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.IOperationApprover;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistoryListener;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.commands.operations.OperationHistoryEvent;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.resources.IResourceChangeListener;
import org.eclipse.core.resources.IResourceDelta;
import org.eclipse.core.resources.IResourceDeltaVisitor;
import org.eclipse.core.resources.ResourcesPlugin;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.core.runtime.SubMonitor;
import org.eclipse.jface.action.IAction;
import org.eclipse.jface.operation.IRunnableWithProgress;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.ControlAdapter;
import org.eclipse.swt.events.ControlEvent;
import org.eclipse.swt.events.KeyEvent;
import org.eclipse.swt.events.KeyListener;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseListener;
import org.eclipse.swt.events.MouseMoveListener;
import org.eclipse.swt.events.SelectionAdapter;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.graphics.RGB;
import org.eclipse.swt.graphics.Rectangle;
import org.eclipse.swt.graphics.Transform;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.opengl.GLCanvas;
import org.eclipse.swt.opengl.GLData;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Listener;
import org.eclipse.swt.widgets.ScrollBar;
import org.eclipse.ui.IActionBars;
import org.eclipse.ui.IEditorInput;
import org.eclipse.ui.IEditorSite;
import org.eclipse.ui.IFileEditorInput;
import org.eclipse.ui.ISelectionListener;
import org.eclipse.ui.IWorkbenchPart;
import org.eclipse.ui.PartInitException;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.actions.ActionFactory;
import org.eclipse.ui.contexts.IContextService;
import org.eclipse.ui.operations.LinearUndoViolationUserApprover;
import org.eclipse.ui.operations.RedoActionHandler;
import org.eclipse.ui.operations.UndoActionHandler;
import org.eclipse.ui.part.EditorPart;
import org.eclipse.ui.progress.IProgressService;
import org.eclipse.ui.statushandlers.StatusManager;
import org.eclipse.ui.views.contentoutline.IContentOutlinePage;
import org.eclipse.ui.views.properties.IPropertySheetPage;

import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.core.ProjectProperties;
import com.dynamo.cr.guieditor.operations.SelectOperation;
import com.dynamo.cr.guieditor.render.GuiRenderer;
import com.dynamo.cr.guieditor.render.IGuiRenderer;
import com.dynamo.cr.guieditor.render.SelectResult;
import com.dynamo.cr.guieditor.render.SelectResult.Pair;
import com.dynamo.cr.guieditor.scene.GuiNode;
import com.dynamo.cr.guieditor.scene.GuiScene;
import com.dynamo.cr.guieditor.scene.IGuiSceneListener;
import com.dynamo.cr.guieditor.tools.SelectMoveTool;
import com.dynamo.cr.guieditor.util.DrawUtil;
import com.dynamo.cr.properties.FormPropertySheetPage;
import com.dynamo.gui.proto.Gui.SceneDesc;
import com.google.protobuf.TextFormat;

public class GuiEditor extends EditorPart implements IGuiEditor, MouseListener,
        MouseMoveListener, Listener, IOperationHistoryListener,
        IGuiSceneListener, ISelectionListener, KeyListener, IResourceChangeListener {

    private GLCanvas canvas;
    private GLContext context;
    private boolean redrawPosted = false;
    private final int[] viewPort = new int[4];
    private GuiScene guiScene;
    private GuiSelectionProvider selectionProvider;
    private Map<String, IAction> actions = new HashMap<String, IAction>();
    private UndoContext undoContext;
    private FormPropertySheetPage propertySheetPage;
    private SelectMoveTool selectMoveTool;
    private boolean refreshPropertySheetPosted;
    private IOperationHistory history;
    private boolean dirty = false;
    private IContainer contentRoot;
    private GuiRenderer renderer;
    private GuiEditorOutlinePage outlinePage;
    private int undoRedoCounter = 0;
    private boolean inSave = false;
    private int width = 0;
    private int height = 0;

    public GuiEditor() {
        selectionProvider = new GuiSelectionProvider();

        renderer = new GuiRenderer();
    }

    @Override
    public IAction getAction(String id) {
        return actions.get(id);
    }

    @Override
    public IGuiRenderer getRenderer() {
        return renderer;
    }

    @Override
    public void doSave(IProgressMonitor monitor) {

        IFileEditorInput i = (IFileEditorInput) getEditorInput();
        this.inSave = true;
        try {
            ByteArrayOutputStream stream = new ByteArrayOutputStream();

            SceneDesc sceneDesc = guiScene.buildSceneDesc();

            OutputStreamWriter writer = new OutputStreamWriter(stream);
            TextFormat.print(sceneDesc, writer);
            writer.close();
            i.getFile().setContents(
                    new ByteArrayInputStream(stream.toByteArray()), false,
                    true, monitor);
            undoRedoCounter = 0;
            dirty = false;
            firePropertyChange(PROP_DIRTY);

        } catch (Throwable e) {
            Status status = new Status(IStatus.ERROR, Activator.PLUGIN_ID, 0,
                    e.getMessage(), null);
            StatusManager.getManager().handle(status, StatusManager.LOG | StatusManager.SHOW);
        } finally {
            this.inSave = false;
        }
    }

    @Override
    public void doSaveAs() {
    }

    @Override
    public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
        if (adapter == IPropertySheetPage.class) {
            return propertySheetPage;
        } else if (adapter == IContentOutlinePage.class) {
            if (outlinePage == null)
                outlinePage = new GuiEditorOutlinePage(this);
            return outlinePage;
        }

        return super.getAdapter(adapter);
    }

    class Loader implements IRunnableWithProgress {

        IFile file;
        Throwable exception;

        public Loader(IFile file) {
            this.file = file;
        }

        @Override
        public void run(IProgressMonitor monitor) {
            final int totalWork = 5;
            monitor.beginTask("Loading Gui Scene...", totalWork);
            try {
                InputStreamReader reader = new InputStreamReader(
                        file.getContents());
                SceneDesc.Builder builder = SceneDesc.newBuilder();

                try {
                    TextFormat.merge(reader, builder);
                    monitor.worked(1);
                    SceneDesc sceneDesc = builder.build();
                    guiScene = new GuiScene(GuiEditor.this, sceneDesc);
                    SubMonitor subMonitor = SubMonitor.convert(monitor,
                            totalWork - 1);
                    guiScene.loadRenderResources(subMonitor, contentRoot,
                            renderer);

                } finally {
                    reader.close();
                }
            } catch (Throwable e) {
                this.exception = e;
            }
        }
    }

    @Override
    public void init(IEditorSite site, IEditorInput input)
            throws PartInitException {
        ResourcesPlugin.getWorkspace().addResourceChangeListener(this);

        setSite(site);
        setInput(input);
        setPartName(input.getName());

        IFileEditorInput fileEditorInput = (IFileEditorInput) input;
        IFile file = fileEditorInput.getFile();
        this.contentRoot = EditorUtil.findContentRoot(file);
        if (this.contentRoot == null) {
            throw new PartInitException(
                    "Unable to locate content root for project");
        }
        propertySheetPage = new FormPropertySheetPage(getContentRoot()) {
            @Override
            public void setActionBars(IActionBars actionBars) {
                super.setActionBars(actionBars);
                String undoId = ActionFactory.UNDO.getId();
                String redoId = ActionFactory.REDO.getId();
                actionBars.setGlobalActionHandler(undoId, actions.get(undoId));
                actionBars.setGlobalActionHandler(redoId, actions.get(redoId));
            }
        };
        IFile projectPropertiesFile = EditorUtil.findGameProjectFile(this.contentRoot);
        ProjectProperties projectProperties = new ProjectProperties();
        try {
            projectProperties.load(projectPropertiesFile.getContents());
            Integer width = projectProperties.getIntValue("display", "width");
            Integer height = projectProperties.getIntValue("display", "height");
            if (width != null) {
                this.width = width.intValue();
            } else {
            	this.width = 960;
            }
            if (height != null) {
                this.height = height.intValue();
            } else {
                this.height = 640;
            }
        } catch (CoreException e) {
            StatusManager.getManager().handle(e, getEditorSite().getPluginId());
        } catch (Exception e) {
            StatusManager.getManager().handle(new Status(IStatus.ERROR, getEditorSite().getPluginId(), e.getMessage()));
        }

        IProgressService service = PlatformUI.getWorkbench()
                .getProgressService();
        Loader loader = new Loader(file);
        try {
            service.runInUI(service, loader, null);
            if (loader.exception != null) {
                throw new PartInitException(loader.exception.getMessage(),
                        loader.exception);
            }
        } catch (Throwable e) {
            throw new PartInitException(e.getMessage(), e);
        }

        guiScene.addGuiSceneListener(this);
    }

    @Override
    public IContainer getContentRoot() {
        return contentRoot;
    }

    @Override
    public boolean isDirty() {
        return dirty;
    }

    void updateDirtyState() {
        dirty = undoRedoCounter != 0;
        firePropertyChange(PROP_DIRTY);

        if (outlinePage != null) {
            outlinePage.refresh();
        }
    }

    @Override
    public boolean isSaveAsAllowed() {
        return false;
    }

    @Override
    public void dispose() {
        super.dispose();

        getSite().getWorkbenchWindow().getSelectionService().removeSelectionListener(this);

        guiScene.dispose();

        if (history != null) {
            history.removeOperationHistoryListener(this);
            history.removeOperationHistoryListener(this);
        }
        if (renderer != null) {
            renderer.dispose();
        }
        if (canvas != null) {
            canvas.dispose();
        }
        if (guiScene != null) {
            guiScene.removeGuiSceneListener(this);
        }
        ResourcesPlugin.getWorkspace().removeResourceChangeListener(this);
    }

    @Override
    public void createPartControl(Composite parent) {
        GLData data = new GLData();
        data.doubleBuffer = true;
        data.depthSize = 24;

        canvas = new GLCanvas(parent, SWT.NO_REDRAW_RESIZE | SWT.H_SCROLL
                | SWT.V_SCROLL, data);
        GridData gd = new GridData(SWT.FILL, SWT.FILL, true, true);
        gd.widthHint = SWT.DEFAULT;
        gd.heightHint = SWT.DEFAULT;
        canvas.setLayoutData(gd);

        // See comment in section "OpenGL and jogl" in README.md about
        // the order below any why the factory must be created before setCurrent
        GLDrawableFactory factory = GLDrawableFactory.getFactory(GLProfile.getGL2GL3());
        this.canvas.setCurrent();
		this.context = factory.createExternalGLContext();

        context.makeCurrent();
        GL gl = context.getGL();
        gl.glClearDepth(1.0f);
        gl.glEnable(GL.GL_DEPTH_TEST);
        gl.glDepthFunc(GL.GL_LEQUAL);
        gl.glHint(GL2.GL_PERSPECTIVE_CORRECTION_HINT, GL.GL_NICEST);

        context.release();

        canvas.addListener(SWT.Resize, this);
        canvas.addListener(SWT.Paint, this);
        canvas.addMouseListener(this);
        canvas.addMouseMoveListener(this);
        canvas.addKeyListener(this);

        this.canvas.addControlListener(new ControlAdapter() {
            @Override
            public void controlResized(ControlEvent e) {
                updateScrollBars();
            }

            @Override
            public void controlMoved(ControlEvent e) {
                updateScrollBars();
            }
        });

        ScrollBar horizontal = canvas.getHorizontalBar();
        ScrollBar vertical = canvas.getVerticalBar();
        horizontal.addSelectionListener(new SelectionAdapter() {
            @Override
            public void widgetSelected(SelectionEvent e) {
                postRedraw();
            }
        });
        vertical.addSelectionListener(new SelectionAdapter() {
            @Override
            public void widgetSelected(SelectionEvent e) {
                postRedraw();
            }
        });

        undoContext = new UndoContext();
        history = PlatformUI.getWorkbench().getOperationSupport()
                .getOperationHistory();
        history.addOperationHistoryListener(this);
        history.setLimit(undoContext, 100);

        IOperationApprover approver = new LinearUndoViolationUserApprover(
                undoContext, this);
        history.addOperationApprover(approver);

        actions.put(ActionFactory.UNDO.getId(),
                new UndoActionHandler(this.getEditorSite(), undoContext));
        actions.put(ActionFactory.REDO.getId(),
                new RedoActionHandler(this.getEditorSite(), undoContext));

        getSite().setSelectionProvider(selectionProvider);
        getSite().getWorkbenchWindow().getSelectionService().addSelectionListener(this);

        IContextService contextService = (IContextService) getSite()
                .getService(IContextService.class);
        contextService
                .activateContext("com.dynamo.cr.guieditor.contexts.GuiEditor");
    }

    private void updateViewPort() {
        Rectangle client = canvas.getClientArea();
        viewPort[0] = 0;
        viewPort[1] = 0;
        viewPort[2] = client.width;
        viewPort[3] = client.height;
    }

    @Override
    public void handleEvent(Event event) {
        if (event.type == SWT.Resize) {
            updateViewPort();
        } else if (event.type == SWT.Paint) {
            paint();
        }
    }

    private void paint() {
        if (!canvas.isDisposed()) {
            canvas.setCurrent();
            context.makeCurrent();
            GL2 gl = context.getGL().getGL2();
            GLU glu = new GLU();
            try {
                gl.glDisable(GL2.GL_LIGHTING);
                gl.glPolygonMode(GL.GL_FRONT_AND_BACK, GL2.GL_FILL);
                gl.glDisable(GL.GL_DEPTH_TEST);

                gl.glMatrixMode(GL2.GL_PROJECTION);
                gl.glLoadIdentity();
                glu.gluOrtho2D(0, viewPort[2], 0, viewPort[3]);
                gl.glViewport(viewPort[0], viewPort[1], viewPort[2], viewPort[3]);

                gl.glMatrixMode(GL2.GL_MODELVIEW);
                gl.glLoadIdentity();
                RGB bg = getScene().getBackgroundColor();
                gl.glClearColor(bg.red / 255.0f, bg.green / 255.0f, bg.blue / 255.0f, 0.0f);
                gl.glClear(GL.GL_COLOR_BUFFER_BIT | GL.GL_DEPTH_BUFFER_BIT);

                doDraw(gl);
            } catch (Throwable e) {
                // Don't show dialog or similar in paint-handle
                e.printStackTrace();
            } finally {
                canvas.swapBuffers();
                context.release();
            }
        }
    }

    private void doDraw(GL2 gl) {
        Display display = Display.getDefault();
        Transform transform = new Transform(display);
        transform.translate(0, 0);

        ScrollBar horizontal = canvas.getHorizontalBar();
        ScrollBar vertical = canvas.getVerticalBar();
        gl.glPushMatrix();

        gl.glTranslatef(-horizontal.getSelection(), -(this.height - canvas.getClientArea().height) + vertical.getSelection(), 0);

        gl.glColor3f(0.5f, 0.5f, 0.5f);
        gl.glPolygonMode(GL.GL_FRONT_AND_BACK, GL2.GL_FILL);

        gl.glDisable(GL.GL_TEXTURE_2D);
        gl.glDisable(GL.GL_BLEND);
        DrawUtil.drawRectangle(gl, 0, 0, this.width, this.height);

        renderer.begin(gl);
        DrawContext drawContext = new DrawContext(renderer, guiScene.getRenderResourceCollection(), selectionProvider.getSelectionList());
        guiScene.draw(drawContext);
        renderer.end();

        if (selectionProvider.size() > 0) {
            drawSelectionBounds(gl);
        }

        renderer.begin(gl);
        guiScene.drawPivot(drawContext);
        renderer.end();

        gl.glPopMatrix();

        if (selectMoveTool != null) {
            selectMoveTool.draw(gl);
        }

    }

    private void drawSelectionBounds(GL2 gl) {
        gl.glDisable(GL2.GL_TEXTURE_2D);
        gl.glDisable(GL2.GL_BLEND);
        gl.glColor3f(0.3f, 0.3f, 0.3f);
        Rectangle2D.Double selectionBounds = selectionProvider.getBounds();
        DrawUtil.drawRectangle(gl, selectionBounds);

        gl.glColor3f(1.0f, 1.0f, 1.0f);
        int size = 8;
        for (double delta[] : new double[][] { { 0, 0 },
                { selectionBounds.getWidth(), 0 },
                { selectionBounds.getWidth(), selectionBounds.getHeight() },
                { 0, selectionBounds.getHeight() }, }) {

            gl.glColor3f(0.0f, 0.0f, 0.0f);
            DrawUtil.fillRectangle(gl, selectionBounds.x + delta[0] - size / 2,
                    selectionBounds.y + delta[1] - size / 2, size, size);

            gl.glColor3f(1.0f, 1.0f, 1.0f);
            DrawUtil.fillRectangle(gl, selectionBounds.x + delta[0] - size / 2
                    + 1, selectionBounds.y + delta[1] - size / 2 + 1, size - 2,
                    size - 2);
        }
    }

    private void updateScrollBars() {
        ScrollBar horizontal = canvas.getHorizontalBar();
        ScrollBar vertical = canvas.getVerticalBar();

        horizontal.setIncrement(16);
        vertical.setIncrement(16);

        Rectangle clientArea = canvas.getClientArea();
        int hThumb = clientArea.width - (this.width - clientArea.width);
        horizontal.setMinimum(0);
        horizontal.setMaximum(this.width);
        horizontal.setThumb(hThumb);

        int vThumb = clientArea.height - (this.height - clientArea.height);
        vertical.setMinimum(0);
        vertical.setMaximum(this.height);
        vertical.setThumb(vThumb);
    }

    @Override
    public void setFocus() {
        canvas.setFocus();
    }

    public void updateActions() {
        IActionBars action_bars = getEditorSite().getActionBars();
        action_bars.updateActionBars();

        for (Entry<String, IAction> e : actions.entrySet()) {
            action_bars.setGlobalActionHandler(e.getKey(), e.getValue());
        }
    }

    @Override
    public void executeOperation(final IUndoableOperation operation) {
        IOperationHistory history = PlatformUI.getWorkbench()
                .getOperationSupport().getOperationHistory();
        operation.addContext(undoContext);
        IStatus status = null;
        try {
            status = history.execute(operation, null, null);

        } catch (final ExecutionException e) {
            Activator.logException(e);
        }

        if (status != Status.OK_STATUS) {
            StatusManager.getManager().handle(status, StatusManager.LOG);
        }
    }

    @Override
    public List<GuiNode> rectangleSelect(int x, int y, int width, int height) {
        ArrayList<GuiNode> toSelect = new ArrayList<GuiNode>(32);

        if (width > 0 && height > 0) {
            context.makeCurrent();
            GL2 gl = context.getGL().getGL2();

            // x and y are center coordinates
            renderer.beginSelect(gl, x + width / 2, y + height / 2, width,
                    height, viewPort);
            DrawContext drawContext = new DrawContext(renderer, guiScene.getRenderResourceCollection(), selectionProvider.getSelectionList());

            ScrollBar horizontal = canvas.getHorizontalBar();
            ScrollBar vertical = canvas.getVerticalBar();
            gl.glTranslatef(-horizontal.getSelection(), -(this.height - canvas.getClientArea().height) + vertical.getSelection(), 0);

            guiScene.drawSelect(drawContext);
            SelectResult result = renderer.endSelect();

            for (Pair pair : result.selected) {
                GuiNode node = guiScene.getNode(pair.index);
                toSelect.add(node);
            }
        }

        // Sort node according to "depth"
        Collections.sort(toSelect, new Comparator<GuiNode>() {
            @Override
            public int compare(GuiNode o1, GuiNode o2) {
                int i1 = guiScene.getNodeIndex(o1);
                int i2 = guiScene.getNodeIndex(o2);
                return i2 - i1;
            }
        });

        return toSelect;
    }

    @Override
    public void mouseDoubleClick(MouseEvent e) {
    }

    @Override
    public void mouseDown(MouseEvent e) {
        e.y = viewPort[3] - e.y;
        selectMoveTool = new SelectMoveTool(this, selectionProvider);
        selectMoveTool.mouseDown(e);
    }

    @Override
    public void mouseUp(MouseEvent e) {
        e.y = viewPort[3] - e.y;
        if (selectMoveTool != null) {
            selectMoveTool.mouseUp(e);
        }
        selectMoveTool = null;
        postRedraw();
    }

    @Override
    public void mouseMove(MouseEvent e) {
        e.y = viewPort[3] - e.y;
        if (selectMoveTool != null) {
            selectMoveTool.mouseMove(e);
            postRefreshPropertySheet();
            postRedraw();
        }
    }

    @Override
    public void keyPressed(KeyEvent e) {
        if (selectMoveTool != null) {
            return;
        }

        selectMoveTool = new SelectMoveTool(this, selectionProvider);
        selectMoveTool.keyPressed(e);
        selectMoveTool = null;
        postRefreshPropertySheet();
        postRedraw();
    }

    @Override
    public void keyReleased(KeyEvent e) {
    }

    private void postRedraw() {
        if (!redrawPosted) {
            redrawPosted = true;
            Display.getDefault().asyncExec(new Runnable() {

                @Override
                public void run() {
                    if (!canvas.isDisposed()) {
                        paint();
                        canvas.update();
                    }
                    redrawPosted = false;
                }
            });
        }
    }

    private void postRefreshPropertySheet() {
        if (!refreshPropertySheetPosted) {
            refreshPropertySheetPosted = true;

            Display.getDefault().timerExec(60 * 2, new Runnable() {

                @Override
                public void run() {
                    refreshPropertySheetPosted = false;
                    propertySheetPage.refresh();
                }
            });
        }
    }

    @Override
    public void historyNotification(OperationHistoryEvent event) {

        if (!event.getOperation().hasContext(this.undoContext)) {
            // Only handle operations related to this editor
            return;
        }


        int eventType = event.getEventType();

        if (!(event.getOperation() instanceof SelectOperation)) {
            if (eventType == OperationHistoryEvent.DONE || eventType == OperationHistoryEvent.REDONE) {
                undoRedoCounter++;
            } else if (eventType == OperationHistoryEvent.UNDONE) {
                undoRedoCounter--;
            }
        }

        if (eventType == OperationHistoryEvent.DONE || eventType == OperationHistoryEvent.REDONE || eventType == OperationHistoryEvent.UNDONE) {
            Display display = Display.getDefault();
            display.asyncExec(new Runnable() {
                @Override
                public void run() {
                    propertySheetPage.refresh();
                    updateDirtyState();
                    postRedraw();
                }
            });
        }
    }

    @Override
    public void resourcesChanged() {
        postRedraw();
    }

    @Override
    public void propertyChanged(GuiNode node, String property) {
        postRedraw();
    }

    @Override
    public void nodeRemoved(GuiNode node) {
        selectionProvider.setSelection(new ArrayList<GuiNode>());
        postRedraw();
    }

    @Override
    public GuiScene getScene() {
        return guiScene;
    }

    @Override
    public List<GuiNode> getSelectedNodes() {
        return new ArrayList<GuiNode>(selectionProvider.getSelectionList());
    }

    @Override
    public GuiSelectionProvider getSelectionProvider() {
        return selectionProvider;
    }

    @Override
    public void selectionChanged(IWorkbenchPart part, ISelection selection) {

        if (part instanceof GuiEditor) {
            postRedraw();
            return;
        }

        if (selection instanceof IStructuredSelection) {
            IStructuredSelection structuredSelection = (IStructuredSelection) selection;
            @SuppressWarnings("unchecked")
            List<Object> selectionList = structuredSelection.toList();
            List<GuiNode> nodes = new ArrayList<GuiNode>();
            for (Object object : selectionList) {
                if (object instanceof GuiNode) {
                    GuiNode node = (GuiNode) object;
                    // Only add nodes that belongs to "this" scene.
                    if (node.getScene() == guiScene) {
                        nodes.add(node);
                    }
                }
            }
            if (nodes.size() > 0) {
                // Only respond to selection if at least one node was selected
                selectionProvider.setSelectionNoFireEvent(nodes);
                postRedraw();
            }
        }
    }

    @Override
    public void resourceChanged(IResourceChangeEvent event) {
        if (this.inSave)
            return;

        final IFileEditorInput input = (IFileEditorInput) getEditorInput();
        try {
            event.getDelta().accept(new IResourceDeltaVisitor() {

                @Override
                public boolean visit(IResourceDelta delta) throws CoreException {
                    if ((delta.getKind() & IResourceDelta.REMOVED) == IResourceDelta.REMOVED) {
                        IResource resource = delta.getResource();
                        if (resource.equals(input.getFile())) {
                            getSite().getShell().getDisplay().asyncExec(new Runnable() {
                                @Override
                                public void run() {
                                    getSite().getPage().closeEditor(GuiEditor.this, false);
                                }
                            });
                            return false;
                        }
                    } else if ((delta.getKind() & IResourceDelta.CHANGED) == IResourceDelta.CHANGED
                            && (delta.getFlags() & IResourceDelta.CONTENT) == IResourceDelta.CONTENT) {
                        IResource resource = delta.getResource();
                        if (resource.equals(input.getFile())) {
                            try {
                                guiScene.removeGuiSceneListener(GuiEditor.this);

                                IProgressService service = PlatformUI.getWorkbench().getProgressService();
                                Loader loader = new Loader(input.getFile());
                                try {
                                    service.runInUI(service, loader, null);
                                    if (loader.exception != null) {
                                        throw new PartInitException(loader.exception.getMessage(),
                                                loader.exception);
                                    }
                                } catch (Throwable e) {
                                    throw new PartInitException(e.getMessage(), e);
                                }

                                guiScene.addGuiSceneListener(GuiEditor.this);
                                postRedraw();

                            } catch (CoreException e) {
                                throw new PartInitException(e.getMessage(), e);
                            }
                        }
                    }
                    return true;
                }
            });
        } catch (CoreException e) {
            Activator.logException(e);
        }
    }

}
