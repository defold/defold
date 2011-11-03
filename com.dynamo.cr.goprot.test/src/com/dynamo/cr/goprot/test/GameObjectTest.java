package com.dynamo.cr.goprot.test;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.DefaultOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.core.runtime.CoreException;
import org.junit.Before;
import org.junit.Test;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import com.dynamo.cr.goprot.core.INodeView;
import com.dynamo.cr.goprot.core.Node;
import com.dynamo.cr.goprot.core.NodeManager;
import com.dynamo.cr.goprot.core.NodeModel;
import com.dynamo.cr.goprot.gameobject.ComponentNode;
import com.dynamo.cr.goprot.gameobject.GameObjectNode;
import com.dynamo.cr.goprot.gameobject.GameObjectPresenter;
import com.dynamo.cr.goprot.core.ILogger;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.Singleton;

public class GameObjectTest {
    private NodeModel model;
    private Injector injector;
    private INodeView view;
    private NodeManager manager;
    private IOperationHistory history;
    private IUndoContext undoContext;
    private Map<Node, Integer> updateCounts;

    static class TestLogger implements ILogger {

        @Override
        public void logException(Throwable exception) {
            throw new UnsupportedOperationException(exception);
        }

    }

    class TestModule extends AbstractModule {
        @Override
        protected void configure() {
            bind(GameObjectPresenter.class).in(Singleton.class);
            bind(NodeModel.class).in(Singleton.class);
            bind(NodeManager.class).in(Singleton.class);
            bind(INodeView.class).toInstance(view);
            bind(IOperationHistory.class).to(DefaultOperationHistory.class).in(Singleton.class);
            bind(IUndoContext.class).to(UndoContext.class).in(Singleton.class);
            bind(ILogger.class).to(TestLogger.class);
        }
    }

    @Before
    public void setup() throws CoreException, IOException {
        System.setProperty("java.awt.headless", "true");

        this.view = mock(INodeView.class);

        TestModule module = new TestModule();
        this.injector = Guice.createInjector(module);
        this.model = this.injector.getInstance(NodeModel.class);
        this.model.setRoot(new GameObjectNode());
        this.manager = this.injector.getInstance(NodeManager.class);
        this.manager.registerPresenter(GameObjectNode.class, this.injector.getInstance(GameObjectPresenter.class));
        this.history = this.injector.getInstance(IOperationHistory.class);
        this.undoContext = this.injector.getInstance(IUndoContext.class);

        this.updateCounts = new HashMap<Node, Integer>();
    }

    // Helpers

    private void undo() throws ExecutionException {
        this.history.undo(this.undoContext, null, null);
    }

    private void redo() throws ExecutionException {
        this.history.redo(this.undoContext, null, null);
    }

    private void verifyUpdate(Node node) {
        Integer count = this.updateCounts.get(node);
        if (count == null) {
            count = new Integer(1);
        } else {
            count = new Integer(count.intValue() + 1);
        }
        this.updateCounts.put(node, count);
        verify(this.view, times(count.intValue())).updateNode(node);
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

}
