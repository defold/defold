package com.dynamo.cr.tileeditor.atlas;

import java.awt.image.BufferedImage;
import java.util.List;

public class AtlasMap {

    private final BufferedImage image;
    private final List<Tile> tiles;

    public static class Tile {
        private String id;
        // x, y, z, u, v
        private float[] vertices;
        public Tile(String id, float[] vertices) {
            this.id = id;
            this.vertices = vertices;
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
