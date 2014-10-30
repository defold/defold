package com.dynamo.cr.integrationtest;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;

import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.util.Enumeration;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.DefaultOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IFolder;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.resources.IProjectDescription;
import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.resources.IResourceChangeListener;
import org.eclipse.core.resources.IWorkspace;
import org.eclipse.core.resources.ResourcesPlugin;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Platform;
import org.eclipse.jface.viewers.StructuredSelection;
import org.eclipse.swt.widgets.Display;
import org.junit.Before;
import org.osgi.framework.Bundle;

import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.ui.IImageProvider;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.sceneed.Activator;
import com.dynamo.cr.sceneed.core.IClipboard;
import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.IManipulatorRegistry;
import com.dynamo.cr.sceneed.core.IModelListener;
import com.dynamo.cr.sceneed.core.INodeTypeRegistry;
import com.dynamo.cr.sceneed.core.IRenderView;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.ManipulatorController;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.SceneModel;
import com.dynamo.cr.sceneed.core.ScenePresenter;
import com.dynamo.cr.sceneed.core.test.DummyClipboard;
import com.dynamo.cr.sceneed.ui.LoaderContext;
import com.dynamo.cr.sceneed.ui.PresenterContext;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.Singleton;

public abstract class AbstractSceneTest {
    private ISceneModel model;
    private ISceneView view;
    private ISceneView.IPresenter presenter;
    private IOperationHistory history;
    private IUndoContext undoContext;
    private IContainer contentRoot;
    private IProject project;
    private INodeTypeRegistry nodeTypeRegistry;
    private IImageProvider imageProvider;
    private IPresenterContext presenterContext;
    private ILoaderContext loaderContext;

    private int executionCount;

    protected class TestModule extends AbstractModule {
        @Override
        protected void configure() {
            bind(ISceneModel.class).to(SceneModel.class).in(Singleton.class);
            bind(ISceneView.class).toInstance(view);
            bind(ISceneView.IPresenter.class).to(ScenePresenter.class).in(Singleton.class);
            bind(IModelListener.class).to(ScenePresenter.class).in(Singleton.class);
            bind(ILoaderContext.class).to(LoaderContext.class).in(Singleton.class);
            bind(ISceneView.IPresenterContext.class).to(PresenterContext.class).in(Singleton.class);
            bind(IOperationHistory.class).to(DefaultOperationHistory.class).in(Singleton.class);
            bind(IUndoContext.class).to(UndoContext.class).in(Singleton.class);
            bind(IContainer.class).toInstance(contentRoot);
            bind(INodeTypeRegistry.class).toInstance(nodeTypeRegistry);
            bind(IImageProvider.class).toInstance(imageProvider);
            bind(ManipulatorController.class).in(Singleton.class);
            bind(IRenderView.class).toInstance(mock(IRenderView.class));
            bind(IManipulatorRegistry.class).toInstance(mock(IManipulatorRegistry.class));
            bind(IClipboard.class).to(DummyClipboard.class).in(Singleton.class);
        }
    }

    @Before
    public void setup() throws CoreException, IOException {
        // Avoid hang when running unit-test on Mac OSX
        // Related to SWT and threads?
        if (System.getProperty("os.name").toLowerCase().indexOf("mac") != -1) {
            Display.getDefault();
        }

        System.setProperty("java.awt.headless", "true");

        IWorkspace workspace = ResourcesPlugin.getWorkspace();
        this.project = workspace.getRoot().getProject("test");
        if (this.project.exists()) {
            this.project.delete(true, null);
        }
        this.project.create(null);
        this.project.open(null);

        IProjectDescription pd = this.project.getDescription();
        pd.setNatureIds(new String[] { "com.dynamo.cr.editor.core.crnature" });
        this.project.setDescription(pd, null);

        Bundle bundle = Platform.getBundle("com.dynamo.cr.integrationtest");
        Enumeration<URL> entries = bundle.findEntries("/test", "*", true);
        while (entries.hasMoreElements()) {
            URL url = entries.nextElement();
            IPath path = new Path(url.getPath()).removeFirstSegments(1);
            if (url.getFile().endsWith("/")) {
                // A path - skip
            } else {
                InputStream is = url.openStream();
                IFile file = this.project.getFile(path);
                IContainer parent = file.getParent();
                if (parent instanceof IFolder) {
                    IFolder folder = (IFolder) file.getParent();
                    while (!folder.exists()) {
                        folder.create(true, true, null);
                        parent = folder.getParent();
                        if (parent instanceof IFolder) {
                            folder = (IFolder) parent;
                        } else {
                            break;
                        }
                    }
                }
                file.create(is, true, null);
                is.close();
            }
        }
        this.contentRoot = EditorUtil.findContentRoot(this.project.getFile("game.project"));

        this.view = mock(ISceneView.class);

        this.nodeTypeRegistry = Activator.getDefault().getNodeTypeRegistry();

        this.imageProvider = mock(IImageProvider.class);

        Injector injector = Guice.createInjector(new TestModule());
        this.model = injector.getInstance(ISceneModel.class);
        this.presenter = injector.getInstance(ISceneView.IPresenter.class);
        this.presenterContext = injector.getInstance(ISceneView.IPresenterContext.class);
        this.history = injector.getInstance(IOperationHistory.class);
        this.undoContext = injector.getInstance(IUndoContext.class);
        this.history.setLimit(this.undoContext, 50); // 20 is default
        this.loaderContext = injector.getInstance(ILoaderContext.class);

        ResourcesPlugin.getWorkspace().addResourceChangeListener(new IResourceChangeListener() {
            @Override
            public void resourceChanged(IResourceChangeEvent event) {
                try {
                    presenter.onResourceChanged(event);
                } catch (CoreException e) {
                    throw new RuntimeException(e);
                }
            }
        }, IResourceChangeEvent.POST_CHANGE);

        this.executionCount = 0;
    }

