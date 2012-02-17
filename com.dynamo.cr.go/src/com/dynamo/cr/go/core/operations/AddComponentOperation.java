package com.dynamo.cr.go.core.operations;

import com.dynamo.cr.go.core.ComponentNode;
import com.dynamo.cr.go.core.GameObjectNode;
import com.dynamo.cr.go.core.RefComponentNode;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.NodeUtil;
import com.dynamo.cr.sceneed.core.operations.AddChildOperation;

public class AddComponentOperation extends AddChildOperation {

    public AddComponentOperation(GameObjectNode gameObject, ComponentNode component, IPresenterContext presenterContext) {
        super("Add Component", gameObject, component, presenterContext);
        String id = "";
        if (component instanceof RefComponentNode) {
            String path = ((RefComponentNode)component).getComponent();
            int index = path.lastIndexOf('.');
            if (index >= 0) {
                id = path.substring(index + 1);
            }
        } else {
            id = gameObject.getModel().getExtension(component.getClass());
        }
        id = NodeUtil.getUniqueId(gameObject, id, new NodeUtil.IdFetcher<Node>() {
            @Override
            public String getId(Node child) {
                return ((ComponentNode)child).getId();
            }
        });
        component.setId(id);
    }

}
