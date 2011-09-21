package com.dynamo.cr.goeditor;

import com.dynamo.cr.editor.core.IResourceTypeRegistry;
import com.dynamo.gameobject.proto.GameObject.ComponentDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;

public class ResourceComponent extends Component {

    private String resource;

    public ResourceComponent(IResourceTypeRegistry resourceTypeRegistry, String resource) {
        super(resourceTypeRegistry);
        this.resource = resource;
    }

    public ResourceComponent(IResourceTypeRegistry resourceTypeRegistry, ComponentDesc component) {
        super(resourceTypeRegistry);
        this.resource = component.getComponent();
        setId(component.getId());
    }

    @Override
    public String getFileExtension() {
        int index = resource.lastIndexOf(".");
        return resource.substring(index + 1);
    }

    public String getResource() {
        return resource;
    }

    public void addComponenent(PrototypeDesc.Builder builder) {
        ComponentDesc component = ComponentDesc.newBuilder()
                .setComponent(resource)
                .setId(getId()).build();
        builder.addComponents(component);
    }

}
