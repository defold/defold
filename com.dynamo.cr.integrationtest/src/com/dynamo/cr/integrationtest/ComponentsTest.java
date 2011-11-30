package com.dynamo.cr.integrationtest;

import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.any;
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

import com.dynamo.cr.editor.core.IResourceType;
import com.dynamo.cr.go.core.ComponentTypeNode;
import com.dynamo.cr.go.core.GameObjectNode;
import com.dynamo.cr.go.core.GameObjectPresenter;
import com.dynamo.cr.go.core.operations.AddComponentOperation;
import com.dynamo.cr.sceneed.core.INodeType;
import com.dynamo.cr.sceneed.core.Node;

public class ComponentsTest extends AbstractSceneTest {

    @Override
    @Before
    public void setup() throws CoreException, IOException {

        super.setup();

        when(getPresenterContext().getSelection()).thenAnswer(new Answer<IStructuredSelection>() {
            @Override
            public IStructuredSelection answer(InvocationOnMock invocation) throws Throwable {
                return getModel().getSelection();
            }
        });
        when(getPresenterContext().getView()).thenReturn(getView());
    }

    // Tests

    @Test
    public void testLoad() throws Exception {
        String ddf = "";
        getPresenter().onLoad("go", new ByteArrayInputStream(ddf.getBytes()));
        Node root = getModel().getRoot();
        assertTrue(root instanceof GameObjectNode);
        assertTrue(getModel().getSelection().toList().contains(root));
        verify(getView(), times(1)).setRoot(root);
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

        IFolder folder = getContentRoot().getFolder(new Path("tmp"));
        if (!folder.exists()) {
            folder.create(true, true, null);
        }

        INodeType goNodeType = getNodeTypeRegistry().getNodeType(GameObjectNode.class);
        GameObjectPresenter presenter = (GameObjectPresenter)goNodeType.getPresenter();

        INodeType[] nodeTypes = getNodeTypeRegistry().getNodeTypes();
        int count = 1;
        for (INodeType nodeType : nodeTypes) {
            boolean isComponentType = false;
            Class<?> nodeClass = nodeType.getNodeClass();
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
                IResourceType resourceType = nodeType.getResourceType();
                if (resourceType.isEmbeddable()) {
                    // Setup picking of the type from gui
                    when(getView().selectComponentType()).thenReturn(resourceType.getFileExtension());

                    // Perform operation
                    presenter.onAddComponent(getPresenterContext(), getLoaderContext());
                    verify(getPresenterContext(), times(count++)).executeOperation(any(AddComponentOperation.class));
                }

                String path = String.format("tmp/test.%s", resourceType.getFileExtension());
                IFile file = getContentRoot().getFile(new Path(path));
                if (file.exists()) {
                    file.delete(true, null);
                }
                byte[] data = resourceType.getTemplateData();
                file.create(new ByteArrayInputStream(data != null ? data : "".getBytes()), true, null);

                // Setup picking of the file from gui
                when(getView().selectComponentFromFile()).thenReturn(path);

                // Perform operation
                presenter.onAddComponentFromFile(getPresenterContext(), getLoaderContext());
                verify(getPresenterContext(), times(count++)).executeOperation(any(AddComponentOperation.class));
            }
        }

    }

}
