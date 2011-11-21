package com.dynamo.cr.ddfeditor.scene;

import org.eclipse.core.runtime.IStatus;

import com.dynamo.cr.go.core.ComponentTypeNode;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Resource;

public class CollectionProxyNode extends ComponentTypeNode {

    @Property(isResource=true)
    @Resource
    private String collection = "";

    public CollectionProxyNode() {
        super();
    }

    public String getCollection() {
        return this.collection;
    }

    public void setCollection(String collection) {
        if (!this.collection.equals(collection)) {
            this.collection = collection;
            notifyChange();
        }
    }

    @Override
    public String getTypeId() {
        return "collectionproxy";
    }

    @Override
    public String getTypeName() {
        return "Collection Proxy";
    }

    @Override
    protected IStatus doValidate() {
        return validateProperties(new String[] {"collection"});
    }
}
