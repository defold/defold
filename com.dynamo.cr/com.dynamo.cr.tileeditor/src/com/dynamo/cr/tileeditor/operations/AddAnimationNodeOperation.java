package com.dynamo.cr.tileeditor.operations;

import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.operations.AddChildrenOperation;
import com.dynamo.cr.tileeditor.scene.AnimationNode;
import com.dynamo.cr.tileeditor.scene.TileSetNode;

public class AddAnimationNodeOperation extends AddChildrenOperation {

    public AddAnimationNodeOperation(TileSetNode tileSet, AnimationNode animation, IPresenterContext presenterContext) {
        super(Messages.AddAnimationNodeOperation_label, tileSet, animation, presenterContext);
    }

}
