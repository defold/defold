package com.defold.extension.editor;

import java.util.List;
import java.util.ArrayList;
import java.util.Map;
import java.util.HashMap;
import com.dynamo.bob.plugin.PluginScanner;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.Project;

import com.dynamo.bob.ClassLoaderScanner;
import com.dynamo.gamesys.proto.Tile.TileCell;
import com.dynamo.gamesys.proto.Tile.TileCell.Builder;
import com.dynamo.gamesys.proto.Tile.TileLayer;


public class TilemapPlugins {

    /**
     * Map for the TileCell objects of a TileLayer, where each cell
     * is keyed on an index created from the x and y position of
     * the cell.
     */
    public static class TileLayerCellMap extends HashMap<Long, TileCell> {

        private String layerId;

        public TileLayerCellMap(TileLayer layer) {
            for (TileCell cell : layer.getCellList()) {
                long x = (long)cell.getX();
                long y = (long)cell.getY();
                long i = (y << Integer.SIZE) | (x & 0xFFFFFFFF);
                put(i, cell);
            }
            this.layerId = layer.getId();
        }

        public String getLayerId() {
            return layerId;
        }

        public long xyToIndex(int x, int y) {
            return ((long)y << Integer.SIZE) | ((long)x & 0xFFFFFFFF);
        }

        public TileCell get(int x, int y) {
            return get(xyToIndex(x, y));
        }
    }


    private static List<ITilemapPlugin> tilemapPlugins =  null;

    public static void init(ClassLoader loader) {
        if (tilemapPlugins != null) {
            return;
        }
        ClassLoaderScanner scanner = new ClassLoaderScanner(loader);
        try {
            tilemapPlugins = PluginScanner.getOrCreatePlugins(scanner, "com.defold.extension.editor", ITilemapPlugin.class);
        }
        catch(Exception e) {
            System.err.println("exception scanning plugins");
            tilemapPlugins = new ArrayList<>();
        }
    }

    public static List<TileCell> onClearTile(int x, int y, TileLayer layer) {
        TileLayerCellMap cells = new TileLayerCellMap(layer);
        List<TileCell> changedCells = new ArrayList<>();
        for (ITilemapPlugin plugin : tilemapPlugins) {
            changedCells.addAll(plugin.onClearTile(x, y, cells));
        }
        return changedCells;
    }
    public static List<TileCell> onPaintTile(int x, int y, TileLayer layer) {
        TileLayerCellMap cells = new TileLayerCellMap(layer);
        List<TileCell> changedCells = new ArrayList<>();
        for (ITilemapPlugin plugin : tilemapPlugins) {
            changedCells.addAll(plugin.onPaintTile(x, y, cells));
        }
        return changedCells;
    }
}