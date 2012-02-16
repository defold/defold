package com.dynamo.cr.go.core.operations;

import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.Path;

import com.dynamo.cr.go.core.CollectionInstanceNode;
import com.dynamo.cr.go.core.CollectionNode;
import com.dynamo.cr.go.core.GameObjectInstanceNode;
import com.dynamo.cr.go.core.InstanceNode;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.NodeUtil;
import com.dynamo.cr.sceneed.core.operations.AddChildOperation;

public class AddInstanceOperation extends AddChildOperation {

    public AddInstanceOperation(CollectionNode collection, InstanceNode instance, IPresenterContext presenterContext) {
        super((instance instanceof GameObjectInstanceNode) ? "Add Game Object" : "Add Collection", collection, instance, presenterContext);
        String id = "";
        String path = "";
        if (instance instanceof GameObjectInstanceNode) {
            path = ((GameObjectInstanceNode)instance).getGameObject();
        } else if (instance instanceof CollectionInstanceNode) {
            path = ((CollectionInstanceNode)instance).getCollection();
        }
        IPath p = new Path(path).removeFileExtension();
        id = p.lastSegment();
        id = NodeUtil.getUniqueId(collection, id, new NodeUtil.IdFetcher<Node>() {
            @Override
            public String getId(Node child) {
                return ((InstanceNode)child).getId();
            }
        });
        instance.setId(id);
    }

}
