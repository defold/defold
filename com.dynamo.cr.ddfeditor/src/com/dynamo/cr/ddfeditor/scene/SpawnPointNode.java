package com.dynamo.cr.ddfeditor.scene;

import com.dynamo.cr.go.core.ComponentTypeNode;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Resource;

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

}
