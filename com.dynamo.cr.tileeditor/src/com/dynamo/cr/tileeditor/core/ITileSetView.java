package com.dynamo.cr.tileeditor.core;

import java.awt.Color;
import java.awt.image.BufferedImage;
import java.util.List;

import javax.vecmath.Vector3f;


public interface ITileSetView {

    void setImageProperty(String newValue);

    void setImageTags(List<Tag> tags);

    void setTileWidthProperty(int tileWidth);

    void setTileWidthTags(List<Tag> tags);

    void setTileHeightProperty(int tileHeight);

    void setTileHeightTags(List<Tag> tags);

    void setTileMarginProperty(int tileMargin);

    void setTileSpacingProperty(int tileSpacing);

    void setCollisionProperty(String newValue);

    void setCollisionTags(List<Tag> tags);

    void setMaterialTagProperty(String newValue);

    void setMaterialTagTags(List<Tag> tags);

    void setCollisionGroups(List<String> collisionGroups, List<Color> colors);

    void setTiles(BufferedImage image, float[] v, int[] hullIndices,
            int[] hullCounts, Color[] hullColors, Vector3f hullScale);

    void clearTiles();

    void setTileHullColor(int tileIndex, Color color);

}
