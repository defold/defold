package com.dynamo.cr.goprot.test;

import java.io.ByteArrayInputStream;
import java.io.IOException;

import org.eclipse.core.runtime.CoreException;
import org.junit.Before;
import org.junit.Test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import com.dynamo.cr.goprot.core.Node;
import com.dynamo.cr.goprot.gameobject.ComponentNode;
import com.dynamo.cr.goprot.gameobject.GameObjectNode;
import com.dynamo.cr.goprot.gameobject.GameObjectPresenter;
import com.google.inject.Module;
import com.google.inject.Singleton;

public class GameObjectTest extends AbstractTest {

    class TestModule extends GenericTestModule {
        @Override
        protected void configure() {
            super.configure();
            bind(GameObjectPresenter.class).in(Singleton.class);
        }
    }

    @Override
    @Before
    public void setup() throws CoreException, IOException {
        super.setup();
        this.manager.registerPresenter(GameObjectNode.class, this.injector.getInstance(GameObjectPresenter.class));
    }

    // Tests

    @Test
    public void testLoad() throws IOException {
        String ddf = "";
        GameObjectPresenter presenter = (GameObjectPresenter)this.manager.getPresenter(GameObjectNode.class);
        presenter.onLoad(new ByteArrayInputStream(ddf.getBytes()));
        Node root = this.model.getRoot();
        assertTrue(root instanceof GameObjectNode);
        verifyUpdate(root);
    }

    @Test
    public void testAddComponent() throws Exception {
        testLoad();

        GameObjectNode node = (GameObjectNode)this.model.getRoot();
        GameObjectPresenter presenter = (GameObjectPresenter)this.manager.getPresenter(GameObjectNode.class);
        String componentType = "sprite";
        presenter.onAddComponent(node, componentType);
        assertEquals(1, node.getChildren().size());
        assertEquals("sprite", node.getChildren().get(0).toString());
        verifyUpdate(node);

        undo();
        assertEquals(0, node.getChildren().size());
        verifyUpdate(node);

        redo();
        assertEquals(1, node.getChildren().size());
        assertEquals("sprite", node.getChildren().get(0).toString());
        verifyUpdate(node);
    }

    @Test
    public void testRemoveComponent() throws Exception {
        testAddComponent();

        GameObjectNode node = (GameObjectNode)this.model.getRoot();
        ComponentNode component = (ComponentNode)node.getChildren().get(0);
        GameObjectPresenter presenter = (GameObjectPresenter)this.manager.getPresenter(GameObjectNode.class);

        presenter.onRemoveComponent(component);
        assertEquals(0, node.getChildren().size());
        assertEquals(null, component.getParent());
        verifyUpdate(node);

        undo();
        assertEquals(1, node.getChildren().size());
        assertEquals(component, node.getChildren().get(0));
        assertEquals(node, component.getParent());
        verifyUpdate(node);

        redo();
        assertEquals(0, node.getChildren().size());
        assertEquals(null, component.getParent());
        verifyUpdate(node);
    }

    @Test
    public void testSetId() throws Exception {
        testAddComponent();

        ComponentNode component = (ComponentNode)this.model.getRoot().getChildren().get(0);
        String oldId = component.getId();
        String newId = "sprite1";

        setNodeProperty(component, "id", newId);
        assertEquals(newId, component.getId());
        verifyUpdate(component);

        undo();
        assertEquals(oldId, component.getId());
        verifyUpdate(component);

        redo();
        assertEquals(newId, component.getId());
        verifyUpdate(component);
    }

    @Override
    Module getModule() {
        return new TestModule();
    }

}
