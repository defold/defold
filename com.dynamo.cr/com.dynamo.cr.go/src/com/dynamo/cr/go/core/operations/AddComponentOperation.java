package com.dynamo.cr.go.core.operations;

import com.dynamo.cr.go.core.ComponentNode;
import com.dynamo.cr.go.core.GameObjectNode;
import com.dynamo.cr.go.core.RefComponentNode;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.operations.AddChildrenOperation;

public class AddComponentOperation extends AddChildrenOperation {

    public AddComponentOperation(GameObjectNode gameObject, ComponentNode component, IPresenterContext presenterContext) {
        super("Add Component", gameObject, component, presenterContext);
        if (!(component instanceof RefComponentNode)) {
            String id = gameObject.getModel().getExtension(component.getClass());
            component.setId(id);
        }
    }

}
