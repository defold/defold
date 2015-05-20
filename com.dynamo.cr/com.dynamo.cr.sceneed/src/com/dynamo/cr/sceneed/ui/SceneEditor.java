package com.dynamo.cr.sceneed.ui;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;

import javax.inject.Singleton;
import javax.media.opengl.GL2;

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
import org.eclipse.jface.window.Window;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Display;
import org.eclipse.ui.IActionBars;
import org.eclipse.ui.IEditorInput;
import org.eclipse.ui.IEditorSite;
import org.eclipse.ui.IFileEditorInput;
import org.eclipse.ui.IPartListener;
import org.eclipse.ui.IWorkbenchPart;
import org.eclipse.ui.PartInitException;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.actions.ActionFactory;
import org.eclipse.ui.contexts.IContextService;
import org.eclipse.ui.dialogs.SaveAsDialog;
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
import com.dynamo.cr.editor.core.ProjectProperties;
import com.dynamo.cr.editor.core.inject.LifecycleModule;
import com.dynamo.cr.editor.ui.AbstractDefoldEditor;
import com.dynamo.cr.editor.ui.IImageProvider;
import com.dynamo.cr.properties.IFormPropertySheetPage;
import com.dynamo.cr.sceneed.Activator;
import com.dynamo.cr.sceneed.core.CameraController;
import com.dynamo.cr.sceneed.core.IClipboard;
import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.IManipulatorMode;
import com.dynamo.cr.sceneed.core.IManipulatorRegistry;
import com.dynamo.cr.sceneed.core.IModelListener;
import com.dynamo.cr.sceneed.core.INodeType;
import com.dynamo.cr.sceneed.core.INodeTypeRegistry;
import com.dynamo.cr.sceneed.core.IRenderView;
import com.dynamo.cr.sceneed.core.ISceneEditor;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.ManipulatorController;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.SceneGrid;
import com.dynamo.cr.sceneed.core.SceneModel;
import com.dynamo.cr.sceneed.core.ScenePresenter;
import com.dynamo.cr.sceneed.handlers.ShowGridHandler;
import com.dynamo.cr.sceneed.handlers.ShowGroupHandler;
import com.dynamo.cr.sceneed.handlers.ShowOutlineHandler;
import com.dynamo.cr.sceneed.ui.preferences.PreferenceConstants;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.util.Modules;

public class SceneEditor extends AbstractDefoldEditor implements ISceneEditor, IPropertyChangeListener, IPartListener {


    private static Logger logger = LoggerFactory.getLogger(SceneEditor.class);
    private ISceneOutlinePage outlinePage;
    private IFormPropertySheetPage propertySheetPage;
    private IRenderView renderView;
    @SuppressWarnings("unused")
    private BackgroundRenderViewProvider backgroundRenderViewProvider;
    @SuppressWarnings("unused")
    private GridRenderViewProvider gridRenderViewProvider;

    private IContainer contentRoot;
    private LifecycleModule module;
    private Injector injector;
    private ISceneView.IPresenter presenter;
    private ISceneView.IPresenterContext presenterContext;
    private ILoaderContext loaderContext;
    private INodeTypeRegistry nodeTypeRegistry;
    // TODO Currently only needed for dispose(), see below
    private ISceneModel sceneModel;
    private ISceneView sceneView;

    private boolean dirty;
    protected SceneRenderViewProvider sceneRenderViewProvider;
    @SuppressWarnings("unused")
    private CameraController cameraController;
    private ManipulatorController manipulatorController;
    private IManipulatorRegistry manipulatorRegistry;

