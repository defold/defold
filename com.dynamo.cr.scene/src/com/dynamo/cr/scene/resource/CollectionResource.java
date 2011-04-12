package com.dynamo.cr.scene.resource;

import java.util.ArrayList;
import java.util.List;

import com.dynamo.gameobject.proto.GameObject.CollectionDesc;

public class CollectionResource extends Resource {
    private CollectionDesc desc;
    private List<CollectionResource> collectionInstances;
    private List<PrototypeResource> prototypeInstances;

    public CollectionResource(String path, CollectionDesc desc) {
        super(path);
        this.desc = desc;
        this.collectionInstances = new ArrayList<CollectionResource>();
        this.prototypeInstances = new ArrayList<PrototypeResource>();
    }

    public CollectionDesc getCollectionDesc() {
        return this.desc;
    }

    public void setCollectionDesc(CollectionDesc desc) {
        this.desc = desc;
    }

    public List<CollectionResource> getCollectionInstances() {
        return this.collectionInstances;
    }

    public void setCollectionInstances(List<CollectionResource> collectionInstances) {
        this.collectionInstances = collectionInstances;
    }

    public List<PrototypeResource> getPrototypeInstances() {
        return this.prototypeInstances;
    }

    public void setPrototypeInstances(List<PrototypeResource> prototypeInstances) {
        this.prototypeInstances = prototypeInstances;
    }

}
