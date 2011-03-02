package com.dynamo.cr.contenteditor.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.DefaultOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.contenteditor.operations.AddGameObjectOperation;
import com.dynamo.cr.contenteditor.operations.AddSubCollectionOperation;
import com.dynamo.cr.contenteditor.operations.DeleteOperation;
import com.dynamo.cr.contenteditor.operations.ParentOperation;
import com.dynamo.cr.contenteditor.operations.PasteOperation;
import com.dynamo.cr.contenteditor.operations.SetIdentifierOperation;
import com.dynamo.cr.contenteditor.scene.AbstractNodeLoaderFactory;
import com.dynamo.cr.contenteditor.scene.CollectionInstanceNode;
import com.dynamo.cr.contenteditor.scene.CollectionNode;
import com.dynamo.cr.contenteditor.scene.InstanceNode;
import com.dynamo.cr.contenteditor.scene.Node;
import com.dynamo.cr.contenteditor.scene.PrototypeNode;
import com.dynamo.cr.contenteditor.scene.Scene;

public class OperationsTest {

    private AbstractNodeLoaderFactory factory;
    private Scene scene;
    private IOperationHistory history;
    private IProgressMonitor monitor;
    private IUndoContext undoContext;

    @Before
    public void setup() {
        SceneContext context = new SceneContext();
        this.factory = context.factory;
        this.scene = context.scene;
        this.history = new DefaultOperationHistory();
        this.monitor = new NullProgressMonitor();
        this.undoContext = new UndoContext();
    }

    private void execute(IUndoableOperation operation) throws ExecutionException {
        operation.addContext(undoContext);
        this.history.execute(operation, this.monitor, null);
    }

    private void undo() throws ExecutionException {
        this.history.undo(this.undoContext, this.monitor, null);
    }

    private void redo() throws ExecutionException {
        this.history.redo(this.undoContext, this.monitor, null);
    }

    @Test
    public void testAddGameObject() {
        InstanceNode node = null;
        CollectionNode parent = null;
        PrototypeNode prototype = new PrototypeNode("prototype", this.scene);
        // Empty operations, should fail
        try {
            node = new InstanceNode("instance", this.scene, null, prototype);
            execute(new AddGameObjectOperation(node, parent));
            assertTrue(false);
        } catch (ExecutionException e) {
            assertTrue(true);
        }
        assertThat(this.history.getUndoHistory(this.undoContext).length, is(0));
        try {
            node = null;
            parent = new CollectionNode("parent", this.scene, null);
            execute(new AddGameObjectOperation(node, parent));
            assertTrue(false);
        } catch (ExecutionException e) {
            assertTrue(true);
        }
        assertThat(this.history.getUndoHistory(this.undoContext).length, is(0));
        // Valid operations, should succeed
        try {
            String childId = "instance";
            node = new InstanceNode(childId, this.scene, null, prototype);
            parent = new CollectionNode("parent", this.scene, null);
            Node child = new InstanceNode(childId, this.scene, null, prototype);
            child.setParent(parent);
            assertThat(parent.getChildren().length, is(1));
            execute(new AddGameObjectOperation(node, parent));
            assertThat(parent.getChildren().length, is(2));
            assertTrue(!node.getIdentifier().equals(childId));
            undo();
            assertThat(parent.getChildren().length, is(1));
            assertTrue(node.getIdentifier().equals(childId));
            redo();
            assertThat(parent.getChildren().length, is(2));
            assertTrue(!node.getIdentifier().equals(childId));
        } catch (ExecutionException e) {
            assertTrue(false);
        }
    }

