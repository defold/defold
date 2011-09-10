package com.dynamo.cr.tileeditor.core;

import java.awt.Color;
import java.awt.image.BufferedImage;
import java.util.List;

import javax.vecmath.Vector3f;


public interface ITileSetView {

    void refreshProperties();

    void setCollisionGroups(List<String> collisionGroups, List<Color> colors, String[] selectedCollisionGroups);

    void setTiles(BufferedImage image, float[] v, int[] hullIndices,
            int[] hullCounts, Color[] hullColors, Vector3f hullScale);

    void clearTiles();

    void setTileHullColor(int tileIndex, Color color);

    void setDirty(boolean dirty);

}
