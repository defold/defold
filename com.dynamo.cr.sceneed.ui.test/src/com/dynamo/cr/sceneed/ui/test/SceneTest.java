package com.dynamo.cr.sceneed.ui.test;

import static org.mockito.Mockito.mock;

import java.io.IOException;

import org.eclipse.core.runtime.CoreException;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.test.AbstractTest;
import com.dynamo.cr.sceneed.ui.SceneModel;
import com.dynamo.cr.sceneed.ui.ScenePresenter;
import com.google.inject.Module;
import com.google.inject.Singleton;

public class SceneTest extends AbstractTest {

    class TestModule extends GenericTestModule {
        @Override
        protected void configure() {
            super.configure();
            bind(ISceneModel.class).to(SceneModel.class).in(Singleton.class);
            bind(ISceneView.class).toInstance(view);
            bind(ISceneView.Presenter.class).to(ScenePresenter.class).in(Singleton.class);
        }
    }

    @Override
    @Before
    public void setup() throws CoreException, IOException {
        this.view = mock(ISceneView.class);

        super.setup();
        this.model = this.injector.getInstance(ISceneModel.class);
        this.presenter = this.injector.getInstance(ISceneView.Presenter.class);
    }

    // Tests

    @Test
    public void testDummy() throws Exception {
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
