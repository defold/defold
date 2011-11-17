package com.dynamo.cr.sceneed.test;

import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Map;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.DefaultOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
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
import org.eclipse.jface.viewers.IStructuredSelection;
import org.junit.Before;
import org.osgi.framework.Bundle;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.any;

import static org.junit.Assert.assertTrue;

import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.sceneed.NodeManager;
import com.dynamo.cr.sceneed.SceneModel;
import com.dynamo.cr.sceneed.ScenePresenter;
import com.dynamo.cr.sceneed.core.ILogger;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.Node;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.Module;
import com.google.inject.Singleton;

public abstract class AbstractTest {
    protected ISceneModel model;
    protected Injector injector;
    protected ISceneView view;
    protected ISceneView.Presenter presenter;
    protected NodeManager manager;
    protected IOperationHistory history;
    protected IUndoContext undoContext;
    protected IContainer contentRoot;
    private Map<Node, Integer> updateCounts;
    private int selectCount;
    private int dirtyCount;
    private int cleanCount;
    private IProject project;

    protected static class TestLogger implements ILogger {

        @Override
        public void logException(Throwable exception) {
            throw new UnsupportedOperationException(exception);
        }

    }

    protected class GenericTestModule extends AbstractModule {
        @Override
        protected void configure() {
            bind(ISceneModel.class).to(SceneModel.class).in(Singleton.class);
            bind(NodeManager.class).in(Singleton.class);
            bind(ISceneView.class).toInstance(view);
            bind(ISceneView.Presenter.class).to(ScenePresenter.class).in(Singleton.class);
            bind(IOperationHistory.class).to(DefaultOperationHistory.class).in(Singleton.class);
            bind(IUndoContext.class).to(UndoContext.class).in(Singleton.class);
            bind(ILogger.class).to(TestLogger.class);
            bind(IContainer.class).toInstance(contentRoot);
        }
    }

    abstract Module getModule();

    @Before
    public void setup() throws CoreException, IOException {
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

        Bundle bundle = Platform.getBundle("com.dynamo.cr.sceneed.test");
        Enumeration<URL> entries = bundle.findEntries("/test", "*", true);
        while (entries.hasMoreElements()) {
            URL url = entries.nextElement();
            IPath path = new Path(url.getPath()).removeFirstSegments(1);
            // Create path of url-path and remove first element, ie /test/sounds/ -> /sounds
            if (url.getFile().endsWith("/")) {
                this.project.getFolder(path).create(true, true, null);
            } else {
                InputStream is = url.openStream();
                IFile file = this.project.getFile(path);
                file.create(is, true, null);
                is.close();
            }
        }
        this.contentRoot = EditorUtil.findContentRoot(this.project.getFile("game.project"));

        this.view = mock(ISceneView.class);

        this.injector = Guice.createInjector(getModule());
        this.model = this.injector.getInstance(ISceneModel.class);
        this.presenter = this.injector.getInstance(ISceneView.Presenter.class);
        this.manager = this.injector.getInstance(NodeManager.class);
        this.history = this.injector.getInstance(IOperationHistory.class);
        this.undoContext = this.injector.getInstance(IUndoContext.class);

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

        this.updateCounts = new HashMap<Node, Integer>();
        this.selectCount = 0;
        this.dirtyCount = 0;
        this.cleanCount = 0;
    }

    // Helpers

    protected void undo() throws ExecutionException {
        this.history.undo(this.undoContext, null, null);
    }

    protected void redo() throws ExecutionException {
        this.history.redo(this.undoContext, null, null);
    }

    protected void verifyUpdate(Node node) {
        Integer count = this.updateCounts.get(node);
        if (count == null) {
            count = 1;
        } else {
            count = count + 1;
        }
        this.updateCounts.put(node, count);
        verify(this.view, times(count.intValue())).updateNode(node);
    }

    protected void verifySelection() {
        ++this.selectCount;
        verify(this.view, times(this.selectCount)).updateSelection(any(IStructuredSelection.class));
    }

    protected void verifyNoSelection() {
        verify(this.view, times(this.selectCount)).updateSelection(any(IStructuredSelection.class));
    }

    protected void verifyDirty() {
        ++this.dirtyCount;
        verify(this.view, times(this.dirtyCount)).setDirty(true);
    }

    protected void verifyNoDirty() {
        verify(this.view, times(this.dirtyCount)).setDirty(true);
    }

    protected void verifyClean() {
        ++this.cleanCount;
        verify(this.view, times(this.cleanCount)).setDirty(false);
    }

    protected void verifyNoClean() {
        verify(this.view, times(this.cleanCount)).setDirty(false);
    }

    @SuppressWarnings("unchecked")
    protected void setNodeProperty(Node node, Object id, Object value) {
        IPropertyModel<? extends Node, ISceneModel> propertyModel = (IPropertyModel<? extends Node, ISceneModel>)node.getAdapter(IPropertyModel.class);
        this.model.executeOperation(propertyModel.setPropertyValue(id, value));
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
}
