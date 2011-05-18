package com.dynamo.cr.scene.operations;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.scene.graph.Node;

public class PasteOperation extends AbstractOperation {

    private Node[] nodes;
    private Node target;
    private Map<ComparableNode, List<Node>> addedNodes;

    public PasteOperation(Node[] nodes, Node target) {
        super("Paste");
        this.nodes = nodes;
        this.target = target;
        this.addedNodes = new TreeMap<ComparableNode, List<Node>>();
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

        this.addedNodes.clear();

        try {
            for (Node node : this.nodes) {
                Node target = this.target;
                while (target != null && ((target.getIdentifier() != null && target.getIdentifier().equals(node.getIdentifier())) || !target.acceptsChild(node))) {
                    target = target.getParent();
                }
                if (target != null) {
                    ComparableNode compTarget = new ComparableNode(target);
                    if (!addedNodes.containsKey(compTarget)) {
                        addedNodes.put(compTarget, new ArrayList<Node>());
                    }
                    modifyIdentifiers(target, node);
                    target.addNode(node);
                    addedNodes.get(compTarget).add(node);
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
