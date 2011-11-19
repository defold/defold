package com.dynamo.cr.ddfeditor.scene;

import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.go.ComponentTypeNode;
import com.dynamo.cr.sceneed.ui.Resource;

public class SpawnPointNode extends ComponentTypeNode {

    @Property(isResource=true)
    @Resource
    private String prototype = "";

    public SpawnPointNode() {
        super();
    }

    public String getPrototype() {
        return this.prototype;
    }

    public void setPrototype(String prototype) {
        if (!this.prototype.equals(prototype)) {
            this.prototype = prototype;
            notifyChange();
        }
    }

    @Override
    public String getTypeId() {
        return "spawnpoint";
    }

    @Override
    public String getTypeName() {
        return "Spawn Point";
    }

}
