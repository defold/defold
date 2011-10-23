package com.dynamo.cr.scene.resource;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.scene.graph.CreateException;
import com.dynamo.tile.proto.Tile.TileSet;
import com.dynamo.tile.proto.Tile.TileSet.Builder;
import com.google.protobuf.TextFormat;

public class TileSetLoader implements IResourceLoader {

    @Override
    public Resource load(IProgressMonitor monitor, String name,
            InputStream stream, IResourceFactory factory)
                    throws IOException, CreateException, CoreException {

        InputStreamReader reader = new InputStreamReader(stream);
        Builder builder = com.dynamo.tile.proto.Tile.TileSet.newBuilder();
        TextFormat.merge(reader, builder);
        TileSet tileSet = builder.build();
        return new TileSetResource(name, tileSet, (TextureResource)factory.load(monitor, tileSet.getImage()));
    }

    @Override
    public void reload(Resource resource, IProgressMonitor monitor, String name,
            InputStream stream, IResourceFactory factory)
                    throws IOException, CreateException, CoreException {
        InputStreamReader reader = new InputStreamReader(stream);
        Builder builder = TileSet.newBuilder();
        TextFormat.merge(reader, builder);
        TileSet tileSet = builder.build();
        TileSetResource tileSetResource = (TileSetResource)resource;
        tileSetResource.setTileSet(tileSet);
        tileSetResource.setTextureResource((TextureResource)factory.load(monitor, tileSet.getImage()));
    }
}
