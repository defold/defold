package com.dynamo.cr.tileeditor;

import java.awt.Color;
import java.awt.image.BufferedImage;
import java.util.List;

import javax.vecmath.Vector3f;

import org.eclipse.core.commands.operations.IOperationApprover;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistoryListener;
import org.eclipse.core.commands.operations.OperationHistoryEvent;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.resources.IResourceChangeListener;
import org.eclipse.core.resources.ResourcesPlugin;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.swt.events.KeyEvent;
import org.eclipse.swt.events.KeyListener;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseListener;
import org.eclipse.swt.events.MouseMoveListener;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Listener;
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
import org.eclipse.ui.part.IPageSite;
import org.eclipse.ui.progress.IProgressService;
import org.eclipse.ui.views.contentoutline.IContentOutlinePage;

import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.tileeditor.core.ITileSetView;
import com.dynamo.cr.tileeditor.core.Tag;
import com.dynamo.cr.tileeditor.core.TileSetModel;
import com.dynamo.cr.tileeditor.core.TileSetPresenter;

public class TileSetEditor extends EditorPart implements ITileSetView,
MouseListener, MouseMoveListener, Listener, IOperationHistoryListener,
ISelectionListener, KeyListener, IResourceChangeListener {

    private IContainer contentRoot;
    private IOperationHistory history;
    private UndoContext undoContext;
    private TileSetPresenter presenter;
    private TileSetEditorOutlinePage outlinePage;
    private List<String> collisionGroups;

    // EditorPart

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

        this.undoContext = new UndoContext();
        this.history = PlatformUI.getWorkbench().getOperationSupport()
                .getOperationHistory();
        this.history.addOperationHistoryListener(this);
        this.history.setLimit(this.undoContext, 100);

        IOperationApprover approver = new LinearUndoViolationUserApprover(
                this.undoContext, this);
        this.history.addOperationApprover(approver);

        TileSetModel model = new TileSetModel(this.history, this.undoContext);
        this.presenter = new TileSetPresenter(model, this);

        final String undoId = ActionFactory.UNDO.getId();
        final UndoActionHandler undoHandler = new UndoActionHandler(this.getEditorSite(), undoContext);
        final String redoId = ActionFactory.REDO.getId();
        final RedoActionHandler redoHandler = new RedoActionHandler(this.getEditorSite(), undoContext);

        IActionBars actionBars = site.getActionBars();
        actionBars.setGlobalActionHandler(undoId, undoHandler);
        actionBars.setGlobalActionHandler(redoId, redoHandler);

        this.outlinePage = new TileSetEditorOutlinePage(this.presenter) {
            @Override
            public void init(IPageSite pageSite) {
                super.init(pageSite);
                IActionBars actionBars = pageSite.getActionBars();
                actionBars.setGlobalActionHandler(undoId, undoHandler);
                actionBars.setGlobalActionHandler(redoId, redoHandler);
            }
        };

        IProgressService service = PlatformUI.getWorkbench()
                .getProgressService();
        TileSetLoader loader = new TileSetLoader(file, this.presenter);
        try {
            service.runInUI(service, loader, null);
            if (loader.exception != null) {
                throw new PartInitException(loader.exception.getMessage(),
                        loader.exception);
            }
        } catch (Throwable e) {
            throw new PartInitException(e.getMessage(), e);
        }
    }

    @Override
    public void dispose() {
        super.dispose();

        getSite().getWorkbenchWindow().getSelectionService().removeSelectionListener(this);

        if (history != null) {
            history.removeOperationHistoryListener(this);
            history.removeOperationHistoryListener(this);
        }
        ResourcesPlugin.getWorkspace().removeResourceChangeListener(this);
    }

    @Override
    public void createPartControl(Composite parent) {
        // This makes sure the context will be active while this component is
        IContextService contextService = (IContextService) getSite()
                .getService(IContextService.class);
        contextService.activateContext(Activator.CONTEXT_ID);
    }

    public TileSetPresenter getPresenter() {
        return this.presenter;
    }

    public List<String> getCollisionGroups() {
        return this.collisionGroups;
    }

    @Override
    public boolean isDirty() {
        // TODO Auto-generated method stub
        return false;
    }

    @Override
    public void doSave(IProgressMonitor monitor) {
        // TODO Auto-generated method stub

    }

    @Override
    public void doSaveAs() {
        // TODO Auto-generated method stub

    }

    @Override
    public boolean isSaveAsAllowed() {
        // TODO Auto-generated method stub
        return false;
    }

    @Override
    public void setFocus() {
        // TODO Auto-generated method stub

    }

    // IResourceChangeListener

    @Override
    public void resourceChanged(IResourceChangeEvent event) {
        // TODO Auto-generated method stub

    }

    // ISelectionListener

    @Override
    public void selectionChanged(IWorkbenchPart part, ISelection selection) {
        // TODO Auto-generated method stub

    }

    // IOperationHistoryListener

    @Override
    public void historyNotification(OperationHistoryEvent event) {
        // TODO Auto-generated method stub

    }

    // Listener

    @Override
    public void handleEvent(Event event) {
        // TODO Auto-generated method stub

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

    // ITileSetView

    @Override
    public void setImageProperty(String newValue) {
        // TODO Auto-generated method stub

    }

    @Override
    public void setImageTags(List<Tag> tags) {
        // TODO Auto-generated method stub

    }

    @Override
    public void setTileWidthProperty(int tileWidth) {
        // TODO Auto-generated method stub

    }

    @Override
    public void setTileWidthTags(List<Tag> tags) {
        // TODO Auto-generated method stub

    }

    @Override
    public void setTileHeightProperty(int tileHeight) {
        // TODO Auto-generated method stub

    }

    @Override
    public void setTileHeightTags(List<Tag> tags) {
        // TODO Auto-generated method stub

    }

    @Override
    public void setTileMarginProperty(int tileMargin) {
        // TODO Auto-generated method stub

    }

    @Override
    public void setTileMarginTags(List<Tag> tags) {
        // TODO Auto-generated method stub

    }

    @Override
    public void setTileSpacingProperty(int tileSpacing) {
        // TODO Auto-generated method stub

    }

    @Override
    public void setTileSpacingTags(List<Tag> tags) {
        // TODO Auto-generated method stub

    }

    @Override
    public void setCollisionProperty(String newValue) {
        // TODO Auto-generated method stub

    }

    @Override
    public void setCollisionTags(List<Tag> tags) {
        // TODO Auto-generated method stub

    }

    @Override
    public void setMaterialTagProperty(String newValue) {
        // TODO Auto-generated method stub

    }

    @Override
    public void setMaterialTagTags(List<Tag> tags) {
        // TODO Auto-generated method stub

    }

    @Override
    public void setCollisionGroups(List<String> collisionGroups, List<Color> colors) {
        this.collisionGroups = collisionGroups;
        outlinePage.setInput(collisionGroups, colors);
    }

    @Override
    public void setTiles(BufferedImage image, float[] v, int[] hullIndices,
            int[] hullCounts, Color[] hullColors, Vector3f hullScale) {
        // TODO Auto-generated method stub

    }

    @Override
    public void clearTiles() {
        // TODO Auto-generated method stub

    }

    @Override
    public void setTileHullColor(int tileIndex, Color color) {
        // TODO Auto-generated method stub

    }

    @Override
    public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
        if (adapter == IContentOutlinePage.class) {
            if (this.outlinePage == null)
                this.outlinePage = new TileSetEditorOutlinePage(this.presenter);
            return this.outlinePage;
        }

        return super.getAdapter(adapter);
    }

}
