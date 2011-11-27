package com.dynamo.cr.go.core.test;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.CharArrayWriter;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import javax.vecmath.Point3f;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.osgi.util.NLS;
import org.junit.Before;
import org.junit.Test;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import com.dynamo.cr.go.core.ComponentNode;
import com.dynamo.cr.go.core.ComponentTypeNode;
import com.dynamo.cr.go.core.GameObjectNode;
import com.dynamo.cr.go.core.GameObjectPresenter;
import com.dynamo.cr.go.core.Messages;
import com.dynamo.cr.go.core.RefComponentNode;
import com.dynamo.cr.go.core.operations.AddComponentOperation;
import com.dynamo.cr.go.core.operations.RemoveComponentOperation;
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
import com.dynamo.proto.DdfMath.Point3;
import com.google.inject.Module;
import com.google.inject.Singleton;
import com.google.protobuf.TextFormat;

public class GameObjectTest extends AbstractTest {

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

    @Test
    public void testAddComponentOperation() throws Exception {
        testLoad();

        GameObjectNode go = (GameObjectNode)this.model.getRoot();
        TestComponentNode componentType = new TestComponentNode();
        ComponentNode component = new ComponentNode(componentType);
        AddComponentOperation op = new AddComponentOperation(go, component);
        this.model.executeOperation(op);
        assertEquals(1, go.getChildren().size());
        assertEquals("test", go.getChildren().get(0).toString());
        verifyUpdate(go);
        verifySelection();
        verifyDirty();

        undo();
        assertEquals(0, go.getChildren().size());
        verifyUpdate(go);
        verifySelection();
        verifyClean();

        redo();
        assertEquals(1, go.getChildren().size());
        assertEquals("test", go.getChildren().get(0).toString());
        verifyUpdate(go);
        verifySelection();
        verifyDirty();
    }

    @Test
    public void testAddComponent() throws Exception {
        testLoad();

        when(this.view.selectComponentType()).thenReturn("test");

        when(this.loaderContext.loadNodeFromTemplate("test")).thenAnswer(new Answer<Node>() {
            @Override
            public Node answer(InvocationOnMock invocation) throws Throwable {
                return new TestComponentNode();
            }
        });

        GameObjectPresenter presenter = (GameObjectPresenter)this.nodeTypeRegistry.getPresenter(GameObjectNode.class);
        presenter.onAddComponent(this.presenterContext, this.loaderContext);
        verify(this.presenterContext, times(1)).executeOperation(any(IUndoableOperation.class));
    }

    @Test
    public void testAddComponentFromFileOperation() throws Exception {
        testLoad();

        GameObjectNode go = (GameObjectNode)this.model.getRoot();
        TestComponentNode componentType = new TestComponentNode();
        RefComponentNode component = new RefComponentNode(componentType);
        component.setComponent("/test.test");
        AddComponentOperation op = new AddComponentOperation(go, component);
        this.model.executeOperation(op);
        assertEquals(1, go.getChildren().size());
        assertEquals("test", ((ComponentNode)go.getChildren().get(0)).getId());
        verifyUpdate(go);
        verifySelection();

        undo();
        assertEquals(0, go.getChildren().size());
        verifyUpdate(go);
        verifySelection();

        redo();
        assertEquals(1, go.getChildren().size());
        assertEquals("test", ((ComponentNode)go.getChildren().get(0)).getId());
        verifyUpdate(go);
        verifySelection();
    }

    @Test
    public void testAddComponentFromFile() throws Exception {
        testLoad();

        String path = "/test.test";
        when(this.view.selectComponentFromFile()).thenReturn(path);
        when(this.loaderContext.loadNode(path)).thenReturn(new TestComponentNode());

        GameObjectPresenter presenter = (GameObjectPresenter)this.nodeTypeRegistry.getPresenter(GameObjectNode.class);
        presenter.onAddComponentFromFile(this.presenterContext, this.loaderContext);
        verify(this.presenterContext).executeOperation(any(IUndoableOperation.class));
    }

