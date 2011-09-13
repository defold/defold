package com.dynamo.cr.tileeditor.core;

import java.awt.Color;
import java.awt.image.BufferedImage;
import java.util.List;


public interface ITileSetView {

    public void setImage(BufferedImage image);
    public void setTileWidth(int tileWidth);
    public void setTileHeight(int tileHeight);
    public void setTileMargin(int tileMargin);
    public void setTileSpacing(int tileSpacing);
    public void setCollision(BufferedImage collision);

    public void refreshProperties();

    public void setCollisionGroups(List<String> collisionGroups, List<Color> colors, String[] selectedCollisionGroups);

    public void setHulls(float[] vertices, int[] indices, int[] counts, Color[] colors);
    public void setHullColor(int index, Color color);

    public void setDirty(boolean dirty);

    public void setValid(boolean valid);

}
