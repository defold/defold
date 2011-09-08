package com.dynamo.cr.tileeditor.operations;

import java.util.ArrayList;
import java.util.List;

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
    List<TileSetModel.ConvexHull> convexHulls;
    boolean collision;

    public RenameCollisionGroupOperation(TileSetModel model, String collisionGroup, String newCollisionGroup) {
        super("rename " + collisionGroup + " to " + newCollisionGroup);
        this.model = model;
        collision = this.model.getCollisionGroups().contains(newCollisionGroup);
        this.collisionGroup = collisionGroup;
        this.newCollisionGroup = newCollisionGroup;
        convexHulls = new ArrayList<TileSetModel.ConvexHull>();
        for (TileSetModel.ConvexHull convexHull : this.model.getConvexHulls()) {
            if (convexHull.getCollisionGroup().equals(collisionGroup)) {
                convexHulls.add(convexHull);
            }
        }
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.renameCollisionGroup(this.collisionGroup, this.newCollisionGroup);
        for (TileSetModel.ConvexHull hull : this.convexHulls) {
            hull.setCollisionGroup(this.newCollisionGroup);
        }
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.renameCollisionGroup(this.collisionGroup, this.newCollisionGroup);
        for (TileSetModel.ConvexHull hull : this.convexHulls) {
            hull.setCollisionGroup(this.newCollisionGroup);
        }
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        if (collision) {
            this.model.addCollisionGroup(this.collisionGroup);
        } else {
            this.model.renameCollisionGroup(this.newCollisionGroup, this.collisionGroup);
        }
        for (TileSetModel.ConvexHull hull : this.convexHulls) {
            hull.setCollisionGroup(this.collisionGroup);
        }
        return Status.OK_STATUS;
    }

}
