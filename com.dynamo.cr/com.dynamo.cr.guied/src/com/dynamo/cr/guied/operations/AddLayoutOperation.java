package com.dynamo.cr.guied.operations;

import java.util.List;

import com.dynamo.cr.guied.core.GuiSceneNode;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.operations.AddChildrenOperation;

public class AddLayoutOperation extends AddChildrenOperation {

    public AddLayoutOperation(GuiSceneNode scene, List<Node> nodes, IPresenterContext presenterContext) {
        super("Add Layout", scene.getLayoutsNode(), nodes, presenterContext);
    }

}
