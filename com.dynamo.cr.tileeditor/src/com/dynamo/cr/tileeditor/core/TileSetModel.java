package com.dynamo.cr.tileeditor.core;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import java.util.TreeSet;


public class TileSetModel {
    // properties
    String image;
    int tileWidth;
    int tileHeight;
    int tileCount;
    int tileMargin;
    int tileSpacing;
    String collision;
    String materialTag;

    public class Tile {
        int row;
        int column;
        String collisionGroup;

        public Tile(int row, int column) {
            this.row = row;
            this.column = column;
        }

        @Override
        public boolean equals(Object o) {
            if (o instanceof Tile) {
                Tile tile = (Tile)o;
                return this.row == tile.row && this.column == tile.column;
            } else {
                return super.equals(o);
            }
        }

        public String getCollisionGroup() {
            return this.collisionGroup;
        }

        public void setCollisionGroup(String collisionGroup) {
            this.collisionGroup = collisionGroup;
        }
    }

    List<Tile> tiles;
    Set<Tile> selectedTiles;

    public TileSetModel() {
        this.tiles = new ArrayList<Tile>();
        this.selectedTiles = new TreeSet<Tile>();
    }

    public String getImage() {
        return this.image;
    }

    public void setImage(String image) {
        this.image = image;
    }

    public int getTileWidth() {
        return this.tileWidth;
    }

    public void setTileWidth(int tileWidth) {
        this.tileWidth = tileWidth;
    }

    public int getTileHeight() {
        return this.tileHeight;
    }

    public void setTileHeight(int tileHeight) {
        this.tileHeight = tileHeight;
    }

    public int getTileCount() {
        return this.tileCount;
    }

    public void setTileCount(int tileCount) {
        this.tileCount = tileCount;
    }

    public int getTileMargin() {
        return this.tileMargin;
    }

    public void setTileMargin(int tileMargin) {
        this.tileMargin = tileMargin;
    }

    public int getTileSpacing() {
        return this.tileSpacing;
    }

    public void setTileSpacing(int tileSpacing) {
        this.tileSpacing = tileSpacing;
    }

    public String getCollision() {
        return this.collision;
    }

    public void setCollision(String collision) {
        this.collision = collision;
    }

    public String getMaterialTag() {
        return this.materialTag;
    }

    public void setMaterialTag(String materialTag) {
        this.materialTag = materialTag;
    }

    public List<Tile> getTiles() {
        return this.tiles;
    }

    public void setTiles(List<Tile> tiles) {
        this.tiles = tiles;
    }

    public Set<Tile> getSelectedTiles() {
        return this.selectedTiles;
    }

    public void setSelectedTiles(Set<Tile> selectedTiles) {
        this.selectedTiles = selectedTiles;
    }

}
