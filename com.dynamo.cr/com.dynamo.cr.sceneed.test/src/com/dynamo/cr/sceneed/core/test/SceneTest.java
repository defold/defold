package com.dynamo.cr.sceneed.core.test;

import static org.mockito.Matchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import java.io.IOException;

import org.eclipse.core.commands.operations.DefaultOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.StructuredSelection;
import org.eclipse.ui.ISelectionService;
import org.junit.Before;
import org.junit.Test;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import com.dynamo.cr.editor.ui.IImageProvider;
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
import com.dynamo.cr.sceneed.core.operations.SelectOperation;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.Singleton;

public class SceneTest {

    private ISceneView view;
    private ISceneView.IPresenter presenter;
    private ILoaderContext loaderContext;
    private IPresenterContext presenterContext;
    private IImageProvider imageProvider;
    private IContainer contentRoot;
    private INodeTypeRegistry nodeTypeRegistry;

    private int expectedSelectCount;
    private IStructuredSelection currentSelection;

    class TestModule extends AbstractModule {
        @Override
        protected void configure() {
            bind(ISceneModel.class).to(SceneModel.class).in(Singleton.class);
            bind(ISceneView.class).toInstance(view);
            bind(ISceneView.IPresenter.class).to(ScenePresenter.class).in(Singleton.class);
            bind(IModelListener.class).to(ScenePresenter.class).in(Singleton.class);
            bind(ILoaderContext.class).toInstance(loaderContext);
            bind(IPresenterContext.class).toInstance(presenterContext);
            bind(IImageProvider.class).toInstance(imageProvider);
            bind(INodeTypeRegistry.class).toInstance(nodeTypeRegistry);
            bind(IOperationHistory.class).to(DefaultOperationHistory.class).in(Singleton.class);
            bind(IUndoContext.class).to(UndoContext.class).in(Singleton.class);
            bind(IContainer.class).toInstance(contentRoot);
            bind(ManipulatorController.class).in(Singleton.class);
            bind(ISelectionService.class).toInstance(mock(ISelectionService.class));
            bind(IRenderView.class).toInstance(mock(IRenderView.class));
            bind(IManipulatorRegistry.class).toInstance(mock(IManipulatorRegistry.class));
            bind(IClipboard.class).to(DummyClipboard.class).in(Singleton.class);
        }
    }

    @Before
    public void setup() throws CoreException, IOException {
        this.view = mock(ISceneView.class);
        this.loaderContext = mock(ILoaderContext.class);
        this.presenterContext = mock(IPresenterContext.class);
        this.imageProvider = mock(IImageProvider.class);
        this.contentRoot = mock(IContainer.class);
        this.nodeTypeRegistry = mock(INodeTypeRegistry.class);

        Injector injector = Guice.createInjector(new TestModule());
        this.presenter = injector.getInstance(ISceneView.IPresenter.class);

        this.expectedSelectCount = 0;

        when(this.presenterContext.getSelection()).thenAnswer(new Answer<IStructuredSelection>() {
            @Override
            public IStructuredSelection answer(InvocationOnMock invocation) throws Throwable {
                return currentSelection;
            }
        });
    }

    // Helpers
    protected void verifySelect() {
        ++this.expectedSelectCount;
        verify(this.presenterContext, times(this.expectedSelectCount)).executeOperation(any(SelectOperation.class));
    }

    protected void verifyNoSelect() {
        verify(this.presenterContext, times(this.expectedSelectCount)).executeOperation(any(SelectOperation.class));
    }

    // Tests

    private void select(Node node) {
        IStructuredSelection selection = new StructuredSelection(node);
        this.presenter.onSelect(this.presenterContext, selection);
        this.currentSelection = selection;
    }

    @Test
    public void testSelection() throws Exception {
        DummyNode root = new DummyNode();
        DummyNode child = new DummyNode();
        root.addChild(child);

        select(root);
        verifySelect();
        select(root);
        verifyNoSelect();
        select(child);
        verifySelect();
    }

}
