package com.dynamo.cr.contenteditor.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.contenteditor.scene.Node;

public class SetIdentifierOperation extends AbstractOperation {

    private String identifier;
    private String originalIdentifier;
    private Node node;

    public SetIdentifierOperation(Node node, String identifier) {
        super("Set identifier");
        this.node = node;
        this.identifier = identifier;
        this.originalIdentifier = node.getIdentifier();
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        node.setIdentifier(identifier);
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
        node.setIdentifier(originalIdentifier);
        return Status.OK_STATUS;
    }
}
