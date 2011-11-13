package com.dynamo.cr.sceneed;

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
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.ISelectionProvider;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.window.Window;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.ui.IActionBars;
import org.eclipse.ui.IEditorInput;
import org.eclipse.ui.IEditorSite;
import org.eclipse.ui.IFileEditorInput;
import org.eclipse.ui.ISelectionListener;
import org.eclipse.ui.IWorkbenchPart;
import org.eclipse.ui.PartInitException;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.actions.ActionFactory;
import org.eclipse.ui.dialogs.SaveAsDialog;
import org.eclipse.ui.operations.RedoActionHandler;
import org.eclipse.ui.operations.UndoActionHandler;
import org.eclipse.ui.part.FileEditorInput;
import org.eclipse.ui.progress.IProgressService;
import org.eclipse.ui.statushandlers.StatusManager;
import org.eclipse.ui.views.contentoutline.ContentOutline;
import org.eclipse.ui.views.contentoutline.IContentOutlinePage;
import org.eclipse.ui.views.properties.IPropertySheetPage;

import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.core.inject.LifecycleModule;
import com.dynamo.cr.properties.IFormPropertySheetPage;
import com.dynamo.cr.sceneed.core.ILogger;
import com.dynamo.cr.sceneed.core.INodeView;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.NodeManager;
import com.dynamo.cr.sceneed.core.NodeModel;
import com.dynamo.cr.sceneed.gameobject.CollectionProxyNode;
import com.dynamo.cr.sceneed.gameobject.CollisionObjectNode;
import com.dynamo.cr.sceneed.gameobject.ComponentPresenter;
import com.dynamo.cr.sceneed.gameobject.GameObjectNode;
import com.dynamo.cr.sceneed.gameobject.GameObjectPresenter;
import com.dynamo.cr.sceneed.gameobject.GenericComponentTypeNode;
import com.dynamo.cr.sceneed.gameobject.SpawnPointNode;
import com.dynamo.cr.sceneed.gameobject.SpriteNode;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;

public class NodeEditor extends AbstractDefoldEditor implements ISelectionListener {

    private INodeOutlinePage outlinePage;
    private IFormPropertySheetPage propertySheetPage;
    private ISelectionProvider selectionProvider;
    private SceneView sceneView;

    private IContainer contentRoot;
    private LifecycleModule module;
    private NodeManager manager;
    private boolean dirty;

    class Module extends AbstractModule {
        @Override
        protected void configure() {
            bind(INodeOutlinePage.class).to(NodeOutlinePage.class).in(Singleton.class);
            bind(IFormPropertySheetPage.class).to(NodePropertySheetPage.class).in(Singleton.class);
            bind(INodeView.class).to(NodeView.class).in(Singleton.class);
            bind(SceneView.class).in(Singleton.class);
            bind(ISelectionProvider.class).to(NodeSelectionProvider.class).in(Singleton.class);
            bind(NodeModel.class).in(Singleton.class);
            bind(NodeManager.class).in(Singleton.class);
            bind(DefaultNodePresenter.class).in(Singleton.class);
            bind(GameObjectPresenter.class).in(Singleton.class);
            bind(NodeEditor.class).toInstance(NodeEditor.this);

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

        this.outlinePage = injector.getInstance(INodeOutlinePage.class);
        this.propertySheetPage = injector.getInstance(IFormPropertySheetPage.class);
        this.selectionProvider = injector.getInstance(ISelectionProvider.class);
        this.sceneView = injector.getInstance(SceneView.class);

        this.manager = injector.getInstance(NodeManager.class);
        this.manager.setDefaultPresenter(injector.getInstance(DefaultNodePresenter.class));
        // TODO: Replace with extension point
        this.manager.registerNodeType("go", GameObjectNode.class, injector.getInstance(GameObjectPresenter.class), null);
        this.manager.registerNodeType("sprite", SpriteNode.class, injector.getInstance(ComponentPresenter.class), null);
        this.manager.registerNodeType("spawnpoint", SpawnPointNode.class, injector.getInstance(ComponentPresenter.class), null);
        this.manager.registerNodeType("collisionobject", CollisionObjectNode.class, injector.getInstance(ComponentPresenter.class), null);
        this.manager.registerNodeType("collectionproxy", CollectionProxyNode.class, injector.getInstance(ComponentPresenter.class), null);
        this.manager.registerNodeType("script", GenericComponentTypeNode.class, injector.getInstance(ComponentPresenter.class), null);
        this.manager.registerNodeType("tilegrid", GenericComponentTypeNode.class, injector.getInstance(ComponentPresenter.class), null);
        this.manager.registerNodeType("camera", GenericComponentTypeNode.class, injector.getInstance(ComponentPresenter.class), null);
        this.manager.registerNodeType("gui", GenericComponentTypeNode.class, injector.getInstance(ComponentPresenter.class), null);
        this.manager.registerNodeType("light", GenericComponentTypeNode.class, injector.getInstance(ComponentPresenter.class), null);
        this.manager.registerNodeType("model", GenericComponentTypeNode.class, injector.getInstance(ComponentPresenter.class), null);

        IProgressService service = PlatformUI.getWorkbench().getProgressService();

        this.dirty = false;

        NodeLoaderRunnable loader = new NodeLoaderRunnable(file, injector.getInstance(DefaultNodePresenter.class));
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
        if (this.sceneView != null) {
            this.sceneView.dispose();
        }

        getSite().getPage().removeSelectionListener(this);
    }

    @Override
    protected void doReload(IFile file) {
        // TODO Auto-generated method stub

    }

    @Override
    protected void handleResourceChanged(IResourceChangeEvent event) {
        // TODO Auto-generated method stub

    }

    @Override
    public void doSave(IProgressMonitor monitor) {
        IFileEditorInput input = (IFileEditorInput) getEditorInput();
        IFile file = input.getFile();
        this.inSave = true;
        try {
            ByteArrayOutputStream stream = new ByteArrayOutputStream();
            this.manager.getDefaultPresenter().onSave(stream, monitor);
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
        this.sceneView.createControls(parent);

        // Set the outline as selection provider
        getSite().setSelectionProvider(this.selectionProvider);
        getSite().getPage().addSelectionListener(this);

        INodeView.Presenter presenter = this.manager.getDefaultPresenter();
        presenter.onRefresh();
    }

    @Override
    public void setFocus() {
        this.sceneView.setFocus();
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

    public INodeView.Presenter getPresenter(Class<? extends Node> c) {
        return this.manager.getPresenter(c);
    }

    @Override
    public void selectionChanged(IWorkbenchPart part, ISelection selection) {
        if ((part instanceof NodeEditor || part instanceof ContentOutline)
                && selection instanceof IStructuredSelection) {
            this.manager.getDefaultPresenter().onSelect((IStructuredSelection)selection);
        }
    }

    public void setDirty(boolean dirty) {
        if (this.dirty != dirty) {
            this.dirty = dirty;
            firePropertyChange(PROP_DIRTY);
        }
    }

}
