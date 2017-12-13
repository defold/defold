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
        int tc = ddf.getTexturesCount();
        node.setTexture0(tc > 0 ? ddf.getTextures(0) : "");
        node.setTexture1(tc > 1 ? ddf.getTextures(1) : "");
        node.setTexture2(tc > 2 ? ddf.getTextures(2) : "");
        node.setTexture3(tc > 3 ? ddf.getTextures(3) : "");
        node.setTexture4(tc > 4 ? ddf.getTextures(4) : "");
        node.setTexture5(tc > 5 ? ddf.getTextures(5) : "");
        node.setTexture6(tc > 6 ? ddf.getTextures(6) : "");
        node.setTexture7(tc > 7 ? ddf.getTextures(7) : "");
        return node;
    }

    @Override
    public Message buildMessage(ILoaderContext context, SpriteNode node,
            IProgressMonitor monitor) throws IOException, CoreException {
        SpriteDesc.Builder builder = SpriteDesc.newBuilder();
        builder.setTileSet(node.getTileSource()).setDefaultAnimation(node.getDefaultAnimation())
                .setMaterial(node.getMaterial()).setBlendMode(node.getBlendMode());
        builder.addTextures(node.getTexture0());
        builder.addTextures(node.getTexture1());
        builder.addTextures(node.getTexture2());
        builder.addTextures(node.getTexture3());
        builder.addTextures(node.getTexture4());
        builder.addTextures(node.getTexture5());
        builder.addTextures(node.getTexture6());
        builder.addTextures(node.getTexture7());
        return builder.build();
    }
}
