package com.dynamo.cr.ddfeditor.scene;

import com.dynamo.cr.go.core.ComponentTypeNode;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Resource;
import com.dynamo.cr.properties.Property.EditorType;

@SuppressWarnings("serial")
public class CollectionFactoryNode extends ComponentTypeNode {

    @Property(editorType=EditorType.RESOURCE, extensions={"collection"})
    @Resource
    @NotEmpty
    private String prototype = "";

    @Property
    private boolean loadDynamically = false;

    public CollectionFactoryNode() {
        super();
    }

    public String getPrototype() {
        return this.prototype;
    }

    public void setPrototype(String prototype) {
        this.prototype = prototype;
    }

    public boolean getLoadDynamically() {
        return this.loadDynamically;
    }

    public void setLoadDynamically(boolean loadDynamically) {
        this.loadDynamically = loadDynamically;
    }

}
