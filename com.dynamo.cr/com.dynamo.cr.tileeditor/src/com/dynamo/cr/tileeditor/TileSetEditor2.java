package com.dynamo.cr.tileeditor;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;

import javax.inject.Singleton;
import javax.media.opengl.GLContext;

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
import com.dynamo.cr.sceneed.core.ISceneView.IPresenter;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.ManipulatorController;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.SceneModel;
import com.dynamo.cr.sceneed.core.ScenePresenter;
import com.dynamo.cr.sceneed.ui.ISceneOutlinePage;
import com.dynamo.cr.sceneed.ui.LoaderContext;
import com.dynamo.cr.sceneed.ui.PresenterContext;
import com.dynamo.cr.sceneed.ui.RenderView;
import com.dynamo.cr.sceneed.ui.SceneClipboard;
import com.dynamo.cr.sceneed.ui.SceneOutlinePage;
import com.dynamo.cr.sceneed.ui.ScenePropertySheetPage;
import com.dynamo.cr.sceneed.ui.preferences.PreferenceConstants;
import com.dynamo.cr.tileeditor.scene.TileSetNode;
import com.dynamo.cr.tileeditor.scene.TileSetNodePresenter;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;

public class TileSetEditor2 extends AbstractDefoldEditor implements ISceneEditor, IPropertyChangeListener {

    private static Logger logger = LoggerFactory.getLogger(TileSetEditor2.class);
    private ISceneOutlinePage outlinePage;
    private IFormPropertySheetPage propertySheetPage;
    private TileSetRenderer2 tileSetRenderer;

    private IContainer contentRoot;
    private LifecycleModule module;
    private ISceneView.IPresenter presenter;
    private ISceneView.IPresenterContext presenterContext;
    private ILoaderContext loaderContext;
    private INodeTypeRegistry nodeTypeRegistry;
    private IImageProvider imageProvider;
    private ISceneModel sceneModel;
    private ISceneView sceneView;

    private boolean dirty;
    private ManipulatorController manipulatorController;
    private IManipulatorRegistry manipulatorRegistry;

