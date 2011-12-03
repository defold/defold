package com.dynamo.cr.tileeditor.operations;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.StructuredSelection;

import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.tileeditor.scene.CollisionGroupNode;
import com.dynamo.cr.tileeditor.scene.TileSetNode;

public class RemoveCollisionGroupNodeOperation extends AbstractOperation {

    final private TileSetNode tileSet;
    final private CollisionGroupNode collisionGroup;
    final private IStructuredSelection oldSelection;
    final private IStructuredSelection newSelection;
    final private List<String> oldTileCollisionGroups;
    final private List<String> newTileCollisionGroups;

    public RemoveCollisionGroupNodeOperation(CollisionGroupNode collisionGroup) {
        super("Remove Component");
        this.tileSet = (TileSetNode) collisionGroup.getParent();
        this.collisionGroup = collisionGroup;
        this.oldSelection = collisionGroup.getModel().getSelection();
        List<Node> children = this.tileSet.getChildren();
        int index = children.indexOf(collisionGroup);
        if (index + 1 < children.size()) {
            ++index;
        } else {
            --index;
        }
        Node selected = null;
        if (index >= 0) {
            selected = children.get(index);
        } else {
            selected = this.tileSet;
        }
        this.newSelection = new StructuredSelection(selected);
        this.oldTileCollisionGroups = this.tileSet.getTileCollisionGroups();
        this.newTileCollisionGroups = new ArrayList<String>(this.oldTileCollisionGroups);
        String name = collisionGroup.getName();
        boolean match = false;
        for (Node child : this.tileSet.getChildren()) {
            if (child != collisionGroup
                    && ((CollisionGroupNode) child).getName().equals(name)) {
                match = true;
            }
        }
        if (!match) {
            Collections.replaceAll(this.newTileCollisionGroups, name, "");
        }
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.tileSet.removeCollisionGroup(this.collisionGroup);
        this.tileSet.getModel().setSelection(this.newSelection);
        this.tileSet.setTileCollisionGroups(this.newTileCollisionGroups);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.tileSet.removeCollisionGroup(this.collisionGroup);
        this.tileSet.getModel().setSelection(this.newSelection);
        this.tileSet.setTileCollisionGroups(this.newTileCollisionGroups);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.tileSet.addCollisionGroup(this.collisionGroup);
        this.tileSet.getModel().setSelection(this.oldSelection);
        this.tileSet.setTileCollisionGroups(this.oldTileCollisionGroups);
        return Status.OK_STATUS;
    }

}
