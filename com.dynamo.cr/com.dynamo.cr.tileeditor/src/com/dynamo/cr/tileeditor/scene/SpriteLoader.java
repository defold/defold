package com.dynamo.cr.tileeditor.scene;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;

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

        ArrayList<String> textures = new ArrayList<String>();
        textures.add(node.getTexture0());
        textures.add(node.getTexture1());
        textures.add(node.getTexture2());
        textures.add(node.getTexture3());
        textures.add(node.getTexture4());
        textures.add(node.getTexture5());
        textures.add(node.getTexture6());
        textures.add(node.getTexture7());
        int tc = 7;
        for(; tc > 0; --tc) {
            if(!textures.get(tc).isEmpty()) {
                break;
            }
        }
        for(int i = 0; i <= tc; ++i) {
            builder.addTextures(textures.get(i));
        }

        return builder.build();
    }
}
