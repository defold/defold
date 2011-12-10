package com.dynamo.cr.tileeditor.scene;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.StructuredSelection;

import com.dynamo.cr.sceneed.core.ISceneView.INodePresenter;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.tileeditor.operations.AddCollisionGroupNodeOperation;
import com.dynamo.cr.tileeditor.operations.RemoveCollisionGroupNodeOperation;
import com.dynamo.cr.tileeditor.operations.SetTileCollisionGroupsOperation;

public class TileSetNodePresenter implements INodePresenter<TileSetNode> {
    // Used for painting collision groups onto tiles (convex hulls)
    // TODO remove this state, should be handled by a painter tool with state
    private List<String> oldTileCollisionGroups;
    private List<String> newTileCollisionGroups;
    private String currentCollisionGroup;

    public void onBeginPaintTile(IPresenterContext presenterContext) {
        IStructuredSelection selection = presenterContext.getSelection();
        TileSetNode tileSet = TileSetUtil.getCurrentTileSet(selection);
        this.oldTileCollisionGroups = tileSet.getTileCollisionGroups();
        this.newTileCollisionGroups = new ArrayList<String>(this.oldTileCollisionGroups);
        this.currentCollisionGroup = "";
        CollisionGroupNode collisionGroup = TileSetUtil.getCurrentCollisionGroup(selection);
        if (collisionGroup != null) {
            this.currentCollisionGroup = collisionGroup.getId();
        }
    }

    public void onEndPaintTile(IPresenterContext presenterContext) {
        if (this.currentCollisionGroup != null) {
            IStructuredSelection selection = presenterContext.getSelection();
            TileSetNode tileSet = TileSetUtil.getCurrentTileSet(selection);
            if (!this.oldTileCollisionGroups.equals(this.newTileCollisionGroups)) {
                presenterContext.executeOperation(new SetTileCollisionGroupsOperation(
                        tileSet, this.oldTileCollisionGroups,
                        this.newTileCollisionGroups,
                        this.currentCollisionGroup));
            }
            this.currentCollisionGroup = null;
        }
    }

    public void onPaintTile(IPresenterContext presenterContext, int index) {
        if (this.currentCollisionGroup != null) {
            IStructuredSelection selection = presenterContext.getSelection();
            TileSetNode tileSet = TileSetUtil.getCurrentTileSet(selection);
            if (!this.newTileCollisionGroups.get(index).equals(this.currentCollisionGroup)) {
                this.newTileCollisionGroups.set(index, this.currentCollisionGroup);
                tileSet.setTileCollisionGroups(this.newTileCollisionGroups);
                presenterContext.refreshView();
            }
        }
    }

    public void onAddCollisionGroup(IPresenterContext presenterContext) {
        TileSetNode tileSet = TileSetUtil.getCurrentTileSet(presenterContext.getSelection());
        presenterContext.executeOperation(new AddCollisionGroupNodeOperation(tileSet, new CollisionGroupNode(), presenterContext));
    }

    public void onRemoveCollisionGroup(IPresenterContext presenterContext) {
        CollisionGroupNode collisionGroup = TileSetUtil.getCurrentCollisionGroup(presenterContext.getSelection());
        presenterContext.executeOperation(new RemoveCollisionGroupNodeOperation(collisionGroup, presenterContext));
    }

    public void onSelectCollisionGroup(IPresenterContext presenterContext, int index) {
        TileSetNode tileSet = TileSetUtil.getCurrentTileSet(presenterContext.getSelection());
        if (tileSet != null) {
            Node selected = tileSet;
            List<CollisionGroupNode> collisionGroups = tileSet.getCollisionGroups();
            if (index >= 0 && index < collisionGroups.size()) {
                selected = collisionGroups.get(index);
            }
            IStructuredSelection selection = new StructuredSelection(selected);
            if (!selection.equals(presenterContext.getSelection())) {
                presenterContext.setSelection(selection);
                presenterContext.refreshView();
            }
        }
    }

}
