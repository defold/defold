package com.dynamo.cr.scene.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.scene.graph.CollectionNode;
import com.dynamo.cr.scene.graph.InstanceNode;

public class AddGameObjectOperation extends AbstractOperation {

    private InstanceNode node;
    private CollectionNode parent;
    private String oldId;

    public AddGameObjectOperation(InstanceNode node, CollectionNode parent) {
        super("Add GameObject");
        this.node = node;
        this.parent = parent;
        if (node != null) {
            this.oldId = node.getIdentifier();
        }
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        if (this.node == null) {
            throw new ExecutionException("No child supplied.");
        } else if (this.parent == null) {
            throw new ExecutionException("No parent supplied.");
        }
        if (parent.isChildIdentifierUsed(node, node.getIdentifier())) {
            node.setIdentifier(parent.getUniqueChildIdentifier(node));
        }
        parent.addNode(node);
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
        parent.removeNode(node);
        node.setIdentifier(oldId);
        return Status.OK_STATUS;
    }

}
