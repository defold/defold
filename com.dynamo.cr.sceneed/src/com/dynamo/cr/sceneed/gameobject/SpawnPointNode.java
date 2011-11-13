package com.dynamo.cr.sceneed.gameobject;

import com.dynamo.cr.properties.Property;

public class SpawnPointNode extends ComponentTypeNode {

    @Property(isResource=true)
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
