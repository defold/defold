package com.dynamo.cr.tileeditor.operations;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.bob.tile.ConvexHull;
import com.dynamo.cr.tileeditor.core.TileSetModel;

public class RemoveCollisionGroupsOperation extends AbstractOperation {
    TileSetModel model;
    String[] collisionGroups;
    List<ConvexHull> oldConvexHulls;
    List<ConvexHull> convexHulls;

    public RemoveCollisionGroupsOperation(TileSetModel model) {
        super("Delete Collision Group(s)");
        this.model = model;
        this.collisionGroups = model.getSelectedCollisionGroups();
        this.oldConvexHulls = model.getConvexHulls();
        int n = this.oldConvexHulls.size();
        this.convexHulls = new ArrayList<ConvexHull>(n);
        for (int i = 0; i < n; ++i) {
            ConvexHull convexHull = new ConvexHull(this.oldConvexHulls.get(i));
            for (String collisionGroup : this.collisionGroups) {
                if (convexHull.getCollisionGroup().equals(collisionGroup)) {
                    convexHull.setCollisionGroup("");
                    break;
                }
            }
            this.convexHulls.add(convexHull);
        }
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.setSelectedCollisionGroups(new String[0]);
        this.model.removeCollisionGroups(this.collisionGroups);
        this.model.setConvexHulls(this.convexHulls);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.setSelectedCollisionGroups(new String[0]);
        this.model.removeCollisionGroups(this.collisionGroups);
        this.model.setConvexHulls(this.convexHulls);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.setSelectedCollisionGroups(this.collisionGroups);
        this.model.addCollisionGroups(this.collisionGroups);
        this.model.setConvexHulls(this.oldConvexHulls);
        return Status.OK_STATUS;
    }

}
