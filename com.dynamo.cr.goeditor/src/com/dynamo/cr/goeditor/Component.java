package com.dynamo.cr.goeditor;

import com.dynamo.cr.editor.core.IResourceTypeRegistry;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;

public abstract class Component {
    private int index = -1;
    private String id;
    protected IResourceTypeRegistry resourceTypeRegistry;

    public Component(IResourceTypeRegistry resourceTypeRegistry) {
        this.resourceTypeRegistry = resourceTypeRegistry;
    }

    public int getIndex() {
        return index;
    }

    public void setIndex(int index) {
        this.index = index;
    }

    public String getId() {
        return this.id;
    }

    public void setId(String id) {
        this.id = id;
    }

    public abstract String getFileExtension();

    public abstract void addComponenent(PrototypeDesc.Builder builder);

}
