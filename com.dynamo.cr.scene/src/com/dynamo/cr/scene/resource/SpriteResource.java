package com.dynamo.cr.scene.resource;

import com.dynamo.sprite.proto.Sprite.SpriteDesc;

public class SpriteResource extends Resource {

    private SpriteDesc spriteDesc;
    private TextureResource textureResource;

    public SpriteResource(String path, SpriteDesc spriteDesc, TextureResource textureResource) {
        super(path);
        this.spriteDesc = spriteDesc;
        this.textureResource = textureResource;
    }

    public SpriteDesc getSpriteDesc() {
        return spriteDesc;
    }

    public void setSpriteDesc(SpriteDesc spriteDesc) {
        this.spriteDesc = spriteDesc;
    }

    public TextureResource getTextureResource() {
        return this.textureResource;
    }

    public void setTextureResource(TextureResource textureResource) {
        this.textureResource = textureResource;
    }

}
