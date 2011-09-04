package com.dynamo.cr.tileeditor.core;

import java.util.Set;

public interface ITileSetView {

    void refreshProperties();

    void setSelectedTiles(Set<TileSetModel.Tile> selectedTiles);
}
