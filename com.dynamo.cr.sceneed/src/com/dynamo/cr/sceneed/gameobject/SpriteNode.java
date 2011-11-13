package com.dynamo.cr.sceneed.gameobject;

import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.Resource;

public class SpriteNode extends ComponentTypeNode {

    @Property(isResource=true)
    @Resource
    private String texture = "";

    @Property
    private float width;

    @Property
    private float height;

    @Property
    private int tileWidth;

    @Property
    private int tileHeight;

    @Property
    private int tilesPerRow;

    @Property
    private int tileCount;

    public String getTexture() {
        return this.texture;
    }

    public void setTexture(String texture) {
        this.texture = texture;
    }

    public float getWidth() {
        return this.width;
    }

    public void setWidth(float width) {
        this.width = width;
    }

    public float getHeight() {
        return this.height;
    }

    public void setHeight(float height) {
        this.height = height;
    }

    public int getTileWidth() {
        return this.tileWidth;
    }

    public void setTileWidth(int tileWidth) {
        this.tileWidth = tileWidth;
    }

    public int getTileHeight() {
        return this.tileHeight;
    }

    public void setTileHeight(int tileHeight) {
        this.tileHeight = tileHeight;
    }

    public int getTilesPerRow() {
        return this.tilesPerRow;
    }

    public void setTilesPerRow(int tilesPerRow) {
        this.tilesPerRow = tilesPerRow;
    }

    public int getTileCount() {
        return this.tileCount;
    }

    public void setTileCount(int tileCount) {
        this.tileCount = tileCount;
    }

    @Override
    public String getTypeId() {
        return "sprite";
    }

    @Override
    public String getTypeName() {
        return "Sprite";
    }

}
