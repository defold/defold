package com.dynamo.cr.goprot.sprite;

import com.dynamo.cr.goprot.core.INodeView;
import com.dynamo.cr.goprot.core.NodeModel;
import com.dynamo.cr.goprot.core.NodePresenter;
import com.google.inject.Inject;

public class SpritePresenter extends NodePresenter {

    @Inject
    public SpritePresenter(NodeModel model, INodeView view) {
        super(model, view);
    }

    public void onAddAnimation(SpriteNode sprite, AnimationNode animation) {
        this.model.executeOperation(new AddAnimationOperation(sprite, animation));
    }

    public void onRemoveAnimation(SpriteNode sprite, AnimationNode animation) {
        this.model.executeOperation(new RemoveAnimationOperation(sprite, animation));
    }

}
