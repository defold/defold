package com.dynamo.cr.sceneed.gameobject;

import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.Resource;

public class CollectionProxyNode extends ComponentTypeNode {

    @Property(isResource=true)
    @Resource
    private String collection = "";

    public String getCollection() {
        return this.collection;
    }

    public void setCollection(String collection) {
        this.collection = collection;
    }

    @Override
    public String getTypeId() {
        return "collectionproxy";
    }

    @Override
    public String getTypeName() {
        return "Collection Proxy";
    }

}
