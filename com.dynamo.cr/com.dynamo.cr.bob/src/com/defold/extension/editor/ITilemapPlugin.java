package com.defold.extension.editor;

public interface ITilemapPlugin {

    public void onClearTile(int x, int y);
    public void onPaintTile(int x, int y, int tile);
}