    // Accessors

    protected ISceneModel getModel() {
        return this.model;
    }

    protected ISceneView getView() {
        return this.view;
    }

    protected ISceneView.IPresenter getPresenter() {
        return this.presenter;
    }

    protected IPresenterContext getPresenterContext() {
        return this.presenterContext;
    }

    protected ILoaderContext getLoaderContext() {
        return this.loaderContext;
    }

    protected IContainer getContentRoot() {
        return this.contentRoot;
    }

    protected INodeTypeRegistry getNodeTypeRegistry() {
        return this.nodeTypeRegistry;
    }

    // Helpers

    protected void undo() throws ExecutionException {
        this.history.undo(this.undoContext, null, null);
    }

    protected void redo() throws ExecutionException {
        this.history.redo(this.undoContext, null, null);
    }

    protected void select(Node node) {
        this.presenterContext.setSelection(new StructuredSelection(node));
    }

    protected Object getNodeProperty(Node node, Object id) throws ExecutionException {
        @SuppressWarnings("unchecked")
        IPropertyModel<? extends Node, ISceneModel> propertyModel = (IPropertyModel<? extends Node, ISceneModel>)node.getAdapter(IPropertyModel.class);
        return propertyModel.getPropertyValue(id);
    }

    @SuppressWarnings("unchecked")
    protected void setNodeProperty(Node node, Object id, Object value) {
        IPropertyModel<? extends Node, ISceneModel> propertyModel = (IPropertyModel<? extends Node, ISceneModel>)node.getAdapter(IPropertyModel.class);
        this.model.executeOperation(propertyModel.setPropertyValue(id, value));
    }

    @SuppressWarnings("unchecked")
    protected void resetNodeProperty(Node node, Object id) {
        IPropertyModel<? extends Node, ISceneModel> propertyModel = (IPropertyModel<? extends Node, ISceneModel>)node.getAdapter(IPropertyModel.class);
        this.model.executeOperation(propertyModel.resetPropertyValue(id));
    }

    protected boolean isNodePropertyOverridden(Node node, Object id) throws ExecutionException {
        @SuppressWarnings("unchecked")
        IPropertyModel<? extends Node, ISceneModel> propertyModel = (IPropertyModel<? extends Node, ISceneModel>)node.getAdapter(IPropertyModel.class);
        return propertyModel.isPropertyOverridden(id);
    }

    @SuppressWarnings("unchecked")
    protected IStatus getNodePropertyStatus(Node node, Object id) {
        IPropertyModel<? extends Node, ISceneModel> propertyModel = (IPropertyModel<? extends Node, ISceneModel>)node.getAdapter(IPropertyModel.class);
        return propertyModel.getPropertyStatus(id);
    }

    protected void assertNodePropertyStatus(Node node, Object id, int severity, String message) {
        IStatus status = getNodePropertyStatus(node, id);
        assertTrue(testStatus(status, severity, message));
    }

    private boolean testStatus(IStatus status, int severity, String message) {
        if (status.isMultiStatus()) {
            for (IStatus child : status.getChildren()) {
                if (testStatus(child, severity, message)) {
                    return true;
                }
            }
            return false;
        } else {
            return status.getSeverity() == severity && (message == null || message.equals(status.getMessage()));
        }
    }

    protected void verifyExcecution() {
        ++this.executionCount;
        assertThat(this.history.getUndoHistory(this.undoContext).length, is(this.executionCount));
    }
}
