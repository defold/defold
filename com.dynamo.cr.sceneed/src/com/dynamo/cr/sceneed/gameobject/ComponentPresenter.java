package com.dynamo.cr.sceneed.gameobject;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.CoreException;

import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.NodePresenter;
import com.dynamo.sprite.proto.Sprite.SpriteDesc;
import com.dynamo.sprite.proto.Sprite.SpriteDesc.Builder;
import com.google.protobuf.TextFormat;

/**
 * Place holder class to support generic components, will be replaced with component-specific presenters in time.
 * @author rasv
 *
 */
public class ComponentPresenter extends NodePresenter {

    @Override
    public Node load(String type, InputStream contents) throws IOException, CoreException {
        if (type.equals("sprite")) {
            InputStreamReader reader = new InputStreamReader(contents);
            Builder builder = SpriteDesc.newBuilder();
            TextFormat.merge(reader, builder);
            SpriteDesc desc = builder.build();
            SpriteNode sprite = new SpriteNode();
            sprite.setTexture(desc.getTexture());
            sprite.setWidth(desc.getWidth());
            sprite.setHeight(desc.getHeight());
            sprite.setTileWidth(desc.getTileWidth());
            sprite.setTileHeight(desc.getTileHeight());
            sprite.setTilesPerRow(desc.getTilesPerRow());
            sprite.setTileCount(desc.getTileCount());
            return sprite;
        } else {
            return new GenericComponentTypeNode(type);
        }
    }

    @Override
    public Node create(String type) throws IOException, CoreException {
        if (type.equals("sprite")) {
            return new SpriteNode();
        } else {
            return new GenericComponentTypeNode(type);
        }
    }

}
