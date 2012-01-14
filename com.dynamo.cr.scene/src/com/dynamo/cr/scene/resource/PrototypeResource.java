package com.dynamo.cr.scene.resource;

import java.util.List;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;

public class PrototypeResource extends Resource {

    PrototypeDesc desc;
    List<Resource> componentResources;
    private List<Vector4d> componentTranslations;
    private List<Quat4d> componentRotations;

    public PrototypeResource(String path, PrototypeDesc desc, List<Resource> componentResources, List<Vector4d> componentTranslations, List<Quat4d> componentRotations) {
        super(path);
        this.desc = desc;
        this.componentResources = componentResources;
        this.componentTranslations = componentTranslations;
        this.componentRotations = componentRotations;
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

    public List<Vector4d> getComponentTranslations() {
        return componentTranslations;
    }

    public List<Quat4d> getComponentRotations() {
        return componentRotations;
    }

}
