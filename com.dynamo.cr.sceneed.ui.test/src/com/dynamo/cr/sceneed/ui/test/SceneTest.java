package com.dynamo.cr.sceneed.ui.test;

import static org.mockito.Mockito.mock;

import java.io.IOException;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.jface.viewers.StructuredSelection;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.ISceneView.ILoaderContext;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.test.AbstractTest;
import com.dynamo.cr.sceneed.ui.SceneModel;
import com.dynamo.cr.sceneed.ui.ScenePresenter;
import com.google.inject.Module;
import com.google.inject.Singleton;

public class SceneTest extends AbstractTest {

    private ILoaderContext loaderContext;
    private IPresenterContext presenterContext;

    class TestModule extends GenericTestModule {
        @Override
        protected void configure() {
            super.configure();
            bind(ISceneModel.class).to(SceneModel.class).in(Singleton.class);
            bind(ISceneView.class).toInstance(view);
            bind(ISceneView.IPresenter.class).to(ScenePresenter.class).in(Singleton.class);
            bind(ILoaderContext.class).toInstance(loaderContext);
            bind(IPresenterContext.class).toInstance(presenterContext);
        }
    }

    @Override
    @Before
    public void setup() throws CoreException, IOException {
        this.view = mock(ISceneView.class);
        this.loaderContext = mock(ILoaderContext.class);
        this.presenterContext = mock(IPresenterContext.class);

        super.setup();
        this.model = this.injector.getInstance(ISceneModel.class);
        this.presenter = this.injector.getInstance(ISceneView.IPresenter.class);
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
        verifySelection();
        select(root);
        verifyNoSelection();
        select(child);
        verifySelection();
    }

    @Override
    protected Module getModule() {
        return new TestModule();
    }

    @Override
    protected String getBundleName() {
        return "com.dynamo.cr.sceneed.ui.test";
    }

}
