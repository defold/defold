package com.dynamo.cr.goprot.sprite;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import com.dynamo.cr.goprot.core.INodeView;
import com.dynamo.cr.goprot.core.NodeModel;
import com.dynamo.cr.goprot.core.NodePresenter;
import com.dynamo.sprite.proto.Sprite.SpriteDesc;
import com.dynamo.sprite.proto.Sprite.SpriteDesc.Builder;
import com.google.inject.Inject;
import com.google.protobuf.TextFormat;

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

    @Override
    public void onLoad(InputStream contents) throws IOException {
        // TODO: Move to loader
        InputStreamReader reader = new InputStreamReader(contents);
        Builder builder = SpriteDesc.newBuilder();
        TextFormat.merge(reader, builder);
        @SuppressWarnings("unused")
        SpriteDesc desc = builder.build();
        SpriteNode sprite = new SpriteNode();
        this.model.setRoot(sprite);
        // TODO: Fill out sprite
    }

}
