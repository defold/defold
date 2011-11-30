package com.dynamo.cr.go.core.test;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import java.io.ByteArrayInputStream;
import java.io.IOException;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.osgi.util.NLS;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.go.core.ComponentNode;
import com.dynamo.cr.go.core.GameObjectLoader;
import com.dynamo.cr.go.core.GameObjectNode;
import com.dynamo.cr.go.core.Messages;
import com.dynamo.cr.go.core.RefComponentNode;
import com.dynamo.cr.go.core.operations.AddComponentOperation;
import com.dynamo.cr.go.core.operations.RemoveComponentOperation;
import com.dynamo.cr.sceneed.core.INodeType;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.test.AbstractNodeTest;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;

public class GameObjectTest extends AbstractNodeTest {

    private GameObjectLoader loader;
    private GameObjectNode goNode;

    @Override
    @Before
    public void setup() throws CoreException, IOException {
        this.loader = new GameObjectLoader();
        super.setup();

        when(getModel().getExtension(TestComponentNode.class)).thenReturn("test");

        String ddf = "";
        goNode = this.loader.load(getLoaderContext(), new ByteArrayInputStream(ddf.getBytes()));
        goNode.setModel(getModel());
    }

    // Helpers

    private void addComponent() throws Exception {
        TestComponentNode componentType = new TestComponentNode();
        ComponentNode component = new ComponentNode(componentType);
        execute(new AddComponentOperation(this.goNode, component));
        verifyUpdate();
        verifySelection();
    }

    private void addComponentFromFile() throws Exception {
        TestComponentNode componentType = new TestComponentNode();
        RefComponentNode component = new RefComponentNode(componentType);
        component.setComponent("/test.test");
        execute(new AddComponentOperation(this.goNode, component));
        verifyUpdate();
        verifySelection();
    }

    private void removeComponent(int i) throws Exception {
        execute(new RemoveComponentOperation(component(i)));
        verifyUpdate();
        verifySelection();
    }

    private int componentCount() {
        return this.goNode.getChildren().size();
    }

    private ComponentNode component(int i) {
        return (ComponentNode)this.goNode.getChildren().get(i);
    }

    private void assertOneComponent() {
        assertThat(componentCount(), is(1));
        assertThat(component(0).getId(), is("test"));
        assertThat((GameObjectNode)component(0).getParent(), equalTo(this.goNode));
    }

    private void assertNoComponent() {
        assertThat(componentCount(), is(0));
    }

    // Tests

    @Test
    public void testAddComponent() throws Exception {
        addComponent();

        assertOneComponent();

        undo();
        assertNoComponent();
        verifyUpdate();
        verifySelection();

        redo();
        assertOneComponent();
        verifyUpdate();
        verifySelection();
    }

    @Test
    public void testAddComponentFromFile() throws Exception {
        addComponentFromFile();

        assertOneComponent();

        undo();
        assertNoComponent();
        verifyUpdate();
        verifySelection();

        redo();
        assertOneComponent();
        verifyUpdate();
        verifySelection();
    }

    @Test
    public void testRemoveComponent() throws Exception {
        addComponent();

        ComponentNode component = component(0);

        removeComponent(0);
        assertNoComponent();
        assertThat(component.getParent(), equalTo(null));

        undo();
        assertOneComponent();
        verifyUpdate();
        verifySelection();

        redo();
        assertNoComponent();
        assertThat(component.getParent(), equalTo(null));
        verifyUpdate();
        verifySelection();
    }

    @Test
    public void testSetId() throws Exception {
        addComponent();

        ComponentNode component = component(0);

        String oldId = component.getId();
        String newId = "test1";

        setNodeProperty(component, "id", newId);
        assertThat(component.getId(), is(newId));
        verifyUpdate();

        undo();
        assertThat(component.getId(), is(oldId));
        verifyUpdate();

        redo();
        assertThat(component.getId(), is(newId));
        verifyUpdate();
    }

    @SuppressWarnings("unchecked")
    @Test
    public void testBuildMessage() throws Exception {
        INodeType testNodeType = mock(INodeType.class);
        // Not a nice cast, but ok since this is merely a test
        when(testNodeType.getLoader()).thenReturn((ISceneView.INodeLoader<Node>)(Object)new TestComponentLoader());
        when(testNodeType.getExtension()).thenReturn("test");

        when(getNodeTypeRegistry().getNodeType(TestComponentNode.class)).thenReturn(testNodeType);

        addComponent();

        addComponentFromFile();

        PrototypeDesc ddf = (PrototypeDesc)this.loader.buildMessage(getLoaderContext(), this.goNode, null);

        assertThat(ddf.getEmbeddedComponentsCount(), is(1));
        assertThat(ddf.getComponentsCount(), is(1));
        assertThat(ddf.getEmbeddedComponents(0).getId(), is(component(0).getId()));
        assertThat(ddf.getComponents(0).getId(), is(component(1).getId()));
    }

    @Test
    public void testUniqueId() throws Exception {
        addComponent();
        assertThat(component(0).getId(), is("test"));

        addComponent();
        assertThat(component(1).getId(), is("test1"));

        addComponent();
        assertThat(component(2).getId(), is("test2"));
    }

    @Test
    public void testComponentMessages() throws Exception {
        addComponent();

        ComponentNode component = component(0);

        assertNodePropertyStatus(component, "id", IStatus.OK, null);

        setNodeProperty(component, "id", "");
        assertNodePropertyStatus(component, "id", IStatus.ERROR, Messages.ComponentNode_id_NOT_SPECIFIED);
        verifyUpdate();

        setNodeProperty(component, "id", "test");
        assertNodePropertyStatus(component, "id", IStatus.OK, null);
        verifyUpdate();

        addComponent();

        setNodeProperty(component(1), "id", "test");
        assertNodePropertyStatus(component(1), "id", IStatus.ERROR, NLS.bind(Messages.ComponentNode_id_DUPLICATED, "test"));
        assertThat(component(1).validate().getSeverity(), is(IStatus.ERROR));
        verifyUpdate();
    }

    @Test
    public void testComponentFromFileMessages() throws Exception {

        // mocking to simulate file loading

        IFile invalidContentFile = mock(IFile.class);
        when(invalidContentFile.exists()).thenReturn(true);
        when(invalidContentFile.getContents()).thenReturn(new ByteArrayInputStream("x: 0 y: 0 z: 0".getBytes()));
        IPath invalidContentPath = new Path("/test.test");
        when(getContentRoot().getFile(invalidContentPath)).thenReturn(invalidContentFile);

        IFile invalidTypeFile = mock(IFile.class);
        when(invalidTypeFile.exists()).thenReturn(true);
        when(invalidTypeFile.getContents()).thenReturn(new ByteArrayInputStream(new byte[0]));
        IPath invalidTypePath = new Path("/test.test2");
        when(getContentRoot().getFile(invalidTypePath)).thenReturn(invalidTypeFile);

        // test

        addComponentFromFile();

        ComponentNode component = component(0);

        assertNodePropertyStatus(component, "component", IStatus.ERROR, Messages.RefComponentNode_component_INVALID_REFERENCE);

        setNodeProperty(component, "component", "/test.test2");
        assertNodePropertyStatus(component, "component", IStatus.ERROR, NLS.bind(Messages.RefComponentNode_component_INVALID_TYPE, "test2"));
        verifyUpdate();
    }

}
