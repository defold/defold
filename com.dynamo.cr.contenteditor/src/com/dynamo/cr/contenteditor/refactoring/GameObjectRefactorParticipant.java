package com.dynamo.cr.contenteditor.refactoring;

import java.util.List;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.contenteditor.Activator;
import com.dynamo.cr.editor.core.IResourceRefactorParticipant;
import com.dynamo.cr.editor.core.ResourceRefactorContext;
import com.dynamo.gameobject.proto.GameObject.ComponentDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc.Builder;
import com.google.protobuf.TextFormat;
import com.google.protobuf.TextFormat.ParseException;

public class GameObjectRefactorParticipant implements
        IResourceRefactorParticipant {

    public GameObjectRefactorParticipant() {
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

            boolean changed = false;
            List<ComponentDesc> originalList = prototype.getComponentsList();
            for (ComponentDesc componentDesc : originalList) {
                IFile componentFile = contentRoot.getFile(new Path(componentDesc.getComponent()));
                if (reference.equals(componentFile)) {
                    changed = true;
                }
                else {
                    newBuilder.addComponents(componentDesc);
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
