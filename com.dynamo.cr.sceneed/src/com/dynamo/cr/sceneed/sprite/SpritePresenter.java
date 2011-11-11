package com.dynamo.cr.sceneed.sprite;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.jface.viewers.IStructuredSelection;

import com.dynamo.cr.sceneed.core.INodeView;
import com.dynamo.cr.sceneed.core.NodeModel;
import com.dynamo.cr.sceneed.core.NodePresenter;
import com.dynamo.sprite.proto.Sprite.SpriteDesc;
import com.dynamo.sprite.proto.Sprite.SpriteDesc.Builder;
import com.google.inject.Inject;
import com.google.protobuf.TextFormat;

public class SpritePresenter extends NodePresenter {

    @Inject
    public SpritePresenter(NodeModel model, INodeView view) {
        super(model, view);
    }

    public void onAddAnimation() {
        // Find selected sprite
        // TODO: Support multi selection
        IStructuredSelection structuredSelection = this.model.getSelection();
        Object[] nodes = structuredSelection.toArray();
        SpriteNode sprite = null;
        for (Object node : nodes) {
            if (node instanceof SpriteNode) {
                sprite = (SpriteNode)node;
                break;
            }
        }
        this.model.executeOperation(new AddAnimationOperation(sprite));
    }

    public void onRemoveAnimation() {
        // Find selected components
        // TODO: Support multi selection
        IStructuredSelection structuredSelection = this.model.getSelection();
        Object[] nodes = structuredSelection.toArray();
        AnimationNode animation = null;
        for (Object node : nodes) {
            if (node instanceof AnimationNode) {
                animation = (AnimationNode)node;
                break;
            }
        }
        this.model.executeOperation(new RemoveAnimationOperation(animation));
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
        // TODO: Fill out sprite correctly
        sprite.setTileSet("");
        this.model.setRoot(sprite);
    }

}
