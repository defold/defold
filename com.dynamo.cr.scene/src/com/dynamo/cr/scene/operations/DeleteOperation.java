package com.dynamo.cr.scene.operations;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.scene.graph.Node;

public class DeleteOperation extends AbstractOperation {

    private Node[] nodes;
    private Node[] deletedNodes;
    private Node[] oldParents;

    public DeleteOperation(Node[] nodes) {
        super("Delete GameObject");
        this.nodes = nodes;
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        if (this.nodes == null || this.nodes.length == 0) {
            throw new ExecutionException("Nothing to delete.");
        } else {
            for (Node node : this.nodes) {
                if (node == null) {
                    throw new ExecutionException("Empty item.");
                } else if (node.getParent() == null) {
                    throw new ExecutionException(String.format("The item %s can not be deleted since it does not have a parent.", node.getIdentifier()));
                }
            }
        }
        List<Node> nodeList = new ArrayList<Node>();
        Node prevNode = null;
        for (Node node : this.nodes) {
            while ((node.getFlags() & Node.FLAG_EDITABLE) == 0) {
                node = node.getParent();
            }
            boolean add = true;
            if (prevNode != null) {
                Node parent = node;
                while (parent != null && parent != prevNode) {
                    parent = parent.getParent();
                }
                if (parent == prevNode) {
                    add = false;
                }
            }
            if (add && node != null && (node.getFlags() & Node.FLAG_EDITABLE) != 0) {
                nodeList.add(node);
                prevNode = node;
            }
        }
        if (nodeList.size() == 0) {
            throw new ExecutionException("Nothing could be deleted.");
        }
        this.deletedNodes = nodeList.toArray(this.nodes);
        this.oldParents = new Node[this.deletedNodes.length];
        for (int i = 0; i < this.deletedNodes.length; ++i) {
            this.oldParents[i] = this.deletedNodes[i].getParent();
        }
        for (int i = 0; i < this.deletedNodes.length; ++i) {
            this.oldParents[i] = this.deletedNodes[i].getParent();
            this.oldParents[i].removeNode(this.deletedNodes[i]);
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
        for (int i = 0; i < this.deletedNodes.length; ++i) {
            oldParents[i].addNode(this.deletedNodes[i]);
        }
        return Status.OK_STATUS;
    }

}
