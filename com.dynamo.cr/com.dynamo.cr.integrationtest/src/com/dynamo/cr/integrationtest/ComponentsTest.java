package com.dynamo.cr.integrationtest;

import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyString;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

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

    static private int MapObjectsArrayByType(Map<String, String> map, Object objects[]) {
        int count = 0;
        for(Object node : objects) {
            assert(node instanceof Node);
            Node go = (Node) node;
            count += MapObjectsArrayByType(map, go.getChildren().toArray());
            map.put(go.getClass().getName().toString(), go.toString());
            count++;
        }
        return count;
    }

    /**
     * Test that every registered component type can be added, loaded and saved as embedded (where applicable).
     * @throws Exception
     */
    @Test
    public void testLoadSaveEmbeddedComponents() throws Exception {
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
            }
        }

        // select all and save
        ByteArrayOutputStream stream = new ByteArrayOutputStream();
        getPresenter().onSave(stream, null);
        Map<String, String> preSaveMap = new HashMap<String, String>();
        getPresenter().onSelectAll(getPresenterContext());
        verifyExcecution();
        int preSaveCount = MapObjectsArrayByType(preSaveMap, getModel().getSelection().toList().toArray());

        // select all and delete (although, load will do this)
        getPresenter().onSelectAll(getPresenterContext());
        getPresenter().onDeleteSelection(getPresenterContext());
        verifyExcecution();

        // load and compare pre/post equality
        ByteArrayInputStream stream_in = new ByteArrayInputStream(stream.toByteArray());
        getPresenter().onLoad("go", stream_in);
        Map<String, String> postLoadMap = new HashMap<String, String>();
        getPresenter().onSelectAll(getPresenterContext());
        verifyExcecution();
        int postLoadCount = MapObjectsArrayByType(postLoadMap, getModel().getSelection().toList().toArray());
        assert(preSaveCount == postLoadCount);
        assert(preSaveMap.equals(postLoadMap));
    }


}
