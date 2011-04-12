package com.dynamo.cr.scene.resource;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.scene.graph.CreateException;
import com.dynamo.sprite.proto.Sprite;
import com.dynamo.sprite.proto.Sprite.SpriteDesc;
import com.dynamo.sprite.proto.Sprite.SpriteDesc.Builder;
import com.google.protobuf.TextFormat;

public class SpriteLoader implements IResourceLoader {

    @Override
    public Resource load(IProgressMonitor monitor, String name,
            InputStream stream, IResourceFactory factory)
            throws IOException, CreateException, CoreException {

        InputStreamReader reader = new InputStreamReader(stream);
        Builder builder = Sprite.SpriteDesc.newBuilder();
        TextFormat.merge(reader, builder);
        SpriteDesc desc = builder.build();
        return new SpriteResource(name, desc, (TextureResource)factory.load(monitor, desc.getTexture()));
    }

    @Override
    public void reload(Resource resource, IProgressMonitor monitor, String name,
            InputStream stream, IResourceFactory factory)
            throws IOException, CreateException, CoreException {
        InputStreamReader reader = new InputStreamReader(stream);
        Builder builder = Sprite.SpriteDesc.newBuilder();
        TextFormat.merge(reader, builder);
        SpriteDesc desc = builder.build();
        SpriteResource spriteResource = (SpriteResource)resource;
        spriteResource.setSpriteDesc(desc);
        spriteResource.setTextureResource((TextureResource)factory.load(monitor, desc.getTexture()));
    }
}
