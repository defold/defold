package com.dynamo.cr.go.core.refactoring;

import java.util.List;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.editor.core.GenericRefactorParticipant;
import com.dynamo.cr.editor.core.ResourceRefactorContext;
import com.dynamo.cr.go.Constants;
import com.dynamo.cr.protobind.MessageNode;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.dynamo.gameobject.proto.GameObject.EmbeddedInstanceDesc;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc.Builder;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.google.protobuf.TextFormat;
import com.google.protobuf.TextFormat.ParseException;

public class CollectionRefactorParticipant extends GenericRefactorParticipant {

    public CollectionRefactorParticipant() {
    }


    @Override
    public String updateReferences(ResourceRefactorContext context,
            String resource, IFile reference, String newPath) throws CoreException {
        Builder builder = CollectionDesc.newBuilder();

        try {
            // First use regular reference updating.
            // Then check all embedded objects.
            boolean changed = false;

            String newResource = super.updateReferences(context, resource, reference, newPath);
            if (newResource != null)
            {
                resource = newResource;
                changed = true;
            }

            TextFormat.merge(resource, builder);
            CollectionDesc collection = builder.build();

            Builder newBuilder = CollectionDesc.newBuilder(collection);
            newBuilder.clearEmbeddedInstances();

            GameObjectRefactorParticipant gorp = new GameObjectRefactorParticipant();

            List<EmbeddedInstanceDesc> originalList = collection.getEmbeddedInstancesList();
            for (EmbeddedInstanceDesc desc : originalList) {
                String newContent = gorp.updateReferences(context, desc.getData(), reference, newPath);
                if (newContent != null) {
                    EmbeddedInstanceDesc.Builder b = EmbeddedInstanceDesc.newBuilder();
                    b.mergeFrom(desc);
                    b.setData(newContent);
                    newBuilder.addEmbeddedInstances(b.build());
                    changed = true;
                } else {
                    newBuilder.addEmbeddedInstances(desc);
                }
            }
            if (changed) {
                return newBuilder.build().toString();
            }
        } catch (ParseException e) {
            throw new CoreException(new Status(IStatus.ERROR, Constants.PLUGIN_ID, e.getMessage()));
        }
        return null;
    }

    @Override
    public Builder getBuilder() {
        return CollectionDesc.newBuilder();
    }

}
