package com.dynamo.cr.tileeditor.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.StructuredSelection;

import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.NodeUtil;
import com.dynamo.cr.tileeditor.scene.CollisionGroupNode;
import com.dynamo.cr.tileeditor.scene.TileSetNode;

public class AddCollisionGroupNodeOperation extends AbstractOperation {

    final private TileSetNode tileSet;
    final private CollisionGroupNode collisionGroup;
    final private IStructuredSelection oldSelection;

    public AddCollisionGroupNodeOperation(TileSetNode tileSet, CollisionGroupNode collisionGroup, IPresenterContext presenterContext) {
        super("Add Collision Group");
        this.tileSet = tileSet;
        this.collisionGroup = collisionGroup;
        this.oldSelection = presenterContext.getSelection();
        String id = "default";
        id = NodeUtil.getUniqueId(this.tileSet, id, new NodeUtil.IdFetcher() {
            @Override
            public String getId(Node child) {
                return ((CollisionGroupNode) child).getId();
            }
        });
        this.collisionGroup.setId(id);
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.tileSet.addCollisionGroup(this.collisionGroup);
        this.tileSet.getModel().setSelection(new StructuredSelection(this.collisionGroup));
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.tileSet.addCollisionGroup(this.collisionGroup);
        this.tileSet.getModel().setSelection(new StructuredSelection(this.collisionGroup));
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.tileSet.removeCollisionGroup(this.collisionGroup);
        this.tileSet.getModel().setSelection(this.oldSelection);
        return Status.OK_STATUS;
    }

}