    class Module extends AbstractModule {
        @Override
        protected void configure() {
            bind(ISceneOutlinePage.class).to(SceneOutlinePage.class).in(Singleton.class);
            bind(IFormPropertySheetPage.class).to(ScenePropertySheetPage.class).in(Singleton.class);
            bind(ISceneView.class).to(SceneView.class).in(Singleton.class);
            bind(IRenderView.class).to(RenderView.class).in(Singleton.class);
            bind(BackgroundRenderViewProvider.class).in(Singleton.class);
            bind(GridRenderViewProvider.class).in(Singleton.class);
            bind(SceneRenderViewProvider.class).in(Singleton.class);
            bind(ISceneModel.class).to(SceneModel.class).in(Singleton.class);
            bind(INodeTypeRegistry.class).toInstance(nodeTypeRegistry);
            bind(ISceneView.IPresenter.class).to(ScenePresenter.class).in(Singleton.class);
            bind(IModelListener.class).to(ScenePresenter.class).in(Singleton.class);
            bind(SceneEditor.class).toInstance(SceneEditor.this);
            bind(ILoaderContext.class).to(LoaderContext.class).in(Singleton.class);
            bind(IPresenterContext.class).to(PresenterContext.class).in(Singleton.class);
            bind(IImageProvider.class).toInstance(Activator.getDefault());
            bind(IClipboard.class).to(SceneClipboard.class).in(Singleton.class);

            bind(CameraController.class).in(Singleton.class);

            bind(ManipulatorController.class).in(Singleton.class);
            bind(IManipulatorRegistry.class).toInstance(manipulatorRegistry);

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
        getEditorSite().getPage().addPartListener(this);

        IFileEditorInput fileEditorInput = (IFileEditorInput) input;
        final IFile file = fileEditorInput.getFile();
        this.contentRoot = EditorUtil.findContentRoot(file);
        if (this.contentRoot == null) {
            throw new PartInitException(
                    "Unable to locate content root for project");
        }

        this.nodeTypeRegistry = Activator.getDefault().getNodeTypeRegistry();
        this.manipulatorRegistry = Activator.getDefault().getManipulatorRegistry();

        this.module = new LifecycleModule(Modules.override(new Module()).with(createOverrideModule()));
        this.injector = Guice.createInjector(module);

        final String undoId = ActionFactory.UNDO.getId();
        final String redoId = ActionFactory.REDO.getId();

        IActionBars actionBars = site.getActionBars();
        actionBars.setGlobalActionHandler(undoId, undoHandler);
        actionBars.setGlobalActionHandler(redoId, redoHandler);

        this.outlinePage = this.injector.getInstance(ISceneOutlinePage.class);
        this.propertySheetPage = this.injector.getInstance(IFormPropertySheetPage.class);
        this.renderView = this.injector.getInstance(IRenderView.class);
        this.backgroundRenderViewProvider = this.injector.getInstance(BackgroundRenderViewProvider.class);
        this.gridRenderViewProvider = this.injector.getInstance(GridRenderViewProvider.class);
        this.sceneRenderViewProvider = this.injector.getInstance(SceneRenderViewProvider.class);
        site.setSelectionProvider(this.sceneRenderViewProvider);
        this.cameraController = this.injector.getInstance(CameraController.class);

        updateSceneGrid();

        this.manipulatorController = this.injector.getInstance(ManipulatorController.class);
        IManipulatorMode selectMode = manipulatorRegistry.getMode(Activator.SELECT_MODE_ID);
        manipulatorController.setManipulatorMode(selectMode);

        this.presenter = this.injector.getInstance(ISceneView.IPresenter.class);
        // Provide presenter with project properties
        IFile gameProject = EditorUtil.findGameProjectFile(this.contentRoot);
        if (gameProject != null && gameProject.exists()) {
            ProjectProperties projectProperties = new ProjectProperties();
            try {
                projectProperties.load(gameProject.getContents());
                try {
                    this.presenter.onProjectPropertiesChanged(projectProperties);
                } catch (CoreException e) {
                    throw new PartInitException(e.getMessage(), e);
                }
            } catch (Exception e) {
                // Ignore errors related to game project loading
            }
        }
        this.presenterContext = this.injector.getInstance(ISceneView.IPresenterContext.class);
        this.loaderContext = this.injector.getInstance(ILoaderContext.class);

        this.sceneModel = this.injector.getInstance(ISceneModel.class);
        this.sceneView = this.injector.getInstance(ISceneView.class);
        site.getPage().addSelectionListener(this.sceneView);

        IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        store.addPropertyChangeListener(this);
        String editorId = getEditorSite().getId();
        for (INodeType nodeType : this.nodeTypeRegistry.getNodeTypes()) {
            boolean hidden = store.getBoolean(ShowGroupHandler.getPreferenceName(editorId, nodeType.getDisplayGroup()));
            this.renderView.setNodeTypeVisible(nodeType, !hidden);
        }
        this.renderView.setGridShown(!store.getBoolean(ShowGridHandler.getPreferenceName(editorId)));
        this.renderView.setOutlineShown(!store.getBoolean(ShowOutlineHandler.getPreferenceName(editorId)));

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

    protected com.google.inject.Module createOverrideModule() {
        return new AbstractModule() {
            @Override
            protected void configure() {}
        };
    }

    public ManipulatorController getManipulatorController() {
        return manipulatorController;
    }

    /**
     * TODO This method currently manually disposes the model after activating the gl-context, in the case
     * that nodes have created graphics resources needing an active gl-context to be disposed.
     * This will need some re-design when the time comes.
     */
    @Override
    public void dispose() {
        super.dispose();
        // NOTE: All OpenGL related stuff (including model and nodes)
        // is disposed in partClosed instead.
        // At this point the canvas is disposed and the context
        // activation required a canvas.setCurrent(). context.makeCurrent()
        // wasn't sufficient. The effect was that the context wasn't activated
        // when the editor was closed while in inactive state, eg
        // press the close button without activation. The result
        // was that texture resource in the active editor was disposed
        // instead of textures in "this" editor/context. OpenGL handles
        // are just numbers.

        getSite().getPage().removeSelectionListener(this.sceneView);

        module.close();
        IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        store.removePropertyChangeListener(this);

        getSite().getPage().removePartListener(this);
    }

    @Override
    protected void doReload(IFile file) {
        IProgressService service = PlatformUI.getWorkbench()
                .getProgressService();
        SceneLoader loader = new SceneLoader(file, this.presenter);
        try {
            service.runInUI(service, loader, null);
            if (loader.exception != null) {
                logger.error("Error occurred while reloading", loader.exception);
            }
        } catch (Throwable e) {
            logger.error("Error occurred while reloading", loader.exception);
        }
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
                    logger.error("Error occurred while refreshing resource", e);
                }
            }
        });
    }

    @Override
    protected void handleProjectPropertiesChanged(final ProjectProperties properties) {
        try {
            presenter.onProjectPropertiesChanged(properties);
        } catch (CoreException e) {
            logger.error("Error occurred while handling project properties", e);
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
        return this.dirty;
    }

    @Override
    public boolean isSaveAsAllowed() {
        return true;
    }

    public String getContextID() {
        return Activator.SCENEED_CONTEXT_ID;
    }

    @Override
    public void createPartControl(Composite parent) {
        this.renderView.createControls(parent);

        // This makes sure the context will be active while this component is
        IContextService contextService = (IContextService) getSite()
                .getService(IContextService.class);
        contextService.activateContext(getContextID());

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
        } else if (adapter == IUndoContext.class) {
            return undoContext;
        } else {
            return super.getAdapter(adapter);
        }
    }

    @Override
    public ILoaderContext getLoaderContext() {
        return this.loaderContext;
    }

    @Override
    public ISceneView.IPresenterContext getPresenterContext() {
        return this.presenterContext;
    }

    @Override
    public ISceneView.IPresenter getScenePresenter() {
        return this.presenter;
    }

    @Override
    public ISceneView.INodePresenter<? extends Node> getNodePresenter(Class<? extends Node> nodeClass) {
        INodeType nodeType = this.nodeTypeRegistry.getNodeTypeClass(nodeClass);
        if (nodeType != null) {
            return nodeType.getPresenter();
        }
        return null;
    }

    public void setDirty(boolean dirty) {
        if (this.dirty != dirty) {
            this.dirty = dirty;
            firePropertyChange(PROP_DIRTY);
        }
    }

    @Override
    public void propertyChange(PropertyChangeEvent event) {
        if (event.getSource().equals(Activator.getDefault().getPreferenceStore())) {
            String property = event.getProperty();
            String editorId = getEditorSite().getId();
            if (property.startsWith(ShowGroupHandler.PREFERENCE_PREFIX)) {
                String displayGroup = ShowGroupHandler.getGroupName(editorId, property);
                for (INodeType nodeType : this.nodeTypeRegistry.getNodeTypes()) {
                    if (displayGroup.equals(nodeType.getDisplayGroup())) {
                        this.renderView.setNodeTypeVisible(nodeType, !(Boolean)event.getNewValue());
                    }
                }
            } else if (property.equals(PreferenceConstants.P_GRID)
                    || property.equals(PreferenceConstants.P_GRID_SIZE)
                    || property.equals(PreferenceConstants.P_GRID_COLOR)) {
                updateSceneGrid();
            } else if (property.equals(ShowGridHandler.getPreferenceName(editorId))) {
                this.renderView.setGridShown(!(Boolean)event.getNewValue());
            } else if (property.startsWith(ShowOutlineHandler.getPreferenceName(editorId))) {
                this.renderView.setOutlineShown(!(Boolean)event.getNewValue());
            }
            this.renderView.refresh();
        }
    }

    private void updateSceneGrid() {
        SceneGrid grid = this.renderView.getGrid();
        IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        grid.setAutoGrid(store.getString(PreferenceConstants.P_GRID).equals(PreferenceConstants.P_GRID_AUTO_VALUE));
        grid.setGridColor(RenderUtil.parseColor(PreferenceConstants.P_GRID_COLOR));
        grid.setFixedGridSize(store.getInt(PreferenceConstants.P_GRID_SIZE));
    }

    @Override
    public void partActivated(IWorkbenchPart part) {}

    @Override
    public void partBroughtToTop(IWorkbenchPart part) {}

    @Override
    public void partClosed(IWorkbenchPart part) {
        // dispose() above why we does disposal here
        if (part == this) {
            if (this.renderView != null) {
                GL2 gl2 = this.renderView.activateGLContext();
                ((SceneModel)this.sceneModel).dispose(gl2);
                this.renderView.releaseGLContext();
                this.renderView.dispose();
            } else {
                // Good luck! :)
                // Will *probably* work since no rendering took place and there should be no lingering graphics resources
                ((SceneModel)this.sceneModel).dispose(null);
            }
        }
    }

    @Override
    public void partDeactivated(IWorkbenchPart part) {}

    @Override
    public void partOpened(IWorkbenchPart part) {}

    public void toggleRecord() {
        renderView.toggleRecord();
    }

    protected Injector getInjector() {
        return this.injector;
    }

    public ISceneModel getModel() {
        return this.sceneModel;
    }
}
