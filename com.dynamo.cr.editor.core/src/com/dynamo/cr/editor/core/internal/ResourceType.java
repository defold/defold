package com.dynamo.cr.editor.core.internal;

import java.io.StringReader;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

import com.dynamo.cr.editor.core.EditorCorePlugin;
import com.dynamo.cr.editor.core.IResourceRefactorParticipant;
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
    private String type;
    private IResourceRefactorParticipant refacorParticipant;
    private List<String> referenceTypeClasses;
    private List<String> referenceResourceTypeIds;
    private List<IResourceType> referenceResourceTypes;

    public ResourceType(String id, String name, String fileExtension,
            String templateData, Class<GeneratedMessage> messageClass, boolean embeddable, IResourceTypeEditSupport editSupport, String type, IResourceRefactorParticipant refacorParticipant, List<String> referenceTypeClasses, List<String> referenceResourceTypeIds) {
        this.id = id;
        this.name = name;
        this.fileExtension = fileExtension;
        if (templateData != null)
            this.templateData = templateData.getBytes();
        this.messageClass = messageClass;
        this.embeddable = embeddable;
        this.editSupport = editSupport;
        this.type = type;
        this.refacorParticipant = refacorParticipant;
        this.referenceTypeClasses = referenceTypeClasses;
        this.referenceResourceTypeIds = referenceResourceTypeIds;
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
    public String getTypeClass() {
        return type;
    }

    @Override
    public IResourceRefactorParticipant getResourceRefactorParticipant() {
        return refacorParticipant;
    }

    @Override
    public String toString() {
        return this.name;
    }

    @Override
    public String[] getReferenceTypeClasses() {
        return this.referenceTypeClasses.toArray(new String[referenceTypeClasses.size()]);
    }

    @Override
    public IResourceType[] getReferenceResourceTypes() {
        if (this.referenceResourceTypes == null) {
            // Resolve types
            referenceResourceTypes = new ArrayList<IResourceType>();
            for (String id : this.referenceResourceTypeIds) {
                IResourceType rt = EditorCorePlugin.getDefault().getResourceTypeFromId(id);
                if (rt != null) {
                    referenceResourceTypes.add(rt);
                }
                else {
                    System.err.println("WARNING: Unknown resource-type id: " + id);
                }
            }
        }
        return referenceResourceTypes.toArray(new IResourceType[referenceResourceTypes.size()]);
    }

    @Override
    public boolean equals(Object obj) {
        if (obj instanceof ResourceType) {
            ResourceType other = (ResourceType) obj;
            return this.id.equals(other.id);
        }
        return super.equals(obj);
    }

    @Override
    public int hashCode() {
        return id.hashCode();
    }
}

