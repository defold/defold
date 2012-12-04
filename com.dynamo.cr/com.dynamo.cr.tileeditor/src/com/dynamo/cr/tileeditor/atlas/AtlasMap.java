package com.dynamo.cr.tileeditor.atlas;

import java.awt.image.BufferedImage;
import java.util.List;

public class AtlasMap {

    private final BufferedImage image;
    private final List<Tile> tiles;

    public static class Tile {
        private String id;
        // u, v, x, y, z
        private float[] vertices;
        private float centerX;
        private float centerY;
        public Tile(String id, float cx, float cy, float[] vertices) {
            this.centerX = cx;
            this.centerY = cy;
            this.id = id;
            this.vertices = vertices;
        }

        public float getCenterX() {
            return centerX;
        }

        public float getCenterY() {
            return centerY;
        }

        public String getId() {
            return id;
        }

        public float[] getVertices() {
            return vertices;
        }
    }

    public AtlasMap(BufferedImage image, List<Tile> tiles) {
        this.image = image;
        this.tiles = tiles;
    }

    public BufferedImage getImage() {
        return image;
    }

    public List<Tile> getTiles() {
        return tiles;
    }

}
