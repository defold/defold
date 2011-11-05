package com.dynamo.cr.goprot.sprite;

import com.dynamo.cr.goprot.core.Resource;
import com.dynamo.cr.goprot.gameobject.ComponentTypeNode;
import com.dynamo.cr.properties.Property;

public class SpriteNode extends ComponentTypeNode {

    @Property(isResource = true)
    @Resource
    private String tileSet = "";

    public String getTileSet() {
        return this.tileSet;
    }

    public void setTileSet(String tileSet) {
        if (this.tileSet != null ? !this.tileSet.equals(tileSet) : tileSet != null) {
            this.tileSet = tileSet;
            notifyChange();
        }
    }

    public void addAnimation(AnimationNode animation) {
        addChild(animation);
    }

    public void removeAnimation(AnimationNode animation) {
        removeChild(animation);
    }

    @Override
    public String getTypeName() {
        return "Sprite";
    }

    @Override
    public String toString() {
        return getTypeName();
    }

}
