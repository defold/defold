package com.dynamo.cr.tileeditor.test;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import java.io.IOException;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.Path;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.junit.Before;
import org.junit.Test;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import com.dynamo.cr.sceneed.core.Activator;
import com.dynamo.cr.sceneed.core.IImageProvider;
import com.dynamo.cr.sceneed.core.IModelListener;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.ISceneView.ILoaderContext;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.SceneModel;
import com.dynamo.cr.sceneed.core.ScenePresenter;
import com.dynamo.cr.sceneed.core.test.AbstractTest;
import com.dynamo.cr.tileeditor.scene.Sprite2Node;
import com.google.inject.Module;
import com.google.inject.Singleton;

public class Sprite2Test extends AbstractTest {

    private ILoaderContext loaderContext;
    private IPresenterContext presenterContext;
    private IImageProvider imageProvider;

    class TestModule extends GenericTestModule {
        @Override
        protected void configure() {
            super.configure();
            bind(ISceneModel.class).to(SceneModel.class).in(Singleton.class);
            bind(ISceneView.class).toInstance(view);
            bind(ISceneView.IPresenter.class).to(ScenePresenter.class).in(Singleton.class);
            bind(IModelListener.class).to(ScenePresenter.class).in(Singleton.class);
            bind(ISceneView.ILoaderContext.class).toInstance(loaderContext);
            bind(ISceneView.IPresenterContext.class).toInstance(presenterContext);
            bind(IImageProvider.class).toInstance(imageProvider);
        }
    }

    @Override
    @Before
    public void setup() throws CoreException, IOException {
        this.view = mock(ISceneView.class);
        this.loaderContext = mock(ILoaderContext.class);
        this.presenterContext = mock(IPresenterContext.class);
        this.imageProvider = mock(IImageProvider.class);

        super.setup();
        this.model = this.injector.getInstance(ISceneModel.class);
        this.presenter = this.injector.getInstance(ISceneView.IPresenter.class);

        when(this.presenterContext.getSelection()).thenAnswer(new Answer<IStructuredSelection>() {
            @Override
            public IStructuredSelection answer(InvocationOnMock invocation) throws Throwable {
                return model.getSelection();
            }
        });
        when(this.presenterContext.getView()).thenReturn(this.view);

        when(this.loaderContext.getNodeTypeRegistry()).thenReturn(Activator.getDefault());
    }

    // Tests

    @Test
    public void testLoad() throws Exception {
        IFile sprite = this.contentRoot.getFile(new Path("test.sprite2"));
        this.presenter.onLoad("sprite2", sprite.getContents());
        Node root = this.model.getRoot();
        assertTrue(root instanceof Sprite2Node);
        assertTrue(this.model.getSelection().toList().contains(root));
        verify(this.view, times(1)).setRoot(root);
        verifySelection();
        verifyNoClean();
        verifyNoDirty();
    }

    @Override
    protected Module getModule() {
        return new TestModule();
    }

    @Override
    protected String getBundleName() {
        return "com.dynamo.cr.tileeditor.test";
    }

}