    @Test
    public void testAddSubCollection() {
        CollectionInstanceNode node = null;
        CollectionNode parent = null;
        CollectionNode collection = new CollectionNode("collection", this.scene, null);
        // Empty operations, should fail
        try {
            node = new CollectionInstanceNode("instance", this.scene, null, collection);
            execute(new AddSubCollectionOperation(node, parent));
            assertTrue(false);
        } catch (ExecutionException e) {
            assertTrue(true);
        }
        assertThat(this.history.getUndoHistory(this.undoContext).length, is(0));
        try {
            node = null;
            parent = new CollectionNode("parent", this.scene, null);
            execute(new AddSubCollectionOperation(node, parent));
            assertTrue(false);
        } catch (ExecutionException e) {
            assertTrue(true);
        }
        assertThat(this.history.getUndoHistory(this.undoContext).length, is(0));
        // Valid operations, should succeed
        try {
            String childId = "instance";
            node = new CollectionInstanceNode(childId, this.scene, null, collection);
            parent = new CollectionNode("parent", this.scene, null);
            Node child = new CollectionInstanceNode(childId, this.scene, null, collection);
            child.setParent(parent);
            assertThat(parent.getChildren().length, is(1));
            execute(new AddSubCollectionOperation(node, parent));
            assertThat(parent.getChildren().length, is(2));
            assertTrue(!node.getIdentifier().equals(childId));
            undo();
            assertThat(parent.getChildren().length, is(1));
            assertTrue(node.getIdentifier().equals(childId));
            redo();
            assertThat(parent.getChildren().length, is(2));
            assertTrue(!node.getIdentifier().equals(childId));
        } catch (ExecutionException e) {
            assertTrue(false);
        }
    }

    @Test
    public void testDelete() {
        Node[] nodes = null;
        // Empty operations, should fail
        try {
            execute(new DeleteOperation(nodes));
            assertTrue(false);
        } catch (ExecutionException e) {
            assertTrue(true);
        }
        assertThat(this.history.getUndoHistory(this.undoContext).length, is(0));
        // Empty operations, should fail
        try {
            nodes = new Node[1];
            execute(new DeleteOperation(nodes));
            assertTrue(false);
        } catch (ExecutionException e) {
            assertTrue(true);
        }
        assertThat(this.history.getUndoHistory(this.undoContext).length, is(0));
        // Valid operations, should succeed
        try {
            CollectionNode root = new CollectionNode("root", this.scene, null);
            nodes = new Node[] {new InstanceNode("child", this.scene, null, null)};
            nodes[0].setParent(root);
            assertThat(root.getChildren().length, is(1));
            execute(new DeleteOperation(nodes));
            assertThat(root.getChildren().length, is(0));
            undo();
            assertThat(root.getChildren().length, is(1));
            redo();
            assertThat(root.getChildren().length, is(0));
        } catch (ExecutionException e) {
            assertTrue(false);
        }
        assertThat(this.history.getUndoHistory(this.undoContext).length, is(1));
    }

    @Test
    public void testParent() {
        InstanceNode[] nodes = null;
        InstanceNode parent = null;
        // Empty operations, should fail
        try {
            execute(new ParentOperation(nodes, parent));
            assertTrue(false);
        } catch (ExecutionException e) {
            assertTrue(true);
        }
        assertThat(this.history.getUndoHistory(this.undoContext).length, is(0));
        try {
            nodes = new InstanceNode[2];
            execute(new ParentOperation(nodes, parent));
            assertTrue(false);
        } catch (ExecutionException e) {
            assertTrue(true);
        }
        assertThat(this.history.getUndoHistory(this.undoContext).length, is(0));
        try {
            nodes = new InstanceNode[] {new InstanceNode("id1", this.scene, null, null), new InstanceNode("id2", this.scene, null, null)};
            execute(new ParentOperation(nodes, parent));
            assertTrue(false);
        } catch (ExecutionException e) {
            assertTrue(true);
        }
        assertThat(this.history.getUndoHistory(this.undoContext).length, is(0));
        // Valid operations, should succeed
        try {
            nodes = new InstanceNode[] {new InstanceNode("id1", this.scene, null, null), new InstanceNode("id2", this.scene, null, null)};
            parent = new InstanceNode("parent", this.scene, null, null);
            assertThat(parent.getChildren().length, is(0));
            execute(new ParentOperation(nodes, parent));
            assertThat(parent.getChildren().length, is(2));
            undo();
            assertThat(parent.getChildren().length, is(0));
            redo();
            assertThat(parent.getChildren().length, is(2));
            assertTrue(true);
        } catch (ExecutionException e) {
            assertTrue(false);
        }
        assertThat(this.history.getUndoHistory(this.undoContext).length, is(1));
        // Invalid operations, should fail
        try {
            parent = new InstanceNode("parent", this.scene, null, null);
            InstanceNode child = new InstanceNode("child", this.scene, null, null);
            child.setParent(parent);
            nodes = new InstanceNode[] {parent};
            execute(new ParentOperation(nodes, child));
            assertTrue(false);
        } catch (ExecutionException e) {
            assertTrue(true);
        }
        assertThat(this.history.getUndoHistory(this.undoContext).length, is(1));
    }

