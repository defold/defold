package com.dynamo.cr.ddfeditor.scene;

import com.dynamo.cr.go.core.ComponentTypeNode;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.properties.Resource;

@SuppressWarnings("serial")
public class CollectionProxyNode extends ComponentTypeNode {

    @Property(editorType=EditorType.RESOURCE, extensions={"collection"})
    @Resource
    @NotEmpty
    private String collection = "";

    @Property
    private boolean exclude = false;

    public String getCollection() {
        return this.collection;
    }

    public void setCollection(String collection) {
        this.collection = collection;
    }
    
    public boolean getExclude() {
    	return this.exclude;
    }
    
    public void setExclude(boolean exclude) {
    	this.exclude = exclude;
    }

    @Override
    public String toString() {
        return String.format("%s (%s)", getId(), this.collection);
    }

}
