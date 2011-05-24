package com.dynamo.cr.guieditor;

import java.awt.geom.Rectangle2D;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;

import javax.media.opengl.GL;
import javax.media.opengl.GLContext;
import javax.media.opengl.GLDrawableFactory;
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
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.core.runtime.SubMonitor;
import org.eclipse.jface.action.IAction;
import org.eclipse.jface.dialogs.ErrorDialog;
import org.eclipse.jface.operation.IRunnableWithProgress;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.ControlAdapter;
import org.eclipse.swt.events.ControlEvent;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseListener;
import org.eclipse.swt.events.MouseMoveListener;
import org.eclipse.swt.events.SelectionAdapter;
import org.eclipse.swt.events.SelectionEvent;
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
import org.eclipse.ui.PartInitException;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.actions.ActionFactory;
import org.eclipse.ui.contexts.IContextService;
import org.eclipse.ui.operations.LinearUndoViolationUserApprover;
import org.eclipse.ui.operations.RedoActionHandler;
import org.eclipse.ui.operations.UndoActionHandler;
import org.eclipse.ui.part.EditorPart;
import org.eclipse.ui.progress.IProgressService;
import org.eclipse.ui.views.contentoutline.IContentOutlinePage;
import org.eclipse.ui.views.properties.IPropertySheetPage;
import org.eclipse.ui.views.properties.PropertySheetPage;

import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.guieditor.operations.SelectOperation;
import com.dynamo.cr.guieditor.render.GuiRenderer;
import com.dynamo.cr.guieditor.render.SelectResult;
import com.dynamo.cr.guieditor.render.SelectResult.Pair;
import com.dynamo.cr.guieditor.scene.GuiNode;
import com.dynamo.cr.guieditor.scene.GuiScene;
import com.dynamo.cr.guieditor.scene.IGuiSceneListener;
import com.dynamo.cr.guieditor.tools.SelectMoveTool;
import com.dynamo.cr.guieditor.util.DrawUtil;
import com.dynamo.gui.proto.Gui.SceneDesc;
import com.google.protobuf.TextFormat;

