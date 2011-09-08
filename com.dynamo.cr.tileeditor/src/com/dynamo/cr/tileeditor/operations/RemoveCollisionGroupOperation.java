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
import com.dynamo.cr.tileeditor.core.TileSetModel.ConvexHull;

public class RemoveCollisionGroupOperation extends AbstractOperation {
    TileSetModel model;
    String collisionGroup;
    List<ConvexHull> convexHulls;

    public RemoveCollisionGroupOperation(TileSetModel model, String collisionGroup) {
        super("remove " + collisionGroup);
        this.model = model;
        this.collisionGroup = collisionGroup;
        this.convexHulls = new ArrayList<ConvexHull>();
        for (ConvexHull convexHull : this.model.getConvexHulls()) {
            if (convexHull.getCollisionGroup().equals(collisionGroup)) {
                this.convexHulls.add(convexHull);
            }
        }
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.removeCollisionGroup(this.collisionGroup);
        for (ConvexHull convexHull : this.convexHulls) {
            convexHull.setCollisionGroup("");
        }
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.removeCollisionGroup(this.collisionGroup);
        for (ConvexHull convexHull : this.convexHulls) {
            convexHull.setCollisionGroup("");
        }
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.addCollisionGroup(this.collisionGroup);
        for (ConvexHull convexHull : this.convexHulls) {
            convexHull.setCollisionGroup(this.collisionGroup);
        }
        return Status.OK_STATUS;
    }

}
