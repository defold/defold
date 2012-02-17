package com.dynamo.cr.tileeditor.operations;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

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
    final private List<String> oldTileCollisionGroups;
    final private List<String> newTileCollisionGroups;

    public RemoveTileSetChildrenOperation(List<Node> children, IPresenterContext presenterContext) {
        super(children, presenterContext);
        this.tileSet = (TileSetNode)children.get(0).getParent();
        this.oldTileCollisionGroups = this.tileSet.getTileCollisionGroups();
        this.newTileCollisionGroups = new ArrayList<String>(this.oldTileCollisionGroups);
        // Clear out all unused collision groups
        Set<String> removedSet = new HashSet<String>();
        for (Node node : children) {
            if (node instanceof CollisionGroupNode) {
                removedSet.add(((CollisionGroupNode)node).getId());
            }
        }
        List<Node> remainingGroups = new ArrayList<Node>(this.tileSet.getCollisionGroups());
        remainingGroups.removeAll(children);
        Set<String> remainingSet = new HashSet<String>();
        for (Node node : remainingGroups) {
            remainingSet.add(((CollisionGroupNode)node).getId());
        }
        removedSet.removeAll(remainingSet);
        for (String group : removedSet) {
            Collections.replaceAll(this.newTileCollisionGroups, group, "");
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
