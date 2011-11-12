package com.dynamo.cr.sceneed.gameobject;

import com.dynamo.cr.properties.Property;

public class CollectionProxyNode extends ComponentTypeNode {

    @Property(isResource=true)
    private String collection = "";

    public String getCollection() {
        return this.collection;
    }

    public void setCollection(String collection) {
        this.collection = collection;
    }

    @Override
    public String getTypeName() {
        return "Collection Proxy";
    }

}
