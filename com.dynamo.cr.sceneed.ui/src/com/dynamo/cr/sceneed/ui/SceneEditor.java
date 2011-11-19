package com.dynamo.cr.sceneed.ui;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;

import javax.inject.Singleton;

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
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Display;
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
import org.eclipse.ui.dialogs.SaveAsDialog;
import org.eclipse.ui.operations.RedoActionHandler;
import org.eclipse.ui.operations.UndoActionHandler;
import org.eclipse.ui.part.FileEditorInput;
import org.eclipse.ui.part.IContributedContentsView;
import org.eclipse.ui.progress.IProgressService;
import org.eclipse.ui.statushandlers.StatusManager;
import org.eclipse.ui.views.contentoutline.ContentOutline;
import org.eclipse.ui.views.contentoutline.IContentOutlinePage;
import org.eclipse.ui.views.properties.IPropertySheetPage;

import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.core.inject.LifecycleModule;
import com.dynamo.cr.properties.IFormPropertySheetPage;
import com.dynamo.cr.sceneed.core.ILogger;
import com.dynamo.cr.sceneed.core.INodeTypeRegistry;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.ISceneView.Context;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.ui.preferences.PreferenceConstants;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;

public class SceneEditor extends AbstractDefoldEditor implements ISelectionListener, IPropertyChangeListener {

    private ISceneOutlinePage outlinePage;
    private IFormPropertySheetPage propertySheetPage;
    private RenderView renderView;

    private IContainer contentRoot;
    private LifecycleModule module;
    private ISceneView.Presenter presenter;
    private boolean dirty;

    class Module extends AbstractModule {
        @Override
        protected void configure() {
            bind(ISceneOutlinePage.class).to(SceneOutlinePage.class).in(Singleton.class);
            bind(IFormPropertySheetPage.class).to(ScenePropertySheetPage.class).in(Singleton.class);
            bind(ISceneView.class).to(SceneView.class).in(Singleton.class);
            bind(RenderView.class).in(Singleton.class);
            bind(ISceneModel.class).to(SceneModel.class).in(Singleton.class);
            bind(INodeTypeRegistry.class).toInstance(com.dynamo.cr.sceneed.core.Activator.getDefault());
            bind(ISceneView.Presenter.class).to(ScenePresenter.class).in(Singleton.class);
            bind(SceneEditor.class).toInstance(SceneEditor.this);

            bind(IOperationHistory.class).toInstance(history);
            bind(IUndoContext.class).toInstance(undoContext);
            bind(UndoActionHandler.class).toInstance(undoHandler);
            bind(RedoActionHandler.class).toInstance(redoHandler);

            bind(ILogger.class).to(Logger.class);

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

        this.module = new LifecycleModule(new Module());
        Injector injector = Guice.createInjector(module);

        final String undoId = ActionFactory.UNDO.getId();
        final String redoId = ActionFactory.REDO.getId();

        IActionBars actionBars = site.getActionBars();
        actionBars.setGlobalActionHandler(undoId, undoHandler);
        actionBars.setGlobalActionHandler(redoId, redoHandler);

        this.outlinePage = injector.getInstance(ISceneOutlinePage.class);
        this.propertySheetPage = injector.getInstance(IFormPropertySheetPage.class);
        this.renderView = injector.getInstance(RenderView.class);

        this.presenter = injector.getInstance(ISceneView.Presenter.class);

        IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        store.addPropertyChangeListener(this);

        IProgressService service = PlatformUI.getWorkbench().getProgressService();

        this.dirty = false;

        SceneLoader loader = new SceneLoader(file, this.presenter);
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
        module.close();
        if (this.renderView != null) {
            this.renderView.dispose();
        }
        IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        store.removePropertyChangeListener(this);

        getSite().getPage().removeSelectionListener(this);
    }

    @Override
    protected void doReload(IFile file) {
        // TODO Auto-generated method stub

    }

    @Override
    protected void handleResourceChanged(final IResourceChangeEvent event) {
        Display display= getSite().getShell().getDisplay();
        display.asyncExec(new Runnable() {
            @Override
            public void run() {
                try {
                    presenter.onResourceChanged(event);
                } catch (Throwable e) {
                    logger.logException(e);
                }
            }
        });
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
            logger.logException(e);
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
        return this.dirty;
    }

    @Override
    public boolean isSaveAsAllowed() {
        return true;
    }

    @Override
    public void createPartControl(Composite parent) {
        this.renderView.createControls(parent);

        // This makes sure the context will be active while this component is
        IContextService contextService = (IContextService) getSite()
                .getService(IContextService.class);
        contextService.activateContext(Activator.SCENEED_CONTEXT_ID);

        // Set the outline as selection provider
        getSite().setSelectionProvider(this.renderView);
        getSite().getPage().addSelectionListener(this);

        this.presenter.onRefresh();
    }

    @Override
    public void setFocus() {
        this.renderView.setFocus();
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

    public ISceneView.Presenter getPresenter() {
        return this.presenter;
    }

    public ISceneView.NodePresenter getNodePresenter(Class<? extends Node> c) {
        return com.dynamo.cr.sceneed.core.Activator.getDefault().getPresenter(c);
    }

    @Override
    public void selectionChanged(IWorkbenchPart part, ISelection selection) {
        boolean currentSelection = false;
        if (part == this) {
            currentSelection = true;
        } else if (part instanceof ContentOutline) {
            IContributedContentsView view = (IContributedContentsView)((ContentOutline)part).getAdapter(IContributedContentsView.class);
            currentSelection = view.getContributingPart() == this;
        }
        if (currentSelection && selection instanceof IStructuredSelection) {
            this.presenter.onSelect((IStructuredSelection)selection);
        }
    }

    public void setDirty(boolean dirty) {
        if (this.dirty != dirty) {
            this.dirty = dirty;
            firePropertyChange(PROP_DIRTY);
        }
    }

    public Context getContext() {
        return this.presenter.getContext();
    }

    @Override
    public void propertyChange(PropertyChangeEvent event) {
        if (event.getSource().equals(Activator.getDefault().getPreferenceStore())) {
            if (event.getProperty().equals(PreferenceConstants.P_TOP_BKGD_COLOR)
                    || event.getProperty().equals(PreferenceConstants.P_BOTTOM_BKGD_COLOR)) {
                this.renderView.refresh();
            }
        }
    }

}