    @Test
    public void testRemoveComponentOperation() throws Exception {
        testAddComponentOperation();

        GameObjectNode go = (GameObjectNode)this.model.getRoot();
        ComponentNode component = (ComponentNode)go.getChildren().get(0);

        RemoveComponentOperation op = new RemoveComponentOperation(component);

        this.model.executeOperation(op);
        assertEquals(0, go.getChildren().size());
        assertEquals(null, component.getParent());
        verifyUpdate(go);
        verifySelection();

        undo();
        assertEquals(1, go.getChildren().size());
        assertEquals(component, go.getChildren().get(0));
        assertEquals(go, component.getParent());
        verifyUpdate(go);
        verifySelection();

        redo();
        assertEquals(0, go.getChildren().size());
        assertEquals(null, component.getParent());
        verifyUpdate(go);
        verifySelection();
    }

    @Test
    public void testRemoveComponent() throws Exception {
        testAddComponentOperation();

        GameObjectPresenter presenter = (GameObjectPresenter)this.nodeTypeRegistry.getPresenter(GameObjectNode.class);
        presenter.onRemoveComponent(this.presenterContext);
        verify(this.presenterContext).executeOperation(any(IUndoableOperation.class));
    }

    @Test
    public void testSetId() throws Exception {
        testAddComponentOperation();

        Node root = this.model.getRoot();
        ComponentNode component = (ComponentNode)root.getChildren().get(0);
        String oldId = component.getId();
        String newId = "test1";

        setNodeProperty(component, "id", newId);
        assertEquals(newId, component.getId());
        verifyUpdate(root);

        undo();
        assertEquals(oldId, component.getId());
        verifyUpdate(root);

        redo();
        assertEquals(newId, component.getId());
        verifyUpdate(root);
    }

    @Test
    public void testSave() throws Exception {
        testAddComponentOperation();

        String path = "/test.go";
        IFile file = this.contentRoot.getFile(new Path(path));
        if (file.exists()) {
            file.delete(true, null);
        }
        OutputStream os = new FileOutputStream(file.getLocation().toFile());
        Node root = this.model.getRoot();
        this.presenter.onSave(os, null);
        file = this.contentRoot.getFile(new Path(path));
        this.model.setRoot(null);
        file.refreshLocal(0, null);
        this.presenter.onLoad(file.getFileExtension(), file.getContents());
        assertNotSame(root, this.model.getRoot());
        assertEquals(root.getClass(), this.model.getRoot().getClass());
        assertNotSame(0, this.model.getRoot().getChildren().size());
        assertEquals(root.getChildren().size(), this.model.getRoot().getChildren().size());
    }

    private void saveTestComponent(String path, Point3f position) throws IOException, CoreException {
        Point3.Builder builder = Point3.newBuilder();
        builder.setX(position.getX()).setY(position.getY()).setZ(position.getZ());
        IFile file = this.contentRoot.getFile(new Path(path));
        CharArrayWriter output = new CharArrayWriter();
        TextFormat.print(builder.build(), output);
        output.close();
        InputStream stream = new ByteArrayInputStream(output.toString().getBytes());
        if (!file.exists()) {
            file.create(stream, true, null);
        } else {
            file.setContents(stream, 0, null);
        }
    }

    @Test
    public void testReloadComponentFromFile() throws Exception {
        String path = "/reload.sprite";
        Point3f position = new Point3f(1.0f, 0.0f, 0.0f);

        when(this.view.selectComponentFromFile()).thenReturn(path);
        TestComponentNode componentType = new TestComponentNode();
        componentType.setPosition(position);
        when(this.loaderContext.loadNode(path)).thenReturn(componentType);

        saveTestComponent(path, new Point3f(0.0f, 0.0f, 0.0f));

        testLoad();

        GameObjectNode go = (GameObjectNode)this.model.getRoot();
        RefComponentNode component = new RefComponentNode(new TestComponentNode());
        component.setComponent(path);
        AddComponentOperation op = new AddComponentOperation(go, component);
        this.model.executeOperation(op);
        assertEquals(1, go.getChildren().size());
        assertNodePropertyStatus(component, "component", IStatus.ERROR, null);
        ComponentTypeNode type = component.getType();
        verifyUpdate(go);

        saveTestComponent(path, position);

        assertNodePropertyStatus(component, "component", IStatus.OK, null);
        assertEquals(component, go.getChildren().get(0));
        assertThat(type, is(not(component.getType())));
        verifyUpdate(go);
    }

