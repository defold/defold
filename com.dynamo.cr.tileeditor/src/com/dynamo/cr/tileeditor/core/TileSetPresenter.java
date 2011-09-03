package com.dynamo.cr.tileeditor.core;

import java.awt.image.BufferedImage;
import java.io.FileInputStream;
import java.util.ArrayList;
import java.util.List;

import javax.imageio.ImageIO;

import com.dynamo.cr.tileeditor.pipeline.ConvexHull2D;
import com.dynamo.tile.proto.Tile.TileSet;

public class TileSetPresenter {
    TileSetModel model;
    ITileSetView view;
    private BufferedImage image;
    private BufferedImage collisionImage;

    // TODO: Should be configurable
    private static final int PLANE_COUNT = 16;

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
        try {
            this.image = ImageIO.read(new FileInputStream(image));
            updateTiles();
            if (!verifyImageDimensions()) {
                this.image = null;
            }
        } catch (Exception e) {
            // TODO: Report error
        }
    }

    public void setTileWidth(int tileWidth) {
        this.model.setTileWidth(tileWidth);
        updateTiles();
    }

    public void setTileHeight(int tileHeight) {
        this.model.setTileHeight(tileHeight);
        updateTiles();
    }

    public void setTileMargin(int tileMargin) {
        this.model.setTileMargin(tileMargin);
        updateTiles();
    }

    public void setTileSpacing(int tileSpacing) {
        this.model.setTileSpacing(tileSpacing);
        updateTiles();
    }

    public void setCollision(String collision) {
        this.model.setCollision(collision);
        try {
            this.collisionImage = ImageIO.read(new FileInputStream(collision));
            if (verifyImageDimensions()) {
                updateConvexHulls();
            } else {
                this.collisionImage = null;
            }
        } catch (Exception e) {
            // TODO: Report error
        }
    }

    public void setMaterialTag(String materialTag) {
        this.model.setMaterialTag(materialTag);
    }

    private int calcTileCount(int tileSize, int imageSize) {
        int actualTileSize = (2 * this.model.getTileMargin() + this.model.getTileSpacing() + tileSize);
        if (actualTileSize != 0) {
            return (imageSize + this.model.getTileSpacing())/actualTileSize;
        } else {
            return 0;
        }
    }

    private void updateTiles() {
        if (this.image == null && this.collisionImage == null) {
            return;
        }
        int imageWidth = 0;
        int imageHeight = 0;
        if (this.image != null) {
            imageWidth = this.image.getWidth();
            imageHeight = this.image.getHeight();
        } else {
            imageWidth = this.collisionImage.getWidth();
            imageHeight = this.collisionImage.getHeight();
        }
        int tilesPerRow = calcTileCount(this.model.getTileWidth(), imageWidth);
        int tilesPerColumn = calcTileCount(this.model.getTileHeight(), imageHeight);
        int tileCount = tilesPerRow * tilesPerColumn;
        int prevTileCount = this.model.getTiles().size();
        if (tileCount != prevTileCount) {
            List<TileSetModel.Tile> tiles = new ArrayList<TileSetModel.Tile>(tileCount);
            int i;
            for (i = 0; i < tileCount && i < prevTileCount; ++i) {
                tiles.add(this.model.getTiles().get(i));
            }
            for (; i < tileCount; ++i) {
                TileSetModel.Tile tile = this.model.new Tile();
                tile.setCollisionGroup("tile");
                tiles.add(tile);
            }
            this.model.setTiles(tiles);
            if (this.collisionImage != null) {
                updateConvexHulls();
            }
        }
    }

    private boolean verifyImageDimensions() {
        if (this.image != null && this.collisionImage != null) {
            if (this.image.getWidth() != this.collisionImage.getWidth() || this.image.getHeight() != this.collisionImage.getHeight()) {
                // TODO: Report error
                return false;
            }
        }
        return true;
    }

    private void updateConvexHulls() {
        int width = this.collisionImage.getWidth();
        int height = this.collisionImage.getHeight();
        int spacing = this.model.getTileSpacing();
        int margin = this.model.getTileMargin();
        int tileWidth = this.model.getTileWidth();
        int tileHeight = this.model.getTileHeight();

        int tilesPerRow = calcTileCount(tileWidth, width);
        int tilesPerColumn = calcTileCount(tileHeight, height);
        ConvexHull2D.Point[][] points = new ConvexHull2D.Point[tilesPerRow * tilesPerColumn][];
        int pointCount = 0;
        int[] mask = new int[tileWidth * tileHeight];
        for (int row = 0; row < tilesPerColumn; ++row) {
            for (int col = 0; col < tilesPerRow; ++col) {
                int x = margin + col * (2 * margin + spacing + tileWidth);
                int y = margin + row * (2 * margin + spacing + tileHeight);
                mask = collisionImage.getAlphaRaster().getPixels(x, y, tileWidth, tileHeight, mask);
                int index = col + row * tilesPerRow;
                points[index] = ConvexHull2D.imageConvexHull(mask, tileWidth, tileHeight, PLANE_COUNT);
                TileSetModel.Tile tile = this.model.getTiles().get(index);
                tile.setConvexHullIndex(pointCount);
                tile.setConvexHullCount(points[index].length);
                pointCount += points[index].length;
            }
        }
        float[] convexHulls = new float[pointCount * 2];
        for (int row = 0; row < tilesPerRow; ++row) {
            for (int col = 0; col < tilesPerColumn; ++col) {
                int index = col + row * tilesPerRow;
                for (int i = 0; i < points.length; ++i) {
                    convexHulls[i*2 + 0] = points[index][i].getX();
                    convexHulls[i*2 + 1] = points[index][i].getY();
                }
            }
        }
        this.model.setConvexHulls(convexHulls);
    }

    public void setTileCollisionGroup(int index, String collisionGroup) {
        this.model.tiles.get(index).setCollisionGroup(collisionGroup);
    }

}
