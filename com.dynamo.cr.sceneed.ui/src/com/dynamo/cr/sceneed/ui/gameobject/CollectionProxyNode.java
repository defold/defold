package com.dynamo.cr.sceneed.ui.gameobject;

import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.ui.Resource;

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

}
