package com.dynamo.cr.integrationtest;

import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyString;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import java.io.ByteArrayInputStream;
import java.io.IOException;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IFolder;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.Path;
import org.eclipse.jface.viewers.ILabelProvider;
import org.junit.Before;
import org.junit.Test;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import com.dynamo.cr.editor.core.IResourceType;
import com.dynamo.cr.go.core.ComponentTypeNode;
import com.dynamo.cr.go.core.GameObjectNode;
import com.dynamo.cr.go.core.GameObjectPresenter;
import com.dynamo.cr.sceneed.core.INodeType;
import com.dynamo.cr.sceneed.core.Node;

public class ComponentsTest extends AbstractSceneTest {

    @Override
    @Before
    public void setup() throws CoreException, IOException {
        super.setup();
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

        INodeType goNodeType = getNodeTypeRegistry().getNodeTypeClass(GameObjectNode.class);
        GameObjectPresenter presenter = (GameObjectPresenter)goNodeType.getPresenter();

        INodeType[] nodeTypes = getNodeTypeRegistry().getNodeTypes();
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
                final IResourceType resourceType = nodeType.getResourceType();
                if (resourceType.isEmbeddable()) {
                    // Setup picking of the type from gui
                    when(getPresenterContext().selectFromArray(anyString(), anyString(), any(Object[].class), any(ILabelProvider.class))).thenReturn(resourceType);

                    // Perform operation
                    presenter.onAddComponent(getPresenterContext(), getLoaderContext());
                    verifyExcecution();
                }

                final String path = String.format("tmp/test.%s", resourceType.getFileExtension());
                IFile file = getContentRoot().getFile(new Path(path));
                if (file.exists()) {
                    file.delete(true, null);
                }
                byte[] data = resourceType.getTemplateData();
                file.create(new ByteArrayInputStream(data != null ? data : "".getBytes()), true, null);

                // Setup picking of the file from gui
                doAnswer(new Answer<String>() {
                    @Override
                    public String answer(InvocationOnMock invocation) throws Throwable {
                        String[] exts = (String[])invocation.getArguments()[1];
                        for (String ext : exts) {
                            if (ext.equals(resourceType.getFileExtension())) {
                                return path;
                            }
                        }
                        return null;
                    }

                }).when(getView()).selectFile(anyString(), any(String[].class));

                // Perform operation
                presenter.onAddComponentFromFile(getPresenterContext(), getLoaderContext());
                verifyExcecution();
            }
        }

    }

}
