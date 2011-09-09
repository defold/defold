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

public class RemoveCollisionGroupsOperation extends AbstractOperation {
    TileSetModel model;
    String[] collisionGroups;
    List<ConvexHullGroup> convexHulls;

    static class ConvexHullGroup {
        private int index;
        private String group;
    }

    public RemoveCollisionGroupsOperation(TileSetModel model) {
        super("remove collision group(s)");
        this.model = model;
        this.collisionGroups = model.getSelectedCollisionGroups();
        this.convexHulls = new ArrayList<ConvexHullGroup>();
        int n = model.getConvexHulls().size();
        for (int i = 0; i < n; ++i) {
            ConvexHull convexHull = model.getConvexHulls().get(i);
            for (String collisionGroup : this.collisionGroups) {
                if (convexHull.getCollisionGroup().equals(collisionGroup)) {
                    ConvexHullGroup group = new ConvexHullGroup();
                    group.index = i;
                    group.group = collisionGroup;
                    this.convexHulls.add(group);
                }
            }
        }
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.setSelectedCollisionGroups(new String[0]);
        this.model.removeCollisionGroups(this.collisionGroups);
        for (ConvexHullGroup hull : this.convexHulls) {
            this.model.getConvexHulls().get(hull.index).setCollisionGroup("");
        }
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.setSelectedCollisionGroups(new String[0]);
        this.model.removeCollisionGroups(this.collisionGroups);
        for (ConvexHullGroup hull : this.convexHulls) {
            this.model.getConvexHulls().get(hull.index).setCollisionGroup("");
        }
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.setSelectedCollisionGroups(this.collisionGroups);
        this.model.addCollisionGroups(this.collisionGroups);
        for (ConvexHullGroup hull : this.convexHulls) {
            this.model.getConvexHulls().get(hull.index).setCollisionGroup(hull.group);
        }
        return Status.OK_STATUS;
    }

}
