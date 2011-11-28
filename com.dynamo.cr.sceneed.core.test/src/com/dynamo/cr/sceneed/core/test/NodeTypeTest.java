package com.dynamo.cr.sceneed.core.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.mock;

import java.io.ByteArrayInputStream;
import java.io.IOException;

import org.eclipse.core.runtime.CoreException;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.ISceneView.INodeLoader;
import com.dynamo.cr.sceneed.core.Node;
import com.google.inject.Module;

public class NodeTypeTest extends AbstractSceneTest {

    private ISceneView.ILoaderContext loaderContext;

    class TestModule extends GenericTestModule {
        @Override
        protected void configure() {
            super.configure();
            bind(ISceneModel.class).toInstance(model);
            bind(ISceneView.class).toInstance(view);
            bind(ISceneView.IPresenter.class).toInstance(presenter);
        }
    }

    @Override
    @Before
    public void setup() throws CoreException, IOException {
        this.model = mock(ISceneModel.class);
        this.view = mock(ISceneView.class);
        this.presenter = mock(ISceneView.IPresenter.class);
        this.loaderContext = mock(ISceneView.ILoaderContext.class);

        super.setup();
    }

    public void assertExtToClass(String extension, Class<? extends Node> nodeClass) throws Exception {
        INodeLoader<? extends Node> loader = this.nodeTypeRegistry.getLoader(extension);
        Node node = loader.load(this.loaderContext, new ByteArrayInputStream(new byte[0]));
        assertThat(node, is(nodeClass));
    }

    @Test
    public void testRegistry() throws Exception {
        assertExtToClass("parent", ParentNode.class);
        assertExtToClass("child", ChildNode.class);
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
