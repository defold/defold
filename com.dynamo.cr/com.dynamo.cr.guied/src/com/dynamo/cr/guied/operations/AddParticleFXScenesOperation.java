package com.dynamo.cr.guied.operations;

import java.util.List;

import com.dynamo.cr.guied.core.GuiSceneNode;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.operations.AddChildrenOperation;

public class AddParticleFXScenesOperation extends AddChildrenOperation {

    public AddParticleFXScenesOperation(GuiSceneNode scene, List<Node> nodes, IPresenterContext presenterContext) {
        super("Add ParticleFXs", scene.getParticleFXScenesNode(), nodes, presenterContext);
    }

}
