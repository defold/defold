package com.dynamo.cr.tileeditor.scene;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.HashMap;
import java.util.Map;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.tile.proto.Tile.TileCell;
import com.dynamo.tile.proto.Tile.TileGrid;
import com.dynamo.tile.proto.Tile.TileGrid.Builder;
import com.dynamo.tile.proto.Tile.TileLayer;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class TileGridLoader implements INodeLoader<TileGridNode> {

    @Override
    public TileGridNode load(ILoaderContext context, InputStream contents)
            throws IOException, CoreException {
        InputStreamReader reader = new InputStreamReader(contents);
        Builder builder = TileGrid.newBuilder();
        TextFormat.merge(reader, builder);
        TileGrid ddf = builder.build();
        TileGridNode node = new TileGridNode();
        node.setTileSource(ddf.getTileSet());
        node.setMaterial(ddf.getMaterial());
        node.setBlendMode(ddf.getBlendMode());
        int layerCount = ddf.getLayersCount();
        for (int i = 0; i < layerCount; ++i) {
            TileLayer ddfLayer = ddf.getLayers(i);
            LayerNode layer = new LayerNode();
            layer.setId(ddfLayer.getId());
            layer.setZ(ddfLayer.getZ());
            layer.setVisible(ddfLayer.getIsVisible() != 0);
            Map<Long, Integer> tiles = new HashMap<Long, Integer>();
            int cellCount = ddfLayer.getCellCount();
            for (int j = 0; j < cellCount; ++j) {
                TileCell ddfCell = ddfLayer.getCell(j);
                tiles.put(LayerNode.toCellIndex(ddfCell.getX(), ddfCell.getY()), ddfCell.getTile());
            }
            layer.setCells(tiles);
            node.addLayer(layer);
        }
        return node;
    }

    @Override
    public Message buildMessage(ILoaderContext context, TileGridNode node,
            IProgressMonitor monitor) throws IOException, CoreException {
        return null;
    }
}
