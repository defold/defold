package com.dynamo.cr.integrationtest;

import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import java.io.ByteArrayInputStream;
import java.io.IOException;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IFolder;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.Path;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.junit.Before;
import org.junit.Test;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import com.dynamo.cr.editor.core.EditorCorePlugin;
import com.dynamo.cr.editor.core.IResourceType;
import com.dynamo.cr.go.core.ComponentTypeNode;
import com.dynamo.cr.go.core.GameObjectNode;
import com.dynamo.cr.go.core.GameObjectPresenter;
import com.dynamo.cr.go.core.operations.AddComponentOperation;
import com.dynamo.cr.sceneed.core.IImageProvider;
import com.dynamo.cr.sceneed.core.IModelListener;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.ISceneView.ILoaderContext;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.SceneModel;
import com.dynamo.cr.sceneed.core.ScenePresenter;
import com.dynamo.cr.sceneed.core.test.AbstractSceneTest;
import com.dynamo.cr.sceneed.ui.LoaderContext;
import com.google.inject.Module;
import com.google.inject.Singleton;

public class ComponentsTest extends AbstractSceneTest {

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
            bind(ISceneView.ILoaderContext.class).to(LoaderContext.class).in(Singleton.class);
            bind(ISceneView.IPresenterContext.class).toInstance(presenterContext);
            bind(IImageProvider.class).toInstance(imageProvider);
        }
    }

    @Override
    @Before
    public void setup() throws CoreException, IOException {
        this.view = mock(ISceneView.class);
        this.presenterContext = mock(IPresenterContext.class);
        this.imageProvider = mock(IImageProvider.class);

        super.setup();
        this.model = this.injector.getInstance(ISceneModel.class);
        this.presenter = this.injector.getInstance(ISceneView.IPresenter.class);
        this.loaderContext = this.injector.getInstance(ISceneView.ILoaderContext.class);

        when(this.presenterContext.getSelection()).thenAnswer(new Answer<IStructuredSelection>() {
            @Override
            public IStructuredSelection answer(InvocationOnMock invocation) throws Throwable {
                return model.getSelection();
            }
        });
        when(this.presenterContext.getView()).thenReturn(this.view);
    }

    // Tests

    @Test
    public void testLoad() throws Exception {
        String ddf = "";
        this.presenter.onLoad("go", new ByteArrayInputStream(ddf.getBytes()));
        Node root = this.model.getRoot();
        assertTrue(root instanceof GameObjectNode);
        assertTrue(this.model.getSelection().toList().contains(root));
        verify(this.view, times(1)).setRoot(root);
        verifySelection();
        verifyNoClean();
        verifyNoDirty();
    }

    /**
     * Test that every registered component type can be added as embedded (where applicable) and from files.
     * @throws Exception
     */
    @Test
    public void testAddComponents() throws Exception {
        testLoad();

        IFolder folder = this.contentRoot.getFolder(new Path("tmp"));
        if (!folder.exists()) {
            folder.create(true, true, null);
        }

        GameObjectPresenter presenter = (GameObjectPresenter)this.nodeTypeRegistry.getPresenter(GameObjectNode.class);

        // TODO: Iterate through node types directly (needs to be exposed through INodeTypeRegistry)
        IResourceType[] resourceTypes = EditorCorePlugin.getDefault().getResourceTypes();
        int count = 1;
        for (IResourceType type : resourceTypes) {
            boolean isComponentType = false;
            Class<?> nodeClass = this.nodeTypeRegistry.getNodeClass(type.getFileExtension());
            if (nodeClass != null) {
                Class<?> superClass = nodeClass.getSuperclass();
                while (superClass != null) {
                    if (superClass.equals(ComponentTypeNode.class)) {
                        isComponentType = true;
                        break;
                    }
                    superClass = superClass.getSuperclass();
                }
            }
            if (isComponentType) {
                if (type.isEmbeddable()) {
                    // Setup picking of the type from gui
                    when(this.view.selectComponentType()).thenReturn(type.getFileExtension());

                    // Perform operation
                    presenter.onAddComponent(this.presenterContext, this.loaderContext);
                    verify(this.presenterContext, times(count++)).executeOperation(any(AddComponentOperation.class));
                }

                String path = String.format("tmp/test.%s", type.getFileExtension());
                IFile file = this.contentRoot.getFile(new Path(path));
                if (file.exists()) {
                    file.delete(true, null);
                }
                byte[] data = type.getTemplateData();
                file.create(new ByteArrayInputStream(data != null ? data : "".getBytes()), true, null);

                // Setup picking of the file from gui
                when(this.view.selectComponentFromFile()).thenReturn(path);

                // Perform operation
                presenter.onAddComponentFromFile(this.presenterContext, this.loaderContext);
                verify(this.presenterContext, times(count++)).executeOperation(any(AddComponentOperation.class));
            }
        }

    }

    @Override
    protected Module getModule() {
        return new TestModule();
    }

    @Override
    protected String getBundleName() {
        return "com.dynamo.cr.integrationtest";
    }

}
