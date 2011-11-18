package com.dynamo.cr.sceneed.core.test;

import static org.junit.Assert.assertTrue;

import java.io.IOException;

import static org.mockito.Mockito.mock;

import org.eclipse.core.runtime.CoreException;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.google.inject.Module;

public class NodeTypeTest extends AbstractTest {

    class TestModule extends GenericTestModule {
        @Override
        protected void configure() {
            super.configure();
            bind(ISceneModel.class).toInstance(model);
            bind(ISceneView.class).toInstance(view);
            bind(ISceneView.Presenter.class).toInstance(presenter);
        }
    }

    @Override
    @Before
    public void setup() throws CoreException, IOException {
        this.model = mock(ISceneModel.class);
        this.view = mock(ISceneView.class);
        this.presenter = mock(ISceneView.Presenter.class);

        super.setup();
    }

    @Test
    public void testRegistry() throws Exception {
        assertTrue(this.nodeTypeRegistry.getPresenter("parent") instanceof ParentPresenter);
        assertTrue(this.nodeTypeRegistry.getPresenter("child") instanceof ChildPresenter);
    }

    @Override
    protected Module getModule() {
        return new TestModule();
    }

    @Override
    protected String getBundleName() {
        return "com.dynamo.cr.sceneed.core.test";
    }

}
