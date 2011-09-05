package com.dynamo.cr.tileeditor.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.tileeditor.core.TileSetModel;

public class RemoveCollisionGroupOperation extends AbstractOperation {
    TileSetModel model;
    String collisionGroup;

    public RemoveCollisionGroupOperation(TileSetModel model, String collisionGroup) {
        super("remove " + collisionGroup);
        this.model = model;
        this.collisionGroup = collisionGroup;
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.removeCollisionGroup(this.collisionGroup);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.removeCollisionGroup(this.collisionGroup);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.addCollisionGroup(this.collisionGroup);
        return Status.OK_STATUS;
    }

}
