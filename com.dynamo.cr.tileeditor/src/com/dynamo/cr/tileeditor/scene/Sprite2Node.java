package com.dynamo.cr.tileeditor.scene;

import com.dynamo.cr.go.core.ComponentTypeNode;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Resource;

public class Sprite2Node extends ComponentTypeNode {

    @Property(isResource=true)
    @Resource
    private String tileSet = "";

    @Property
    private String defaultAnimation = "";

    public String getTileSet() {
        return tileSet;
    }

    public void setTileSet(String tileSet) {
        if (!this.tileSet.equals(tileSet)) {
            this.tileSet = tileSet;
            notifyChange();
        }
    }

    public String getDefaultAnimation() {
        return this.defaultAnimation;
    }

    public void setDefaultAnimation(String defaultAnimation) {
        if (!this.defaultAnimation.equals(defaultAnimation)) {
            this.defaultAnimation = defaultAnimation;
            notifyChange();
        }
    }

}
