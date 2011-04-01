package com.dynamo.cr.scene.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.scene.graph.CollectionInstanceNode;
import com.dynamo.cr.scene.graph.CollectionNode;

public class AddSubCollectionOperation extends AbstractOperation {

    private CollectionInstanceNode node;
    private CollectionNode parent;
    private String oldId;

    public AddSubCollectionOperation(CollectionInstanceNode node, CollectionNode parent) {
        super("Add Subcollection");
        this.node = node;
        this.parent = parent;
        if (node != null) {
            oldId = node.getIdentifier();
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
        if (this.parent.isChildIdentifierUsed(this.node, this.node.getIdentifier())) {
            this.node.setIdentifier(this.parent.getUniqueChildIdentifier(this.node));
        }
        this.parent.addNode(node);
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
        this.parent.removeNode(this.node);
        this.node.setIdentifier(this.oldId);
        return Status.OK_STATUS;
    }

}
