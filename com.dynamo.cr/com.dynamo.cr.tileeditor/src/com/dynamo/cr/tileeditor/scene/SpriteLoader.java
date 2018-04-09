package com.dynamo.cr.tileeditor.scene;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.sprite.proto.Sprite.SpriteDesc;
import com.dynamo.sprite.proto.Sprite.SpriteDesc.Builder;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class SpriteLoader implements INodeLoader<SpriteNode> {

    @Override
    public SpriteNode load(ILoaderContext context, InputStream contents)
            throws IOException, CoreException {
        InputStreamReader reader = new InputStreamReader(contents);
        Builder builder = SpriteDesc.newBuilder();
        TextFormat.merge(reader, builder);
        SpriteDesc ddf = builder.build();
        SpriteNode node = new SpriteNode();
        node.setTileSource(ddf.getTileSet());
        node.setDefaultAnimation(ddf.getDefaultAnimation());
        node.setMaterial(ddf.getMaterial());
        node.setBlendMode(ddf.getBlendMode());
        return node;
    }

    @Override
    public Message buildMessage(ILoaderContext context, SpriteNode node,
            IProgressMonitor monitor) throws IOException, CoreException {
        SpriteDesc.Builder builder = SpriteDesc.newBuilder();
        builder.setTileSet(node.getTileSource()).setDefaultAnimation(node.getDefaultAnimation())
                .setMaterial(node.getMaterial()).setBlendMode(node.getBlendMode());
        return builder.build();
    }
}
