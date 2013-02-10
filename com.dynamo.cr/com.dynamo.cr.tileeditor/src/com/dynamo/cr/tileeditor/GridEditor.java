package com.dynamo.cr.tileeditor;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;

import javax.inject.Singleton;

import org.eclipse.core.commands.Command;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.resources.IWorkspace;
import org.eclipse.core.resources.ResourcesPlugin;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.action.IStatusLineManager;
import org.eclipse.jface.preference.IPreferenceStore;
import org.eclipse.jface.util.IPropertyChangeListener;
import org.eclipse.jface.util.PropertyChangeEvent;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.window.Window;
import org.eclipse.swt.graphics.Cursor;
import org.eclipse.swt.graphics.ImageData;
import org.eclipse.swt.graphics.ImageLoader;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Display;
import org.eclipse.ui.IActionBars;
import org.eclipse.ui.IEditorInput;
import org.eclipse.ui.IEditorSite;
import org.eclipse.ui.IFileEditorInput;
import org.eclipse.ui.IWorkbenchPart;
import org.eclipse.ui.PartInitException;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.actions.ActionFactory;
import org.eclipse.ui.commands.ICommandService;
import org.eclipse.ui.contexts.IContextService;
import org.eclipse.ui.dialogs.SaveAsDialog;
import org.eclipse.ui.handlers.HandlerUtil;
import org.eclipse.ui.operations.RedoActionHandler;
import org.eclipse.ui.operations.UndoActionHandler;
import org.eclipse.ui.part.FileEditorInput;
import org.eclipse.ui.progress.IProgressService;
import org.eclipse.ui.statushandlers.StatusManager;
import org.eclipse.ui.views.contentoutline.IContentOutlinePage;
import org.eclipse.ui.views.properties.IPropertySheetPage;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.core.inject.LifecycleModule;
import com.dynamo.cr.editor.ui.AbstractDefoldEditor;
import com.dynamo.cr.properties.FormPropertySheetPage;
import com.dynamo.cr.tileeditor.commands.ShowPalette;
import com.dynamo.cr.tileeditor.core.GridModel;
import com.dynamo.cr.tileeditor.core.GridPresenter;
import com.dynamo.cr.tileeditor.core.IGridView;
import com.dynamo.cr.tileeditor.core.IGridView.Presenter;
import com.dynamo.cr.tileeditor.core.Layer;
import com.dynamo.cr.tileeditor.handlers.ShowGridHandler;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;

public class GridEditor extends AbstractDefoldEditor implements IPropertyChangeListener {

    public static final int CURSOR_TYPE_PENCIL = 0;
    public static final int CURSOR_TYPE_ERASER = 1;
    public static final int CURSOR_TYPE_UNAVAILABLE = 2;
    public static final int CURSOR_TYPE_CROSS = 3;
    public static final int CURSOR_TYPE_COUNT = 4;

    private static Logger logger = LoggerFactory.getLogger(GridEditor.class);

    private IGridEditorOutlinePage outlinePage;
    private FormPropertySheetPage propertySheetPage;

    private IGridView.Presenter presenter;
    private GridRenderer renderer;

    private IContainer contentRoot;
    private LifecycleModule module;

    private Cursor[] cursors = new Cursor[CURSOR_TYPE_COUNT];
    private String[] cursorPaths = new String[] {
            "/icons/pencil.png",
            "/icons/draw_eraser.png",
            // NOTE: Icons from http://gitorious.org/opensuse/art/trees/master/cursors/dmz/pngs/24x24
            // MIT license
            "/icons/unavailable.png",
            "/icons/cross.png",
    };

    class Module extends AbstractModule {
        @Override
        protected void configure() {
            bind(IGridEditorOutlinePage.class).to(GridEditorOutlinePage.class).in(Singleton.class);
            bind(IGridView.class).to(GridView.class).in(Singleton.class);
            bind(IGridView.Presenter.class).to(GridPresenter.class).in(Singleton.class);
            bind(GridModel.class).in(Singleton.class);
            bind(GridRenderer.class).in(Singleton.class);
            bind(GridEditor.class).toInstance(GridEditor.this);

            bind(IOperationHistory.class).toInstance(history);
            bind(IUndoContext.class).toInstance(undoContext);
            bind(UndoActionHandler.class).toInstance(undoHandler);
            bind(RedoActionHandler.class).toInstance(redoHandler);

            bind(IContainer.class).toInstance(contentRoot);

        }
    }

