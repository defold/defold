package com.dynamo.cr.tileeditor.operations;

import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.NodeUtil;
import com.dynamo.cr.sceneed.core.operations.AddChildrenOperation;
import com.dynamo.cr.tileeditor.scene.AnimationNode;
import com.dynamo.cr.tileeditor.scene.TileSetNode;

public class AddAnimationNodeOperation extends AddChildrenOperation {

    public AddAnimationNodeOperation(TileSetNode tileSet, AnimationNode animation, IPresenterContext presenterContext) {
        super(Messages.AddAnimationNodeOperation_label, tileSet, animation, presenterContext);
        String id = "anim"; //$NON-NLS-1$
        id = NodeUtil.<AnimationNode>getUniqueId(tileSet.getAnimations(), id, new NodeUtil.IdFetcher<AnimationNode>() {
            @Override
            public String getId(AnimationNode node) {
                return node.getId();
            }
        });
        animation.setId(id);
    }

}
