package com.dynamo.cr.guieditor.scene;

import java.util.HashMap;
import java.util.Map;

import com.dynamo.cr.guieditor.render.GuiFontResource;
import com.dynamo.cr.guieditor.render.GuiTextureResource;
import com.jogamp.opengl.util.awt.TextRenderer;
import com.jogamp.opengl.util.texture.Texture;

public class RenderResourceCollection {

    private Map<String, GuiTextureResource> textures = new HashMap<String, GuiTextureResource>();
    private Map<String, GuiFontResource> fonts = new HashMap<String, GuiFontResource>();

    public void addTextureResource(String name, GuiTextureResource textureResource) {
        textures.put(name, textureResource);
    }

    public Texture getTexture(String name) {
        GuiTextureResource textureResource = textures.get(name);
        if (textureResource != null)
            return textureResource.getTexture();
        else
            return null;
    }

    public boolean hasTexture(String name) {
        return textures.containsKey(name);
    }

    public void addFontResource(String name, GuiFontResource fontResource) {
        fonts.put(name, fontResource);
    }

    public boolean hasTextRenderer(String name) {
        return fonts.containsKey(name);
    }

    public TextRenderer getTextRenderer(String name) {
        GuiFontResource fontResource = fonts.get(name);
        if (fontResource != null)
            return fontResource.getTextRenderer();
        else
            return null;
    }
}