    @Override
    public void init(IEditorSite site, IEditorInput input)
            throws PartInitException {

        super.init(site, input);

        IFileEditorInput fileEditorInput = (IFileEditorInput) input;
        final IFile file = fileEditorInput.getFile();
        this.contentRoot = EditorUtil.findContentRoot(file);
        if (this.contentRoot == null) {
            throw new PartInitException(
                    "Unable to locate content root for project");
        }

        module = new LifecycleModule(new Module());
        Injector injector = Guice.createInjector(module);

        final String undoId = ActionFactory.UNDO.getId();
        final String redoId = ActionFactory.REDO.getId();

        IActionBars actionBars = site.getActionBars();
        actionBars.setGlobalActionHandler(undoId, undoHandler);
        actionBars.setGlobalActionHandler(redoId, redoHandler);

        this.outlinePage = injector.getInstance(GridEditorOutlinePage.class);

        final GridModel model = injector.getInstance(GridModel.class);
        this.propertySheetPage = new FormPropertySheetPage(this.contentRoot) {
            @Override
            public void setActionBars(IActionBars actionBars) {
                super.setActionBars(actionBars);
                actionBars.setGlobalActionHandler(undoId, undoHandler);
                actionBars.setGlobalActionHandler(redoId, redoHandler);
            }

            @Override
            public void selectionChanged(IWorkbenchPart part,
                    ISelection selection) {
                if (selection instanceof IStructuredSelection) {
                    IStructuredSelection structuredSelection = (IStructuredSelection)selection;
                    for (Object object : structuredSelection.toArray()) {
                        if (!(object instanceof Layer)) {
                            getViewer().setInput(new Object[] {model});
                            return;
                        }
                    }
                }
                super.selectionChanged(part, selection);
            }
        };

        this.presenter = injector.getInstance(IGridView.Presenter.class);
        this.renderer = injector.getInstance(GridRenderer.class);

        ImageLoader imageLoader = new ImageLoader();
        Display display = this.getSite().getShell().getDisplay();
        for (int i = 0; i < CURSOR_TYPE_COUNT; ++i) {
            ImageData[] data = imageLoader.load(getClass().getResourceAsStream(this.cursorPaths[i]));
            this.cursors[i] = new Cursor(display, data[0], 0, 15);
        }

        IProgressService service = PlatformUI.getWorkbench()
                .getProgressService();
        GridLoader loader = new GridLoader(file, this.presenter);
        try {
            service.runInUI(service, loader, null);
            if (loader.exception != null) {
                throw new PartInitException(loader.exception.getMessage(),
                        loader.exception);
            }
        } catch (Throwable e) {
            throw new PartInitException(e.getMessage(), e);
        }
        IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        store.addPropertyChangeListener(this);
        this.renderer.setVisibleGrid(!store.getBoolean(ShowGridHandler.PREFERENCE_NAME));
    }

    @Override
    public void dispose() {
        super.dispose();
        module.close();
        if (this.renderer != null) {
            this.renderer.dispose();
        }
        for (int i = 0; i < CURSOR_TYPE_COUNT; ++i) {
            if (this.cursors[i] != null) {
                this.cursors[i].dispose();
            }
        }
        IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        store.removePropertyChangeListener(this);
    }

    @Override
    public void createPartControl(Composite parent) {

        this.renderer.createControls(parent);

        // This makes sure the context will be active while this component is
        IContextService contextService = (IContextService) getSite()
                .getService(IContextService.class);
        contextService.activateContext(Activator.GRID_CONTEXT_ID);

        // Set the outline as selection provider
        getSite().setSelectionProvider(this.outlinePage);

        this.presenter.onRefresh();
    }

    @Override
    public void updateActions() {
        super.updateActions();

        // Make sure the state of the command is updated when switching between multiple editor instances
        // Maybe not the best solution, but the only known one so far
        ICommandService commandService = (ICommandService)getSite().getService(ICommandService.class);
        Command command = commandService.getCommand(ShowPalette.COMMAND_ID);
        boolean isShowing = this.renderer.isShowingPalette();
        try {
            boolean prevValue = HandlerUtil.toggleCommandState(command);
            if (prevValue == isShowing) {
                HandlerUtil.toggleCommandState(command);
            }
        } catch (ExecutionException e) {
            logger.error("Error occurred while updating actions", e);
        }
    }

