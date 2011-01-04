package com.dynamo.cr.contenteditor.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.contenteditor.math.Transform;
import com.dynamo.cr.contenteditor.scene.Node;
import com.dynamo.cr.contenteditor.scene.NodeUtil;

public class ParentOperation extends AbstractOperation {

    private Node[] nodes;
    private Node[] oldParents;

    public ParentOperation(Node[] nodes) {
        super("Parent");
        this.nodes = new Node[nodes.length];
        this.oldParents = new Node[nodes.length];

        for (int i = 0; i < nodes.length; ++i) {
            this.nodes[i] = nodes[i];
            this.oldParents[i] = nodes[i].getParent();
        }
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        Transform t = new Transform();
        for (int i = 1; i < nodes.length; ++i) {
            nodes[i].getTransform(t);
            nodes[i].setParent(nodes[0]);
            NodeUtil.setWorldTransform(nodes[i], t);
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
        for (int i = 1; i < nodes.length; ++i) {
            nodes[i].getTransform(t);
            nodes[i].setParent(oldParents[i]);
            NodeUtil.setWorldTransform(nodes[i], t);
        }
        return Status.OK_STATUS;
    }

}
