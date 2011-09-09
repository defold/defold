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

public class RenameCollisionGroupsOperation extends AbstractOperation {
    TileSetModel model;
    String[] oldCollisionGroups;
    String[] newCollisionGroups;
    boolean[] collisions;
    List<ConvexHullGroup> convexHulls;

    static class ConvexHullGroup {
        public int index;
        public String oldGroup;
        public String newGroup;
    }

    public RenameCollisionGroupsOperation(TileSetModel model, String[] newCollisionGroups) {
        super("rename collision group(s)");
        this.model = model;
        this.oldCollisionGroups = model.getSelectedCollisionGroups();
        this.newCollisionGroups = newCollisionGroups;
        int n = newCollisionGroups.length;
        this.collisions = new boolean[n];
        for (int i = 0; i < n; ++i) {
            this.collisions[i] = model.getCollisionGroups().contains(newCollisionGroups[i]);
        }
        this.convexHulls = new ArrayList<ConvexHullGroup>();
        n = model.getConvexHulls().size();
        for (int i = 0; i < n; ++i) {
            ConvexHull convexHull = model.getConvexHulls().get(i);
            int ng = this.oldCollisionGroups.length;
            for (int j = 0; j < ng; ++j) {
                if (convexHull.getCollisionGroup().equals(this.oldCollisionGroups[j])) {
                    ConvexHullGroup hullGroup = new ConvexHullGroup();
                    hullGroup.index = i;
                    hullGroup.oldGroup = this.oldCollisionGroups[j];
                    hullGroup.newGroup = this.newCollisionGroups[j];
                    this.convexHulls.add(hullGroup);
                }
            }
        }
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.setSelectedCollisionGroups(this.newCollisionGroups);
        this.model.renameCollisionGroups(this.oldCollisionGroups, this.newCollisionGroups);
        for (ConvexHullGroup hullGroup : this.convexHulls) {
            ConvexHull convexHull = this.model.getConvexHulls().get(hullGroup.index);
            convexHull.setCollisionGroup(hullGroup.newGroup);
        }
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.setSelectedCollisionGroups(this.newCollisionGroups);
        this.model.renameCollisionGroups(this.oldCollisionGroups, this.newCollisionGroups);
        for (ConvexHullGroup hullGroup : this.convexHulls) {
            ConvexHull convexHull = this.model.getConvexHulls().get(hullGroup.index);
            convexHull.setCollisionGroup(hullGroup.newGroup);
        }
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
        for (ConvexHullGroup hullGroup : this.convexHulls) {
            ConvexHull convexHull = this.model.getConvexHulls().get(hullGroup.index);
            convexHull.setCollisionGroup(hullGroup.oldGroup);
        }
        return Status.OK_STATUS;
    }

}
