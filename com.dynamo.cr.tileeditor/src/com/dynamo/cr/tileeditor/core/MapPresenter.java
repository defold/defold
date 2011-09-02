package com.dynamo.cr.tileeditor.core;

import java.util.Set;

import com.dynamo.cr.tileeditor.core.MapModel.Tile;
import com.dynamo.tile.proto.Tile.TileMap;

public class MapPresenter {
    MapModel model;
    IMapView view;
    private int imageWidth;
    private int imageHeight;

    public MapPresenter(MapModel model, IMapView view) {
        this.model = model;
        this.view = view;
    }

    public void load(TileMap tileMap) {
        this.model.setImage(tileMap.getImage());
        this.view.setImage(tileMap.getImage());

        this.model.setTileWidth(tileMap.getTileWidth());
        this.view.setTileWidth(tileMap.getTileWidth());

        this.model.setTileHeight(tileMap.getTileHeight());
        this.view.setTileHeight(tileMap.getTileHeight());

        this.model.setTileCount(tileMap.getTileCount());
        this.view.setTileCount(tileMap.getTileCount());

        this.model.setTileMargin(tileMap.getTileMargin());
        this.view.setTileMargin(tileMap.getTileMargin());

        this.model.setTileSpacing(tileMap.getTileSpacing());
        this.view.setTileSpacing(tileMap.getTileSpacing());

        this.model.setCollision(tileMap.getCollision());
        this.view.setCollision(tileMap.getCollision());

        this.model.setMaterialTag(tileMap.getMaterialTag());
        this.view.setMaterialTag(tileMap.getMaterialTag());
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
        for (MapModel.Tile tile : this.model.getSelectedTiles()) {
            tile.setCollisionGroup(collisionGroup);
        }
    }

}
