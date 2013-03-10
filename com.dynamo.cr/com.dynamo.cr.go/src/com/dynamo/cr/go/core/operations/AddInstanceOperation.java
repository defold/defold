package com.dynamo.cr.go.core.operations;

import javax.vecmath.Point3d;

import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.Path;

import com.dynamo.cr.go.core.CollectionInstanceNode;
import com.dynamo.cr.go.core.CollectionNode;
import com.dynamo.cr.go.core.GameObjectInstanceNode;
import com.dynamo.cr.go.core.GameObjectNode;
import com.dynamo.cr.go.core.InstanceNode;
import com.dynamo.cr.go.core.RefGameObjectInstanceNode;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.operations.AddChildrenOperation;

public class AddInstanceOperation extends AddChildrenOperation {

    public AddInstanceOperation(CollectionNode collection, InstanceNode instance, IPresenterContext presenterContext) {
        super((instance instanceof GameObjectInstanceNode) ? "Add Game Object" : "Add Collection", collection, instance, presenterContext);
        Point3d position = new Point3d();
        presenterContext.getCameraFocusPoint(position);
        instance.setTranslation(position);
        if (instance instanceof GameObjectInstanceNode) {
            if (instance instanceof RefGameObjectInstanceNode) {
                RefGameObjectInstanceNode n = (RefGameObjectInstanceNode)instance;
                IPath p = new Path(n.getGameObject()).removeFileExtension();
                n.setId(p.lastSegment());
            } else if (instance instanceof GameObjectNode) {
                GameObjectNode n = (GameObjectNode)instance;
                n.setId("go");
            }
        } else if (instance instanceof CollectionInstanceNode) {
            CollectionInstanceNode n = (CollectionInstanceNode)instance;
            IPath p = new Path(n.getCollection()).removeFileExtension();
            n.setId(p.lastSegment());
        }
    }

}
