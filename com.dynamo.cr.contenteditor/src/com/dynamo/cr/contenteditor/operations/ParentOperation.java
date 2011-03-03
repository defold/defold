package com.dynamo.cr.contenteditor.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.contenteditor.math.Transform;
import com.dynamo.cr.contenteditor.scene.InstanceNode;
import com.dynamo.cr.contenteditor.scene.Node;
import com.dynamo.cr.contenteditor.scene.NodeUtil;

public class ParentOperation extends AbstractOperation {

    private InstanceNode[] nodes;
    private InstanceNode parent;
    private Node[] oldParents;

    public ParentOperation(InstanceNode[] nodes, InstanceNode parent) {
        super("Parent");
        this.nodes = nodes;
        this.parent = parent;

        if (nodes != null) {
            this.oldParents = new Node[nodes.length];
            for (int i = 0; i < nodes.length; ++i) {
                if (nodes[i] != null) {
                    this.oldParents[i] = nodes[i].getParent();
                }
            }
        }
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        Transform t = new Transform();
        if (this.parent == null) {
            throw new ExecutionException("No parent was selected for parenting.");
        } else if (this.nodes == null || this.nodes.length == 0) {
            throw new ExecutionException("No items were selected to be parented.");
        }
        for (Node node : this.nodes) {
            Node parent = this.parent;
            if (parent == node) {
                throw new ExecutionException(String.format("The item %s can not be parented to itself.", node.getIdentifier()));
            }
            while (parent != null && parent != node) {
                parent = parent.getParent();
            }
            if (parent == node) {
                throw new ExecutionException(String.format("The item %s is already above %s and can not be parented.", node.getIdentifier(), this.parent.getIdentifier()));
            }
        }
        for (Node node : this.nodes) {
            node.getWorldTransform(t);
            node.setParent(this.parent);
            NodeUtil.setWorldTransform(node, t);
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
        Transform t = new Transform();
        for (int i = 0; i < nodes.length; ++i) {
            nodes[i].getWorldTransform(t);
            nodes[i].setParent(oldParents[i]);
            NodeUtil.setWorldTransform(nodes[i], t);
        }
        return Status.OK_STATUS;
    }

}
