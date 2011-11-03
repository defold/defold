package com.dynamo.cr.goprot.test;

import java.io.IOException;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.runtime.CoreException;
import org.junit.Before;
import org.junit.Test;

import static org.junit.Assert.assertEquals;

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
        this.model.setRoot(new GameObjectNode());
        this.manager.registerPresenter(GameObjectNode.class, this.injector.getInstance(GameObjectPresenter.class));
    }

    // Tests

    @Test
    public void testAddComponent() throws ExecutionException {
        GameObjectNode node = (GameObjectNode)this.model.getRoot();
        GameObjectPresenter presenter = (GameObjectPresenter)this.manager.getPresenter(GameObjectNode.class);
        ComponentNode component = new ComponentNode();
        presenter.onAddComponent(node, component);
        assertEquals(1, node.getChildren().size());
        assertEquals(component, node.getChildren().get(0));
        verifyUpdate(node);

        undo();
        assertEquals(0, node.getChildren().size());
        verifyUpdate(node);

        redo();
        assertEquals(1, node.getChildren().size());
        assertEquals(component, node.getChildren().get(0));
        verifyUpdate(node);
    }

    @Test
    public void testRemoveComponent() throws ExecutionException {
        GameObjectNode node = (GameObjectNode)this.model.getRoot();
        GameObjectPresenter presenter = (GameObjectPresenter)this.manager.getPresenter(GameObjectNode.class);
        ComponentNode component = new ComponentNode();
        presenter.onAddComponent(node, component);
        assertEquals(1, node.getChildren().size());
        assertEquals(component, node.getChildren().get(0));
        verifyUpdate(node);

        undo();
        assertEquals(0, node.getChildren().size());
        verifyUpdate(node);

        redo();
        assertEquals(1, node.getChildren().size());
        assertEquals(component, node.getChildren().get(0));
        verifyUpdate(node);
    }

    @Override
    Module getModule() {
        return new TestModule();
    }

}
