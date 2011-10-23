package com.dynamo.cr.scene.resource;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.scene.graph.CreateException;
import com.dynamo.tile.proto.Tile.TileGrid;
import com.dynamo.tile.proto.Tile.TileGrid.Builder;
import com.google.protobuf.TextFormat;

public class TileGridLoader implements IResourceLoader {

    @Override
    public Resource load(IProgressMonitor monitor, String name,
            InputStream stream, IResourceFactory factory)
                    throws IOException, CreateException, CoreException {

        InputStreamReader reader = new InputStreamReader(stream);
        Builder builder = TileGrid.newBuilder();
        TextFormat.merge(reader, builder);
        TileGrid tileGrid = builder.build();
        return new TileGridResource(name, tileGrid, (TileSetResource)factory.load(monitor, tileGrid.getTileSet()));
    }

    @Override
    public void reload(Resource resource, IProgressMonitor monitor, String name,
            InputStream stream, IResourceFactory factory)
                    throws IOException, CreateException, CoreException {
        InputStreamReader reader = new InputStreamReader(stream);
        Builder builder = TileGrid.newBuilder();
        TextFormat.merge(reader, builder);
        TileGrid tileGrid = builder.build();
        TileGridResource tileGridResource = (TileGridResource)resource;
        tileGridResource.setTileGrid(tileGrid);
        tileGridResource.setTileSetResource((TileSetResource)factory.load(monitor, tileGrid.getTileSet()));
    }
}
