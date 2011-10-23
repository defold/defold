package com.dynamo.cr.scene.resource;

import java.nio.FloatBuffer;

import com.dynamo.cr.scene.math.AABB;
import com.dynamo.tile.TileSetUtil;
import com.dynamo.tile.proto.Tile.TileCell;
import com.dynamo.tile.proto.Tile.TileGrid;
import com.dynamo.tile.proto.Tile.TileLayer;
import com.dynamo.tile.proto.Tile.TileSet;
import com.sun.opengl.util.BufferUtil;
import com.sun.opengl.util.texture.Texture;

public class TileGridResource extends Resource {

    private TileGrid tileGrid;
    private TileSetResource tileSetResource;
    private AABB aabb;
    private FloatBuffer vertexBuffer;

    public TileGridResource(String path, TileGrid tileGrid, TileSetResource tileSetResource) {
        super(path);
        this.tileGrid = tileGrid;
        this.tileSetResource = tileSetResource;
        this.aabb = null;
        this.vertexBuffer = null;
    }

    public TileGrid getTileGrid() {
        return tileGrid;
    }

    public void setTileGrid(TileGrid tileGrid) {
        this.tileGrid = tileGrid;
        this.aabb = null;
        this.vertexBuffer = null;
    }

    public TileSetResource getTileSetResource() {
        return this.tileSetResource;
    }

    public void setTileSetResource(TileSetResource tileSetResource) {
        this.tileSetResource = tileSetResource;
        this.aabb = null;
        this.vertexBuffer = null;
    }

    public AABB getAABB() {
        if (this.aabb == null) {
            calculateAABB();
        }
        return this.aabb;
    }

    public FloatBuffer getVertexBuffer() {
        if (this.vertexBuffer == null) {
            calculateVertexBuffer();
        }
        return this.vertexBuffer;
    }

    private void calculateAABB() {
        if (this.tileGrid != null && this.tileSetResource != null) {
            this.aabb = new AABB();

            int layerCount = this.tileGrid.getLayersCount();
            TileSet tileSet = this.tileSetResource.getTileSet();
            int tileWidth = tileSet.getTileWidth();
            int tileHeight = tileSet.getTileHeight();
            for (int i = 0; i < layerCount; ++i) {
                TileLayer layer = this.tileGrid.getLayers(i);
                float z = layer.getZ();
                int cellCount = layer.getCellCount();
                for (int j = 0; j < cellCount; ++j) {
                    TileCell cell = layer.getCell(j);
                    int x = cell.getX();
                    int y = cell.getY();
                    this.aabb.union(x * tileWidth, y * tileHeight, z);
                    ++x; ++y;
                    this.aabb.union(x * tileWidth, y * tileHeight, z);
                }
            }
        }
    }

    private void calculateVertexBuffer() {
        if (this.tileGrid != null && this.tileSetResource != null) {
            int layerCount = this.tileGrid.getLayersCount();
            TileSet tileSet = this.tileSetResource.getTileSet();
            Texture texture = this.tileSetResource.getTextureResource().getTexture();
            int tileWidth = tileSet.getTileWidth();
            int tileHeight = tileSet.getTileHeight();
            int tileMargin = tileSet.getTileMargin();
            int tileSpacing = tileSet.getTileSpacing();
            int tileColumnCount = TileSetUtil.calculateTileCount(tileWidth, texture.getImageWidth(), tileMargin, tileSpacing);
            int tileRowCount = TileSetUtil.calculateTileCount(tileHeight, texture.getImageHeight(), tileMargin, tileSpacing);

            // VBO
            int totalCellCount = 0;
            for (int i = 0; i < layerCount; ++i) {
                totalCellCount += this.tileGrid.getLayers(i).getCellCount();
            }
            if (totalCellCount > 0) {
                final int vertexCount = totalCellCount * 4;
                final int componentCount = 5;
                final int vertexBufferSize = vertexCount * componentCount;
                final float recipImageWidth = 1.0f / texture.getImageWidth();
                final float recipImageHeight = 1.0f / texture.getImageHeight();
                this.vertexBuffer = BufferUtil.newFloatBuffer(vertexBufferSize);
                FloatBuffer v = this.vertexBuffer;
                for (int i = 0; i < layerCount; ++i) {
                    TileLayer layer = this.tileGrid.getLayers(i);
                    float z = layer.getZ();
                    int cellCount = layer.getCellCount();
                    for (int j = 0; j < cellCount; ++j) {
                        TileCell cell = layer.getCell(j);
                        int x = cell.getX();
                        int y = cell.getY();
                        float x0 = x * tileWidth;
                        float y0 = y * tileHeight;
                        float x1 = x0 + tileWidth;
                        float y1 = y0 + tileHeight;

                        int tile = cell.getTile();
                        x = tile % tileColumnCount;
                        y = tile / tileRowCount;
                        float u0 = (x * (tileSpacing + 2*tileMargin + tileWidth) + tileMargin) * recipImageWidth;
                        float u1 = u0 + tileWidth * recipImageWidth;
                        float v1 = (y * (tileSpacing + 2*tileMargin + tileHeight) + tileMargin) * recipImageHeight;
                        float v0 = v1 + tileHeight * recipImageHeight;

                        v.put(u0); v.put(v0); v.put(x0); v.put(y0); v.put(z);
                        v.put(u0); v.put(v1); v.put(x0); v.put(y1); v.put(z);
                        v.put(u1); v.put(v1); v.put(x1); v.put(y1); v.put(z);
                        v.put(u1); v.put(v0); v.put(x1); v.put(y0); v.put(z);
                    }
                }
                v.flip();
            }
        }
    }
}
