package com.dynamo.cr.tileeditor.core;

import com.dynamo.tile.proto.Tile.TileMap;

public class MapPresenter {
    MapModel model;
    IMapView view;

    public MapPresenter(MapModel model, IMapView view) {
        this.model = model;
        this.view = view;
    }

    public void setImage(String string) {

    }

    public void init(TileMap tileMap) {
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

}
