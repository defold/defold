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

public class UnparentOperation extends AbstractOperation {

    private InstanceNode[] nodes;
    private Node[] oldParents;

    public UnparentOperation(InstanceNode[] nodes) {
        super("Unparent");
        this.nodes = nodes;
        if (nodes != null && nodes.length > 0) {
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
        if (this.nodes == null || this.nodes.length == 0) {
            throw new ExecutionException("No items could be unparented.");
        }
        Node[] newParents = new Node[this.nodes.length];
        boolean foundParents = false;
        for (int i = 0; i < this.nodes.length; ++i) {
            Node node = this.nodes[i];
            if (node != null) {
                Node parent = node.getParent();
                if (parent != null) {
                    parent = parent.getParent();
                    while (parent != null && !parent.acceptsChild(node)) {
                        parent = parent.getParent();
                    }
                    newParents[i] = parent;
                    if (parent != null) {
                        foundParents = true;
                    }
                }
            }
        }
        if (!foundParents) {
            throw new ExecutionException("No items could be unparented.");
        }
        Transform t = new Transform();
        for (int i = 0; i < this.nodes.length; ++i) {
            Node node = this.nodes[i];
            node.getWorldTransform(t);
            node.setParent(newParents[i]);;
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
        for (int i = 0; i < this.nodes.length; ++i) {
            Node node = this.nodes[i];
            node.getWorldTransform(t);
            node.setParent(this.oldParents[i]);
            NodeUtil.setWorldTransform(node, t);
        }
        return Status.OK_STATUS;
    }

}
