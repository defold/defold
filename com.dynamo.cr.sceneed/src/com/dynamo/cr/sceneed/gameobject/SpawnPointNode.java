package com.dynamo.cr.sceneed.gameobject;

import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.Resource;

public class SpawnPointNode extends ComponentTypeNode {

    @Property(isResource=true)
    @Resource
    private String prototype = "";

    public String getPrototype() {
        return this.prototype;
    }

    public void setPrototype(String prototype) {
        this.prototype = prototype;
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
