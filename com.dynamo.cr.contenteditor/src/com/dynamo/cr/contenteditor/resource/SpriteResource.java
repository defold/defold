package com.dynamo.cr.contenteditor.resource;

import com.dynamo.sprite.proto.Sprite.SpriteDesc;

public class SpriteResource {

    private SpriteDesc spriteDesc;

    public SpriteResource(SpriteDesc spriteDesc) {
        this.spriteDesc = spriteDesc;
    }

    public SpriteDesc getSpriteDesc() {
        return spriteDesc;
    }

}
