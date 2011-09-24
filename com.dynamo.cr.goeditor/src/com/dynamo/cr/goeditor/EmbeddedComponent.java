package com.dynamo.cr.goeditor;

import java.io.IOException;
import java.io.StringReader;

import com.dynamo.cr.editor.core.IResourceType;
import com.dynamo.cr.editor.core.IResourceTypeRegistry;
import com.dynamo.gameobject.proto.GameObject.EmbeddedComponentDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.google.protobuf.Descriptors.Descriptor;
import com.google.protobuf.DynamicMessage;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class EmbeddedComponent extends Component {

    private Message message;
    private String type;

    public EmbeddedComponent(IResourceTypeRegistry resourceTypeRegistry, Message message, String type) {
        super(resourceTypeRegistry);
        this.message = message;
        this.type = type;
    }

    public EmbeddedComponent(IResourceTypeRegistry resourceTypeRegistry, EmbeddedComponentDesc component) {
        super(resourceTypeRegistry);
        setId(component.getId());
        this.type = component.getType();

        IResourceType resourceType = resourceTypeRegistry.getResourceTypeFromExtension(getFileExtension());
        Descriptor protoDescriptor = resourceType.getMessageDescriptor();

        DynamicMessage.Builder builder = DynamicMessage.newBuilder(protoDescriptor);
        try {
            TextFormat.merge(new StringReader(component.getData()), builder);
            this.message = builder.build();
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    public String getFileExtension() {
        return type;
    }

    public Message getMessage() {
        return message;
    }

    public void addComponenent(PrototypeDesc.Builder builder) {
        String data = TextFormat.printToString(message);
        EmbeddedComponentDesc component = EmbeddedComponentDesc.newBuilder()
                .setId(getId())
                .setType(type)
                .setData(data)
                .build();

        builder.addEmbeddedComponents(component);
    }

    @Override
    public String toString() {
        return getId();
    }

}
