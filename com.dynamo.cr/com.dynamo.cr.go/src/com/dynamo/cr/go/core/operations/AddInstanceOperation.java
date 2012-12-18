package com.dynamo.cr.go.core.operations;

import javax.vecmath.Point3d;

import com.dynamo.cr.go.core.CollectionNode;
import com.dynamo.cr.go.core.GameObjectInstanceNode;
import com.dynamo.cr.go.core.InstanceNode;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.operations.AddChildrenOperation;

public class AddInstanceOperation extends AddChildrenOperation {

    public AddInstanceOperation(CollectionNode collection, InstanceNode instance, IPresenterContext presenterContext) {
        super((instance instanceof GameObjectInstanceNode) ? "Add Game Object" : "Add Collection", collection, instance, presenterContext);
        Point3d position = new Point3d();
        presenterContext.getCameraFocusPoint(position);
        instance.setTranslation(position);
    }

}
