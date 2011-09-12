package com.dynamo.cr.tileeditor.core;

import java.awt.Color;
import java.awt.image.BufferedImage;
import java.util.List;


public interface ITileSetView {

    public void refreshProperties();

    public void setCollisionGroups(List<String> collisionGroups, List<Color> colors, String[] selectedCollisionGroups);

    public void setTileData(BufferedImage image, BufferedImage collision,
            int tileWidth, int tileHeight, int tileMargin, int tileSpacing,
            float[] hullVertices, int[] hullIndices, int[] hullCounts, Color[] hullColors);

    public void clearTiles();

    public void setTileHullColor(int tileIndex, Color color);

    public void setDirty(boolean dirty);

}
