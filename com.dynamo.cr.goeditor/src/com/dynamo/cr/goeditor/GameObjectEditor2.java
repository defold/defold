package com.dynamo.cr.goeditor;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;

import javax.inject.Singleton;

import org.eclipse.core.commands.operations.IOperationApprover;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.ui.IActionBars;
import org.eclipse.ui.IEditorInput;
import org.eclipse.ui.IEditorSite;
import org.eclipse.ui.IFileEditorInput;
import org.eclipse.ui.IWorkbenchPartSite;
import org.eclipse.ui.PartInitException;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.actions.ActionFactory;
import org.eclipse.ui.operations.LinearUndoViolationUserApprover;
import org.eclipse.ui.operations.RedoActionHandler;
import org.eclipse.ui.operations.UndoActionHandler;
import org.eclipse.ui.part.EditorPart;
import org.eclipse.ui.statushandlers.StatusManager;

import com.dynamo.cr.editor.core.EditorCorePlugin;
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.core.IResourceTypeRegistry;
import com.dynamo.cr.editor.core.inject.LifecycleModule;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;

public class GameObjectEditor2 extends EditorPart {

    private IOperationHistory undoHistory;
    private UndoContext undoContext;
    private GameObjectPresenter presenter;
    private Composite parent;
    private ImageFactory imageFactory = new ImageFactory();
    private IGameObjectView view;
    private UndoActionHandler undoAction;
    private RedoActionHandler redoAction;
    private LifecycleModule module;
    private IWorkbenchPartSite site;
    private IContainer contentRoot;

    public GameObjectEditor2() {
        // TODO: Rename to GameObjectEditor
    }

    @Override
    public void dispose() {
        super.dispose();
        imageFactory.dispose();
        module.close();
    }

    class Module extends AbstractModule {
        @Override
        protected void configure() {
            bind(IGameObjectView.class).to(GameObjectView.class).in(Singleton.class);
            bind(GameObjectModel.class).in(Singleton.class);
            bind(IGameObjectView.Presenter.class).to(GameObjectPresenter.class).in(Singleton.class);
            bind(IResourceTypeRegistry.class).toInstance(EditorCorePlugin.getDefault().getResourceTypeRegistry());

            bind(GameObjectEditor2.class).toInstance(GameObjectEditor2.this);

            bind(IOperationHistory.class).toInstance(undoHistory);
            bind(IUndoContext.class).toInstance(undoContext);

            bind(ILogger.class).toInstance(Activator.getDefault());

            bind(Composite.class).toInstance(parent);
            bind(ImageFactory.class).toInstance(imageFactory);
            bind(IWorkbenchPartSite.class).toInstance(site);
            bind(IContainer.class).toInstance(contentRoot);
        }
    }

    @Override
    public void doSave(IProgressMonitor monitor) {
        IFileEditorInput input = (IFileEditorInput) getEditorInput();
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        try {
            presenter.onSave(output);
            ByteArrayInputStream source = new ByteArrayInputStream(output.toByteArray());
            input.getFile().setContents(source, false, true, monitor);
        } catch (Throwable e) {
            Status status = new Status(IStatus.ERROR, Activator.PLUGIN_ID, e.getMessage(), e);
            StatusManager.getManager().handle(status, StatusManager.SHOW);
        }
    }

    @Override
    public void doSaveAs() {
        // TODO Auto-generated method stub
    }

    @Override
    public void init(IEditorSite site, IEditorInput input)
            throws PartInitException {
        setSite(site);
        setInput(input);
        setPartName(input.getName());

        IFile file = ((IFileEditorInput) input).getFile();
        this.contentRoot = EditorUtil.findContentRoot(file);

        undoHistory = PlatformUI.getWorkbench().getOperationSupport().getOperationHistory();
        undoContext = new UndoContext();
        undoHistory.setLimit(this.undoContext, 100);

        @SuppressWarnings("unused")
        IOperationApprover approver = new LinearUndoViolationUserApprover(this.undoContext, this);

        this.undoAction = new UndoActionHandler(this.getEditorSite(), this.undoContext);
        this.redoAction = new RedoActionHandler(this.getEditorSite(), this.undoContext);
    }

    @Override
    public boolean isDirty() {
        return presenter.isDirty();
    }

    @Override
    public boolean isSaveAsAllowed() {
        // TODO Auto-generated method stub
        return false;
    }

    @Override
    public void createPartControl(Composite parent) {
        this.parent = parent;
        site = getSite();
        module = new LifecycleModule(new Module(), Activator.getDefault().getInjectImageModule());
        Injector injector = Guice.createInjector(module);
        presenter = injector.getInstance(GameObjectPresenter.class);
        view = injector.getInstance(IGameObjectView.class);
        view.create(getEditorInput().getName());

        IFileEditorInput input = (IFileEditorInput) getEditorInput();
        try {
            // TODO: finally close..
            presenter.onLoad(input.getFile().getContents());
        } catch (IOException e) {
            Status status = new Status(IStatus.ERROR, Activator.PLUGIN_ID, e.getMessage(), e);
            StatusManager.getManager().handle(status, StatusManager.SHOW);
        } catch (CoreException e) {
            Status status = new Status(IStatus.ERROR, Activator.PLUGIN_ID, e.getMessage(), e);
            StatusManager.getManager().handle(status, StatusManager.SHOW);
        }
    }

    @Override
    public void setFocus() {
        view.setFocus();
    }

    public void updateActions() {
        IActionBars actionBars = getEditorSite().getActionBars();
        actionBars.updateActionBars();

        actionBars.setGlobalActionHandler(ActionFactory.UNDO.getId(), undoAction);
        actionBars.setGlobalActionHandler(ActionFactory.REDO.getId(), redoAction);
    }

    public GameObjectPresenter getPresenter() {
        return presenter;
    }

    public void fireDirty() {
        firePropertyChange(PROP_DIRTY);
    }

}