public class GuiEditor extends EditorPart implements IGuiEditor, MouseListener,
        MouseMoveListener, Listener, IOperationHistoryListener,
        IGuiSceneListener {

    private GLCanvas canvas;
    private GLContext context;
    private boolean redrawPosted = false;
    private final int[] viewPort = new int[4];
    private GuiScene guiScene;
    private GuiSelectionProvider selectionProvider;
    private Map<String, IAction> actions = new HashMap<String, IAction>();
    private UndoContext undoContext;
    private PropertySheetPage propertySheetPage;
    private SelectMoveTool selectMoveTool;
    private boolean refreshPropertySheetPosted;
    private IOperationHistory history;
    private boolean dirty = false;
    private IContainer contentRoot;
    private GuiRenderer renderer;
    private GuiEditorOutlinePage outlinePage;
    private int undoRedoCounter = 0;

    public GuiEditor() {
        selectionProvider = new GuiSelectionProvider();
        renderer = new GuiRenderer();
        propertySheetPage = new PropertySheetPage() {
            public void setActionBars(IActionBars actionBars) {
                super.setActionBars(actionBars);
                String undoId = ActionFactory.UNDO.getId();
                String redoId = ActionFactory.REDO.getId();
                actionBars.setGlobalActionHandler(undoId, actions.get(undoId));
                actionBars.setGlobalActionHandler(redoId, actions.get(redoId));
            }
        };
    }

    @Override
    public IAction getAction(String id) {
        return actions.get(id);
    }

    @Override
    public void doSave(IProgressMonitor monitor) {

        IFileEditorInput i = (IFileEditorInput) getEditorInput();
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
            ErrorDialog.openError(Display.getCurrent().getActiveShell(),
                    "Unable to save file", "Unable to save file", status);
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

        guiScene.addPropertyChangeListener(this);
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
            guiScene.removePropertyChangeListener(this);
        }
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

        canvas.setCurrent();

        context = GLDrawableFactory.getFactory().createExternalGLContext();

        context.makeCurrent();
        GL gl = context.getGL();
        gl.glClearDepth(1.0f);
        gl.glEnable(GL.GL_DEPTH_TEST);
        gl.glDepthFunc(GL.GL_LEQUAL);
        gl.glHint(GL.GL_PERSPECTIVE_CORRECTION_HINT, GL.GL_NICEST);

        context.release();

        canvas.addListener(SWT.Resize, this);
        canvas.addListener(SWT.Paint, this);
        canvas.addMouseListener(this);
        canvas.addMouseMoveListener(this);

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
            gl.glClearColor(0.6f, 0.6f, 0.6f, 1);
            gl.glClear(GL.GL_COLOR_BUFFER_BIT | GL.GL_DEPTH_BUFFER_BIT);

            doDraw(gl);
        } catch (Throwable e) {
            e.printStackTrace();
        } finally {
            canvas.swapBuffers();
            context.release();
        }
    }

    private void doDraw(GL gl) {
        this.redrawPosted = false;

        Display display = Display.getDefault();
        Transform transform = new Transform(display);
        transform.translate(0, 0);

        ScrollBar horizontal = canvas.getHorizontalBar();
        ScrollBar vertical = canvas.getVerticalBar();
        gl.glPushMatrix();

        int refWidth = guiScene.getReferenceWidth();
        int refHeight = guiScene.getReferenceHeight();

        gl.glColor3f(0, 0, 1.0f / 200.0f);
        gl.glPolygonMode(GL.GL_FRONT_AND_BACK, GL.GL_FILL);

        DrawUtil.drawRectangle(gl, 0, 0, refWidth, refHeight);

        gl.glTranslatef(-horizontal.getSelection(), vertical.getSelection(), 0);
        renderer.begin(gl);
        DrawContext drawContext = new DrawContext(renderer,
                selectionProvider.getSelectionList());
        guiScene.draw(drawContext);
        renderer.end();

        if (selectionProvider.size() > 0) {
            drawSelectionBounds(gl);
        }

        gl.glPopMatrix();

        if (selectMoveTool != null) {
            selectMoveTool.draw(gl);
        }

    }

    private void drawSelectionBounds(GL gl) {
        gl.glDisable(GL.GL_TEXTURE_2D);
        gl.glDisable(GL.GL_BLEND);
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

        int refWidth = guiScene.getReferenceWidth();
        int refHeight = guiScene.getReferenceHeight();

        Rectangle clientArea = canvas.getClientArea();
        int hThumb = clientArea.width - (refWidth - clientArea.width);
        horizontal.setMinimum(0);
        horizontal.setMaximum(refWidth);
        horizontal.setThumb(hThumb);

        int vThumb = clientArea.height - (refHeight - clientArea.height);
        vertical.setMinimum(0);
        vertical.setMaximum(refHeight);
        vertical.setThumb(vThumb);
    }

    @Override
    public void setFocus() {
        canvas.setFocus();
        updateActions();
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
            e.printStackTrace();
        }

        if (status != Status.OK_STATUS) {
            System.err.println("Failed to execute operation: " + operation);
        }
    }

    @Override
    public List<GuiNode> rectangleSelect(int x, int y, int width, int height) {
        ArrayList<GuiNode> toSelect = new ArrayList<GuiNode>(32);

        if (width > 0 && height > 0) {
            context.makeCurrent();
            GL gl = context.getGL();

            // x and y are center coordinates
            renderer.beginSelect(gl, x + width / 2, y + height / 2, width,
                    height, viewPort);
            DrawContext drawContext = new DrawContext(renderer,
                    selectionProvider.getSelectionList());

            ScrollBar horizontal = canvas.getHorizontalBar();
            ScrollBar vertical = canvas.getVerticalBar();
            gl.glTranslatef(-horizontal.getSelection(),
                    vertical.getSelection(), 0);

            guiScene.drawSelect(drawContext);
            SelectResult result = renderer.endSelect();

            for (Pair pair : result.selected) {
                GuiNode node = guiScene.getNode(pair.index);
                toSelect.add(node);
            }
        }
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

    private void postRedraw() {
        if (!redrawPosted) {
            redrawPosted = true;
            if (!canvas.isDisposed()) {
                canvas.redraw();
            }
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
        if (!(event.getOperation() instanceof SelectOperation)) {
            if (event.getEventType() == OperationHistoryEvent.DONE || event.getEventType() == OperationHistoryEvent.REDONE) {
                undoRedoCounter++;
            } else if (event.getEventType() == OperationHistoryEvent.UNDONE) {
                undoRedoCounter--;
            }
        }

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

}