    private ComponentNode addTestComponent() {
        GameObjectNode go = (GameObjectNode)this.model.getRoot();
        ComponentNode component = new ComponentNode(new TestComponentNode());
        AddComponentOperation op = new AddComponentOperation(go, component);
        this.model.executeOperation(op);
        return component;
    }

    @Test
    public void testUniqueId() throws Exception {
        testLoad();

        ComponentNode component = addTestComponent();
        assertThat(component.getId(), is("test"));

        component = addTestComponent();
        assertThat(component.getId(), is("test1"));

        component = addTestComponent();
        assertThat(component.getId(), is("test2"));
    }

    @Test
    public void testComponentMessages() throws Exception {
        testAddComponentOperation();

        ComponentNode component = (ComponentNode)this.model.getRoot().getChildren().get(0);
        assertNodePropertyStatus(component, "id", IStatus.OK, null);

        setNodeProperty(component, "id", "");
        assertNodePropertyStatus(component, "id", IStatus.ERROR, Messages.ComponentNode_id_NOT_SPECIFIED);

        setNodeProperty(component, "id", "test");
        assertNodePropertyStatus(component, "id", IStatus.OK, null);

        GameObjectNode go = (GameObjectNode)this.model.getRoot();
        TestComponentNode componentType = new TestComponentNode();
        component = new ComponentNode(componentType);
        AddComponentOperation op = new AddComponentOperation(go, component);
        this.model.executeOperation(op);
        component.setId("test");

        assertNodePropertyStatus(component, "id", IStatus.ERROR, NLS.bind(Messages.ComponentNode_id_DUPLICATED, "test"));
        assertThat(component.validate().getSeverity(), is(IStatus.ERROR));
    }

    @Test
    public void testComponentFromFileMessages() throws Exception {
        testAddComponentFromFileOperation();

        RefComponentNode component = (RefComponentNode)this.model.getRoot().getChildren().get(0);
        assertNodePropertyStatus(component, "component", IStatus.ERROR, Messages.RefComponentNode_component_INVALID_REFERENCE);

        setNodeProperty(component, "component", "");
        assertNodePropertyStatus(component, "component", IStatus.INFO, Messages.SceneModel_ResourceValidator_component_NOT_SPECIFIED);

        String path = "/test.dummy";
        setNodeProperty(component, "component", path);
        assertNodePropertyStatus(component, "component", IStatus.ERROR, NLS.bind(Messages.SceneModel_ResourceValidator_component_NOT_FOUND, path));

        setNodeProperty(component, "component", "/test.test2");
        assertNodePropertyStatus(component, "component", IStatus.ERROR, NLS.bind(Messages.RefComponentNode_component_INVALID_TYPE, "test2"));
    }

    @Test
    public void testProperties() throws Exception {
        String type = "test";
        String[] properties = new String[] {"position"};
        Object[] values = new Object[] {new Point3f(1.0f, 0.0f, 0.0f)};

        when(this.view.selectComponentType()).thenReturn(type);

        testAddComponentOperation();

        this.presenter.onSave(new ByteArrayOutputStream(), null);

        Node root = this.model.getRoot();
        Node component = root.getChildren().get(0);
        Node componentType = component.getChildren().get(0);
        for (int i = 0; i < properties.length; ++i) {
            verifyClean();
            setNodeProperty(componentType, properties[i], values[i]);
            assertNodePropertyStatus(componentType, properties[i], IStatus.OK, null);
            verifyDirty();
            verifyUpdate(root);
            undo();
            verifyUpdate(root);
        }
    }

    @Override
    protected Module getModule() {
        return new TestModule();
    }

    @Override
    protected String getBundleName() {
        return "com.dynamo.cr.go.core.test";
    }

}
