package com.dynamo.cr.tileeditor.core;

public class TileSetUtil {
    public static int calculateTileCount(int tileSize, int imageSize, int tileMargin, int tileSpacing) {
        int actualTileSize = (2 * tileMargin + tileSpacing + tileSize);
        if (actualTileSize > 0) {
            return (imageSize + tileSpacing)/actualTileSize;
        } else {
            return 0;
        }
    }

}
