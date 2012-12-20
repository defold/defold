package com.dynamo.cr.tileeditor.operations;

import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.operations.AddChildrenOperation;
import com.dynamo.cr.tileeditor.scene.AtlasAnimationNode;
import com.dynamo.cr.tileeditor.scene.AtlasNode;

public class AddAnimationGroupNodeOperation extends AddChildrenOperation {

    public AddAnimationGroupNodeOperation(AtlasNode atlasNode, AtlasAnimationNode atlasAnimationNode, IPresenterContext presenterContext) {
        super("Add Animation Group", atlasNode, atlasAnimationNode, presenterContext);
    }

}
