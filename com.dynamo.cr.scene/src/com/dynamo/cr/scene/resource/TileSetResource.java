package com.dynamo.cr.scene.resource;

import com.dynamo.tile.proto.Tile.TileSet;

public class TileSetResource extends Resource {

    private TileSet tileSet;
    private TextureResource textureResource;

    public TileSetResource(String path, TileSet tileSet, TextureResource textureResource) {
        super(path);
        this.tileSet = tileSet;
        this.textureResource = textureResource;
    }

    public TileSet getTileSet() {
        return tileSet;
    }

    public void setTileSet(TileSet tileSet) {
        this.tileSet = tileSet;
    }

    public TextureResource getTextureResource() {
        return this.textureResource;
    }

    public void setTextureResource(TextureResource textureResource) {
        this.textureResource = textureResource;
    }

}
