package com.dynamo.cr.tileeditor.operations;

import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.operations.AddChildrenOperation;
import com.dynamo.cr.tileeditor.scene.CollisionGroupNode;
import com.dynamo.cr.tileeditor.scene.TileSetNode;

public class AddCollisionGroupNodeOperation extends AddChildrenOperation {

    public AddCollisionGroupNodeOperation(TileSetNode tileSet, CollisionGroupNode collisionGroup, IPresenterContext presenterContext) {
        super("Add Collision Group", tileSet, collisionGroup, presenterContext);
    }

}
