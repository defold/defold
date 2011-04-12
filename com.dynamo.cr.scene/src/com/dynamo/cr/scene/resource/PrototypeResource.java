package com.dynamo.cr.scene.resource;

import java.util.List;

import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;

public class PrototypeResource extends Resource {

    PrototypeDesc desc;
    List<Resource> componentResources;

    public PrototypeResource(String path, PrototypeDesc desc, List<Resource> componentResources) {
        super(path);
        this.desc = desc;
        this.componentResources = componentResources;
    }

    public PrototypeDesc getDesc() {
        return desc;
    }

    public void setDesc(PrototypeDesc desc) {
        this.desc = desc;
    }

    public List<Resource> getComponentResources() {
        return componentResources;
    }

    public void setComponentResources(List<Resource> componentResources) {
        this.componentResources = componentResources;
    }

}
