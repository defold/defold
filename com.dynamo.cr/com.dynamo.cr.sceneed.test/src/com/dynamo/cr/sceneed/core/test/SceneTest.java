package com.dynamo.cr.sceneed.core.test;

import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyBoolean;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

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

import com.dynamo.cr.editor.core.ILogger;
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
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.Singleton;

public class SceneTest {

    private ISceneView view;
    private ISceneView.IPresenter presenter;
    private ILoaderContext loaderContext;
    private IPresenterContext presenterContext;
    private ILogger logger;
    private IImageProvider imageProvider;
    private IContainer contentRoot;
    private INodeTypeRegistry nodeTypeRegistry;

    private int selectCount;

    class TestModule extends AbstractModule {
        @Override
        protected void configure() {
            bind(ISceneModel.class).to(SceneModel.class).in(Singleton.class);
            bind(ISceneView.class).toInstance(view);
            bind(ISceneView.IPresenter.class).to(ScenePresenter.class).in(Singleton.class);
            bind(IModelListener.class).to(ScenePresenter.class).in(Singleton.class);
            bind(ILoaderContext.class).toInstance(loaderContext);
            bind(IPresenterContext.class).toInstance(presenterContext);
            bind(ILogger.class).toInstance(logger);
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
        this.logger = mock(ILogger.class);
        this.imageProvider = mock(IImageProvider.class);
        this.contentRoot = mock(IContainer.class);
        this.nodeTypeRegistry = mock(INodeTypeRegistry.class);

        doThrow(new RuntimeException()).when(this.logger).logException(any(Throwable.class));

        Injector injector = Guice.createInjector(new TestModule());
        this.presenter = injector.getInstance(ISceneView.IPresenter.class);

        this.selectCount = 0;
    }

    // Helpers
    protected void verifyRefresh() {
        ++this.selectCount;
        verify(this.view, times(this.selectCount)).refresh(any(IStructuredSelection.class), anyBoolean());
    }

    protected void verifyNoRefresh() {
        verify(this.view, times(this.selectCount)).refresh(any(IStructuredSelection.class), anyBoolean());
    }

    // Tests

    private void select(Node node) {
        this.presenter.onSelect(new StructuredSelection(node));
    }

    @Test
    public void testSelection() throws Exception {
        DummyNode root = new DummyNode();
        DummyNode child = new DummyNode();
        root.addChild(child);

        select(root);
        verifyRefresh();
        select(root);
        verifyRefresh();
        select(child);
        verifyRefresh();
    }

}