    @Override
    protected void doReload(IFile file) {
        IProgressService service = PlatformUI.getWorkbench()
                .getProgressService();
        GridLoader loader = new GridLoader(file, this.presenter);
        try {
            service.runInUI(service, loader, null);
            if (loader.exception != null) {
                logger.error("Error occurred while reloading", loader.exception);
            }
        } catch (Throwable e) {
            logger.error("Error occurred while reloading", e);
        }
    }

    @Override
    protected void handleResourceChanged(final IResourceChangeEvent event) {
        if (!this.inSave) {
            Display display= getSite().getShell().getDisplay();
            display.asyncExec(new Runnable() {
                @Override
                public void run() {
                    try {
                        presenter.onResourceChanged(event);
                    } catch (Throwable e) {
                        logger.error("Error occurred while reloading", e);
                    }
                }
            });
        }
    }

    @Override
    public void doSave(IProgressMonitor monitor) {
        IFileEditorInput input = (IFileEditorInput) getEditorInput();
        IFile file = input.getFile();
        this.inSave = true;
        try {
            ByteArrayOutputStream stream = new ByteArrayOutputStream();
            this.presenter.onSave(stream, monitor);
            file.setContents(
                    new ByteArrayInputStream(stream.toByteArray()), false,
                    true, monitor);
        } catch (Throwable e) {
            logger.error("Error occurred while saving", e);
        } finally {
            this.inSave = false;
        }
    }

    @Override
    public void doSaveAs() {
        IFileEditorInput input= (IFileEditorInput) getEditorInput();
        IFile file = input.getFile();
        SaveAsDialog dialog = new SaveAsDialog(getSite().getShell());
        dialog.setOriginalFile(file);
        dialog.create();

        if (dialog.open() == Window.OK) {
            IPath filePath = dialog.getResult();
            if (filePath == null) {
                return;
            }

            IWorkspace workspace = ResourcesPlugin.getWorkspace();
            IFile newFile= workspace.getRoot().getFile(filePath);

            try {
                newFile.create(new ByteArrayInputStream(new byte[0]), IFile.FORCE, new NullProgressMonitor());
            } catch (CoreException e) {
                Status status = new Status(IStatus.ERROR, Activator.PLUGIN_ID, 0,
                        e.getMessage(), null);
                StatusManager.getManager().handle(status, StatusManager.LOG | StatusManager.SHOW);
                return;
            }
            FileEditorInput newInput = new FileEditorInput(newFile);
            setInput(newInput);
            setPartName(newInput.getName());

            IStatusLineManager lineManager = getEditorSite().getActionBars().getStatusLineManager();
            IProgressMonitor pm = lineManager.getProgressMonitor();
            doSave(pm);
        }
    }

    @Override
    public boolean isDirty() {
        return presenter.isDirty();
    }

    @Override
    public boolean isSaveAsAllowed() {
        return true;
    }

    @Override
    public void setFocus() {
        this.renderer.setFocus();
    }

    @Override
    public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
        if (adapter == IPropertySheetPage.class) {
            return this.propertySheetPage;
        } else if (adapter == IContentOutlinePage.class) {
            return this.outlinePage;
        } else {
            return super.getAdapter(adapter);
        }
    }

    public void showPalette(boolean show) {
        this.renderer.showPalette(show);
    }

    public void fireDirty() {
        firePropertyChange(PROP_DIRTY);
    }

    public void refreshProperties() {
        this.propertySheetPage.refresh();
    }

    public Presenter getPresenter() {
        return this.presenter;
    }

    public boolean isRenderingEnabled() {
        return this.renderer.isEnabled();
    }

    public Cursor getCursor(int cursorType) {
        return this.cursors[cursorType];
    }

    @Override
    public void propertyChange(PropertyChangeEvent event) {
        if (event.getSource().equals(Activator.getDefault().getPreferenceStore())) {
            String property = event.getProperty();
            if (property.equals(ShowGridHandler.PREFERENCE_NAME)) {
                this.renderer.setVisibleGrid(!(Boolean)event.getNewValue());
            }
        }
    }

}
