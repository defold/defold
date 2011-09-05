package com.dynamo.cr.tileeditor.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.tileeditor.core.TileSetModel;

public class RenameCollisionGroupOperation extends AbstractOperation {
    TileSetModel model;
    String collisionGroup;
    String newCollisionGroup;

    public RenameCollisionGroupOperation(TileSetModel model, String collisionGroup, String newCollisionGroup) {
        super("rename " + collisionGroup + " to " + newCollisionGroup);
        this.model = model;
        this.collisionGroup = collisionGroup;
        this.newCollisionGroup = newCollisionGroup;
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.renameCollisionGroup(this.collisionGroup, this.newCollisionGroup);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.renameCollisionGroup(this.collisionGroup, this.newCollisionGroup);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.renameCollisionGroup(this.newCollisionGroup, this.collisionGroup);
        return Status.OK_STATUS;
    }

}
