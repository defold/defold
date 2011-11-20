package com.dynamo.cr.sceneed.go;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.ISceneView.ILoaderContext;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.sprite.proto.Sprite.SpriteDesc;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

/**
 * Place holder class to support generic components, will be replaced with component-specific presenters in time.
 * @author rasv
 *
 */
public class ComponentLoader implements ISceneView.INodeLoader {

    @Override
    public Node load(ILoaderContext context, String type, InputStream contents) throws IOException, CoreException {
        // TODO: Solve this better through extension point
        if (type.equals("sprite")) {
            InputStreamReader reader = new InputStreamReader(contents);
            SpriteDesc.Builder builder = SpriteDesc.newBuilder();
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
        }
        return createNode(type);
    }

    @Override
    public Node createNode(String type) throws IOException, CoreException {
        // TODO: Solve this better through extension point
        if (type.equals("sprite")) {
            return new SpriteNode();
        } else {
            return new GenericComponentTypeNode(type);
        }
    }

    @Override
    public Message buildMessage(ILoaderContext context, Node node, IProgressMonitor monitor) throws IOException, CoreException {
        if (node instanceof SpriteNode) {
            SpriteDesc.Builder builder = SpriteDesc.newBuilder();
            SpriteNode sprite = (SpriteNode)node;
            builder.setTexture(sprite.getTexture());
            builder.setWidth(sprite.getWidth());
            builder.setHeight(sprite.getHeight());
            builder.setTileWidth(sprite.getTileWidth());
            builder.setTileHeight(sprite.getTileHeight());
            builder.setTilesPerRow(sprite.getTilesPerRow());
            builder.setTileCount(sprite.getTileCount());
            return builder.build();
        }
        if (monitor != null) {
            monitor.done();
        }
        return null;
    }

}
