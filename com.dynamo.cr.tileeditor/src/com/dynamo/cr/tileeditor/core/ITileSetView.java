package com.dynamo.cr.tileeditor.core;

import java.util.Set;

public interface ITileSetView {

    void refreshProperties();

    void refreshOutline();

    void refreshEditingView();

    void setSelectedTiles(Set<TileSetModel.Tile> selectedTiles);

}
