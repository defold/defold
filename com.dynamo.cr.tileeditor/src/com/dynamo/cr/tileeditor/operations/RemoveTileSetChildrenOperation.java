package com.dynamo.cr.tileeditor.operations;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

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

public class RemoveTileSetChildrenOperation extends RemoveChildrenOperation {

    final private TileSetNode tileSet;
    final private List<CollisionGroupNode> oldTileCollisionGroups;
    final private List<CollisionGroupNode> newTileCollisionGroups;

    public RemoveTileSetChildrenOperation(List<Node> children, IPresenterContext presenterContext) {
        super(children, presenterContext);
        this.tileSet = (TileSetNode)children.get(0).getParent();
        this.oldTileCollisionGroups = this.tileSet.getTileCollisionGroups();
        this.newTileCollisionGroups = new ArrayList<CollisionGroupNode>(this.oldTileCollisionGroups);
        // Re-map tiles to other groups with the same ids
        List<CollisionGroupNode> allGroups = this.tileSet.getCollisionGroups();
        Map<CollisionGroupNode, CollisionGroupNode> replacements = new HashMap<CollisionGroupNode, CollisionGroupNode>();
        for (Node child : children) {
            if (child instanceof CollisionGroupNode) {
                CollisionGroupNode group = (CollisionGroupNode)child;
                boolean found = false;
                for (CollisionGroupNode replacement : allGroups) {
                    if (group != replacement && group.getId().equals(replacement.getId())) {
                        replacements.put(group, replacement);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    replacements.put(group, null);
                }
            }
        }
        for (Map.Entry<CollisionGroupNode, CollisionGroupNode> entry : replacements.entrySet()) {
            Collections.replaceAll(this.newTileCollisionGroups, entry.getKey(), entry.getValue());
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
