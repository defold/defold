package com.dynamo.cr.contenteditor.operations;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.contenteditor.editors.IEditor;
import com.dynamo.cr.contenteditor.scene.Node;

public class DeleteOperation extends AbstractOperation {

    private Node[] nodes;
    private Node[] oldParents;

    public DeleteOperation(IEditor editor, Node[] nodes) {
        super("Delete GameObject");
        List<Node> nodeList = new ArrayList<Node>();
        Node prevNode = null;
        for (Node node : nodes) {
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
        this.nodes = nodeList.toArray(nodes);
        this.oldParents = new Node[this.nodes.length];
        for (int i = 0; i < nodes.length; ++i) {
            this.oldParents[i] = this.nodes[i].getParent();
        }
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        for (int i = 0; i < nodes.length; ++i) {
            oldParents[i] = nodes[i].getParent();
            nodes[i].getParent().removeNode(nodes[i]);
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
        for (int i = 0; i < nodes.length; ++i) {
            oldParents[i].addNode(nodes[i]);
        }
        return Status.OK_STATUS;
    }

}
