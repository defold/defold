package com.dynamo.cr.ddfeditor.refactoring;

import java.util.List;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.ddfeditor.Activator;
import com.dynamo.cr.editor.core.EditorCorePlugin;
import com.dynamo.cr.editor.core.IResourceType;
import com.dynamo.cr.editor.core.ResourceRefactorContext;
import com.dynamo.cr.protobind.MessageNode;
import com.dynamo.gameobject.proto.GameObject.ComponentDesc;
import com.dynamo.gameobject.proto.GameObject.EmbeddedComponentDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc.Builder;
import com.google.protobuf.Descriptors.Descriptor;
import com.google.protobuf.DynamicMessage;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;
import com.google.protobuf.TextFormat.ParseException;

public class GameObjectRefactorParticipant extends GenericRefactorParticipant {

    public GameObjectRefactorParticipant() {
    }

    @Override
    public com.google.protobuf.Message.Builder getBuilder() {
        return PrototypeDesc.newBuilder();
    }

    @Override
    public String updateReferences(ResourceRefactorContext context,
                                   String resource, IFile reference, String newPath) throws CoreException {
        Builder builder = PrototypeDesc.newBuilder();

        IContainer contentRoot = context.getContentRoot();
        try {
            TextFormat.merge(resource, builder);
            PrototypeDesc prototype = builder.build();

            Builder newBuilder = PrototypeDesc.newBuilder(prototype);
            newBuilder.clearComponents();
            newBuilder.clearEmbeddedComponents();

            boolean changed = false;
            List<ComponentDesc> originalList = prototype.getComponentsList();
            for (ComponentDesc componentDesc : originalList) {
                IFile componentFile = contentRoot.getFile(new Path(componentDesc.getComponent()));
                if (reference.equals(componentFile)) {
                    newBuilder.addComponents(ComponentDesc.newBuilder(componentDesc).setComponent(newPath));
                    changed = true;
                }
                else {
                    newBuilder.addComponents(componentDesc);
                }
            }

            List<EmbeddedComponentDesc> originalEmbeddedList = prototype.getEmbeddedComponentsList();
            for (EmbeddedComponentDesc embeddedComponentDesc : originalEmbeddedList) {
                String type = embeddedComponentDesc.getType();
                IResourceType resourceType = EditorCorePlugin.getDefault().getResourceTypeFromExtension(type);
                if (resourceType != null) {
                    Descriptor descriptor = resourceType.getMessageDescriptor();
                    DynamicMessage.Builder embeddedBuilder = DynamicMessage.newBuilder(descriptor);
                    TextFormat.merge(embeddedComponentDesc.getData(), embeddedBuilder);
                    MessageNode node = new MessageNode(embeddedBuilder.build());
                    boolean embeddedChanged = GenericRefactorParticipant.doUpdateReferences(contentRoot, reference, newPath, node, node);
                    if (embeddedChanged) {
                        changed = true;
                        Message newEmbeddedData = node.build();
                        newBuilder.addEmbeddedComponents(EmbeddedComponentDesc.newBuilder(embeddedComponentDesc).setData(newEmbeddedData.toString()));
                    }
                    else {
                        newBuilder.addEmbeddedComponents(embeddedComponentDesc);
                    }
                }
            }

            if (changed) {
                return newBuilder.build().toString();
            }

        } catch (ParseException e) {
            throw new CoreException(new Status(IStatus.ERROR, Activator.PLUGIN_ID, e.getMessage()));
        }
        return null;
    }

    @Override
    public String deleteReferences(ResourceRefactorContext context,
            String resource, IFile reference) throws CoreException {
        Builder builder = PrototypeDesc.newBuilder();

        IContainer contentRoot = context.getContentRoot();
        try {
            TextFormat.merge(resource, builder);
            PrototypeDesc prototype = builder.build();

            Builder newBuilder = PrototypeDesc.newBuilder(prototype);
            newBuilder.clearComponents();
            newBuilder.clearEmbeddedComponents();

            boolean changed = false;
            List<ComponentDesc> originalList = prototype.getComponentsList();
            for (ComponentDesc componentDesc : originalList) {
                IFile componentFile = contentRoot.getFile(new Path(componentDesc.getComponent()));
                if (reference.equals(componentFile)) {
                    changed = true;
                    newBuilder.addComponents(ComponentDesc.newBuilder(componentDesc).setComponent(""));
                } else {
                    newBuilder.addComponents(componentDesc);
                }
            }

            List<EmbeddedComponentDesc> originalEmbeddedList = prototype.getEmbeddedComponentsList();
            for (EmbeddedComponentDesc embeddedComponentDesc : originalEmbeddedList) {
                String type = embeddedComponentDesc.getType();
                IResourceType resourceType = EditorCorePlugin.getDefault().getResourceTypeFromExtension(type);
                if (resourceType != null) {
                    Descriptor descriptor = resourceType.getMessageDescriptor();
                    DynamicMessage.Builder embeddedBuilder = DynamicMessage.newBuilder(descriptor);
                    TextFormat.merge(embeddedComponentDesc.getData(), embeddedBuilder);
                    MessageNode node = new MessageNode(embeddedBuilder.build());
                    boolean embeddedChanged = GenericRefactorParticipant.doUpdateReferences(contentRoot, reference, "", node, node);
                    if (embeddedChanged) {
                        changed = true;
                        Message newEmbeddedData = node.build();
                        newBuilder.addEmbeddedComponents(EmbeddedComponentDesc.newBuilder(embeddedComponentDesc).setData(newEmbeddedData.toString()));
                    } else {
                        newBuilder.addEmbeddedComponents(embeddedComponentDesc);
                    }
                }
            }


            if (changed) {
                return newBuilder.build().toString();
            }

        } catch (ParseException e) {
            throw new CoreException(new Status(IStatus.ERROR, Activator.PLUGIN_ID, e.getMessage()));
        }
        return null;
    }

}
