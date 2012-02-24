package com.dynamo.cr.ddfeditor.operations;

import com.dynamo.cr.ddfeditor.scene.CollisionObjectNode;
import com.dynamo.cr.ddfeditor.scene.CollisionShapeNode;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.operations.AddChildrenOperation;

public class AddShapeNodeOperation extends AddChildrenOperation {

    public AddShapeNodeOperation(CollisionObjectNode collisionObjectNode, CollisionShapeNode shapeNode, ISceneView.IPresenterContext presenterContext) {
        super("Add Shape", collisionObjectNode, shapeNode, presenterContext);
    }

}
