package com.dynamo.cr.scene.resource;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.scene.graph.CreateException;
import com.dynamo.sprite2.proto.Sprite2.Sprite2Desc;
import com.dynamo.sprite2.proto.Sprite2.Sprite2Desc.Builder;
import com.google.protobuf.TextFormat;

public class Sprite2Loader implements IResourceLoader {

    @Override
    public Resource load(IProgressMonitor monitor, String name,
            InputStream stream, IResourceFactory factory)
                    throws IOException, CreateException, CoreException {

        InputStreamReader reader = new InputStreamReader(stream);
        Builder builder = Sprite2Desc.newBuilder();
        TextFormat.merge(reader, builder);
        Sprite2Desc sprite = builder.build();
        return new Sprite2Resource(name, sprite, (TileSetResource)factory.load(monitor, sprite.getTileSet()));
    }

    @Override
    public void reload(Resource resource, IProgressMonitor monitor, String name,
            InputStream stream, IResourceFactory factory)
                    throws IOException, CreateException, CoreException {
        InputStreamReader reader = new InputStreamReader(stream);
        Builder builder = Sprite2Desc.newBuilder();
        TextFormat.merge(reader, builder);
        Sprite2Desc sprite = builder.build();
        Sprite2Resource spriteResource = (Sprite2Resource)resource;
        spriteResource.setSprite(sprite);
        spriteResource.setTileSetResource((TileSetResource)factory.load(monitor, sprite.getTileSet()));
    }
}
