package com.dynamo.cr.tileeditor.core;

import java.util.ArrayList;
import java.util.List;


public class TileSetModel {
    // properties
    String image;
    int tileWidth;
    int tileHeight;
    int tileMargin;
    int tileSpacing;
    String collision;
    String materialTag;

    public class Tile {
        String collisionGroup = null;
        int convexHullIndex = 0;
        int convexHullCount = 0;

        public String getCollisionGroup() {
            return this.collisionGroup;
        }

        public void setCollisionGroup(String collisionGroup) {
            this.collisionGroup = collisionGroup;
        }

        public int getConvexHullIndex() {
            return this.convexHullIndex;
        }

        public void setConvexHullIndex(int convexHullIndex) {
            this.convexHullIndex = convexHullIndex;
        }

        public int getConvexHullCount() {
            return this.convexHullCount;
        }

        public void setConvexHullCount(int convexHullCount) {
            this.convexHullCount = convexHullCount;
        }
    }

    List<Tile> tiles;
    float[] convexHulls;

    public TileSetModel() {
        this.tiles = new ArrayList<Tile>();
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

    public float[] getConvexHulls() {
        return this.convexHulls;
    }

    public void setConvexHulls(float[] convexHulls) {
        this.convexHulls = convexHulls;
    }

}
