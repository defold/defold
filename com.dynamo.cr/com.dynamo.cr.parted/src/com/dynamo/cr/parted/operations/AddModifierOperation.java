package com.dynamo.cr.parted.operations;

import com.dynamo.cr.parted.nodes.ModifierNode;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.operations.AddChildrenOperation;

public class AddModifierOperation extends AddChildrenOperation {

    public AddModifierOperation(Node node, ModifierNode modifier, IPresenterContext presenterContext) {
        super("Add Modifier", node, modifier, presenterContext);
    }

}