    class Module extends AbstractModule {
        @Override
        protected void configure() {
            bind(ISceneOutlinePage.class).to(SceneOutlinePage.class).in(Singleton.class);
            bind(IFormPropertySheetPage.class).to(ScenePropertySheetPage.class).in(Singleton.class);
            bind(TileSetRenderer2.class).toInstance(tileSetRenderer);
            bind(IRenderView.class).to(RenderView.class).in(Singleton.class);
            bind(ISceneView.class).to(TileSetSceneView.class).in(Singleton.class);
            bind(ISceneModel.class).to(SceneModel.class).in(Singleton.class);
            bind(INodeTypeRegistry.class).toInstance(nodeTypeRegistry);
            bind(ISceneView.IPresenter.class).to(ScenePresenter.class).in(Singleton.class);
            bind(IModelListener.class).to(ScenePresenter.class).in(Singleton.class);
            bind(TileSetEditor2.class).toInstance(TileSetEditor2.this);
            bind(ILoaderContext.class).to(LoaderContext.class).in(Singleton.class);
            bind(IPresenterContext.class).to(PresenterContext.class).in(Singleton.class);
            bind(IImageProvider.class).toInstance(imageProvider);
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

        IFileEditorInput fileEditorInput = (IFileEditorInput) input;
        final IFile file = fileEditorInput.getFile();
        this.contentRoot = EditorUtil.findContentRoot(file);
        if (this.contentRoot == null) {
            throw new PartInitException(
                    "Unable to locate content root for project");
        }

        this.nodeTypeRegistry = com.dynamo.cr.sceneed.Activator.getDefault().getNodeTypeRegistry();
        this.imageProvider = com.dynamo.cr.sceneed.Activator.getDefault();
        this.tileSetRenderer = new TileSetRenderer2();
        this.manipulatorRegistry = Activator.getDefault().getManipulatorRegistry();

        this.module = new LifecycleModule(new Module());
        Injector injector = Guice.createInjector(module);

        final String undoId = ActionFactory.UNDO.getId();
        final String redoId = ActionFactory.REDO.getId();

        IActionBars actionBars = site.getActionBars();
        actionBars.setGlobalActionHandler(undoId, undoHandler);
        actionBars.setGlobalActionHandler(redoId, redoHandler);

        this.outlinePage = injector.getInstance(ISceneOutlinePage.class);
        this.propertySheetPage = injector.getInstance(IFormPropertySheetPage.class);

        @SuppressWarnings("unused")
        CameraController cameraController = injector.getInstance(CameraController.class);

        this.manipulatorController = injector.getInstance(ManipulatorController.class);
        IManipulatorMode selectMode = manipulatorRegistry.getMode(Activator.SELECT_MODE_ID);
        manipulatorController.setManipulatorMode(selectMode);

        this.presenter = injector.getInstance(ISceneView.IPresenter.class);
        this.presenterContext = injector.getInstance(ISceneView.IPresenterContext.class);
        this.loaderContext = injector.getInstance(ILoaderContext.class);
        this.sceneModel = injector.getInstance(ISceneModel.class);
        this.sceneView = injector.getInstance(ISceneView.class);

        TileSetNodePresenter nodePresenter = (TileSetNodePresenter) this.nodeTypeRegistry.getNodeTypeClass(TileSetNode.class).getPresenter();
        this.tileSetRenderer.setPresenter(nodePresenter, this.presenterContext);
        IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        store.addPropertyChangeListener(this);

        IProgressService service = PlatformUI.getWorkbench().getProgressService();

        this.dirty = false;

        TileSetLoader2 loader = new TileSetLoader2(file, this.presenter);
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
        if (this.sceneModel != null) {
            GLContext context = this.tileSetRenderer.getContext();
            context.makeCurrent();
            ((SceneModel)this.sceneModel).dispose(context.getGL().getGL2());
            context.release();
        }
        if (this.tileSetRenderer != null) {
            this.tileSetRenderer.dispose();
        }
        module.close();
        IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        store.removePropertyChangeListener(this);

        getSite().getPage().removeSelectionListener(this.sceneView);
    }

    @Override
    protected void doReload(IFile file) {
        IProgressService service = PlatformUI.getWorkbench()
                .getProgressService();
        TileSetLoader2 loader = new TileSetLoader2(file, this.presenter);
        try {
            service.runInUI(service, loader, null);
            if (loader.exception != null) {
                logger.error("Error occurred while reloading file", loader.exception);
            }
        } catch (Throwable e) {
            logger.error("Error occurred while reloading file", e);
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
                    logger.error("Error occurred while reloading", e);
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

    @Override
    public void createPartControl(Composite parent) {
        this.tileSetRenderer.createControls(parent);

        // This makes sure the context will be active while this component is
        IContextService contextService = (IContextService) getSite()
                .getService(IContextService.class);
        contextService.activateContext(com.dynamo.cr.sceneed.Activator.SCENEED_CONTEXT_ID);

        // Set the outline as selection provider
        getSite().setSelectionProvider(this.outlinePage);
        getSite().getPage().addSelectionListener(this.sceneView);

        this.presenter.onRefresh();
    }

    @Override
    public void setFocus() {
        this.tileSetRenderer.setFocus();
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

    public ISceneView.IPresenter getPresenter() {
        return this.presenter;
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
            if (event.getProperty().equals(PreferenceConstants.P_TOP_BKGD_COLOR)
                    || event.getProperty().equals(PreferenceConstants.P_BOTTOM_BKGD_COLOR)) {
                this.sceneView.refreshRenderView();
            }
        }
    }

    @Override
    public IPresenter getScenePresenter() {
        return this.presenter;
    }

    @Override
    public ISceneModel getModel() {
        return this.sceneModel;
    }

}
