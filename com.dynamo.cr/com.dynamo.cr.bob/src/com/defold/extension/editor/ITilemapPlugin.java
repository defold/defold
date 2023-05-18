package com.defold.extension.editor;

import java.util.List;

import com.dynamo.gamesys.proto.Tile.TileCell;

import com.defold.extension.editor.TilemapPlugins.TilemapLayer;

public interface ITilemapPlugin {

    public List<TileCell> onClearTile(int x, int y, TilemapLayer layer);
    public List<TileCell> onPaintTile(int x, int y, TilemapLayer layer);
}