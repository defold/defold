package com.dynamo.cr.tileeditor.core;

import java.util.Set;

import com.dynamo.cr.tileeditor.core.TileSetModel.Tile;
import com.dynamo.tile.proto.Tile.TileSet;

public class TileSetPresenter {
    TileSetModel model;
    ITileSetView view;
    private int imageWidth;
    private int imageHeight;

    public TileSetPresenter(TileSetModel model, ITileSetView view) {
        this.model = model;
        this.view = view;
    }

    public void load(TileSet tileSet) {
        this.model.setImage(tileSet.getImage());
        this.view.setImage(tileSet.getImage());

        this.model.setTileWidth(tileSet.getTileWidth());
        this.view.setTileWidth(tileSet.getTileWidth());

        this.model.setTileHeight(tileSet.getTileHeight());
        this.view.setTileHeight(tileSet.getTileHeight());

        this.model.setTileCount(tileSet.getTileCount());
        this.view.setTileCount(tileSet.getTileCount());

        this.model.setTileMargin(tileSet.getTileMargin());
        this.view.setTileMargin(tileSet.getTileMargin());

        this.model.setTileSpacing(tileSet.getTileSpacing());
        this.view.setTileSpacing(tileSet.getTileSpacing());

        this.model.setCollision(tileSet.getCollision());
        this.view.setCollision(tileSet.getCollision());

        this.model.setMaterialTag(tileSet.getMaterialTag());
        this.view.setMaterialTag(tileSet.getMaterialTag());
    }

    public void setImage(String image) {
        this.model.setImage(image);
        // temp, to be read from file
        this.imageWidth = 32;
        this.imageHeight = 64;
    }

    public void setTileWidth(int tileWidth) {
        this.model.setTileWidth(tileWidth);
    }

    public void setTileHeight(int tileHeight) {
        this.model.setTileHeight(tileHeight);
    }

    public void setTileCount(int tileCount) {
        this.model.setTileCount(tileCount);
    }

    public void setTileMargin(int tileMargin) {
        this.model.setTileMargin(tileMargin);
    }

    public void setTileSpacing(int tileSpacing) {
        this.model.setTileSpacing(tileSpacing);
    }

    public void setCollision(String collision) {
        this.model.setCollision(collision);
    }

    public void setMaterialTag(String materialTag) {
        this.model.setMaterialTag(materialTag);
    }

    public void selectTile(int row, int column) {
        int tilesPerRow = this.imageWidth / this.model.getTileWidth();
        if (0 <= column && column < tilesPerRow && 0 <= row && row * tilesPerRow + column < this.model.getTileCount()) {
            Set<Tile> selectedTiles = this.model.getSelectedTiles();
            selectedTiles.clear();
            selectedTiles.add(this.model.new Tile(row, column));
            this.model.setSelectedTiles(selectedTiles);
            this.view.setSelectedTiles(selectedTiles);
        }
    }

    public void setTileCollisionGroup(String collisionGroup) {
        for (TileSetModel.Tile tile : this.model.getSelectedTiles()) {
            tile.setCollisionGroup(collisionGroup);
        }
    }

}
