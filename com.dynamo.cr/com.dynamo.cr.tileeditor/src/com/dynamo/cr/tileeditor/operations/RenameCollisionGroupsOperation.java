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

public class RenameCollisionGroupsOperation extends AbstractOperation {
    TileSetModel model;
    String[] oldCollisionGroups;
    String[] newCollisionGroups;
    String[] newSelectedCollisionGroups;
    boolean[] collisions;
    List<ConvexHull> oldConvexHulls;
    List<ConvexHull> convexHulls;

    public RenameCollisionGroupsOperation(TileSetModel model, String[] newCollisionGroups) {
        super("Rename Collision Group(s)");
        this.model = model;
        this.oldCollisionGroups = model.getSelectedCollisionGroups();
        this.newCollisionGroups = newCollisionGroups;
        int n = newCollisionGroups.length;
        this.collisions = new boolean[n];
        List<String> newSelectedGroups = new ArrayList<String>(n);
        for (int i = 0; i < n; ++i) {
            this.collisions[i] = model.getCollisionGroups().contains(newCollisionGroups[i]);
            boolean collision = false;
            if (!this.collisions[i]) {
                // check if the name collides with another of the earlier new names
                for (int j = 0; j < i; ++j) {
                    if (newCollisionGroups[i].equals(newCollisionGroups[j])) {
                        collision = true;
                        break;
                    }
                }
            }
            if (collision) {
                this.collisions[i] = collision;
            } else {
                newSelectedGroups.add(newCollisionGroups[i]);
            }
        }
        this.newSelectedCollisionGroups = newSelectedGroups.toArray(new String[newSelectedGroups.size()]);
        this.oldConvexHulls = model.getConvexHulls();
        n = this.oldConvexHulls.size();
        this.convexHulls = new ArrayList<ConvexHull>(n);
        for (int i = 0; i < n; ++i) {
            ConvexHull convexHull = new ConvexHull(this.oldConvexHulls.get(i));
            int ng = this.oldCollisionGroups.length;
            for (int j = 0; j < ng; ++j) {
                if (convexHull.getCollisionGroup().equals(this.oldCollisionGroups[j])) {
                    convexHull.setCollisionGroup(this.newCollisionGroups[j]);
                    break;
                }
            }
            this.convexHulls.add(convexHull);
        }
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.setSelectedCollisionGroups(this.newSelectedCollisionGroups);
        this.model.renameCollisionGroups(this.oldCollisionGroups, this.newCollisionGroups);
        this.model.setConvexHulls(this.convexHulls);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.setSelectedCollisionGroups(this.newSelectedCollisionGroups);
        this.model.renameCollisionGroups(this.oldCollisionGroups, this.newCollisionGroups);
        this.model.setConvexHulls(this.convexHulls);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.setSelectedCollisionGroups(this.oldCollisionGroups);
        int n = this.oldCollisionGroups.length;
        for (int i = 0; i < n; ++i) {
            if (this.collisions[i]) {
                this.model.addCollisionGroup(this.oldCollisionGroups[i]);
            } else {
                this.model.renameCollisionGroup(this.newCollisionGroups[i], this.oldCollisionGroups[i]);
            }
        }
        this.model.setConvexHulls(this.oldConvexHulls);
        return Status.OK_STATUS;
    }

}
