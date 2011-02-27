package com.dynamo.cr.contenteditor.operations;

import java.io.ByteArrayInputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.contenteditor.editors.IEditor;
import com.dynamo.cr.contenteditor.editors.NodeLoaderFactory;
import com.dynamo.cr.contenteditor.scene.CollectionNode;
import com.dynamo.cr.contenteditor.scene.Node;
import com.dynamo.cr.contenteditor.scene.Scene;

public class PasteOperation extends AbstractOperation {

    private String data;
    private IEditor editor;
    private Map<ComparableNode, List<Node>> addedNodes;
    private Node pasteTarget;

    public void setScene(Scene scene, Node node) {
        node.setScene(scene);
        for (Node n : node.getChildren()) {
            setScene(scene, n);
        }
    }

    public PasteOperation(IEditor editor, String data, Node paste_target) {
        super("Paste");
        this.editor = editor;
        this.data = data;
        this.pasteTarget = paste_target;
    }

    private class ComparableNode implements Comparable<ComparableNode> {
        public Node node;

        public ComparableNode(Node node) {
            this.node = node;
        }

        @Override
        public int compareTo(ComparableNode o) {
            return this.node.toString().compareTo(o.node.toString());
        }
    }

    private void modifyIdentifiers(Node target, Node node) {
        if (target.isChildIdentifierUsed(node, node.getIdentifier())) {
            node.setIdentifier(target.getUniqueChildIdentifier(node));
        }
        for (Node child : node.getChildren()) {
            modifyIdentifiers(target, child);
        }
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {

        addedNodes = new TreeMap<ComparableNode, List<Node>>();
        NodeLoaderFactory factory = editor.getLoaderFactory();

        ByteArrayInputStream stream = new ByteArrayInputStream(data.getBytes());
        Scene scene = new Scene();
        try {
            CollectionNode node = (CollectionNode) factory.load(new NullProgressMonitor(), scene, "clipboard.collection", stream, null);
            setScene(pasteTarget.getScene(), node);
            for (Node n : node.getChildren()) {
                Node target = pasteTarget;
                while (target != null && ((target.getIdentifier() != null && target.getIdentifier().equals(n.getIdentifier())) || !target.acceptsChild(n))) {
                    target = target.getParent();
                }
                if (target != null) {
                    ComparableNode compTarget = new ComparableNode(target);
                    if (!addedNodes.containsKey(compTarget)) {
                        addedNodes.put(compTarget, new ArrayList<Node>());
                    }
                    addedNodes.get(compTarget).add(n);
                    target.addNode(n);
                    modifyIdentifiers(target, n);
                }
            }
            if (addedNodes.isEmpty()) {
                throw new ExecutionException("No items could be pasted since there was no appropriate destination.");
            }
        } catch (Throwable e) {
            throw new ExecutionException(e.getMessage(), e);
        }

        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        return execute(monitor, info);
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        for (Map.Entry<ComparableNode, List<Node>> entry : addedNodes.entrySet()) {
            for (Node child : entry.getValue()) {
                entry.getKey().node.removeNode(child);
            }
        }
        return Status.OK_STATUS;
    }

}
