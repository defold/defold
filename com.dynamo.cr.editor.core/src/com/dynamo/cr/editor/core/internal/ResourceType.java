package com.dynamo.cr.editor.core.internal;

import java.io.StringReader;
import java.lang.reflect.Method;

import com.dynamo.cr.editor.core.IResourceType;
import com.dynamo.cr.editor.core.IResourceTypeEditSupport;
import com.google.protobuf.Descriptors.Descriptor;
import com.google.protobuf.GeneratedMessage.Builder;
import com.google.protobuf.GeneratedMessage;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class ResourceType implements IResourceType {

    private String id;
    private String name;
    private String fileExtension;
    private byte[] templateData;
    private Class<GeneratedMessage> messageClass;
    private boolean embeddable;
    private IResourceTypeEditSupport editSupport;

    public ResourceType(String id, String name, String fileExtension,
            String templateData, Class<GeneratedMessage> messageClass, boolean embeddable, IResourceTypeEditSupport editSupport) {
        this.id = id;
        this.name = name;
        this.fileExtension = fileExtension;
        if (templateData != null)
            this.templateData = templateData.getBytes();
        this.messageClass = messageClass;
        this.embeddable = embeddable;
        this.editSupport = editSupport;
    }

    @Override
    public String getId() {
        return id;
    }

    @Override
    public String getName() {
        return name;
    }

    @Override
    public String getFileExtension() {
        return fileExtension;
    }

    @Override
    public byte[] getTemplateData() {
        return templateData;
    }

    @Override
    public Class<GeneratedMessage> getMessageClass() {
        return messageClass;
    }

    @Override
    public Descriptor getMessageDescriptor() {
        Method m;
        try {
            m = messageClass.getDeclaredMethod("newBuilder");
            @SuppressWarnings("rawtypes")
            GeneratedMessage.Builder builder = (Builder) m.invoke(null);
            return builder.getDescriptorForType();
        } catch (Throwable e) {
            e.printStackTrace();
        }
        return null;
    }

    @Override
    public Message createTemplateMessage() {
        if (messageClass == null)
            return null;

        Method m;
        try {
            m = messageClass.getDeclaredMethod("newBuilder");
            @SuppressWarnings("rawtypes")
            GeneratedMessage.Builder builder = (Builder) m.invoke(null);

            StringReader reader = new StringReader(new String(templateData));
            TextFormat.merge(reader, builder);
            return builder.build();
        } catch (Throwable e) {
            e.printStackTrace();
        }
        return null;
    }

    @Override
    public boolean isEmbeddable() {
        return this.embeddable;
    }

    @Override
    public IResourceTypeEditSupport getEditSupport() {
        return editSupport;
    }

    @Override
    public String toString() {
        return this.name;
    }

}
