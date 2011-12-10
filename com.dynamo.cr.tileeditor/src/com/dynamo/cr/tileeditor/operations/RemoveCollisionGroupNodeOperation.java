package com.dynamo.cr.tileeditor.operations;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.NodeUtil;
import com.dynamo.cr.sceneed.core.operations.AbstractSelectOperation;
import com.dynamo.cr.tileeditor.scene.CollisionGroupNode;
import com.dynamo.cr.tileeditor.scene.TileSetNode;

public class RemoveCollisionGroupNodeOperation extends AbstractSelectOperation {

    final private TileSetNode tileSet;
    final private CollisionGroupNode collisionGroup;
    final private List<String> oldTileCollisionGroups;
    final private List<String> newTileCollisionGroups;

    public RemoveCollisionGroupNodeOperation(CollisionGroupNode collisionGroup, IPresenterContext presenterContext) {
        super("Remove Component", NodeUtil.getSelectionReplacement(collisionGroup), presenterContext);
        this.tileSet = collisionGroup.getTileSetNode();
        this.collisionGroup = collisionGroup;
        this.oldTileCollisionGroups = this.tileSet.getTileCollisionGroups();
        this.newTileCollisionGroups = new ArrayList<String>(this.oldTileCollisionGroups);
        String id = collisionGroup.getId();
        boolean match = false;
        for (CollisionGroupNode child : this.tileSet.getCollisionGroups()) {
            if (child != collisionGroup
                    && child.getId().equals(id)) {
                match = true;
            }
        }
        if (!match) {
            Collections.replaceAll(this.newTileCollisionGroups, id, "");
        }
    }

    @Override
    protected IStatus doExecute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.tileSet.removeCollisionGroup(this.collisionGroup);
        this.tileSet.setTileCollisionGroups(this.newTileCollisionGroups);
        return Status.OK_STATUS;
    }

    @Override
    protected IStatus doRedo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.tileSet.removeCollisionGroup(this.collisionGroup);
        this.tileSet.setTileCollisionGroups(this.newTileCollisionGroups);
        return Status.OK_STATUS;
    }

    @Override
    protected IStatus doUndo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.tileSet.addCollisionGroup(this.collisionGroup);
        this.tileSet.setTileCollisionGroups(this.oldTileCollisionGroups);
        return Status.OK_STATUS;
    }

}
