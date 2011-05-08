package com.dynamo.cr.scene.resource;

import com.dynamo.gamesystem.proto.GameSystem.CollectionProxyDesc;

public class CollectionProxyResource extends Resource {

    private CollectionProxyDesc desc;
    private Resource collectionResource;

    CollectionProxyResource(String path, CollectionProxyDesc desc, Resource collectionResource) {
        super(path);
        this.desc = desc;
        this.collectionResource = collectionResource;
    }

    public CollectionProxyDesc getCollectionProxyDesc() {
        return desc;
    }

    public void setCollectionProxyDesc(CollectionProxyDesc desc) {
        this.desc = desc;
    }

    public Resource getCollectionResource() {
        return collectionResource;
    }

    public void setCollectionResource(Resource resource) {
        this.collectionResource = resource;
    }
}
