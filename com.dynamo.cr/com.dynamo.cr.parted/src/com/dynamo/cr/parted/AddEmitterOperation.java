package com.dynamo.cr.parted;

import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.operations.AddChildrenOperation;

public class AddEmitterOperation extends AddChildrenOperation {

    public AddEmitterOperation(ParticleFXNode node, EmitterNode emitter, IPresenterContext presenterContext) {
        super("Add Emitter", node, emitter, presenterContext);
    }

}