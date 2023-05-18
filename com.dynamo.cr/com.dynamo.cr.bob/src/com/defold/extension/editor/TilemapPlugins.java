package com.defold.extension.editor;

import java.lang.reflect.Field;

import java.util.List;
import java.util.ArrayList;
import java.util.Map;
import java.util.HashMap;
import com.dynamo.bob.plugin.PluginScanner;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.Project;

import com.dynamo.bob.ClassLoaderScanner;
import com.dynamo.gamesys.proto.Tile.TileCell;


public class TilemapPlugins {

    /**
     * Wrapper for the cells of a tilemap layer. Each cell is
     * keyed on an index created from the x and y position of
     * the cell.
     */
    public static class TilemapLayer {

        private Map<Long, Object> cells;
        private String layerId;

        public TilemapLayer(Map<Long, Object> cells, String layerId) {
            this.cells = cells;
            this.layerId = layerId;
        }

        public String getLayerId() {
            return layerId;
        }

        public long xyToIndex(int x, int y) {
            // 4294967295L = 0xFFFFFFFF
            // using 0xFFFFFFFF messes up the & operation for some reason
            return (y << Integer.SIZE) | (x & 4294967295L);
        }

        public Integer get(int x, int y) {
            Object cell = cells.get(xyToIndex(x, y));
            if (cell == null) {
                return null;
            }
            try {
                // It would be better if the (defrecord Tile) in tile_map.clj
                // implemented a shared interface so that we don't have to use
                // reflection to get the field(s) of the tile
                Class<?> cls = cell.getClass();
                Field field = cls.getField("tile");
                Long value = (Long)field.get(cell);
                return new Integer(value.intValue());
            }
            catch (Exception e) {
                System.err.println("Unable to get field: " + e.getMessage());
            }
            return null;
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
            System.err.println("Exception while scanning for tilemap plugins");
            tilemapPlugins = new ArrayList<>();
        }
    }

    public static List<TileCell> onClearTile(int x, int y, Map<Long, Object> cells, String layerId) {
        List<TileCell> changedCells = new ArrayList<>();
        if (tilemapPlugins != null) {
            TilemapLayer layer = new TilemapLayer(cells, layerId);
            for (ITilemapPlugin plugin : tilemapPlugins) {
                changedCells.addAll(plugin.onClearTile(x, y, layer));
            }
        }
        return changedCells;
    }
    public static List<TileCell> onPaintTile(int x, int y, Map<Long, Object> cells, String layerId) {
        List<TileCell> changedCells = new ArrayList<>();
        if (tilemapPlugins != null) {
            TilemapLayer layer = new TilemapLayer(cells, layerId);
            for (ITilemapPlugin plugin : tilemapPlugins) {
                changedCells.addAll(plugin.onPaintTile(x, y, layer));
            }
        }
        return changedCells;
    }
}