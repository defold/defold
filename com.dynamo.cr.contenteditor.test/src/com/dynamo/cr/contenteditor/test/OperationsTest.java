package com.dynamo.cr.contenteditor.test;

import static org.hamcrest.CoreMatchers.is;
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
import com.dynamo.cr.contenteditor.operations.PasteOperation;
import com.dynamo.cr.contenteditor.scene.AbstractNodeLoaderFactory;
import com.dynamo.cr.contenteditor.scene.BrokenNode;
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
            nodes = new Node[] {new BrokenNode("id1", this.scene, null), new BrokenNode("id2", this.scene, null)};
            execute(new PasteOperation(nodes, target));
            assertTrue(false);
        } catch (ExecutionException e) {
            assertTrue(true);
        }
        assertThat(this.history.getUndoHistory(this.undoContext).length, is(0));
        // Valid operations, should succeed
        try {
            nodes = new Node[] {new BrokenNode("id1", this.scene, null), new BrokenNode("id2", this.scene, null)};
            Node root = new CollectionNode("collection", this.scene, null);
            target = new BrokenNode("broken", this.scene, null);
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
            nodes = new Node[] {new BrokenNode("id1", this.scene, null), new BrokenNode("id2", this.scene, null)};
            target = new BrokenNode("target", this.scene, null);
            execute(new PasteOperation(nodes, target));
            assertTrue(false);
        } catch (ExecutionException e) {
            assertTrue(true);
        }
        assertThat(this.history.getUndoHistory(this.undoContext).length, is(1));
    }
}
