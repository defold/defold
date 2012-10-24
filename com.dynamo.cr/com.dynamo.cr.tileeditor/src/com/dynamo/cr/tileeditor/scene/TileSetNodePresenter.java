package com.dynamo.cr.tileeditor.scene;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.StructuredSelection;

import com.dynamo.cr.sceneed.core.ISceneView.INodePresenter;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.operations.RemoveChildrenOperation;
import com.dynamo.cr.sceneed.core.operations.SelectOperation;
import com.dynamo.cr.tileeditor.operations.AddAnimationNodeOperation;
import com.dynamo.cr.tileeditor.operations.AddCollisionGroupNodeOperation;
import com.dynamo.cr.tileeditor.operations.SetTileCollisionGroupsOperation;
import com.dynamo.cr.tileeditor.util.Animator;

public class TileSetNodePresenter implements INodePresenter<TileSetNode> {
    // Used for painting collision groups onto tiles (convex hulls)
    // TODO remove this state, should be handled by a painter tool with state
    private List<CollisionGroupNode> oldTileCollisionGroups;
    private List<CollisionGroupNode> newTileCollisionGroups;
    private CollisionGroupNode currentCollisionGroup;
    // Used for playing animations
    private Animator animator;

    public void onBeginPaintTile(IPresenterContext presenterContext) {
        IStructuredSelection selection = presenterContext.getSelection();
        TileSetNode tileSet = TileSetUtil.getCurrentTileSet(selection);
        List<Node> collisionGroups = TileSetUtil.getCurrentCollisionGroups(selection);
        CollisionGroupNode collisionGroup = null;
        int collisionGroupCount = collisionGroups.size();
        if (collisionGroupCount == 1) {
            collisionGroup = (CollisionGroupNode)collisionGroups.get(0);
        }
        this.oldTileCollisionGroups = tileSet.getTileCollisionGroups();
        this.newTileCollisionGroups = new ArrayList<CollisionGroupNode>(this.oldTileCollisionGroups);
        this.currentCollisionGroup = null;
        if (collisionGroup != null) {
            this.currentCollisionGroup = collisionGroup;
        }
    }

    public void onEndPaintTile(IPresenterContext presenterContext) {
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

    public void onPaintTile(IPresenterContext presenterContext, int index) {
        IStructuredSelection selection = presenterContext.getSelection();
        TileSetNode tileSet = TileSetUtil.getCurrentTileSet(selection);
        if (this.newTileCollisionGroups.get(index) != this.currentCollisionGroup) {
            this.newTileCollisionGroups.set(index, this.currentCollisionGroup);
            tileSet.setTileCollisionGroups(this.newTileCollisionGroups);
            presenterContext.refreshRenderView();
        }
    }

    public void onAddCollisionGroup(IPresenterContext presenterContext) {
        TileSetNode tileSet = TileSetUtil.getCurrentTileSet(presenterContext.getSelection());
        presenterContext.executeOperation(new AddCollisionGroupNodeOperation(tileSet, new CollisionGroupNode(), presenterContext));
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
                presenterContext.executeOperation(new SelectOperation(presenterContext.getSelection(), selection, presenterContext));
            }
        }
    }

    public void onAddAnimation(IPresenterContext presenterContext) {
        TileSetNode tileSet = TileSetUtil.getCurrentTileSet(presenterContext.getSelection());
        presenterContext.executeOperation(new AddAnimationNodeOperation(tileSet, new AnimationNode(), presenterContext));
    }

    public void onRemoveAnimation(IPresenterContext presenterContext) {
        List<Node> animations = TileSetUtil.getCurrentAnimations(presenterContext.getSelection());
        presenterContext.executeOperation(new RemoveChildrenOperation(animations, presenterContext));
    }

    public void onPlayAnimation(IPresenterContext presenterContext) {
        if (this.animator == null) {
            this.animator = new Animator();
        }
        this.animator.start(presenterContext);
    }

    public void onStopAnimation(IPresenterContext presenterContext) {
        if (this.animator != null) {
            this.animator.stop();
        }
    }

}
