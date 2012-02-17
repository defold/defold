package com.dynamo.cr.tileeditor.operations;

import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.NodeUtil;
import com.dynamo.cr.sceneed.core.operations.AddChildOperation;
import com.dynamo.cr.tileeditor.scene.CollisionGroupNode;
import com.dynamo.cr.tileeditor.scene.TileSetNode;

public class AddCollisionGroupNodeOperation extends AddChildOperation {

    public AddCollisionGroupNodeOperation(TileSetNode tileSet, CollisionGroupNode collisionGroup, IPresenterContext presenterContext) {
        super("Add Collision Group", tileSet, collisionGroup, presenterContext);
        String id = "default";
        id = NodeUtil.getUniqueId(tileSet.getCollisionGroups(), id, new NodeUtil.IdFetcher<CollisionGroupNode>() {
            @Override
            public String getId(CollisionGroupNode node) {
                return node.getId();
            }
        });
        collisionGroup.setId(id);
    }

}
