package com.dynamo.cr.goprot.test;

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
import org.eclipse.core.resources.IWorkspace;
import org.eclipse.core.resources.ResourcesPlugin;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Platform;
import org.junit.Before;
import org.osgi.framework.Bundle;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.goprot.core.INodeView;
import com.dynamo.cr.goprot.core.Node;
import com.dynamo.cr.goprot.core.NodeManager;
import com.dynamo.cr.goprot.core.NodeModel;
import com.dynamo.cr.goprot.core.ILogger;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.Module;
import com.google.inject.Singleton;

public abstract class AbstractTest {
    protected NodeModel model;
    protected Injector injector;
    protected INodeView view;
    protected NodeManager manager;
    protected IOperationHistory history;
    protected IUndoContext undoContext;
    protected Map<Node, Integer> updateCounts;
    private IContainer contentRoot;
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
            bind(NodeModel.class).in(Singleton.class);
            bind(NodeManager.class).in(Singleton.class);
            bind(INodeView.class).toInstance(view);
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

        Bundle bundle = Platform.getBundle("com.dynamo.cr.goprot.test");
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

        this.view = mock(INodeView.class);

        this.injector = Guice.createInjector(getModule());
        this.model = this.injector.getInstance(NodeModel.class);
        this.manager = this.injector.getInstance(NodeManager.class);
        this.history = this.injector.getInstance(IOperationHistory.class);
        this.undoContext = this.injector.getInstance(IUndoContext.class);

        this.updateCounts = new HashMap<Node, Integer>();
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
            count = new Integer(1);
        } else {
            count = new Integer(count.intValue() + 1);
        }
        this.updateCounts.put(node, count);
        verify(this.view, times(count.intValue())).updateNode(node);
    }

}
