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
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.operations.RemoveChildrenOperation;
import com.dynamo.cr.tileeditor.scene.CollisionGroupNode;
import com.dynamo.cr.tileeditor.scene.TileSetNode;

public class RemoveCollisionGroupNodeOperation extends RemoveChildrenOperation {

    final private TileSetNode tileSet;
    final private List<String> oldTileCollisionGroups;
    final private List<String> newTileCollisionGroups;

    public RemoveCollisionGroupNodeOperation(List<Node> collisionGroups, IPresenterContext presenterContext) {
        super(collisionGroups, presenterContext);
        this.tileSet = ((CollisionGroupNode)collisionGroups.get(0)).getTileSetNode();
        this.oldTileCollisionGroups = this.tileSet.getTileCollisionGroups();
        this.newTileCollisionGroups = new ArrayList<String>(this.oldTileCollisionGroups);
        List<CollisionGroupNode> children = new ArrayList<CollisionGroupNode>(this.tileSet.getCollisionGroups());
        for (Node node : collisionGroups) {
            children.remove(node);
        }
        for (Node node : collisionGroups) {
            CollisionGroupNode collisionGroup = (CollisionGroupNode)node;
            String id = collisionGroup.getId();
            boolean match = false;
            for (CollisionGroupNode child : this.tileSet.getCollisionGroups()) {
                if (child.getId().equals(id)) {
                    match = true;
                }
            }
            if (!match) {
                Collections.replaceAll(this.newTileCollisionGroups, id, "");
            }
        }
    }

    @Override
    protected IStatus doExecute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        super.doExecute(monitor, info);
        this.tileSet.setTileCollisionGroups(this.newTileCollisionGroups);
        return Status.OK_STATUS;
    }

    @Override
    protected IStatus doRedo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        super.doRedo(monitor, info);
        this.tileSet.setTileCollisionGroups(this.newTileCollisionGroups);
        return Status.OK_STATUS;
    }

    @Override
    protected IStatus doUndo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        super.doUndo(monitor, info);
        this.tileSet.setTileCollisionGroups(this.oldTileCollisionGroups);
        return Status.OK_STATUS;
    }

}
