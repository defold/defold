package com.dynamo.cr.go.core.test;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import java.io.IOException;
import java.util.Collections;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.osgi.util.NLS;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.go.core.ComponentNode;
import com.dynamo.cr.go.core.GameObjectLoader;
import com.dynamo.cr.go.core.GameObjectNode;
import com.dynamo.cr.go.core.Messages;
import com.dynamo.cr.go.core.RefComponentNode;
import com.dynamo.cr.go.core.operations.AddComponentOperation;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.cr.sceneed.core.INodeType;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.operations.RemoveChildrenOperation;
import com.dynamo.cr.sceneed.core.test.AbstractNodeTest;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;

public class GameObjectNodeTest extends AbstractNodeTest {

    private GameObjectLoader loader;
    private GameObjectNode goNode;
    private DummyComponentLoader dummyLoader;

    @Override
    @Before
    public void setup() throws CoreException, IOException {
        super.setup();

        this.loader = new GameObjectLoader();
        this.dummyLoader = new DummyComponentLoader();

        // Virtual files
        registerFile("/test.test", "x: 0 y: 0 z: 0");
        registerFile("/test.test2", "");

        this.goNode = registerAndLoadRoot(GameObjectNode.class, "go", this.loader);
        registerNodeType(DummyComponentNode.class, "test");
    }

    // Helpers

    private void addComponent() throws Exception {
        DummyComponentNode componentType = new DummyComponentNode();
        execute(new AddComponentOperation(this.goNode, componentType, getPresenterContext()));
        verifySelection();
    }

    private void addComponentFromFile() throws Exception {
        DummyComponentNode componentType = this.dummyLoader.load(getLoaderContext(), getFile("/test.test").getContents());
        RefComponentNode component = new RefComponentNode(componentType);
        component.setComponent("/test.test");
        execute(new AddComponentOperation(this.goNode, component, getPresenterContext()));
        verifySelection();
    }

    private void removeComponent(int i) throws Exception {
        execute(new RemoveChildrenOperation(Collections.singletonList((Node)component(i)), getPresenterContext()));
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
        verifySelection();

        redo();
        assertOneComponent();
        verifySelection();
    }

    @Test
    public void testAddComponentFromFile() throws Exception {
        addComponentFromFile();

        assertOneComponent();

        undo();
        assertNoComponent();
        verifySelection();

        redo();
        assertOneComponent();
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
        verifySelection();

        redo();
        assertNoComponent();
        assertThat(component.getParent(), equalTo(null));
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

        undo();
        assertThat(component.getId(), is(oldId));

        redo();
        assertThat(component.getId(), is(newId));
    }

    @SuppressWarnings("unchecked")
    @Test
    public void testBuildMessage() throws Exception {
        INodeType testNodeType = mock(INodeType.class);
        // Not a nice cast, but ok since this is merely a test
        when(testNodeType.getLoader()).thenReturn((INodeLoader<Node>)(Object)new DummyComponentLoader());
        when(testNodeType.getExtension()).thenReturn("test");

        when(getNodeTypeRegistry().getNodeTypeClass(DummyComponentNode.class)).thenReturn(testNodeType);

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
        assertNodePropertyStatus(component, "id", IStatus.ERROR, Messages.ComponentNode_id_EMPTY);

        setNodeProperty(component, "id", "test");
        assertNodePropertyStatus(component, "id", IStatus.OK, null);

        addComponent();

        setNodeProperty(component(1), "id", "test");
        assertNodePropertyStatus(component(1), "id", IStatus.ERROR, NLS.bind(Messages.ComponentNode_id_DUPLICATE, "test"));
    }

    @Test
    public void testComponentFromFileMessages() throws Exception {

        addComponentFromFile();

        ComponentNode component = component(0);

        assertNodePropertyStatus(component, "component", IStatus.OK, null);

        setNodeProperty(component, "component", "/test.test2");
        assertNodePropertyStatus(component, "component", IStatus.ERROR, NLS.bind(Messages.RefComponentNode_component_INVALID_TYPE, "test2"));

        setNodeProperty(component, "component", "/test");
        assertNodePropertyStatus(component, "component", IStatus.ERROR, NLS.bind(Messages.RefComponentNode_component_UNKNOWN_TYPE, "/test"));
    }

}
