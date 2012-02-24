package com.dynamo.cr.ddfeditor.scene;

import com.dynamo.cr.go.core.ComponentTypeNode;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Resource;

@SuppressWarnings("serial")
public class SpawnPointNode extends ComponentTypeNode {

    @Property(isResource=true)
    @Resource
    @NotEmpty
    private String prototype = "";

    public SpawnPointNode() {
        super();
    }

    public String getPrototype() {
        return this.prototype;
    }

    public void setPrototype(String prototype) {
        this.prototype = prototype;
    }

}
