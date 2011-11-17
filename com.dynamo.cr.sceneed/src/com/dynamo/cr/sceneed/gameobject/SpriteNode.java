package com.dynamo.cr.sceneed.gameobject;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.MultiStatus;

import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.Activator;
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

    public SpriteNode() {
        super();
    }

    public String getTexture() {
        return this.texture;
    }

    public void setTexture(String texture) {
        if (!this.texture.equals(texture)) {
            this.texture = texture;
            notifyChange();
        }
    }

    public float getWidth() {
        return this.width;
    }

    public void setWidth(float width) {
        if (this.width != width) {
            this.width = width;
            notifyChange();
        }
    }

    public float getHeight() {
        return this.height;
    }

    public void setHeight(float height) {
        if (this.height != height) {
            this.height = height;
            notifyChange();
        }
    }

    public int getTileWidth() {
        return this.tileWidth;
    }

    public void setTileWidth(int tileWidth) {
        if (this.tileWidth != tileWidth) {
            this.tileWidth = tileWidth;
            notifyChange();
        }
    }

    public int getTileHeight() {
        return this.tileHeight;
    }

    public void setTileHeight(int tileHeight) {
        if (this.tileHeight != tileHeight) {
            this.tileHeight = tileHeight;
            notifyChange();
        }
    }

    public int getTilesPerRow() {
        return this.tilesPerRow;
    }

    public void setTilesPerRow(int tilesPerRow) {
        if (this.tilesPerRow != tilesPerRow) {
            this.tilesPerRow = tilesPerRow;
            notifyChange();
        }
    }

    public int getTileCount() {
        return this.tileCount;
    }

    public void setTileCount(int tileCount) {
        if (this.tileCount != tileCount) {
            this.tileCount = tileCount;
            notifyChange();
        }
    }

    @Override
    public String getTypeId() {
        return "sprite";
    }

    @Override
    public String getTypeName() {
        return "Sprite";
    }

    @Override
    public IStatus doValidate() {
        IStatus status = validateProperties(new String[] {"texture"});
        MultiStatus multiStatus= null;
        if (status.isMultiStatus()) {
            multiStatus = (MultiStatus)status;
        } else {
            multiStatus = new MultiStatus(Activator.PLUGIN_ID, 0, null, null);
            multiStatus.merge(status);
        }
        multiStatus.merge(super.doValidate());
        return multiStatus;
    }

}
