package com.dynamo.cr.tileeditor.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.tileeditor.core.TileSetModel;

public class SetConvexHullCollisionGroupOperation extends AbstractOperation {
    TileSetModel model;
    int index;
    String oldCollisionGroup;
    String collisionGroup;

    public SetConvexHullCollisionGroupOperation(TileSetModel model, int index, String collisionGroup) {
        super("set collision group to " + collisionGroup);
        this.model = model;
        this.index = index;
        this.oldCollisionGroup = this.model.getConvexHulls().get(index).getCollisionGroup();
        this.collisionGroup = collisionGroup;
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.setConvexHullCollisionGroup(this.index, this.collisionGroup);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.setConvexHullCollisionGroup(this.index, this.collisionGroup);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.setConvexHullCollisionGroup(this.index, this.oldCollisionGroup);
        return Status.OK_STATUS;
    }

}
