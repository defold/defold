package com.dynamo.cr.tileeditor.operations;

import java.util.List;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.bob.tile.ConvexHull;
import com.dynamo.cr.tileeditor.core.TileSetModel;

public class SetConvexHullsOperation extends AbstractOperation {
    TileSetModel model;
    List<ConvexHull> oldConvexHulls;
    List<ConvexHull> convexHulls;

    public SetConvexHullsOperation(TileSetModel model, List<ConvexHull> oldConvexHulls, List<ConvexHull> convexHulls, String collisionGroup) {
        super(collisionGroup.equals("")?"Clear Collision Group":"Set Collision Group To " + collisionGroup);
        this.model = model;
        this.oldConvexHulls = oldConvexHulls;
        this.convexHulls = convexHulls;
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.setConvexHulls(this.convexHulls);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.setConvexHulls(this.convexHulls);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.setConvexHulls(this.oldConvexHulls);
        return Status.OK_STATUS;
    }

}