    @Test
    public void testPaste() {
        Node[] nodes = null;
        Node target = null;
        // Empty operations, should fail
        try {
            execute(new PasteOperation(nodes, target));
            assertTrue(false);
        } catch (ExecutionException e) {
            assertTrue(true);
        }
        assertThat(this.history.getUndoHistory(this.undoContext).length, is(0));
        try {
            nodes = new Node[2];
            execute(new PasteOperation(nodes, target));
            assertTrue(false);
        } catch (ExecutionException e) {
            assertTrue(true);
        }
        assertThat(this.history.getUndoHistory(this.undoContext).length, is(0));
        try {
            nodes = new Node[] {new InstanceNode("id1", this.scene, null, null), new InstanceNode("id2", this.scene, null, null)};
            execute(new PasteOperation(nodes, target));
            assertTrue(false);
        } catch (ExecutionException e) {
            assertTrue(true);
        }
        assertThat(this.history.getUndoHistory(this.undoContext).length, is(0));
        // Valid operations, should succeed
        try {
            nodes = new Node[] {new InstanceNode("id1", this.scene, null, null), new InstanceNode("id2", this.scene, null, null)};
            Node root = new CollectionNode("collection", this.scene, null);
            target = new CollectionInstanceNode("id3", this.scene, null, null);
            target.setParent(root);
            assertThat(root.getChildren().length, is(1));
            execute(new PasteOperation(nodes, target));
            assertThat(root.getChildren().length, is(3));
            undo();
            assertThat(root.getChildren().length, is(1));
            redo();
            assertThat(root.getChildren().length, is(3));
        } catch (ExecutionException e) {
            assertTrue(false);
        }
        assertThat(this.history.getUndoHistory(this.undoContext).length, is(1));
        // Invalid operations, should fail
        try {
            nodes = new Node[] {new InstanceNode("id1", this.scene, null, null), new InstanceNode("id2", this.scene, null, null)};
            target = new InstanceNode("target", this.scene, null, null);
            execute(new PasteOperation(nodes, target));
            assertTrue(false);
        } catch (ExecutionException e) {
            assertTrue(true);
        }
        assertThat(this.history.getUndoHistory(this.undoContext).length, is(1));
    }

    @Test
    public void testSetIdentifier() {
        Node node = null;
        String id = null;
        // Empty operations, should fail
        try {
            execute(new SetIdentifierOperation(node, id));
            assertTrue(false);
        } catch (ExecutionException e) {
            assertTrue(true);
        }
        assertThat(this.history.getUndoHistory(this.undoContext).length, is(0));
        try {
            node = new InstanceNode("id", this.scene, null, null);
            execute(new SetIdentifierOperation(node, id));
            assertTrue(false);
        } catch (ExecutionException e) {
            assertTrue(true);
        }
        assertThat(this.history.getUndoHistory(this.undoContext).length, is(0));
        try {
            node = new InstanceNode("id", this.scene, null, null);
            id = "";
            execute(new SetIdentifierOperation(node, id));
            assertTrue(false);
        } catch (ExecutionException e) {
            assertTrue(true);
        }
        assertThat(this.history.getUndoHistory(this.undoContext).length, is(0));
        // Valid operations, should succeed
        try {
            String oldId = "id";
            node = new InstanceNode(oldId, this.scene, null, null);
            id = "id2";
            assertEquals(node.getIdentifier(), oldId);
            execute(new SetIdentifierOperation(node, id));
            assertEquals(node.getIdentifier(), id);
            undo();
            assertEquals(node.getIdentifier(), oldId);
            redo();
            assertEquals(node.getIdentifier(), id);
        } catch (ExecutionException e) {
            assertTrue(false);
        }
        assertThat(this.history.getUndoHistory(this.undoContext).length, is(1));
        // Invalid operations, should fail
        try {
            CollectionNode root = new CollectionNode("root", this.scene, null);
            InstanceNode child1 = new InstanceNode("id1", this.scene, null, null);
            child1.setParent(root);
            InstanceNode child2 = new InstanceNode("id2", this.scene, null, null);
            child2.setParent(root);
            execute(new SetIdentifierOperation(child2, child1.getIdentifier()));
            assertTrue(false);
        } catch (ExecutionException e) {
            assertTrue(true);
        }
        assertThat(this.history.getUndoHistory(this.undoContext).length, is(1));
    }
}
