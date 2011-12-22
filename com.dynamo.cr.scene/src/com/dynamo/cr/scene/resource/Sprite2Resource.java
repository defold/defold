package com.dynamo.cr.scene.resource;

import java.nio.FloatBuffer;
import java.util.List;

import com.dynamo.cr.scene.math.AABB;
import com.dynamo.sprite2.proto.Sprite2.Sprite2Desc;
import com.dynamo.tile.TileSetUtil;
import com.dynamo.tile.proto.Tile.Animation;
import com.dynamo.tile.proto.Tile.TileSet;

public class Sprite2Resource extends Resource {

    private Sprite2Desc sprite;
    private TileSetResource tileSetResource;
    private AABB aabb;
    private FloatBuffer vertexBuffer;

    public Sprite2Resource(String path, Sprite2Desc sprite, TileSetResource tileSetResource) {
        super(path);
        this.sprite = sprite;
        this.tileSetResource = tileSetResource;
        this.aabb = null;
        this.vertexBuffer = null;
    }

    public Sprite2Desc getSprite() {
        return this.sprite;
    }

    public void setSprite(Sprite2Desc sprite) {
        this.sprite = sprite;
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
            if (this.vertexBuffer != null) {
                calculateAABB();
            }
        }
        return this.vertexBuffer;
    }

    private void calculateAABB() {
        if (this.sprite != null && this.tileSetResource != null) {
            this.aabb = new AABB();

            TileSet tileSet = this.tileSetResource.getTileSet();
            int tileWidth = tileSet.getTileWidth();
            int tileHeight = tileSet.getTileHeight();
            this.aabb.union(tileWidth * 0.5f, tileHeight * 0.5f, 0.0f);
            this.aabb.union(-tileWidth * 0.5f, -tileHeight * 0.5f, 0.0f);
        }
    }

    private void calculateVertexBuffer() {
        if (this.sprite != null && this.tileSetResource != null) {
            TextureResource texture = this.tileSetResource.getTextureResource();
            TileSet tileSet = this.tileSetResource.getTileSet();

            int tile = 0;
            List<Animation> animations = tileSet.getAnimationsList();
            for (Animation animation : animations) {
                if (animation.getId().equals(this.sprite.getDefaultAnimation())) {
                    tile = animation.getStartTile() - 1;
                    break;
                }
            }

            final float recipImageWidth = 1.0f / texture.getWidth();
            final float recipImageHeight = 1.0f / texture.getHeight();

            final int tileWidth = tileSet.getTileWidth();
            final int tileHeight = tileSet.getTileHeight();
            final int tileMargin = tileSet.getTileMargin();
            final int tileSpacing = tileSet.getTileSpacing();
            final int tilesPerRow = TileSetUtil.calculateTileCount(tileWidth, texture.getWidth(), tileMargin, tileSpacing);
            if (tilesPerRow == 0) {
                this.vertexBuffer = null;
                return;
            }

            float f = 0.5f;
            float x0 = -f * tileWidth;
            float x1 = f * tileWidth;
            float y0 = -f * tileHeight;
            float y1 = f * tileHeight;

            int x = tile % tilesPerRow;
            int y = tile / tilesPerRow;
            float u0 = (x * (tileSpacing + 2*tileMargin + tileWidth) + tileMargin) * recipImageWidth;
            float u1 = u0 + tileWidth * recipImageWidth;
            float v0 = (y * (tileSpacing + 2*tileMargin + tileHeight) + tileMargin) * recipImageHeight;
            float v1 = v0 + tileHeight * recipImageHeight;

            final int vertexCount = 4;
            final int componentCount = 5;
            this.vertexBuffer = FloatBuffer.allocate(vertexCount * componentCount);
            FloatBuffer v = this.vertexBuffer;
            float z = 0.0f;
            v.put(u0); v.put(v1); v.put(x0); v.put(y0); v.put(z);
            v.put(u0); v.put(v0); v.put(x0); v.put(y1); v.put(z);
            v.put(u1); v.put(v0); v.put(x1); v.put(y1); v.put(z);
            v.put(u1); v.put(v1); v.put(x1); v.put(y0); v.put(z);
            v.flip();
        }
    }

}
