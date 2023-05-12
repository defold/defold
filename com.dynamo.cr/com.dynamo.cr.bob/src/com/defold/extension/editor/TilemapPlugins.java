package com.defold.extension.editor;

import java.util.List;
import java.util.Map;
import java.util.ArrayList;
import com.dynamo.bob.plugin.PluginScanner;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.Project;

import com.dynamo.bob.ClassLoaderScanner;
import com.dynamo.gamesys.proto.Tile.TileCell;


public class TilemapPlugins {

    private static List<ITilemapPlugin> tilemapPlugins =  null;

    public static void init(ClassLoader loader) {
        if (tilemapPlugins != null) {
            System.out.println("TilemapPlugins already initialised");
            return;
        }
        System.out.println("initialising TilemapPlugins");


        ClassLoaderScanner scanner = new ClassLoaderScanner(loader);
        // ClassLoaderScanner.scanClassLoader(classLoader, "com.defold.extension.editor")

        try {
            tilemapPlugins = PluginScanner.getOrCreatePlugins(scanner, "com.defold.extension.editor", ITilemapPlugin.class);
        }
        catch(Exception e) {
            System.err.println("exception scanning plugins");
            tilemapPlugins = new ArrayList<>();
        }
    }

    public static List<TileCell> onClearTile(int x, int y, List<TileCell> neighbours) {
        System.out.println("onClearTile " + neighbours);
        System.out.println(neighbours.getClass().getName());
        // Class<?>[] interfaces = o.getClass().getInterfaces();
        // for (Class i : interfaces) {
        //     System.out.println("Interface: " + i.getName());
        // }

        for(TileCell cell : neighbours) {
            System.out.println("List object: " + cell);
        }
        for (ITilemapPlugin plugin : tilemapPlugins) {
            plugin.onClearTile(x, y);
        }
        return neighbours;
    }
    public static List<TileCell> onPaintTile(int x, int y, List<TileCell> neighbours, int tile) {
        System.out.println("onPaintTile " + neighbours);
        System.out.println(neighbours.getClass().getName());

        for(TileCell cell : neighbours) {
            System.out.println("List object: " + cell);
        }

        for (ITilemapPlugin plugin : tilemapPlugins) {
            plugin.onPaintTile(x, y, tile);
        }
        return neighbours;
    }
}