package com.dynamo.cr.tileeditor.core;

import java.util.Set;

public interface ITileSetView {

    void setImage(String image);

    void setTileWidth(int tileWidth);

    void setTileHeight(int tileHeight);

    void setTileMargin(int tileMargin);

    void setTileSpacing(int tileSpacing);

    void setCollision(String collision);

    void setMaterialTag(String materialTag);

    void setSelectedTiles(Set<TileSetModel.Tile> selectedTiles);
}
