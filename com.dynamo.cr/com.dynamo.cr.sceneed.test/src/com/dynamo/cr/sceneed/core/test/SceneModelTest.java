package com.dynamo.cr.sceneed.core.test;

import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyBoolean;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.DefaultOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.resources.IResourceDelta;
import org.eclipse.core.resources.IResourceDeltaVisitor;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.junit.Before;
import org.junit.Test;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import com.dynamo.cr.editor.ui.IImageProvider;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.IModelListener;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.SceneModel;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.Singleton;

public class SceneModelTest {

    private SceneModel model;
    private DummyNode node;

    private IOperationHistory history;
    private IUndoContext undoContext;

    // Mocks
    private IModelListener listener;
    private IContainer contentRoot;
    private ILoaderContext loaderContext;
    private IImageProvider imageProvider;

    // Counters
    int rootChanged;
    int stateChanged;
    int selectionChanged;

    public class TestModule extends AbstractModule {
        @Override
        protected void configure() {
            bind(IOperationHistory.class).to(DefaultOperationHistory.class).in(Singleton.class);
            bind(IUndoContext.class).to(UndoContext.class).in(Singleton.class);

            bind(IModelListener.class).toInstance(listener);
            bind(IContainer.class).toInstance(contentRoot);
            bind(ILoaderContext.class).toInstance(loaderContext);
            bind(IImageProvider.class).toInstance(imageProvider);
        }
    }

    @Before
    public void setup() {
        this.listener = mock(IModelListener.class);
        this.contentRoot = mock(IContainer.class);
        when(this.contentRoot.getFullPath()).thenReturn(new Path("/"));
        this.loaderContext = mock(ILoaderContext.class);
        this.imageProvider = mock(IImageProvider.class);

        Injector injector = Guice.createInjector(new TestModule());
        this.model = injector.getInstance(SceneModel.class);
        SceneModel.setUpdateStatusDelay(0);
        this.history = injector.getInstance(IOperationHistory.class);
        this.undoContext = injector.getInstance(IUndoContext.class);

        this.rootChanged = 0;
        this.stateChanged = 0;
        this.selectionChanged = 0;

        this.node = new DummyNode();
        this.model.setRoot(this.node);
        verifyRootChanged();
    }

    // Helpers

    private void execute(IUndoableOperation operation) {
        operation.addContext(this.undoContext);
        this.model.executeOperation(operation);
    }

    private void undo() throws ExecutionException {
        IStatus status = this.history.undo(this.undoContext, null, null);
        assertTrue(status.isOK());
        verifyStateChanged();
    }

    private void redo() throws ExecutionException {
        IStatus status = this.history.redo(this.undoContext, null, null);
        assertTrue(status.isOK());
        verifyStateChanged();
    }

    private void setNodeProperty(Node node, String id, Object value) {
        @SuppressWarnings("unchecked")
        IPropertyModel<? extends Node, ISceneModel> propertyModel = (IPropertyModel<? extends Node, ISceneModel>)node.getAdapter(IPropertyModel.class);
        execute(propertyModel.setPropertyValue(id, value));
        verifyStateChanged();
    }

    private void verifyRootChanged() {
        ++this.rootChanged;
        verify(this.listener, times(this.rootChanged)).rootChanged(any(Node.class));
    }

    private void verifyStateChanged() {
        ++this.stateChanged;
        verify(this.listener, times(this.stateChanged)).stateChanged(any(IStructuredSelection.class), anyBoolean());
    }

    private void verifyNoStateChanged() {
        verify(this.listener, times(this.stateChanged)).stateChanged(any(IStructuredSelection.class), anyBoolean());
    }

    // Tests

    @Test
    public void testSetProperty() throws ExecutionException {
        setNodeProperty(this.node, "dummyProperty", 1);

        undo();

        redo();
    }

    @Test
    public void testReload() throws CoreException {
        IFile missing = mock(IFile.class);
        when(missing.exists()).thenReturn(false);
        when(missing.getFullPath()).thenReturn(new Path("/missing"));
        IFile existing = mock(IFile.class);
        when(existing.exists()).thenReturn(true);
        when(existing.getFullPath()).thenReturn(new Path("/existing"));

        // TODO: Complicated mocking, refactor to make easier, but how?
        final IResourceDelta delta = mock(IResourceDelta.class);
        doAnswer(new Answer<Object>() {
            @Override
            public Object answer(InvocationOnMock invocation) throws Throwable {
                IResourceDeltaVisitor visitor = (IResourceDeltaVisitor)invocation.getArguments()[0];
                visitor.visit(delta);
                return null;
            }
        }).when(delta).accept(any(IResourceDeltaVisitor.class));
        IResourceChangeEvent event = mock(IResourceChangeEvent.class);
        when(event.getDelta()).thenReturn(delta);

        when(delta.getResource()).thenReturn(missing);
        this.model.handleResourceChanged(event);
        verifyNoStateChanged();

        when(delta.getResource()).thenReturn(existing);
        this.model.handleResourceChanged(event);
        verifyStateChanged();
    }
}
